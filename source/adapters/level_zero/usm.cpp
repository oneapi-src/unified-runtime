//===--------- usm.cpp - Level Zero Adapter -------------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <string.h>

#include "common.hpp"
#include "context.hpp"
#include "platform.hpp"
#include "usm.hpp"

#include <umf_helpers.hpp>

usm::DisjointPoolAllConfigs InitializeDisjointPoolConfig() {
  const char *PoolUrTraceVal = std::getenv("UR_L0_USM_ALLOCATOR_TRACE");
  const char *PoolPiTraceVal =
      std::getenv("SYCL_PI_LEVEL_ZERO_USM_ALLOCATOR_TRACE");
  const char *PoolTraceVal = PoolUrTraceVal
                                 ? PoolUrTraceVal
                                 : (PoolPiTraceVal ? PoolPiTraceVal : nullptr);

  int PoolTrace = 0;
  if (PoolTraceVal != nullptr) {
    PoolTrace = std::atoi(PoolTraceVal);
  }

  const char *PoolUrConfigVal = std::getenv("SYCL_PI_LEVEL_ZERO_USM_ALLOCATOR");
  const char *PoolPiConfigVal = std::getenv("UR_L0_USM_ALLOCATOR");
  const char *PoolConfigVal =
      PoolUrConfigVal ? PoolUrConfigVal : PoolPiConfigVal;
  if (PoolConfigVal == nullptr) {
    return usm::DisjointPoolAllConfigs(PoolTrace);
  }

  return usm::parseDisjointPoolConfig(PoolConfigVal, PoolTrace);
}

enum class USMAllocationForceResidencyType {
  // Do not force memory residency at allocation time.
  None = 0,
  // Force memory resident on the device of allocation at allocation time.
  // For host allocation force residency on all devices in a context.
  Device = 1,
  // Force memory resident on all devices in the context with P2P
  // access to the device of allocation.
  // For host allocation force residency on all devices in a context.
  P2PDevices = 2
};

// Input value is of the form 0xHSD, where:
//   4-bits of D control device allocations
//   4-bits of S control shared allocations
//   4-bits of H control host allocations
// Each 4-bit value is holding a USMAllocationForceResidencyType enum value.
// The default is 0x2, i.e. force full residency for device allocations only.
//
static uint32_t USMAllocationForceResidency = [] {
  const char *UrRet = std::getenv("UR_L0_USM_RESIDENT");
  const char *PiRet = std::getenv("SYCL_PI_LEVEL_ZERO_USM_RESIDENT");
  const char *Str = UrRet ? UrRet : (PiRet ? PiRet : nullptr);
  try {
    if (Str) {
      // Auto-detect radix to allow more convinient hex base
      return std::stoi(Str, nullptr, 0);
    }
  } catch (...) {
  }
  return 0x2;
}();

// Convert from an integer value to USMAllocationForceResidencyType enum value
static USMAllocationForceResidencyType
USMAllocationForceResidencyConvert(uint32_t Val) {
  switch (Val) {
  case 1:
    return USMAllocationForceResidencyType::Device;
  case 2:
    return USMAllocationForceResidencyType::P2PDevices;
  default:
    return USMAllocationForceResidencyType::None;
  };
}

static USMAllocationForceResidencyType USMHostAllocationForceResidency = [] {
  return USMAllocationForceResidencyConvert(
      (USMAllocationForceResidency & 0xf00) >> 8);
}();
static USMAllocationForceResidencyType USMSharedAllocationForceResidency = [] {
  return USMAllocationForceResidencyConvert(
      (USMAllocationForceResidency & 0x0f0) >> 4);
}();
static USMAllocationForceResidencyType USMDeviceAllocationForceResidency = [] {
  return USMAllocationForceResidencyConvert(
      (USMAllocationForceResidency & 0x00f));
}();

// Make USM allocation resident as requested
static ur_result_t USMAllocationMakeResident(
    USMAllocationForceResidencyType ForceResidency, ur_context_handle_t Context,
    ur_device_handle_t Device, // nullptr for host allocation
    void *Ptr, size_t Size) {

  if (ForceResidency == USMAllocationForceResidencyType::None)
    return UR_RESULT_SUCCESS;

  std::list<ur_device_handle_t> Devices;
  if (!Device) {
    // Host allocation, make it resident on all devices in the context
    Devices.insert(Devices.end(), Context->Devices.begin(),
                   Context->Devices.end());
  } else {
    Devices.push_back(Device);
    if (ForceResidency == USMAllocationForceResidencyType::P2PDevices) {
      ze_bool_t P2P;
      for (const auto &D : Context->Devices) {
        if (D == Device)
          continue;
        // TODO: Cache P2P devices for a context
        ZE2UR_CALL(zeDeviceCanAccessPeer,
                   (D->ZeDevice, Device->ZeDevice, &P2P));
        if (P2P)
          Devices.push_back(D);
      }
    }
  }
  for (const auto &D : Devices) {
    ZE2UR_CALL(zeContextMakeMemoryResident,
               (Context->ZeContext, D->ZeDevice, Ptr, Size));
  }
  return UR_RESULT_SUCCESS;
}

