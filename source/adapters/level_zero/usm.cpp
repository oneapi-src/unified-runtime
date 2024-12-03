//===--------- usm.cpp - Level Zero Adapter -------------------------------===//
//
// Copyright (C) 2023-2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <climits>
#include <string.h>

#include "context.hpp"
#include "event.hpp"
#include "usm.hpp"

#include "logger/ur_logger.hpp"
#include "ur_interface_loader.hpp"
#include "ur_level_zero.hpp"
#include "ur_util.hpp"

#include <umf/providers/provider_fixed_memory.h>
#include <umf_helpers.hpp>

namespace umf {
ur_result_t getProviderNativeError(const char *providerName,
                                   int32_t nativeError) {
  if (strcmp(providerName, "Level Zero") == 0) {
    // L0 provider stores native errors of ur_result_t type
    return static_cast<ur_result_t>(nativeError);
  }

  return UR_RESULT_ERROR_UNKNOWN;
}
} // namespace umf

usm::DisjointPoolAllConfigs DisjointPoolConfigInstance =
    InitializeDisjointPoolConfig();

ur_result_t umf2urResult(umf_result_t umfResult) {
  if (umfResult == UMF_RESULT_SUCCESS)
    return UR_RESULT_SUCCESS;

  switch (umfResult) {
  case UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY:
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  case UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC: {
    auto hProvider = umfGetLastFailedMemoryProvider();
    if (hProvider == nullptr)
      return UR_RESULT_ERROR_UNKNOWN;

    ur_result_t Err = UR_RESULT_ERROR_UNKNOWN;
    umfMemoryProviderGetLastNativeError(hProvider, nullptr,
                                        reinterpret_cast<int32_t *>(&Err));
    return Err;
  }
  case UMF_RESULT_ERROR_INVALID_ARGUMENT:
    return UR_RESULT_ERROR_INVALID_ARGUMENT;
  case UMF_RESULT_ERROR_INVALID_ALIGNMENT:
    return UR_RESULT_ERROR_UNSUPPORTED_ALIGNMENT;
  case UMF_RESULT_ERROR_NOT_SUPPORTED:
    return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
  default:
    return UR_RESULT_ERROR_UNKNOWN;
  };
}

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
      // Check if the P2P devices are already cached
      auto it = Context->P2PDeviceCache.find(Device);
      if (it != Context->P2PDeviceCache.end()) {
        // Use cached P2P devices
        Devices.insert(Devices.end(), it->second.begin(), it->second.end());
      } else {
        // Query for P2P devices and update the cache
        std::list<ur_device_handle_t> P2PDevices;
        ze_bool_t P2P;
        for (const auto &D : Context->Devices) {
          if (D == Device)
            continue;
          ZE2UR_CALL(zeDeviceCanAccessPeer,
                     (D->ZeDevice, Device->ZeDevice, &P2P));
          if (P2P)
            P2PDevices.push_back(D);
        }
        // Update the cache
        Context->P2PDeviceCache[Device] = P2PDevices;
        Devices.insert(Devices.end(), P2PDevices.begin(), P2PDevices.end());
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
                                      ur_usm_device_mem_flags_t Flags,
                                      size_t Size, uint32_t Alignment) {
  std::ignore = Flags;
  // TODO: translate PI properties to Level Zero flags
  ZeStruct<ze_device_mem_alloc_desc_t> ZeDesc;
  ZeDesc.flags = 0;
  ZeDesc.ordinal = 0;

  ZeStruct<ze_relaxed_allocation_limits_exp_desc_t> RelaxedDesc;
  if (Device->useRelaxedAllocationLimits() &&
      (Size > Device->ZeDeviceProperties->maxMemAllocSize)) {
    // Tell Level-Zero to accept Size > maxMemAllocSize if
    // large allocations are used.
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
                   ur_device_handle_t Device, ur_usm_host_mem_flags_t,
                   ur_usm_device_mem_flags_t, size_t Size, uint32_t Alignment) {

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
                                    ur_usm_host_mem_flags_t Flags, size_t Size,
                                    uint32_t Alignment) {
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

namespace ur::level_zero {

ur_result_t urUSMHostAlloc(
    /// [in] handle of the context object
    ur_context_handle_t Context,
    /// [in][optional] USM memory allocation descriptor
    const ur_usm_desc_t *USMDesc,
    /// [in][optional] Pointer to a pool created using urUSMPoolCreate
    ur_usm_pool_handle_t Pool,
    /// [in] size in bytes of the USM memory object to be allocated
    size_t Size,
    /// [out] pointer to USM host memory object
    void **RetMem) {
  ur_usm_pool_handle_t USMPool = nullptr;
  if (Pool) {
    USMPool = Pool;
  } else {
    USMPool = &Context->DefaultPool;
  }

  return USMPool->allocate(Context, nullptr, USMDesc, UR_USM_TYPE_HOST, Size,
                           RetMem);
}

ur_result_t urUSMDeviceAlloc(
    /// [in] handle of the context object
    ur_context_handle_t Context,
    /// [in] handle of the device object
    ur_device_handle_t Device,
    /// [in][optional] USM memory allocation descriptor
    const ur_usm_desc_t *USMDesc,
    /// [in][optional] Pointer to a pool created using urUSMPoolCreate
    ur_usm_pool_handle_t Pool,
    /// [in] size in bytes of the USM memory object to be allocated
    size_t Size,
    /// [out] pointer to USM device memory object
    void **RetMem) {

  ur_usm_pool_handle_t USMPool = nullptr;
  if (Pool) {
    USMPool = Pool;
  } else {
    USMPool = &Context->DefaultPool;
  }

  return USMPool->allocate(Context, Device, USMDesc, UR_USM_TYPE_DEVICE, Size,
                           RetMem);
}

ur_result_t urUSMSharedAlloc(
    /// [in] handle of the context object
    ur_context_handle_t Context,
    /// [in] handle of the device object
    ur_device_handle_t Device,
    /// [in][optional] USM memory allocation descriptor
    const ur_usm_desc_t *USMDesc,
    /// [in][optional] Pointer to a pool created using urUSMPoolCreate
    ur_usm_pool_handle_t Pool,
    /// [in] size in bytes of the USM memory object to be allocated
    size_t Size,
    /// [out] pointer to USM shared memory object
    void **RetMem) {
  ur_usm_pool_handle_t USMPool = nullptr;
  if (Pool) {
    USMPool = Pool;
  } else {
    USMPool = &Context->DefaultPool;
  }

  return USMPool->allocate(Context, Device, USMDesc, UR_USM_TYPE_SHARED, Size,
                           RetMem);
}

ur_result_t
/// [in] handle of the context object
urUSMFree(ur_context_handle_t Context,
          /// [in] pointer to USM memory object
          void *Mem) {
  ur_platform_handle_t Plt = Context->getPlatform();

  std::scoped_lock<ur_shared_mutex> Lock(
      IndirectAccessTrackingEnabled ? Plt->ContextsMutex : Context->Mutex);

  return USMFreeHelper(Context, Mem);
}

ur_result_t urUSMGetMemAllocInfo(
    /// [in] handle of the context object
    ur_context_handle_t Context,
    /// [in] pointer to USM memory object
    const void *Ptr,
    /// [in] the name of the USM allocation property to query
    ur_usm_alloc_info_t PropName,
    /// [in] size in bytes of the USM allocation property value
    size_t PropValueSize,
    /// [out][optional] value of the USM allocation property
    void *PropValue,
    /// [out][optional] bytes returned in USM allocation property
    size_t *PropValueSizeRet) {
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
      logger::error("urUSMGetMemAllocInfo: unexpected usm memory type");
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
      if (Pool->hasPool(UMFPool)) {
        return ReturnValue(Pool);
      }
    }

    return UR_RESULT_ERROR_INVALID_VALUE;
  }
  default:
    logger::error("urUSMGetMemAllocInfo: unsupported ParamName");
    return UR_RESULT_ERROR_INVALID_VALUE;
  }
  return UR_RESULT_SUCCESS;
}

