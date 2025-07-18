//===----------------------------------------------------------------------===//
/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
 * Exceptions. See LICENSE.TXT
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file msan_interceptor.cpp
 *
 */

#include "msan_interceptor.hpp"
#include "msan_ddi.hpp"
#include "msan_origin.hpp"
#include "msan_report.hpp"
#include "msan_shadow.hpp"
#include "sanitizer_common/sanitizer_stacktrace.hpp"
#include "sanitizer_common/sanitizer_utils.hpp"
#include "ur_sanitizer_layer.hpp"

namespace ur_sanitizer_layer {
namespace msan {

MsanInterceptor::MsanInterceptor() {}

MsanInterceptor::~MsanInterceptor() {
  // We must release these objects before releasing adapters, since
  // they may use the adapter in their destructor
  for (const auto &[_, DeviceInfo] : m_DeviceMap) {
    DeviceInfo->Shadow->Destory();
  }

  m_MemBufferMap.clear();
  m_KernelMap.clear();
  m_ContextMap.clear();

  for (auto Adapter : m_Adapters) {
    getContext()->urDdiTable.Adapter.pfnRelease(Adapter);
  }
}

ur_result_t MsanInterceptor::allocateMemory(ur_context_handle_t Context,
                                            ur_device_handle_t Device,
                                            const ur_usm_desc_t *Properties,
                                            ur_usm_pool_handle_t Pool,
                                            size_t Size, AllocType Type,
                                            void **ResultPtr) {

  auto ContextInfo = getContextInfo(Context);
  std::shared_ptr<DeviceInfo> DI = Device ? getDeviceInfo(Device) : nullptr;

  uint32_t Alignment = Properties ? Properties->align : MSAN_ORIGIN_GRANULARITY;
  // Alignment must be zero or a power-of-two
  if (0 != (Alignment & (Alignment - 1))) {
    return UR_RESULT_ERROR_INVALID_ARGUMENT;
  }
  if (Alignment < MSAN_ORIGIN_GRANULARITY) {
    Alignment = MSAN_ORIGIN_GRANULARITY;
  }

  ur_usm_desc_t NewProperties;
  if (Properties) {
    NewProperties = *Properties;
    NewProperties.align = Alignment;
  } else {
    NewProperties = {UR_STRUCTURE_TYPE_USM_DESC, nullptr,
                     UR_USM_ADVICE_FLAG_DEFAULT, Alignment};
  }

  void *Allocated = nullptr;
  UR_CALL(
      SafeAllocate(Context, Device, Size, Properties, Pool, Type, &Allocated));

  *ResultPtr = Allocated;

  ContextInfo->CleanShadowSize = std::max(ContextInfo->CleanShadowSize, Size);

  bool IsHostOrSharedUSM =
      Type == AllocType::HOST_USM || Type == AllocType::SHARED_USM;
  bool DontCheckHostOrSharedUSM =
      IsHostOrSharedUSM && !getContext()->Options.MsanCheckHostAndSharedUSM;

  // For origin tracking
  HeapType HeapType;
  switch (Type) {
  case AllocType::DEVICE_USM:
    HeapType = HeapType::DeviceUSM;
    break;
  case AllocType::HOST_USM:
    HeapType = HeapType::HostUSM;
    break;
  case AllocType::SHARED_USM:
    HeapType = HeapType::SharedUSM;
    break;
  default:
    UR_LOG_L(getContext()->logger, ERR, "Unknown heap type");
    return UR_RESULT_ERROR_UNKNOWN;
  }

  StackTrace Stack = GetCurrentBacktrace();
  Origin HeapOrigin = DontCheckHostOrSharedUSM
                          ? Origin::FromRawId(0)
                          : Origin::CreateHeapOrigin(Stack, HeapType);

  // Update shadow memory
  auto EnqueuePoison = [&](const std::vector<ur_device_handle_t> &Devices) {
    u8 Value = DontCheckHostOrSharedUSM ? 0 : 0xff;
    for (ur_device_handle_t Device : Devices) {
      ManagedQueue Queue(Context, Device);
      std::shared_ptr<DeviceInfo> DI = getDeviceInfo(Device);
      DI->Shadow->EnqueuePoisonShadowWithOrigin(Queue, (uptr)Allocated, Size,
                                                Value, HeapOrigin.rawId());
    }
  };
  if (Device) { // shared/device USM
    EnqueuePoison({Device});
  } else { // host USM
    EnqueuePoison(ContextInfo->DeviceList);
  }

  UR_LOG_L(getContext()->logger, INFO,
           "AllocInfo {} (Size={}, Type={}, Origin={})", (void *)Allocated,
           Size, ToString(Type), (void *)(uptr)HeapOrigin.rawId());

  return UR_RESULT_SUCCESS;
}

ur_result_t MsanInterceptor::releaseMemory(ur_context_handle_t Context,
                                           void *Ptr) {
  return getContext()->urDdiTable.USM.pfnFree(Context, Ptr);
}

ur_result_t MsanInterceptor::preLaunchKernel(ur_kernel_handle_t Kernel,
                                             ur_queue_handle_t Queue,
                                             USMLaunchInfo &LaunchInfo) {
  auto Context = GetContext(Queue);
  auto Device = GetDevice(Queue);
  auto ContextInfo = getContextInfo(Context);
  auto DeviceInfo = getDeviceInfo(Device);

  ManagedQueue InternalQueue(Context, Device);
  if (!InternalQueue) {
    UR_LOG_L(getContext()->logger, ERR, "Failed to create internal queue");
    return UR_RESULT_ERROR_INVALID_QUEUE;
  }

  UR_CALL(prepareLaunch(DeviceInfo, InternalQueue, Kernel, LaunchInfo));

  return UR_RESULT_SUCCESS;
}

ur_result_t MsanInterceptor::postLaunchKernel(ur_kernel_handle_t Kernel,
                                              ur_queue_handle_t Queue,
                                              USMLaunchInfo &LaunchInfo) {
  // FIXME: We must use block operation here, until we support
  // urEventSetCallback
  auto Result = getContext()->urDdiTable.Queue.pfnFinish(Queue);
  UR_CALL(LaunchInfo.Data.syncFromDevice(Queue));

  if (Result == UR_RESULT_SUCCESS) {
    const auto &Report = LaunchInfo.Data.Host.Report;

    if (!Report.Flag) {
      return Result;
    }

    if (ReportUsesUninitializedValue(LaunchInfo.Data.Host.Report, Kernel)) {
      exitWithErrors();
    }
  }

  return Result;
}

ur_result_t MsanInterceptor::registerProgram(ur_program_handle_t Program) {
  ur_result_t Result = UR_RESULT_SUCCESS;

  UR_LOG_L(getContext()->logger, INFO, "registerSpirKernels");
  Result = registerSpirKernels(Program);
  if (Result != UR_RESULT_SUCCESS) {
    return Result;
  }

  UR_LOG_L(getContext()->logger, INFO, "registerDeviceGlobals");
  Result = registerDeviceGlobals(Program);
  if (Result != UR_RESULT_SUCCESS) {
    return Result;
  }

  return Result;
}

ur_result_t MsanInterceptor::unregisterProgram(ur_program_handle_t) {
  return UR_RESULT_SUCCESS;
}

ur_result_t MsanInterceptor::registerSpirKernels(ur_program_handle_t Program) {
  auto Context = GetContext(Program);
  std::vector<ur_device_handle_t> Devices = GetDevices(Program);

  for (auto Device : Devices) {
    size_t MetadataSize;
    void *MetadataPtr;
    ur_result_t Result =
        getContext()->urDdiTable.Program.pfnGetGlobalVariablePointer(
            Device, Program, kSPIR_MsanSpirKernelMetadata, &MetadataSize,
            &MetadataPtr);
    if (Result != UR_RESULT_SUCCESS) {
      continue;
    }

    const uint64_t NumOfSpirKernel = MetadataSize / sizeof(SpirKernelInfo);
    assert((MetadataSize % sizeof(SpirKernelInfo) == 0) &&
           "SpirKernelMetadata size is not correct");

    ManagedQueue Queue(Context, Device);

    std::vector<SpirKernelInfo> SKInfo(NumOfSpirKernel);
    Result = getContext()->urDdiTable.Enqueue.pfnUSMMemcpy(
        Queue, true, &SKInfo[0], MetadataPtr,
        sizeof(SpirKernelInfo) * NumOfSpirKernel, 0, nullptr, nullptr);
    if (Result != UR_RESULT_SUCCESS) {
      UR_LOG_L(getContext()->logger, ERR, "Can't read the value of <{}>: {}",
               kSPIR_MsanSpirKernelMetadata, Result);
      return Result;
    }

    auto PI = getProgramInfo(Program);
    assert(PI != nullptr && "unregistered program!");
    for (const auto &SKI : SKInfo) {
      if (SKI.Size == 0) {
        continue;
      }
      std::vector<char> KernelNameV(SKI.Size);
      Result = getContext()->urDdiTable.Enqueue.pfnUSMMemcpy(
          Queue, true, KernelNameV.data(), (void *)SKI.KernelName,
          sizeof(char) * SKI.Size, 0, nullptr, nullptr);
      if (Result != UR_RESULT_SUCCESS) {
        UR_LOG_L(getContext()->logger, ERR, "Can't read kernel name: {}",
                 Result);
        return Result;
      }

      std::string KernelName =
          std::string(KernelNameV.begin(), KernelNameV.end());
      bool CheckLocals = SKI.Flags & SanitizedKernelFlags::CHECK_LOCALS;
      bool CheckPrivates = SKI.Flags & SanitizedKernelFlags::CHECK_PRIVATES;
      bool TrackOrigins = SKI.Flags & SanitizedKernelFlags::MSAN_TRACK_ORIGINS;

      UR_LOG_L(getContext()->logger, INFO,
               "SpirKernel(name='{}', isInstrumented={}, "
               "checkLocals={}, checkPrivates={}, trackOrigins={})",
               KernelName, true, CheckLocals, CheckPrivates, TrackOrigins);

      PI->KernelMetadataMap[KernelName] =
          ProgramInfo::KernelMetada{CheckLocals, CheckPrivates, TrackOrigins};
    }
    UR_LOG_L(getContext()->logger, INFO, "Number of sanitized kernel: {}",
             PI->KernelMetadataMap.size());
  }

  return UR_RESULT_SUCCESS;
}

ur_result_t
MsanInterceptor::registerDeviceGlobals(ur_program_handle_t Program) {
  std::vector<ur_device_handle_t> Devices = GetDevices(Program);
  assert(Devices.size() != 0 && "No devices in registerDeviceGlobals");
  auto Context = GetContext(Program);
  auto ContextInfo = getContextInfo(Context);
  auto ProgramInfo = getProgramInfo(Program);
  assert(ProgramInfo != nullptr && "unregistered program!");

  for (auto Device : Devices) {
    ManagedQueue Queue(Context, Device);

    size_t MetadataSize;
    void *MetadataPtr;
    auto Result = getContext()->urDdiTable.Program.pfnGetGlobalVariablePointer(
        Device, Program, kSPIR_MsanDeviceGlobalMetadata, &MetadataSize,
        &MetadataPtr);
    if (Result != UR_RESULT_SUCCESS) {
      UR_LOG_L(getContext()->logger, INFO, "No device globals");
      continue;
    }

    const uint64_t NumOfDeviceGlobal = MetadataSize / sizeof(DeviceGlobalInfo);
    assert((MetadataSize % sizeof(DeviceGlobalInfo) == 0) &&
           "DeviceGlobal metadata size is not correct");
    std::vector<DeviceGlobalInfo> GVInfos(NumOfDeviceGlobal);
    Result = getContext()->urDdiTable.Enqueue.pfnUSMMemcpy(
        Queue, true, &GVInfos[0], MetadataPtr,
        sizeof(DeviceGlobalInfo) * NumOfDeviceGlobal, 0, nullptr, nullptr);
    if (Result != UR_RESULT_SUCCESS) {
      UR_LOG_L(getContext()->logger, ERR, "Device Global[{}] Read Failed: {}",
               kSPIR_MsanDeviceGlobalMetadata, Result);
      return Result;
    }

    auto DeviceInfo = getMsanInterceptor()->getDeviceInfo(Device);
    for (size_t i = 0; i < NumOfDeviceGlobal; i++) {
      const auto &GVInfo = GVInfos[i];

      // Only support device global USM
      if (DeviceInfo->Type == DeviceType::CPU ||
          (DeviceInfo->Type == DeviceType::GPU_PVC &&
           MsanShadowMemoryPVC::IsDeviceUSM(GVInfo.Addr)) ||
          (DeviceInfo->Type == DeviceType::GPU_DG2 &&
           MsanShadowMemoryDG2::IsDeviceUSM(GVInfo.Addr))) {
        UR_CALL(DeviceInfo->Shadow->EnqueuePoisonShadow(Queue, GVInfo.Addr,
                                                        GVInfo.Size, 0));
        ContextInfo->CleanShadowSize =
            std::max(ContextInfo->CleanShadowSize, GVInfo.Size);
      }
    }
  }

  return UR_RESULT_SUCCESS;
}

ur_result_t MsanInterceptor::insertContext(ur_context_handle_t Context,
                                           std::shared_ptr<ContextInfo> &CI) {
  std::scoped_lock<ur_shared_mutex> Guard(m_ContextMapMutex);

  if (m_ContextMap.find(Context) != m_ContextMap.end()) {
    CI = m_ContextMap.at(Context);
    return UR_RESULT_SUCCESS;
  }

  CI = std::make_shared<ContextInfo>(Context);

  // Don't move CI, since it's a return value as well
  m_ContextMap.emplace(Context, CI);

  return UR_RESULT_SUCCESS;
}

ur_result_t MsanInterceptor::eraseContext(ur_context_handle_t Context) {
  std::scoped_lock<ur_shared_mutex> Guard(m_ContextMapMutex);
  assert(m_ContextMap.find(Context) != m_ContextMap.end());
  m_ContextMap.erase(Context);
  // TODO: Remove devices in each context
  return UR_RESULT_SUCCESS;
}

ur_result_t MsanInterceptor::insertDevice(ur_device_handle_t Device,
                                          std::shared_ptr<DeviceInfo> &DI) {
  std::scoped_lock<ur_shared_mutex> Guard(m_DeviceMapMutex);

  if (m_DeviceMap.find(Device) != m_DeviceMap.end()) {
    DI = m_DeviceMap.at(Device);
    return UR_RESULT_SUCCESS;
  }

  DI = std::make_shared<DeviceInfo>(Device);

  DI->IsSupportSharedSystemUSM =
      GetDeviceUSMCapability(Device, UR_DEVICE_INFO_USM_SYSTEM_SHARED_SUPPORT);

  // Query alignment
  UR_CALL(getContext()->urDdiTable.Device.pfnGetInfo(
      Device, UR_DEVICE_INFO_MEM_BASE_ADDR_ALIGN, sizeof(DI->Alignment),
      &DI->Alignment, nullptr));

  // Don't move DI, since it's a return value as well
  m_DeviceMap.emplace(Device, DI);

  return UR_RESULT_SUCCESS;
}

ur_result_t MsanInterceptor::eraseDevice(ur_device_handle_t Device) {
  std::scoped_lock<ur_shared_mutex> Guard(m_DeviceMapMutex);
  assert(m_DeviceMap.find(Device) != m_DeviceMap.end());
  m_DeviceMap.erase(Device);
  // TODO: Remove devices in each context
  return UR_RESULT_SUCCESS;
}

ur_result_t MsanInterceptor::insertProgram(ur_program_handle_t Program) {
  std::scoped_lock<ur_shared_mutex> Guard(m_ProgramMapMutex);
  if (m_ProgramMap.find(Program) != m_ProgramMap.end()) {
    return UR_RESULT_SUCCESS;
  }
  m_ProgramMap.emplace(Program, std::make_shared<ProgramInfo>(Program));
  return UR_RESULT_SUCCESS;
}

ur_result_t MsanInterceptor::eraseProgram(ur_program_handle_t Program) {
  std::scoped_lock<ur_shared_mutex> Guard(m_ProgramMapMutex);
  assert(m_ProgramMap.find(Program) != m_ProgramMap.end());
  m_ProgramMap.erase(Program);
  return UR_RESULT_SUCCESS;
}

KernelInfo &MsanInterceptor::getOrCreateKernelInfo(ur_kernel_handle_t Kernel) {
  {
    std::shared_lock<ur_shared_mutex> Guard(m_KernelMapMutex);
    if (m_KernelMap.find(Kernel) != m_KernelMap.end()) {
      return *m_KernelMap[Kernel].get();
    }
  }

  // Create new KernelInfo
  auto PI = getProgramInfo(GetProgram(Kernel));
  assert(PI != nullptr && "unregistered program!");
  auto KI = std::make_unique<KernelInfo>(Kernel);

  KI->IsInstrumented = PI->isKernelInstrumented(Kernel);
  if (KI->IsInstrumented) {
    auto &KM = PI->getKernelMetadata(Kernel);
    KI->IsCheckLocals = KM.CheckLocals;
    KI->IsCheckPrivates = KM.CheckPrivates;
    KI->IsTrackOrigins = KM.TrackOrigins;
  }

  std::scoped_lock<ur_shared_mutex> Guard(m_KernelMapMutex);
  m_KernelMap.emplace(Kernel, std::move(KI));
  return *m_KernelMap[Kernel].get();
}

ur_result_t MsanInterceptor::eraseKernelInfo(ur_kernel_handle_t Kernel) {
  std::scoped_lock<ur_shared_mutex> Guard(m_KernelMapMutex);
  assert(m_KernelMap.find(Kernel) != m_KernelMap.end());
  m_KernelMap.erase(Kernel);
  return UR_RESULT_SUCCESS;
}

ur_result_t
MsanInterceptor::insertMemBuffer(std::shared_ptr<MemBuffer> MemBuffer) {
  std::scoped_lock<ur_shared_mutex> Guard(m_MemBufferMapMutex);
  assert(m_MemBufferMap.find(ur_cast<ur_mem_handle_t>(MemBuffer.get())) ==
         m_MemBufferMap.end());
  m_MemBufferMap.emplace(reinterpret_cast<ur_mem_handle_t>(MemBuffer.get()),
                         MemBuffer);
  return UR_RESULT_SUCCESS;
}

ur_result_t MsanInterceptor::eraseMemBuffer(ur_mem_handle_t MemHandle) {
  std::scoped_lock<ur_shared_mutex> Guard(m_MemBufferMapMutex);
  assert(m_MemBufferMap.find(MemHandle) != m_MemBufferMap.end());
  m_MemBufferMap.erase(MemHandle);
  return UR_RESULT_SUCCESS;
}

std::shared_ptr<MemBuffer>
MsanInterceptor::getMemBuffer(ur_mem_handle_t MemHandle) {
  std::shared_lock<ur_shared_mutex> Guard(m_MemBufferMapMutex);
  if (m_MemBufferMap.find(MemHandle) != m_MemBufferMap.end()) {
    return m_MemBufferMap[MemHandle];
  }
  return nullptr;
}

ur_result_t MsanInterceptor::prepareLaunch(
    std::shared_ptr<DeviceInfo> &DeviceInfo, ur_queue_handle_t Queue,
    ur_kernel_handle_t Kernel, USMLaunchInfo &LaunchInfo) {
  auto Program = GetProgram(Kernel);

  // Set membuffer arguments
  auto &KernelInfo = getOrCreateKernelInfo(Kernel);
  UR_LOG_L(getContext()->logger, INFO,
           "KernelInfo {} (Name=<{}>, IsInstrumented={}, "
           "CheckLocals={}, CheckPrivates={}, TrackOrigins={})",
           (void *)Kernel, GetKernelName(Kernel), KernelInfo.IsInstrumented,
           KernelInfo.IsCheckLocals, KernelInfo.IsCheckPrivates,
           KernelInfo.IsTrackOrigins);

  std::shared_lock<ur_shared_mutex> Guard(KernelInfo.Mutex);

  for (const auto &[ArgIndex, MemBuffer] : KernelInfo.BufferArgs) {
    char *ArgPointer = nullptr;
    UR_CALL(MemBuffer->getHandle(DeviceInfo->Handle, ArgPointer));
    ur_result_t URes = getContext()->urDdiTable.Kernel.pfnSetArgPointer(
        Kernel, ArgIndex, nullptr, ArgPointer);
    if (URes != UR_RESULT_SUCCESS) {
      UR_LOG_L(getContext()->logger, ERR,
               "Failed to set buffer {} as the {} arg to kernel {}: {}",
               ur_cast<ur_mem_handle_t>(MemBuffer.get()), ArgIndex, Kernel,
               URes);
    }
  }

  if (!KernelInfo.IsInstrumented) {
    return UR_RESULT_SUCCESS;
  }

  // Set LaunchInfo
  auto ContextInfo = getContextInfo(LaunchInfo.Context);
  LaunchInfo.Data.Host.GlobalShadowOffset = DeviceInfo->Shadow->ShadowBegin;
  LaunchInfo.Data.Host.GlobalShadowOffsetEnd = DeviceInfo->Shadow->ShadowEnd;

  LaunchInfo.Data.Host.DeviceTy = DeviceInfo->Type;
  LaunchInfo.Data.Host.Debug = getContext()->Options.Debug ? 1 : 0;
  LaunchInfo.Data.Host.IsRecover = getContext()->Options.Recover ? 1 : 0;

  // Clean shadow
  // Its content is always zero, and is used for unsupport memory types
  UR_CALL(getContext()->urDdiTable.USM.pfnDeviceAlloc(
      ContextInfo->Handle, DeviceInfo->Handle, nullptr, nullptr,
      ContextInfo->CleanShadowSize,
      (void **)&LaunchInfo.Data.Host.CleanShadow));
  UR_CALL(EnqueueUSMSet(Queue, (void *)LaunchInfo.Data.Host.CleanShadow,
                        (char)0, ContextInfo->CleanShadowSize, 0, nullptr,
                        nullptr));

  if (LaunchInfo.LocalWorkSize.empty()) {
    LaunchInfo.LocalWorkSize.resize(LaunchInfo.WorkDim);
    auto URes = getContext()->urDdiTable.Kernel.pfnGetSuggestedLocalWorkSize(
        Kernel, Queue, LaunchInfo.WorkDim, LaunchInfo.GlobalWorkOffset,
        LaunchInfo.GlobalWorkSize, LaunchInfo.LocalWorkSize.data());
    if (URes != UR_RESULT_SUCCESS) {
      if (URes != UR_RESULT_ERROR_UNSUPPORTED_FEATURE) {
        return URes;
      }
      // If urKernelGetSuggestedLocalWorkSize is not supported by driver, we
      // fallback to inefficient implementation
      for (size_t Dim = 0; Dim < LaunchInfo.WorkDim; ++Dim) {
        LaunchInfo.LocalWorkSize[Dim] = 1;
      }
    }
  }

  const size_t *LocalWorkSize = LaunchInfo.LocalWorkSize.data();
  uint32_t NumWG = 1;
  for (uint32_t Dim = 0; Dim < LaunchInfo.WorkDim; ++Dim) {
    NumWG *= (LaunchInfo.GlobalWorkSize[Dim] + LocalWorkSize[Dim] - 1) /
             LocalWorkSize[Dim];
  }

  uint64_t NumWI = 1;
  for (uint32_t Dim = 0; Dim < LaunchInfo.WorkDim; ++Dim) {
    NumWI *= LaunchInfo.GlobalWorkSize[Dim];
  }

  // Write shadow memory offset for local memory
  if (KernelInfo.IsCheckLocals) {
    if (DeviceInfo->Shadow->AllocLocalShadow(
            Queue, NumWG, LaunchInfo.Data.Host.LocalShadowOffset,
            LaunchInfo.Data.Host.LocalShadowOffsetEnd) != UR_RESULT_SUCCESS) {
      UR_LOG_L(getContext()->logger, WARN,
               "Failed to allocate shadow memory for local memory, "
               "maybe the number of workgroup ({}) is too large",
               NumWG);
      UR_LOG_L(getContext()->logger, WARN,
               "Skip checking local memory of kernel <{}> ",
               GetKernelName(Kernel));
    } else {
      UR_LOG_L(getContext()->logger, DEBUG,
               "ShadowMemory(Local, WorkGroup={}, {} - {})", NumWG,
               (void *)LaunchInfo.Data.Host.LocalShadowOffset,
               (void *)LaunchInfo.Data.Host.LocalShadowOffsetEnd);
    }
  }

  // Write shadow memory offset for private memory
  if (KernelInfo.IsCheckPrivates) {
    if (DeviceInfo->Shadow->AllocPrivateShadow(
            Queue, NumWI, NumWG, LaunchInfo.Data.Host.PrivateBase,
            LaunchInfo.Data.Host.PrivateShadowOffset,
            LaunchInfo.Data.Host.PrivateShadowOffsetEnd) != UR_RESULT_SUCCESS) {
      UR_LOG_L(getContext()->logger, WARN,
               "Failed to allocate shadow memory for private memory, "
               "maybe the number of workgroup ({}) is too large",
               NumWG);
      UR_LOG_L(getContext()->logger, WARN,
               "Skip checking private memory of kernel <{}>",
               GetKernelName(Kernel));
      LaunchInfo.Data.Host.PrivateShadowOffset = 0;
    } else {
      UR_LOG_L(
          getContext()->logger, DEBUG,
          "ShadowMemory(Private, WorkGroup={}, PrivateBase={}, Shadow={} - {})",
          NumWG, (void *)LaunchInfo.Data.Host.PrivateBase,
          (void *)LaunchInfo.Data.Host.PrivateShadowOffset,
          (void *)LaunchInfo.Data.Host.PrivateShadowOffsetEnd);
    }

    // Write local arguments info
    if (!KernelInfo.LocalArgs.empty()) {
      std::vector<MsanLocalArgsInfo> LocalArgsInfo;
      for (auto [ArgIndex, ArgInfo] : KernelInfo.LocalArgs) {
        LocalArgsInfo.push_back(ArgInfo);
        UR_LOG_L(getContext()->logger, DEBUG,
                 "LocalArgs (argIndex={}, size={})", ArgIndex, ArgInfo.Size);
      }
      UR_CALL(LaunchInfo.Data.importLocalArgsInfo(Queue, LocalArgsInfo));
    }
  }

  // sync msan runtime data to device side
  UR_CALL(LaunchInfo.Data.syncToDevice(Queue));

  UR_LOG_L(getContext()->logger, INFO,
           "LaunchInfo {} (GlobalShadow={}, LocalShadow={}, PrivateBase={}, "
           "PrivateShadow={}, CleanShadow={}, LocalArgs={}, NumLocalArgs={}, "
           "Device={}, Debug={})",
           (void *)LaunchInfo.Data.getDevicePtr(),
           (void *)LaunchInfo.Data.Host.GlobalShadowOffset,
           (void *)LaunchInfo.Data.Host.LocalShadowOffset,
           (void *)LaunchInfo.Data.Host.PrivateBase,
           (void *)LaunchInfo.Data.Host.PrivateShadowOffset,
           (void *)LaunchInfo.Data.Host.CleanShadow,
           (void *)LaunchInfo.Data.Host.LocalArgs,
           LaunchInfo.Data.Host.NumLocalArgs,
           ToString(LaunchInfo.Data.Host.DeviceTy), LaunchInfo.Data.Host.Debug);

  ur_result_t URes =
      getContext()->urDdiTable.Enqueue.pfnDeviceGlobalVariableWrite(
          Queue, Program, "__MsanLaunchInfo", false,
          sizeof(LaunchInfo.Data.DevicePtr), 0, &LaunchInfo.Data.DevicePtr, 0,
          nullptr, nullptr);
  if (URes != UR_RESULT_SUCCESS) {
    UR_LOG_L(getContext()->logger, INFO,
             "EnqueueWriteGlobal(__MsanLaunchInfo) "
             "failed, maybe empty kernel: {}",
             URes);
  }

  return UR_RESULT_SUCCESS;
}

ur_result_t DeviceInfo::allocShadowMemory(ur_context_handle_t Context) {
  Shadow = GetMsanShadowMemory(Context, Handle, Type);
  assert(Shadow && "Failed to get shadow memory");
  UR_CALL(Shadow->Setup());
  UR_LOG_L(getContext()->logger, INFO, "ShadowMemory(Global): {} - {}",
           (void *)Shadow->ShadowBegin, (void *)Shadow->ShadowEnd);
  return UR_RESULT_SUCCESS;
}

bool ProgramInfo::isKernelInstrumented(ur_kernel_handle_t Kernel) const {
  const auto Name = GetKernelName(Kernel);
  return KernelMetadataMap.find(Name) != KernelMetadataMap.end();
}

const ProgramInfo::KernelMetada &
ProgramInfo::getKernelMetadata(ur_kernel_handle_t Kernel) const {
  const auto Name = GetKernelName(Kernel);
  assert(KernelMetadataMap.find(Name) != KernelMetadataMap.end());
  return KernelMetadataMap.at(Name);
}

ContextInfo::~ContextInfo() {
  [[maybe_unused]] auto Result =
      getContext()->urDdiTable.Context.pfnRelease(Handle);
  assert(Result == UR_RESULT_SUCCESS);
}

ur_result_t USMLaunchInfo::initialize() {
  UR_CALL(getContext()->urDdiTable.Context.pfnRetain(Context));
  UR_CALL(getContext()->urDdiTable.Device.pfnRetain(Device));
  return UR_RESULT_SUCCESS;
}

MsanRuntimeDataWrapper::~MsanRuntimeDataWrapper() {
  if (Host.CleanShadow) {
    [[maybe_unused]] auto Result =
        getContext()->urDdiTable.USM.pfnFree(Context, (void *)Host.CleanShadow);
    assert(Result == UR_RESULT_SUCCESS);
  }
  if (DevicePtr) {
    [[maybe_unused]] auto Result =
        getContext()->urDdiTable.USM.pfnFree(Context, (void *)DevicePtr);
    assert(Result == UR_RESULT_SUCCESS);
  }
}

USMLaunchInfo::~USMLaunchInfo() {
  [[maybe_unused]] ur_result_t Result;
  Result = getContext()->urDdiTable.Context.pfnRelease(Context);
  assert(Result == UR_RESULT_SUCCESS);
  Result = getContext()->urDdiTable.Device.pfnRelease(Device);
  assert(Result == UR_RESULT_SUCCESS);
}

} // namespace msan

using namespace msan;

static MsanInterceptor *interceptor;

MsanInterceptor *getMsanInterceptor() { return interceptor; }

void initMsanInterceptor() {
  if (interceptor) {
    return;
  }
  interceptor = new MsanInterceptor();
}

void destroyMsanInterceptor() {
  delete interceptor;
  interceptor = nullptr;
}

} // namespace ur_sanitizer_layer