static ur_result_t USMDeviceAllocImpl(void **ResultPtr,
                                      ur_context_handle_t Context,
                                      ur_device_handle_t Device,
                                      ur_usm_device_mem_flags_t *Flags,
                                      size_t Size, size_t Alignment) {
  std::ignore = Flags;
  // TODO: translate PI properties to Level Zero flags
  ZeStruct<ze_device_mem_alloc_desc_t> ZeDesc;
  ZeDesc.flags = 0;
  ZeDesc.ordinal = 0;

  if (Device->useOptimized32bitAccess() == 0 &&
      (Size > Device->ZeDeviceProperties->maxMemAllocSize)) {
    // Tell Level-Zero to accept Size > maxMemAllocSize if
    // large allocations are used.
    ZeStruct<ze_relaxed_allocation_limits_exp_desc_t> RelaxedDesc;
    RelaxedDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    ZeDesc.pNext = &RelaxedDesc;
  }

  ze_result_t ZeResult = ZE_CALL_NOCHECK(
      zeMemAllocDevice, (Context->ZeContext, &ZeDesc, Size, Alignment,
                         Device->ZeDevice, ResultPtr));
  if (ZeResult != ZE_RESULT_SUCCESS) {
    if (ZeResult == ZE_RESULT_ERROR_UNSUPPORTED_SIZE) {
      return UR_RESULT_ERROR_INVALID_USM_SIZE;
    }
    return ze2urResult(ZeResult);
  }

  UR_ASSERT(Alignment == 0 ||
                reinterpret_cast<std::uintptr_t>(*ResultPtr) % Alignment == 0,
            UR_RESULT_ERROR_INVALID_VALUE);

  // TODO: Return any non-success result from USMAllocationMakeResident once
  // oneapi-src/level-zero-spec#240 is resolved.
  auto Result = USMAllocationMakeResident(USMDeviceAllocationForceResidency,
                                          Context, Device, *ResultPtr, Size);
  if (Result == UR_RESULT_ERROR_OUT_OF_DEVICE_MEMORY ||
      Result == UR_RESULT_ERROR_OUT_OF_HOST_MEMORY) {
    return Result;
  }
  return UR_RESULT_SUCCESS;
}

static ur_result_t
USMSharedAllocImpl(void **ResultPtr, ur_context_handle_t Context,
                   ur_device_handle_t Device, ur_usm_host_mem_flags_t *,
                   ur_usm_device_mem_flags_t *, size_t Size, size_t Alignment) {

  // TODO: translate PI properties to Level Zero flags
  ZeStruct<ze_host_mem_alloc_desc_t> ZeHostDesc;
  ZeHostDesc.flags = 0;
  ZeStruct<ze_device_mem_alloc_desc_t> ZeDevDesc;
  ZeDevDesc.flags = 0;
  ZeDevDesc.ordinal = 0;

  ZeStruct<ze_relaxed_allocation_limits_exp_desc_t> RelaxedDesc;
  if (Size > Device->ZeDeviceProperties->maxMemAllocSize) {
    // Tell Level-Zero to accept Size > maxMemAllocSize
    RelaxedDesc.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
    ZeDevDesc.pNext = &RelaxedDesc;
  }

  ze_result_t ZeResult = ZE_CALL_NOCHECK(
      zeMemAllocShared, (Context->ZeContext, &ZeDevDesc, &ZeHostDesc, Size,
                         Alignment, Device->ZeDevice, ResultPtr));
  if (ZeResult != ZE_RESULT_SUCCESS) {
    if (ZeResult == ZE_RESULT_ERROR_UNSUPPORTED_SIZE) {
      return UR_RESULT_ERROR_INVALID_USM_SIZE;
    }
    return ze2urResult(ZeResult);
  }

  UR_ASSERT(Alignment == 0 ||
                reinterpret_cast<std::uintptr_t>(*ResultPtr) % Alignment == 0,
            UR_RESULT_ERROR_INVALID_VALUE);

  // TODO: Return any non-success result from USMAllocationMakeResident once
  // oneapi-src/level-zero-spec#240 is resolved.
  auto Result = USMAllocationMakeResident(USMSharedAllocationForceResidency,
                                          Context, Device, *ResultPtr, Size);
  if (Result == UR_RESULT_ERROR_OUT_OF_DEVICE_MEMORY ||
      Result == UR_RESULT_ERROR_OUT_OF_HOST_MEMORY) {
    return Result;
  }

  // TODO: Handle PI_MEM_ALLOC_DEVICE_READ_ONLY.
  return UR_RESULT_SUCCESS;
}