ur_result_t urUSMPoolCreate(
    /// [in] handle of the context object
    ur_context_handle_t Context,
    /// [in] pointer to USM pool descriptor. Can be chained with
    /// ::ur_usm_pool_limits_desc_t
    ur_usm_pool_desc_t *PoolDesc,
    /// [out] pointer to USM memory pool
    ur_usm_pool_handle_t *Pool) {

  try {
    *Pool = reinterpret_cast<ur_usm_pool_handle_t>(
        new ur_usm_pool_handle_t_(Context, PoolDesc));

    std::shared_lock<ur_shared_mutex> ContextLock(Context->Mutex);
    Context->UsmPoolHandles.insert(Context->UsmPoolHandles.cend(), *Pool);

  } catch (const UsmAllocationException &Ex) {
    return Ex.getError();
  } catch (umf_result_t e) {
    return umf2urResult(e);
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }
  return UR_RESULT_SUCCESS;
}

ur_result_t
/// [in] pointer to USM memory pool
urUSMPoolRetain(ur_usm_pool_handle_t Pool) {
  Pool->RefCount.increment();
  return UR_RESULT_SUCCESS;
}

ur_result_t
/// [in] pointer to USM memory pool
urUSMPoolRelease(ur_usm_pool_handle_t Pool) {
  if (Pool->RefCount.decrementAndTest()) {
    std::shared_lock<ur_shared_mutex> ContextLock(Pool->Context->Mutex);
    Pool->Context->UsmPoolHandles.remove(Pool);
    delete Pool;
  }
  return UR_RESULT_SUCCESS;
}