static ur_result_t USMHostAllocImpl(void **ResultPtr,
                                    ur_context_handle_t Context,
                                    ur_usm_host_mem_flags_t *Flags, size_t Size,
                                    size_t Alignment) {
  std::ignore = Flags;
  // TODO: translate PI properties to Level Zero flags
  ZeStruct<ze_host_mem_alloc_desc_t> ZeHostDesc;
  ZeHostDesc.flags = 0;
  ze_result_t ZeResult =
      ZE_CALL_NOCHECK(zeMemAllocHost, (Context->ZeContext, &ZeHostDesc, Size,
                                       Alignment, ResultPtr));
  if (ZeResult != ZE_RESULT_SUCCESS) {
    if (ZeResult == ZE_RESULT_ERROR_UNSUPPORTED_SIZE) {
      return UR_RESULT_ERROR_INVALID_USM_SIZE;
    }
    return ze2urResult(ZeResult);
  }

  UR_ASSERT(Alignment == 0 ||
                reinterpret_cast<std::uintptr_t>(*ResultPtr) % Alignment == 0,
            UR_RESULT_ERROR_INVALID_VALUE);

  // TODO: Return any non-success result from USMAllocationMakeResident once
  // oneapi-src/level-zero-spec#240 is resolved.
  auto Result = USMAllocationMakeResident(USMHostAllocationForceResidency,
                                          Context, nullptr, *ResultPtr, Size);
  if (Result == UR_RESULT_ERROR_OUT_OF_DEVICE_MEMORY ||
      Result == UR_RESULT_ERROR_OUT_OF_HOST_MEMORY) {
    return Result;
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMHostAlloc(
    ur_context_handle_t Context, ///< [in] handle of the context object
    const ur_usm_desc_t
        *USMDesc, ///< [in][optional] USM memory allocation descriptor
    ur_usm_pool_handle_t Pool, ///< [in][optional] Pointer to a pool created
                               ///< using urUSMPoolCreate
    size_t
        Size, ///< [in] size in bytes of the USM memory object to be allocated
    void **RetMem ///< [out] pointer to USM host memory object
) {

  uint32_t Align = USMDesc ? USMDesc->align : 0;
  // L0 supports alignment up to 64KB and silently ignores higher values.
  // We flag alignment > 64KB as an invalid value.
  if (Align > 65536)
    return UR_RESULT_ERROR_INVALID_VALUE;

  ur_platform_handle_t Plt = Context->getPlatform();
  // If indirect access tracking is enabled then lock the mutex which is
  // guarding contexts container in the platform. This prevents new kernels from
  // being submitted in any context while we are in the process of allocating a
  // memory, this is needed to properly capture allocations by kernels with
  // indirect access. This lock also protects access to the context's data
  // structures. If indirect access tracking is not enabled then lock context
  // mutex to protect access to context's data structures.
  std::shared_lock<ur_shared_mutex> ContextLock(Context->Mutex,
                                                std::defer_lock);
  std::unique_lock<ur_shared_mutex> IndirectAccessTrackingLock(
      Plt->ContextsMutex, std::defer_lock);
  if (IndirectAccessTrackingEnabled) {
    IndirectAccessTrackingLock.lock();
    // We are going to defer memory release if there are kernels with indirect
    // access, that is why explicitly retain context to be sure that it is
    // released after all memory allocations in this context are released.
    UR_CALL(urContextRetain(Context));
  } else {
    ContextLock.lock();
  }

  // There is a single allocator for Host USM allocations, so we don't need to
  // find the allocator depending on context as we do for Shared and Device
  // allocations.
  std::optional<umf_memory_pool_handle_t> hPoolInternalOpt = std::nullopt;
  usm::pool_descriptor Desc(nullptr, Context, nullptr, UR_USM_TYPE_HOST,
                            USMDesc);
  if (!UseUSMAllocator ||
      // L0 spec says that allocation fails if Alignment != 2^n, in order to
      // keep the same behavior for the allocator, just call L0 API directly and
      // return the error code.
      ((Align & (Align - 1)) != 0)) {
    hPoolInternalOpt = Context->ProxyPoolManager.getPool(Desc);
  } else if (Pool) {
    // Getting user-created pool requires 'poolHandle' field.
    Desc.poolHandle = Pool;
    hPoolInternalOpt = Pool->getOrCreatePool(Desc);
  } else {
    hPoolInternalOpt = Context->Pool->getOrCreatePool(Desc);
  }

  if (!hPoolInternalOpt.has_value()) {
    // Internal error, every L0 context and usm pool should have Host, Device,
    // Shared and SharedReadOnly UMF pools.
    urPrint("urUSMHostAlloc: unexpected internal error\n");
    return UR_RESULT_ERROR_UNKNOWN;
  }

  auto hPoolInternal = hPoolInternalOpt.value();
  *RetMem = umfPoolAlignedMalloc(hPoolInternal, Size, Align);
  if (*RetMem == nullptr) {
    auto umfRet = umfPoolGetLastAllocationError(hPoolInternal);
    return umf::umf2urResult(umfRet);
  }

  if (IndirectAccessTrackingEnabled) {
    // Keep track of all memory allocations in the context
    Context->MemAllocs.emplace(std::piecewise_construct,
                               std::forward_as_tuple(*RetMem),
                               std::forward_as_tuple(Context));
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMDeviceAlloc(
    ur_context_handle_t Context, ///< [in] handle of the context object
    ur_device_handle_t Device,   ///< [in] handle of the device object
    const ur_usm_desc_t
        *USMDesc, ///< [in][optional] USM memory allocation descriptor
    ur_usm_pool_handle_t Pool, ///< [in][optional] Pointer to a pool created
                               ///< using urUSMPoolCreate
    size_t
        Size, ///< [in] size in bytes of the USM memory object to be allocated
    void **RetMem ///< [out] pointer to USM device memory object
) {

  uint32_t Alignment = USMDesc ? USMDesc->align : 0;

  // L0 supports alignment up to 64KB and silently ignores higher values.
  // We flag alignment > 64KB as an invalid value.
  if (Alignment > 65536)
    return UR_RESULT_ERROR_INVALID_VALUE;

  ur_platform_handle_t Plt = Device->Platform;

  // If indirect access tracking is enabled then lock the mutex which is
  // guarding contexts container in the platform. This prevents new kernels from
  // being submitted in any context while we are in the process of allocating a
  // memory, this is needed to properly capture allocations by kernels with
  // indirect access. This lock also protects access to the context's data
  // structures. If indirect access tracking is not enabled then lock context
  // mutex to protect access to context's data structures.
  std::shared_lock<ur_shared_mutex> ContextLock(Context->Mutex,
                                                std::defer_lock);
  std::unique_lock<ur_shared_mutex> IndirectAccessTrackingLock(
      Plt->ContextsMutex, std::defer_lock);
  if (IndirectAccessTrackingEnabled) {
    IndirectAccessTrackingLock.lock();
    // We are going to defer memory release if there are kernels with indirect
    // access, that is why explicitly retain context to be sure that it is
    // released after all memory allocations in this context are released.
    UR_CALL(urContextRetain(Context));
  } else {
    ContextLock.lock();
  }

  std::optional<umf_memory_pool_handle_t> hPoolInternalOpt = std::nullopt;
  usm::pool_descriptor Desc(nullptr, Context, Device, UR_USM_TYPE_DEVICE,
                            USMDesc);
  if (!UseUSMAllocator ||
      // L0 spec says that allocation fails if Alignment != 2^n, in order to
      // keep the same behavior for the allocator, just call L0 API directly and
      // return the error code.
      ((Alignment & (Alignment - 1)) != 0)) {
    hPoolInternalOpt = Context->ProxyPoolManager.getPool(Desc);
  } else if (Pool) {
    // Getting user-created pool requires 'poolHandle' field.
    Desc.poolHandle = Pool;
    hPoolInternalOpt = Pool->getOrCreatePool(Desc);
  } else {
    hPoolInternalOpt = Context->Pool->getOrCreatePool(Desc);
  }

  if (!hPoolInternalOpt.has_value()) {
    // Internal error, every L0 context and usm pool should have Host, Device,
    // Shared and SharedReadOnly UMF pools.
    urPrint("urUSMDeviceAlloc: unexpected internal error\n");
    return UR_RESULT_ERROR_UNKNOWN;
  }

  auto hPoolInternal = hPoolInternalOpt.value();
  *RetMem = umfPoolAlignedMalloc(hPoolInternal, Size, Alignment);
  if (*RetMem == nullptr) {
    auto umfRet = umfPoolGetLastAllocationError(hPoolInternal);
    return umf::umf2urResult(umfRet);
  }

  if (IndirectAccessTrackingEnabled) {
    // Keep track of all memory allocations in the context
    Context->MemAllocs.emplace(std::piecewise_construct,
                               std::forward_as_tuple(*RetMem),
                               std::forward_as_tuple(Context));
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMSharedAlloc(
    ur_context_handle_t Context, ///< [in] handle of the context object
    ur_device_handle_t Device,   ///< [in] handle of the device object
    const ur_usm_desc_t
        *USMDesc, ///< [in][optional] USM memory allocation descriptor
    ur_usm_pool_handle_t Pool, ///< [in][optional] Pointer to a pool created
                               ///< using urUSMPoolCreate
    size_t
        Size, ///< [in] size in bytes of the USM memory object to be allocated
    void **RetMem ///< [out] pointer to USM shared memory object
) {

  uint32_t Alignment = USMDesc ? USMDesc->align : 0;

  // L0 supports alignment up to 64KB and silently ignores higher values.
  // We flag alignment > 64KB as an invalid value.
  if (Alignment > 65536)
    return UR_RESULT_ERROR_INVALID_VALUE;

  ur_platform_handle_t Plt = Device->Platform;

  // If indirect access tracking is enabled then lock the mutex which is
  // guarding contexts container in the platform. This prevents new kernels from
  // being submitted in any context while we are in the process of allocating a
  // memory, this is needed to properly capture allocations by kernels with
  // indirect access. This lock also protects access to the context's data
  // structures. If indirect access tracking is not enabled then lock context
  // mutex to protect access to context's data structures.
  std::scoped_lock<ur_shared_mutex> Lock(
      IndirectAccessTrackingEnabled ? Plt->ContextsMutex : Context->Mutex);

  if (IndirectAccessTrackingEnabled) {
    // We are going to defer memory release if there are kernels with indirect
    // access, that is why explicitly retain context to be sure that it is
    // released after all memory allocations in this context are released.
    UR_CALL(urContextRetain(Context));
  }

  std::optional<umf_memory_pool_handle_t> hPoolInternalOpt = std::nullopt;
  usm::pool_descriptor Desc(nullptr, Context, Device, UR_USM_TYPE_SHARED,
                            USMDesc);
  if (!UseUSMAllocator ||
      // L0 spec says that allocation fails if Alignment != 2^n, in order to
      // keep the same behavior for the allocator, just call L0 API directly and
      // return the error code.
      ((Alignment & (Alignment - 1)) != 0)) {
    hPoolInternalOpt = Context->ProxyPoolManager.getPool(Desc);
  } else if (Pool) {
    // Getting user-created pool requires 'poolHandle' field.
    Desc.poolHandle = Pool;
    hPoolInternalOpt = Pool->getOrCreatePool(Desc);
  } else {
    hPoolInternalOpt = Context->Pool->getOrCreatePool(Desc);
  }

  if (!hPoolInternalOpt.has_value()) {
    // Internal error, every L0 context and usm pool should have Host, Device,
    // Shared and SharedReadOnly UMF pools.
    urPrint("urUSMSharedAlloc: unexpected internal error\n");
    return UR_RESULT_ERROR_UNKNOWN;
  }

  auto hPoolInternal = hPoolInternalOpt.value();
  *RetMem = umfPoolAlignedMalloc(hPoolInternal, Size, Alignment);
  if (*RetMem == nullptr) {
    auto umfRet = umfPoolGetLastAllocationError(hPoolInternal);
    return umf::umf2urResult(umfRet);
  }

  if (IndirectAccessTrackingEnabled) {
    // Keep track of all memory allocations in the context
    Context->MemAllocs.emplace(std::piecewise_construct,
                               std::forward_as_tuple(*RetMem),
                               std::forward_as_tuple(Context));
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMFree(
    ur_context_handle_t Context, ///< [in] handle of the context object
    void *Mem                    ///< [in] pointer to USM memory object
) {
  ur_platform_handle_t Plt = Context->getPlatform();

  std::scoped_lock<ur_shared_mutex> Lock(
      IndirectAccessTrackingEnabled ? Plt->ContextsMutex : Context->Mutex);

  return USMFreeHelper(Context, Mem);
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMGetMemAllocInfo(
    ur_context_handle_t Context, ///< [in] handle of the context object
    const void *Ptr,             ///< [in] pointer to USM memory object
    ur_usm_alloc_info_t
        PropName, ///< [in] the name of the USM allocation property to query
    size_t PropValueSize, ///< [in] size in bytes of the USM allocation property
                          ///< value
    void *PropValue, ///< [out][optional] value of the USM allocation property
    size_t *PropValueSizeRet ///< [out][optional] bytes returned in USM
                             ///< allocation property
) {
  ze_device_handle_t ZeDeviceHandle;
  ZeStruct<ze_memory_allocation_properties_t> ZeMemoryAllocationProperties;

  ZE2UR_CALL(zeMemGetAllocProperties,
             (Context->ZeContext, Ptr, &ZeMemoryAllocationProperties,
              &ZeDeviceHandle));

  UrReturnHelper ReturnValue(PropValueSize, PropValue, PropValueSizeRet);
  switch (PropName) {
  case UR_USM_ALLOC_INFO_TYPE: {
    ur_usm_type_t MemAllocaType;
    switch (ZeMemoryAllocationProperties.type) {
    case ZE_MEMORY_TYPE_UNKNOWN:
      MemAllocaType = UR_USM_TYPE_UNKNOWN;
      break;
    case ZE_MEMORY_TYPE_HOST:
      MemAllocaType = UR_USM_TYPE_HOST;
      break;
    case ZE_MEMORY_TYPE_DEVICE:
      MemAllocaType = UR_USM_TYPE_DEVICE;
      break;
    case ZE_MEMORY_TYPE_SHARED:
      MemAllocaType = UR_USM_TYPE_SHARED;
      break;
    default:
      urPrint("urUSMGetMemAllocInfo: unexpected usm memory type\n");
      return UR_RESULT_ERROR_INVALID_VALUE;
    }
    return ReturnValue(MemAllocaType);
  }
  case UR_USM_ALLOC_INFO_DEVICE:
    if (ZeDeviceHandle) {
      auto Platform = Context->getPlatform();
      auto Device = Platform->getDeviceFromNativeHandle(ZeDeviceHandle);
      return Device ? ReturnValue(Device) : UR_RESULT_ERROR_INVALID_VALUE;
    } else {
      return UR_RESULT_ERROR_INVALID_VALUE;
    }
  case UR_USM_ALLOC_INFO_BASE_PTR: {
    void *Base;
    ZE2UR_CALL(zeMemGetAddressRange, (Context->ZeContext, Ptr, &Base, nullptr));
    return ReturnValue(Base);
  }
  case UR_USM_ALLOC_INFO_SIZE: {
    size_t Size;
    ZE2UR_CALL(zeMemGetAddressRange, (Context->ZeContext, Ptr, nullptr, &Size));
    return ReturnValue(Size);
  }
  case UR_USM_ALLOC_INFO_POOL: {
    auto UMFPool = umfPoolByPtr(Ptr);
    if (!UMFPool) {
      return UR_RESULT_ERROR_INVALID_VALUE;
    }

    std::shared_lock<ur_shared_mutex> ContextLock(Context->Mutex);

    for (auto &Pool : Context->UsmPoolHandles) {
      if (Pool->PoolManager.hasPool(UMFPool))
        return ReturnValue(Pool);
    }

    return UR_RESULT_ERROR_INVALID_VALUE;
  }
  default:
    urPrint("urUSMGetMemAllocInfo: unsupported ParamName\n");
    return UR_RESULT_ERROR_INVALID_VALUE;
  }
  return UR_RESULT_SUCCESS;
}

ur_result_t USMFreeImpl(ur_context_handle_t Context, void *Ptr) {
  auto ZeResult = ZE_CALL_NOCHECK(zeMemFree, (Context->ZeContext, Ptr));
  // Handle When the driver is already released
  if (ZeResult == ZE_RESULT_ERROR_UNINITIALIZED) {
    return UR_RESULT_SUCCESS;
  } else {
    return ze2urResult(ZeResult);
  }
}

ur_result_t GetL0MinPageSize(ur_context_handle_t Context, void *Ptr,
                             size_t *PageSize,
                             umf::USMMemoryProvider *Provider) {
  // If we didn't get a pointer to check we need to use the memory provider to
  // make a small allocation so we can run the query
  void *CheckAlloc = Ptr;
  if (!Ptr) {
    if (auto AllocRes = Provider->alloc(1, 0, &CheckAlloc);
        AllocRes != UMF_RESULT_SUCCESS) {
      return umf::umf2urResult(AllocRes);
    }
  }

  ZeStruct<ze_memory_allocation_properties_t> AllocProperties = {};
  auto Res = ze2urResult(
      ZE_CALL_NOCHECK(zeMemGetAllocProperties, (Context->ZeContext, CheckAlloc,
                                                &AllocProperties, nullptr)));
  if (Res == UR_RESULT_SUCCESS) {
    *PageSize = AllocProperties.pageSize;
  }

  if (!Ptr) {
    if (auto FreeRes = Provider->free(CheckAlloc, 1);
        FreeRes != UMF_RESULT_SUCCESS) {
      return umf::umf2urResult(FreeRes);
    }
  }

  return Res;
}

umf_result_t L0SharedMemoryProvider::initialize(usm::pool_descriptor Desc) {
  if (auto Res = USMSharedMemoryProvider::initialize(Desc);
      Res != UMF_RESULT_SUCCESS) {
    return Res;
  }
  UMF_PROVIDER_CHECK_UR_RESULT(
      GetL0MinPageSize(Context, nullptr, &MinPageSize, this))
  return UMF_RESULT_SUCCESS;
}

ur_result_t L0SharedMemoryProvider::allocateImpl(void **ResultPtr, size_t Size,
                                                 size_t Alignment) {
  return USMSharedAllocImpl(ResultPtr, Context, Device, nullptr, nullptr, Size,
                            Alignment);
}

umf_result_t L0DeviceMemoryProvider::initialize(usm::pool_descriptor Desc) {
  if (auto Res = USMDeviceMemoryProvider::initialize(Desc);
      Res != UMF_RESULT_SUCCESS) {
    return Res;
  }
  UMF_PROVIDER_CHECK_UR_RESULT(
      GetL0MinPageSize(Context, nullptr, &MinPageSize, this))
  return UMF_RESULT_SUCCESS;
}

ur_result_t L0DeviceMemoryProvider::allocateImpl(void **ResultPtr, size_t Size,
                                                 size_t Alignment) {
  return USMDeviceAllocImpl(ResultPtr, Context, Device, nullptr, Size,
                            Alignment);
}

umf_result_t L0HostMemoryProvider::initialize(usm::pool_descriptor Desc) {
  if (auto Res = USMHostMemoryProvider::initialize(Desc);
      Res != UMF_RESULT_SUCCESS) {
    return Res;
  }
  UMF_PROVIDER_CHECK_UR_RESULT(
      GetL0MinPageSize(Context, nullptr, &MinPageSize, this))
  return UMF_RESULT_SUCCESS;
}

ur_result_t L0HostMemoryProvider::allocateImpl(void **ResultPtr, size_t Size,
                                               size_t Alignment) {
  return USMHostAllocImpl(ResultPtr, Context, nullptr, Size, Alignment);
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMPoolCreate(
    ur_context_handle_t Context, ///< [in] handle of the context object
    ur_usm_pool_desc_t
        *PoolDesc, ///< [in] pointer to USM pool descriptor. Can be chained with
                   ///< ::ur_usm_pool_limits_desc_t
    ur_usm_pool_handle_t *Pool ///< [out] pointer to USM memory pool
) {

  try {
    *Pool = reinterpret_cast<ur_usm_pool_handle_t>(
        new ur_usm_pool_handle_t_(Context, PoolDesc));

    std::shared_lock<ur_shared_mutex> ContextLock(Context->Mutex);
    Context->UsmPoolHandles.insert(Context->UsmPoolHandles.cend(), *Pool);

  } catch (const umf::UsmAllocationException &Ex) {
    return Ex.getError();
  }
  return UR_RESULT_SUCCESS;
}

ur_result_t
urUSMPoolRetain(ur_usm_pool_handle_t Pool ///< [in] pointer to USM memory pool
) {
  Pool->incrementReferenceCount();
  return UR_RESULT_SUCCESS;
}

ur_result_t
urUSMPoolRelease(ur_usm_pool_handle_t Pool ///< [in] pointer to USM memory pool
) {
  if (Pool->decrementReferenceCount() > 0) {
    return UR_RESULT_SUCCESS;
  }
  std::shared_lock<ur_shared_mutex> ContextLock(Pool->Context->Mutex);
  Pool->Context->UsmPoolHandles.remove(Pool);
  delete Pool;
  return UR_RESULT_SUCCESS;
}

ur_result_t urUSMPoolGetInfo(
    ur_usm_pool_handle_t Pool,   ///< [in] handle of the USM memory pool
    ur_usm_pool_info_t PropName, ///< [in] name of the pool property to query
    size_t PropSize, ///< [in] size in bytes of the pool property value provided
    void *PropValue, ///< [out][typename(propName, propSize)] value of the pool
                     ///< property
    size_t *PropSizeRet ///< [out] size in bytes returned in pool property value
) {
  UrReturnHelper ReturnValue(PropSize, PropValue, PropSizeRet);

  switch (PropName) {
  case UR_USM_POOL_INFO_REFERENCE_COUNT: {
    return ReturnValue(Pool->RefCount.load());
  }
  case UR_USM_POOL_INFO_CONTEXT: {
    return ReturnValue(Pool->Context);
  }
  default: {
    return UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
  }
  }
}

// If indirect access tracking is not enabled then this functions just performs
// zeMemFree. If indirect access tracking is enabled then reference counting is
// performed.
ur_result_t ZeMemFreeHelper(ur_context_handle_t Context, void *Ptr) {
  ur_platform_handle_t Plt = Context->getPlatform();
  std::unique_lock<ur_shared_mutex> ContextsLock(Plt->ContextsMutex,
                                                 std::defer_lock);
  if (IndirectAccessTrackingEnabled) {
    ContextsLock.lock();
    auto It = Context->MemAllocs.find(Ptr);
    if (It == std::end(Context->MemAllocs)) {
      die("All memory allocations must be tracked!");
    }
    if (!It->second.RefCount.decrementAndTest()) {
      // Memory can't be deallocated yet.
      return UR_RESULT_SUCCESS;
    }

    // Reference count is zero, it is ok to free memory.
    // We don't need to track this allocation anymore.
    Context->MemAllocs.erase(It);
  }

  ZE2UR_CALL(zeMemFree, (Context->ZeContext, Ptr));

  if (IndirectAccessTrackingEnabled)
    UR_CALL(ContextReleaseHelper(Context));

  return UR_RESULT_SUCCESS;
}

static bool ShouldUseUSMAllocator() {
  // Enable allocator by default if it's not explicitly disabled
  const char *UrRet = std::getenv("UR_L0_DISABLE_USM_ALLOCATOR");
  const char *PiRet = std::getenv("SYCL_PI_LEVEL_ZERO_DISABLE_USM_ALLOCATOR");
  const char *Res = UrRet ? UrRet : (PiRet ? PiRet : nullptr);
  return Res == nullptr;
}

const bool UseUSMAllocator = ShouldUseUSMAllocator();

// Helper function to deallocate USM memory, if indirect access support is
// enabled then a caller must lock the platform-level mutex guarding the
// container with contexts because deallocating the memory can turn RefCount of
// a context to 0 and as a result the context being removed from the list of
// tracked contexts.
// If indirect access tracking is not enabled then caller must lock Context
// mutex.
ur_result_t USMFreeHelper(ur_context_handle_t Context, void *Ptr,
                          bool OwnZeMemHandle) {
  if (!OwnZeMemHandle) {
    // Memory should not be freed
    return UR_RESULT_SUCCESS;
  }

  if (IndirectAccessTrackingEnabled) {
    auto It = Context->MemAllocs.find(Ptr);
    if (It == std::end(Context->MemAllocs)) {
      die("All memory allocations must be tracked!");
    }
    if (!It->second.RefCount.decrementAndTest()) {
      // Memory can't be deallocated yet.
      return UR_RESULT_SUCCESS;
    }

    // Reference count is zero, it is ok to free memory.
    // We don't need to track this allocation anymore.
    Context->MemAllocs.erase(It);
  }

  auto hPool = umfPoolByPtr(Ptr);
  if (!hPool) {
    if (IndirectAccessTrackingEnabled)
      UR_CALL(ContextReleaseHelper(Context));
    return UR_RESULT_ERROR_INVALID_MEM_OBJECT;
  }

  auto umfRet = umfPoolFree(hPool, Ptr);
  if (IndirectAccessTrackingEnabled)
    UR_CALL(ContextReleaseHelper(Context));
  return umf::umf2urResult(umfRet);
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMImportExp(ur_context_handle_t Context,
                                                   void *HostPtr, size_t Size) {
  UR_ASSERT(Context, UR_RESULT_ERROR_INVALID_CONTEXT);

  // Promote the host ptr to USM host memory.
  if (ZeUSMImport.Supported && HostPtr != nullptr) {
    // Query memory type of the host pointer
    ze_device_handle_t ZeDeviceHandle;
    ZeStruct<ze_memory_allocation_properties_t> ZeMemoryAllocationProperties;
    ZE2UR_CALL(zeMemGetAllocProperties,
               (Context->ZeContext, HostPtr, &ZeMemoryAllocationProperties,
                &ZeDeviceHandle));

    // If not shared of any type, we can import the ptr
    if (ZeMemoryAllocationProperties.type == ZE_MEMORY_TYPE_UNKNOWN) {
      // Promote the host ptr to USM host memory
      ze_driver_handle_t driverHandle = Context->getPlatform()->ZeDriver;
      ZeUSMImport.doZeUSMImport(driverHandle, HostPtr, Size);
    }
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMReleaseExp(ur_context_handle_t Context,
                                                    void *HostPtr) {
  UR_ASSERT(Context, UR_RESULT_ERROR_INVALID_CONTEXT);

  // Release the imported memory.
  if (ZeUSMImport.Supported && HostPtr != nullptr)
    ZeUSMImport.doZeUSMRelease(Context->getPlatform()->ZeDriver, HostPtr);
  return UR_RESULT_SUCCESS;
}