ur_result_t urUSMPoolGetInfo(
    /// [in] handle of the USM memory pool
    ur_usm_pool_handle_t Pool,
    /// [in] name of the pool property to query
    ur_usm_pool_info_t PropName,
    /// [in] size in bytes of the pool property value provided
    size_t PropSize,
    /// [out][typename(propName, propSize)] value of the pool property
    void *PropValue,
    /// [out] size in bytes returned in pool property value
    size_t *PropSizeRet) {
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

ur_result_t urUSMImportExp(ur_context_handle_t Context, void *HostPtr,
                           size_t Size) {
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
      ze_driver_handle_t driverHandle =
          Context->getPlatform()->ZeDriverHandleExpTranslated;
      ZeUSMImport.doZeUSMImport(driverHandle, HostPtr, Size);
    }
  }
  return UR_RESULT_SUCCESS;
}

ur_result_t urUSMReleaseExp(ur_context_handle_t Context, void *HostPtr) {
  UR_ASSERT(Context, UR_RESULT_ERROR_INVALID_CONTEXT);

  // Release the imported memory.
  if (ZeUSMImport.Supported && HostPtr != nullptr)
    ZeUSMImport.doZeUSMRelease(
        Context->getPlatform()->ZeDriverHandleExpTranslated, HostPtr);
  return UR_RESULT_SUCCESS;
}

ur_result_t urEnqueueUSMDeviceAllocExp(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_usm_pool_handle_t
        pPool,         ///< [in][optional] handle of the USM memory pool
    const size_t size, ///< [in] minimum size in bytes of the USM memory object
                       ///< to be allocated
    const ur_exp_enqueue_usm_alloc_properties_t
        *pProperties, ///< [in][optional] pointer to the enqueue asynchronous
                      ///< USM allocation properties
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before the kernel execution. If nullptr, the
                          ///< numEventsInWaitList must be 0, indicating no wait
                          ///< events.
    void **ppMem,         ///< [out] pointer to USM memory object
    ur_event_handle_t
        *phEvent ///< [out][optional] return an event object that identifies the
                 ///< asynchronous USM device allocation
) {
  std::ignore = hQueue;
  std::ignore = pPool;
  std::ignore = size;
  std::ignore = pProperties;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = ppMem;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t urEnqueueUSMSharedAllocExp(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_usm_pool_handle_t
        pPool,         ///< [in][optional] handle of the USM memory pool
    const size_t size, ///< [in] minimum size in bytes of the USM memory object
                       ///< to be allocated
    const ur_exp_enqueue_usm_alloc_properties_t
        *pProperties, ///< [in][optional] pointer to the enqueue asynchronous
                      ///< USM allocation properties
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before the kernel execution. If nullptr, the
                          ///< numEventsInWaitList must be 0, indicating no wait
                          ///< events.
    void **ppMem,         ///< [out] pointer to USM memory object
    ur_event_handle_t
        *phEvent ///< [out][optional] return an event object that identifies the
                 ///< asynchronous USM shared allocation
) {
  std::ignore = hQueue;
  std::ignore = pPool;
  std::ignore = size;
  std::ignore = pProperties;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = ppMem;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t urEnqueueUSMHostAllocExp(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_usm_pool_handle_t
        pPool,         ///< [in][optional] handle of the USM memory pool
    const size_t size, ///< [in] minimum size in bytes of the USM memory object
                       ///< to be allocated
    const ur_exp_enqueue_usm_alloc_properties_t
        *pProperties, ///< [in][optional] pointer to the enqueue asynchronous
                      ///< USM allocation properties
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before the kernel execution. If nullptr, the
                          ///< numEventsInWaitList must be 0, indicating no wait
                          ///< events.
    void **ppMem,         ///< [out] pointer to USM memory object
    ur_event_handle_t
        *phEvent ///< [out][optional] return an event object that identifies the
                 ///< asynchronous USM host allocation
) {
  std::ignore = hQueue;
  std::ignore = pPool;
  std::ignore = size;
  std::ignore = pProperties;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = ppMem;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t urEnqueueUSMFreeExp(
    ur_queue_handle_t hQueue, ///< [in] handle of the queue object
    ur_usm_pool_handle_t
        pPool,  ///< [in][optional] handle of the USM memory pooliptor
    void *pMem, ///< [in] pointer to USM memory object
    uint32_t numEventsInWaitList, ///< [in] size of the event wait list
    const ur_event_handle_t
        *phEventWaitList, ///< [in][optional][range(0, numEventsInWaitList)]
                          ///< pointer to a list of events that must be complete
                          ///< before the kernel execution. If nullptr, the
                          ///< numEventsInWaitList must be 0, indicating no wait
                          ///< events.
    ur_event_handle_t *phEvent ///< [out][optional] return an event object that
                               ///< identifies the asynchronous USM deallocation
) {
  std::ignore = hQueue;
  std::ignore = pPool;
  std::ignore = pMem;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
} // namespace ur::level_zero

static ur_result_t USMFreeImpl(ur_context_handle_t Context, void *Ptr) {
  auto ZeResult = ZE_CALL_NOCHECK(zeMemFree, (Context->ZeContext, Ptr));
  // Handle When the driver is already released
  if (ZeResult == ZE_RESULT_ERROR_UNINITIALIZED) {
    return UR_RESULT_SUCCESS;
  } else {
    return ze2urResult(ZeResult);
  }
}

static ur_result_t USMQueryPageSize(ur_context_handle_t Context, void *Ptr,
                                    size_t *PageSize) {
  ZeStruct<ze_memory_allocation_properties_t> AllocProperties = {};
  ZE2UR_CALL(zeMemGetAllocProperties,
             (Context->ZeContext, Ptr, &AllocProperties, nullptr));
  *PageSize = AllocProperties.pageSize;

  return UR_RESULT_SUCCESS;
}

umf_result_t L0MemoryProvider::initialize(ur_context_handle_t Ctx,
                                          ur_device_handle_t Dev) {
  Context = Ctx;
  Device = Dev;

  return UMF_RESULT_SUCCESS;
}

enum umf_result_t L0MemoryProvider::alloc(size_t Size, size_t Align,
                                          void **Ptr) {
  auto Res = allocateImpl(Ptr, Size, Align);
  if (Res != UR_RESULT_SUCCESS) {
    getLastStatusRef() = Res;
    return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
  }

  return UMF_RESULT_SUCCESS;
}

enum umf_result_t L0MemoryProvider::free(void *Ptr, size_t Size) {
  (void)Size;

  auto Res = USMFreeImpl(Context, Ptr);
  if (Res != UR_RESULT_SUCCESS) {
    getLastStatusRef() = Res;
    return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
  }

  return UMF_RESULT_SUCCESS;
}

umf_result_t L0MemoryProvider::GetL0MinPageSize(void *Mem, size_t *PageSize) {
  ur_result_t Res = UR_RESULT_SUCCESS;
  void *Ptr = Mem;

  if (!Mem) {
    Res = allocateImpl(&Ptr, 1, 0);
    if (Res != UR_RESULT_SUCCESS) {
      goto err_set_status;
    }
  }

  // Query L0 for the minimal page size.
  Res = USMQueryPageSize(Context, Ptr, PageSize);
  if (Res != UR_RESULT_SUCCESS) {
    goto err_dealloc;
  }

  if (!Mem) {
    Res = USMFreeImpl(Context, Ptr);
    if (Res != UR_RESULT_SUCCESS) {
      goto err_set_status;
    }
  }

  return UMF_RESULT_SUCCESS;

err_dealloc:
  if (!Mem) {
    USMFreeImpl(Context, Ptr);
  }
err_set_status:
  getLastStatusRef() = Res;
  return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
}

umf_result_t L0MemoryProvider::get_min_page_size(void *Ptr, size_t *PageSize) {
  std::ignore = Ptr;

  // Query L0 for min page size. Use provided 'Ptr'.
  if (Ptr) {
    return GetL0MinPageSize(Ptr, PageSize);
  }

  // Return cached min page size.
  if (MinPageSizeCached) {
    *PageSize = MinPageSize;
    return UMF_RESULT_SUCCESS;
  }

  // Query L0 for min page size and cache it in 'MinPageSize'.
  auto Ret = GetL0MinPageSize(nullptr, &MinPageSize);
  if (Ret) {
    return Ret;
  }

  *PageSize = MinPageSize;
  MinPageSizeCached = true;

  return UMF_RESULT_SUCCESS;
}

typedef struct ze_ipc_data_t {
  int pid;
  ze_ipc_mem_handle_t zeHandle;
} ze_ipc_data_t;

umf_result_t L0MemoryProvider::get_ipc_handle_size(size_t *Size) {
  UR_ASSERT(Size, UMF_RESULT_ERROR_INVALID_ARGUMENT);
  *Size = sizeof(ze_ipc_data_t);

  return UMF_RESULT_SUCCESS;
}

umf_result_t L0MemoryProvider::get_ipc_handle(const void *Ptr, size_t Size,
                                              void *IpcData) {
  std::ignore = Size;

  UR_ASSERT(Ptr && IpcData, UMF_RESULT_ERROR_INVALID_ARGUMENT);
  ze_ipc_data_t *zeIpcData = (ze_ipc_data_t *)IpcData;
  auto Ret = ZE_CALL_NOCHECK(zeMemGetIpcHandle,
                             (Context->ZeContext, Ptr, &zeIpcData->zeHandle));
  if (Ret != ZE_RESULT_SUCCESS) {
    return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
  }

  zeIpcData->pid = ur_getpid();

  return UMF_RESULT_SUCCESS;
}

umf_result_t L0MemoryProvider::put_ipc_handle(void *IpcData) {
  UR_ASSERT(IpcData, UMF_RESULT_ERROR_INVALID_ARGUMENT);
  ze_ipc_data_t *zeIpcData = (ze_ipc_data_t *)IpcData;
  std::ignore = zeIpcData;

  // zeMemPutIpcHandle was introduced in Level Zero 1.6. Before Level Zero 1.6,
  // IPC handle was released automatically when corresponding memory buffer
  // was freed.
#if (ZE_API_VERSION_CURRENT >= ZE_MAKE_VERSION(1, 6))
  auto Ret = ZE_CALL_NOCHECK(zeMemPutIpcHandle,
                             (Context->ZeContext, zeIpcData->zeHandle));
  if (Ret != ZE_RESULT_SUCCESS) {
    return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
  }
#endif

  return UMF_RESULT_SUCCESS;
}

umf_result_t L0MemoryProvider::open_ipc_handle(void *IpcData, void **Ptr) {
  UR_ASSERT(IpcData && Ptr, UMF_RESULT_ERROR_INVALID_ARGUMENT);
  ze_ipc_data_t *zeIpcData = (ze_ipc_data_t *)IpcData;

  int fdLocal = -1;
  if (zeIpcData->pid != ur_getpid()) {
    int fdRemote = -1;
    memcpy(&fdRemote, &zeIpcData->zeHandle, sizeof(fdRemote));
    fdLocal = ur_duplicate_fd(zeIpcData->pid, fdRemote);
    if (fdLocal == -1) {
      logger::error("duplicating file descriptor from IPC handle failed");
      return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
    }

    memcpy(&zeIpcData->zeHandle, &fdLocal, sizeof(fdLocal));
  }

  auto Ret =
      ZE_CALL_NOCHECK(zeMemOpenIpcHandle, (Context->ZeContext, Device->ZeDevice,
                                           zeIpcData->zeHandle, 0, Ptr));
  if (fdLocal != -1) {
    ur_close_fd(fdLocal);
  }

  if (Ret != ZE_RESULT_SUCCESS) {
    return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
  }

  return UMF_RESULT_SUCCESS;
}

umf_result_t L0MemoryProvider::close_ipc_handle(void *Ptr, size_t Size) {
  std::ignore = Size;

  UR_ASSERT(Ptr, UMF_RESULT_ERROR_INVALID_ARGUMENT);
  auto Ret = ZE_CALL_NOCHECK(zeMemCloseIpcHandle, (Context->ZeContext, Ptr));
  if (Ret != ZE_RESULT_SUCCESS) {
    return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
  }

  return UMF_RESULT_SUCCESS;
}

ur_result_t L0SharedMemoryProvider::allocateImpl(void **ResultPtr, size_t Size,
                                                 uint32_t Alignment) {
  return USMSharedAllocImpl(ResultPtr, Context, Device, /*host flags*/ 0,
                            /*device flags*/ 0, Size, Alignment);
}

ur_result_t L0SharedReadOnlyMemoryProvider::allocateImpl(void **ResultPtr,
                                                         size_t Size,
                                                         uint32_t Alignment) {
  ur_usm_device_desc_t UsmDeviceDesc{};
  UsmDeviceDesc.flags = UR_USM_DEVICE_MEM_FLAG_DEVICE_READ_ONLY;
  return USMSharedAllocImpl(ResultPtr, Context, Device, UsmDeviceDesc.flags,
                            /*host flags*/ 0, Size, Alignment);
}

ur_result_t L0DeviceMemoryProvider::allocateImpl(void **ResultPtr, size_t Size,
                                                 uint32_t Alignment) {
  return USMDeviceAllocImpl(ResultPtr, Context, Device, /* flags */ 0, Size,
                            Alignment);
}

ur_result_t L0HostMemoryProvider::allocateImpl(void **ResultPtr, size_t Size,
                                               uint32_t Alignment) {
  return USMHostAllocImpl(ResultPtr, Context, /* flags */ 0, Size, Alignment);
}

static usm::DisjointPoolMemType
DescToDisjointPoolMemType(const usm::pool_descriptor &desc) {
  switch (desc.type) {
  case UR_USM_TYPE_DEVICE:
    return usm::DisjointPoolMemType::Device;
  case UR_USM_TYPE_SHARED:
    if (desc.deviceReadOnly)
      return usm::DisjointPoolMemType::SharedReadOnly;
    else
      return usm::DisjointPoolMemType::Shared;
  case UR_USM_TYPE_HOST:
    return usm::DisjointPoolMemType::Host;
  default:
    throw UR_RESULT_ERROR_INVALID_ARGUMENT;
  }
}

typedef usm::pool_descriptor l0_memory_provider_params_t;

template <typename ProviderParams = std::nullptr_t>
static umf::provider_unique_handle_t
MakeProvider(ProviderParams *Params = nullptr) {
  if constexpr (std::is_same_v<ProviderParams,
                               umf_fixed_memory_provider_params_handle_t>) {
    auto [Ret, FixedProvider] =
        umf::providerMakeUniqueFromOps(umfFixedMemoryProviderOps(), *Params);
    if (Ret) {
      logger::error("urUSMPoolCreate: failed to create UMF fixed provider");
      throw UsmAllocationException(umf::umf2urResult(Ret));
    }

    return std::move(FixedProvider);
  } else if constexpr (std::is_same_v<ProviderParams,
                                      l0_memory_provider_params_t>) {
    umf_result_t Ret = UMF_RESULT_SUCCESS;
    umf::provider_unique_handle_t &&L0Provider = nullptr;

    switch (Params->type) {
    case UR_USM_TYPE_HOST:
      std::tie(Ret, L0Provider) =
          umf::memoryProviderMakeUnique<L0HostMemoryProvider>(Params->hContext,
                                                              Params->hDevice);
      break;
    case UR_USM_TYPE_DEVICE:
      std::tie(Ret, L0Provider) =
          umf::memoryProviderMakeUnique<L0DeviceMemoryProvider>(
              Params->hContext, Params->hDevice);
      break;
    case UR_USM_TYPE_SHARED:
      if (Params->deviceReadOnly) {
        std::tie(Ret, L0Provider) =
            umf::memoryProviderMakeUnique<L0SharedReadOnlyMemoryProvider>(
                Params->hContext, Params->hDevice);
      } else {
        std::tie(Ret, L0Provider) =
            umf::memoryProviderMakeUnique<L0SharedMemoryProvider>(
                Params->hContext, Params->hDevice);
      }
      break;
    default:
      logger::error("urUSMPoolCreate: invalid USM type found");
      Ret = UMF_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (Ret != UMF_RESULT_SUCCESS) {
      logger::error("urUSMPoolCreate: failed to create UMF provider");
      throw UsmAllocationException(umf::umf2urResult(Ret));
    }

    return std::move(L0Provider);
  }

  return nullptr;
}

template <typename PoolParams = std::nullptr_t>
static umf::pool_unique_handle_t
MakePool(umf::provider_unique_handle_t &Provider,
         PoolParams *Params = nullptr) {
  if constexpr (std::is_same_v<PoolParams, usm::umf_disjoint_pool_config_t>) {
    auto UmfParamsHandle = usm::getUmfParamsHandle(*Params);
    auto [Ret, Pool] = umf::poolMakeUniqueFromOps(
        umfDisjointPoolOps(), std::move(Provider), UmfParamsHandle.get());
    if (Ret != UMF_RESULT_SUCCESS) {
      logger::error("urUSMPoolCreate: failed to create UMF pool");
      throw UsmAllocationException(umf::umf2urResult(Ret));
    }

    return std::move(Pool);
  } else if constexpr (std::is_same_v<PoolParams, std::nullptr_t>) {
    auto [Ret, Pool] = umf::poolMakeUnique<USMProxyPool>(std::move(Provider));
    if (Ret != UMF_RESULT_SUCCESS) {
      logger::error("urUSMPoolCreate: failed to create UMF pool");
      throw UsmAllocationException(umf::umf2urResult(Ret));
    }

    return std::move(Pool);
  }

  return nullptr;
}

ur_usm_pool_handle_t_::ur_usm_pool_handle_t_(ur_context_handle_t Context,
                                             ur_usm_pool_desc_t *PoolDesc,
                                             bool IsProxy)
    : Context(Context) {
  // TODO: handle zero-init flag 'UR_USM_POOL_FLAG_ZERO_INITIALIZE_BLOCK'
  auto DisjointPoolConfigs = InitializeDisjointPoolConfig();
  if (auto Limits = find_stype_node<ur_usm_pool_limits_desc_t>(PoolDesc)) {
    for (auto &Config : DisjointPoolConfigs.Configs) {
      Config.MaxPoolableSize = Limits->maxPoolableSize;
      Config.SlabMinSize = Limits->minDriverAllocSize;
    }
  }

  // Create a native pool from user provided buffer.
  if (auto NativeDesc = find_stype_node<ur_usm_pool_native_desc_t>(PoolDesc)) {

    umf_fixed_memory_provider_params_handle_t FixedProviderParams = nullptr;
    auto Ret = umfFixedMemoryProviderParamsCreate(
        &FixedProviderParams, NativeDesc->pMem, NativeDesc->size);
    if (Ret) {
      logger::error(
          "urUSMPoolCreate: failed to create UMF fixed provider params");
      throw UsmAllocationException(umf::umf2urResult(Ret));
    }

    auto Provider = MakeProvider(&FixedProviderParams);
    auto Desc = usm::pool_descriptor{this, Context, NativeDesc->device,
                                     NativeDesc->memType, false};
    PoolManager.addPool(Desc, MakePool(Provider));
    return; // Skip the default pool initialization.
  }

  auto [Ret, Descriptors] = usm::pool_descriptor::create(this, Context);
  if (Ret) {
    logger::error("urUSMPoolCreate: failed to create pool descriptors");
    throw UsmAllocationException(Ret);
  }

  for (auto &Desc : Descriptors) {
    umf::pool_unique_handle_t Pool = nullptr;
    if (IsProxy) {
      auto Provider = MakeProvider(&Desc);
      PoolManager.addPool(Desc, MakePool(Provider));
    } else {
      auto Provider = MakeProvider(&Desc);
      auto &PoolConfig =
          DisjointPoolConfigs.Configs[DescToDisjointPoolMemType(Desc)];
      Pool = MakePool(Provider, &PoolConfig);
    }

    Ret = PoolManager.addPool(Desc, std::move(Pool));
    if (Ret) {
      logger::error("urUSMPoolCreate: failed to store UMF pool");
      throw UsmAllocationException(Ret);
    }
  }
}

umf_memory_pool_handle_t
ur_usm_pool_handle_t_::getPool(const usm::pool_descriptor &Desc) {
  auto PoolOpt = PoolManager.getPool(Desc);
  return PoolOpt.has_value() ? PoolOpt.value() : nullptr;
}

ur_result_t ur_usm_pool_handle_t_::allocate(ur_context_handle_t Context,
                                            ur_device_handle_t Device,
                                            const ur_usm_desc_t *USMDesc,
                                            ur_usm_type_t Type, size_t Size,
                                            void **RetMem) {
  uint32_t Alignment = USMDesc ? USMDesc->align : 0;
  // L0 supports alignment up to 64KB and silently ignores higher values.
  // We flag alignment > 64KB as an invalid value.
  // L0 spec says that alignment values that are not powers of 2 are invalid.
  // If alignment == 0, then we are allowing the L0 driver to choose the
  // alignment so no need to check.
  if (Alignment > 0) {
    if (Alignment > 65536 || (Alignment & (Alignment - 1)) != 0)
      return UR_RESULT_ERROR_INVALID_VALUE;
  }

  // Handle the extension structures for 'ur_usm_desc_t'.
  if (auto UsmHostDesc = find_stype_node<ur_usm_host_desc_t>(USMDesc)) {
    std::ignore = UsmHostDesc; // Unused
  }

  bool DeviceReadOnly = false;
  if (auto UsmDeviceDesc = find_stype_node<ur_usm_device_desc_t>(USMDesc)) {
    DeviceReadOnly =
        (Type == UR_USM_TYPE_SHARED) &&
        (UsmDeviceDesc->flags & UR_USM_DEVICE_MEM_FLAG_DEVICE_READ_ONLY);
  }

  ur_platform_handle_t Plt =
      (Device) ? Device->Platform : Context->getPlatform();
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
    UR_CALL(ur::level_zero::urContextRetain(Context));
  }

  auto umfPool = getPool(
      usm::pool_descriptor{this, Context, Device, Type, DeviceReadOnly});
  if (!umfPool) {
    return UR_RESULT_ERROR_INVALID_ARGUMENT;
  }

  *RetMem = umfPoolAlignedMalloc(umfPool, Size, Alignment);
  if (*RetMem == nullptr) {
    auto umfRet = umfPoolGetLastAllocationError(umfPool);
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

bool ur_usm_pool_handle_t_::hasPool(const umf_memory_pool_handle_t Pool) {
  return PoolManager.hasPool(Pool);
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
  return umf2urResult(umfRet);
}
