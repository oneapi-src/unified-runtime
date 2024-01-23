//===--------- usm.cpp - Libomptarget Adapter -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

#include "usm.hpp"
#include "common.hpp"
#include "context.hpp"
#include "event.hpp"
#include "omptarget.h"
#include "omptargetplugin.h"
#include <cstring>

namespace {

static ur_result_t USMFreeImpl(ur_context_handle_t Context, void *Ptr) {
  USMAllocation alloc;
  UR_ASSERT(Context->findUSMAllocation(alloc, Ptr) == UR_RESULT_SUCCESS,
            UR_RESULT_ERROR_INVALID_MEM_OBJECT);

  int32_t AllocKind;
  // MISSING FUNCTIONALITY: We can't free USM host and USM shared memory -
  // the CUDA plugin implementation first checks to see if the allocations are
  // managed by the memory manager; if not it directly tries to free the memory
  // via CUDA, but throws away the TargetAllocTy, which means it doesn't know
  // whether to call cuMemFree or cuMemFreeHost and may crash.
  // This will leak memory!!
  switch (alloc.alloc_type) {
  case UR_USM_TYPE_HOST:
  case UR_USM_TYPE_SHARED: {
    UR_ASSERT(Context->removeUSMAllocation(Ptr) == UR_RESULT_SUCCESS,
              UR_RESULT_ERROR_INVALID_MEM_OBJECT);
    return UR_RESULT_SUCCESS;
  }

  case UR_USM_TYPE_DEVICE:
    AllocKind = TARGET_ALLOC_DEVICE;
    break;

  case UR_USM_TYPE_UNKNOWN:
    AllocKind = TARGET_ALLOC_DEFAULT;
    break;

  default:
    return UR_RESULT_ERROR_INVALID_MEM_OBJECT;
  }

  __tgt_rtl_data_delete(Context->getDevice()->getID(), alloc.usm_ptr,
                        AllocKind);

  UR_ASSERT(Context->removeUSMAllocation(Ptr) == UR_RESULT_SUCCESS,
            UR_RESULT_ERROR_INVALID_MEM_OBJECT);
  return UR_RESULT_SUCCESS;
}

static ur_result_t
USMDeviceAllocImpl(void **ResultPtr, ur_context_handle_t Context,
                   ur_device_handle_t Device,
                   [[maybe_unused]] ur_usm_device_mem_flags_t *Flags,
                   size_t Size, uint32_t Alignment) {

  *ResultPtr =
      __tgt_rtl_data_alloc(Device->getID(), Size, nullptr, TARGET_ALLOC_DEVICE);
  if (!*ResultPtr) {
    return UR_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
  }
  USMAllocation alloc_info{UR_USM_TYPE_DEVICE, *ResultPtr, Size, Alignment,
                           Device};
  Context->addUSMAllocation(alloc_info);
  return UR_RESULT_SUCCESS;
}

static ur_result_t USMSharedAllocImpl(void **ResultPtr,
                                      ur_context_handle_t Context,
                                      ur_device_handle_t Device,
                                      ur_usm_host_mem_flags_t *,
                                      ur_usm_device_mem_flags_t *, size_t Size,
                                      uint32_t Alignment) {
  *ResultPtr =
      __tgt_rtl_data_alloc(Device->getID(), Size, nullptr, TARGET_ALLOC_SHARED);
  if (!*ResultPtr) {
    return UR_RESULT_ERROR_OUT_OF_RESOURCES;
  }
  USMAllocation alloc_info{UR_USM_TYPE_SHARED, *ResultPtr, Size, Alignment,
                           Device};
  Context->addUSMAllocation(alloc_info);
  return UR_RESULT_SUCCESS;
}

static ur_result_t
USMHostAllocImpl(void **ResultPtr, ur_context_handle_t Context,
                 [[maybe_unused]] ur_usm_host_mem_flags_t *Flags, size_t Size,
                 uint32_t Alignment) {
  *ResultPtr = __tgt_rtl_data_alloc(Context->getDevice()->getID(), Size,
                                    nullptr, TARGET_ALLOC_HOST);
  if (!*ResultPtr) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  }
  USMAllocation alloc_info{UR_USM_TYPE_HOST, *ResultPtr, Size, Alignment,
                           Context->getDevice()};
  Context->addUSMAllocation(alloc_info);
  return UR_RESULT_SUCCESS;
}

} // namespace

ur_usm_pool_handle_t_::ur_usm_pool_handle_t_(ur_context_handle_t Context,
                                             ur_usm_pool_desc_t *PoolDesc)
    : zeroInit(PoolDesc->flags & UR_USM_POOL_FLAG_ZERO_INITIALIZE_BLOCK),
      ReferenceCount(1), Context(Context) {
  urContextRetain(Context);
  const void *pNext = PoolDesc ? PoolDesc->pNext : nullptr;
  while (pNext) {
    const ur_base_desc_t *BaseDesc = static_cast<const ur_base_desc_t *>(pNext);
    switch (BaseDesc->stype) {
    case UR_STRUCTURE_TYPE_USM_POOL_LIMITS_DESC: {
      const ur_usm_pool_limits_desc_t *Limits =
          static_cast<const ur_usm_pool_limits_desc_t *>(pNext);
      for (auto &config : DisjointPoolConfigs.Configs) {
        config.MaxPoolableSize = Limits->maxPoolableSize;
        config.SlabMinSize = Limits->minDriverAllocSize;
      }
      (void)Limits;
    } break;

    default:
      break;
    }
    pNext = BaseDesc->pNext;
  }

  // Host memory allocator
  auto MemProvider =
      umf::memoryProviderMakeUnique<USMHostMemoryProvider>(Context, nullptr)
          .second;
  HostMemPool =
      umf::poolMakeUnique<usm::DisjointPool, 1>(
          {std::move(MemProvider)},
          this->DisjointPoolConfigs.Configs[usm::DisjointPoolMemType::Host])
          .second;

  // Device memory allocator
  MemProvider = umf::memoryProviderMakeUnique<USMDeviceMemoryProvider>(
                    Context, Context->getDevice())
                    .second;
  DeviceMemPool =
      umf::poolMakeUnique<usm::DisjointPool, 1>(
          {std::move(MemProvider)},
          this->DisjointPoolConfigs.Configs[usm::DisjointPoolMemType::Device])
          .second;

  // Shared memory allocator
  MemProvider = umf::memoryProviderMakeUnique<USMSharedMemoryProvider>(
                    Context, Context->getDevice())
                    .second;
  SharedMemPool =
      umf::poolMakeUnique<usm::DisjointPool, 1>(
          {std::move(MemProvider)},
          this->DisjointPoolConfigs.Configs[usm::DisjointPoolMemType::Shared])
          .second;

  // add the pool to the context
  Context->addPool(this);
}

ur_usm_pool_handle_t_::~ur_usm_pool_handle_t_() {
  Context->removePool(this);
  urContextRelease(Context);
}

bool ur_usm_pool_handle_t_::hasUMFPool(umf_memory_pool_t *UMFPool) {
  return HostMemPool.get() == UMFPool || DeviceMemPool.get() == UMFPool ||
         SharedMemPool.get() == UMFPool;
}

umf_result_t USMMemoryProvider::initialize(ur_context_handle_t Ctx,
                                           ur_device_handle_t Dev) {
  Context = Ctx;
  Device = Dev;
  MinPageSize = 0;
  return UMF_RESULT_SUCCESS;
}

umf_result_t USMMemoryProvider::alloc(size_t Size, size_t Align, void **Ptr) {
  auto Result = allocateImpl(Ptr, Size, Align);
  if (Result != UR_RESULT_SUCCESS) {
    getLastStatusRef() = Result;
    return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
  }
  return UMF_RESULT_SUCCESS;
}

umf_result_t USMMemoryProvider::free(void *Ptr, size_t Size) {
  std::ignore = Size;
  auto Result = USMFreeImpl(Context, Ptr);
  if (Result != UR_RESULT_SUCCESS) {
    getLastStatusRef() = Result;
    return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
  }
  return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
}

void USMMemoryProvider::get_last_native_error(const char **ErrMsg,
                                              int32_t *ErrCode) {
  std::ignore = ErrMsg;
  *ErrCode = static_cast<int32_t>(getLastStatusRef());
}

umf_result_t USMMemoryProvider::get_min_page_size(void *Ptr, size_t *PageSize) {
  std::ignore = Ptr;
  *PageSize = MinPageSize;
  return UMF_RESULT_SUCCESS;
}

ur_result_t USMSharedMemoryProvider::allocateImpl(void **ResultPtr, size_t Size,
                                                  uint32_t Alignment) {
  return USMSharedAllocImpl(ResultPtr, Context, Device, nullptr, nullptr, Size,
                            Alignment);
}

ur_result_t USMDeviceMemoryProvider::allocateImpl(void **ResultPtr, size_t Size,
                                                  uint32_t Alignment) {
  return USMDeviceAllocImpl(ResultPtr, Context, Device, nullptr, Size,
                            Alignment);
}

ur_result_t USMSharedReadOnlyMemoryProvider::allocateImpl(void **ResultPtr,
                                                          size_t Size,
                                                          uint32_t Alignment) {
  ur_usm_device_mem_flags_t DeviceFlags =
      UR_USM_DEVICE_MEM_FLAG_DEVICE_READ_ONLY;
  return USMSharedAllocImpl(ResultPtr, Context, Device, nullptr, &DeviceFlags,
                            Size, Alignment);
}

ur_result_t USMHostMemoryProvider::allocateImpl(void **ResultPtr, size_t Size,
                                                uint32_t Alignment) {
  return USMHostAllocImpl(ResultPtr, Context, nullptr, Size, Alignment);
}

UR_APIEXPORT ur_result_t UR_APICALL
urUSMHostAlloc(ur_context_handle_t hContext, const ur_usm_desc_t *pUSMDesc,
               ur_usm_pool_handle_t hPool, size_t size, void **ppMem) {
  uint32_t Alignment = pUSMDesc ? pUSMDesc->align : 0;
  if (!hPool) {
    ur_usm_host_mem_flags_t HostFlags = 0;
    const void *pNext = pUSMDesc ? pUSMDesc->pNext : nullptr;
    while (pNext) {
      auto *BaseDesc = static_cast<const ur_base_desc_t *>(pNext);
      switch (BaseDesc->stype) {
      case UR_STRUCTURE_TYPE_USM_HOST_DESC:
        HostFlags |= static_cast<const ur_usm_host_desc_t *>(pNext)->flags;
        break;
      default:
        break;
      }
      pNext = BaseDesc->pNext;
    }
    return USMHostAllocImpl(ppMem, hContext, &HostFlags, size, Alignment);
  }

  auto HostPool = hPool->HostMemPool.get();
  *ppMem = umfPoolAlignedMalloc(HostPool, size, Alignment);
  if (!*ppMem) {
    const auto umfErr = umfPoolGetLastAllocationError(HostPool);
    return umf::umf2urResult(umfErr);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urUSMDeviceAlloc(ur_context_handle_t hContext, ur_device_handle_t hDevice,
                 const ur_usm_desc_t *pUSMDesc, ur_usm_pool_handle_t hPool,
                 size_t size, void **ppMem) {
  uint32_t Alignment = pUSMDesc ? pUSMDesc->align : 0;
  if (!hPool) {
    ur_usm_device_mem_flags_t DeviceFlags = 0;
    const void *pNext = pUSMDesc ? pUSMDesc->pNext : nullptr;
    while (pNext) {
      auto *BaseDesc = static_cast<const ur_base_desc_t *>(pNext);
      switch (BaseDesc->stype) {
      case UR_STRUCTURE_TYPE_USM_DEVICE_DESC:
        DeviceFlags |= static_cast<const ur_usm_device_desc_t *>(pNext)->flags;
        break;
      default:
        break;
      }
      pNext = BaseDesc->pNext;
    }

    return USMDeviceAllocImpl(ppMem, hContext, hDevice, &DeviceFlags, size,
                              Alignment);
  }

  auto DevicePool = hPool->DeviceMemPool.get();
  *ppMem = umfPoolAlignedMalloc(DevicePool, size, Alignment);
  if (!*ppMem) {
    auto umfErr = umfPoolGetLastAllocationError(DevicePool);
    return umf::umf2urResult(umfErr);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urUSMSharedAlloc(ur_context_handle_t hContext, ur_device_handle_t hDevice,
                 const ur_usm_desc_t *pUSMDesc, ur_usm_pool_handle_t hPool,
                 size_t size, void **ppMem) {
  uint32_t Alignment = pUSMDesc ? pUSMDesc->align : 0;
  if (!hPool) {
    ur_usm_host_mem_flags_t HostFlags = 0;
    ur_usm_device_mem_flags_t DeviceFlags = 0;
    const void *pNext = pUSMDesc ? pUSMDesc->pNext : nullptr;
    while (pNext) {
      auto *BaseDesc = static_cast<const ur_base_desc_t *>(pNext);
      switch (BaseDesc->stype) {
      case UR_STRUCTURE_TYPE_USM_DEVICE_DESC:
        DeviceFlags |= static_cast<const ur_usm_device_desc_t *>(pNext)->flags;
        break;
      case UR_STRUCTURE_TYPE_USM_HOST_DESC:
        HostFlags |= static_cast<const ur_usm_host_desc_t *>(pNext)->flags;
        break;
      default:
        break;
      }
      pNext = BaseDesc->pNext;
    }

    return USMSharedAllocImpl(ppMem, hContext, hDevice, &HostFlags,
                              &DeviceFlags, size, Alignment);
  }

  auto SharedPool = hPool->SharedMemPool.get();
  *ppMem = umfPoolAlignedMalloc(SharedPool, size, Alignment);
  if (!*ppMem) {
    auto umfErr = umfPoolGetLastAllocationError(SharedPool);
    return umf::umf2urResult(umfErr);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urUSMFree(ur_context_handle_t hContext,
                                              void *pMem) {
  return USMFreeImpl(hContext, pMem);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMFill(
    ur_queue_handle_t hQueue, void *ptr, size_t patternSize,
    const void *pPattern, size_t size, uint32_t numEventsInWaitList,
    const ur_event_handle_t *phEventWaitList, ur_event_handle_t *phEvent) {
  ur_result_t Result = UR_RESULT_SUCCESS;
  std::unique_ptr<ur_event_handle_t_> EventPtr{nullptr};

  Result = waitForEvents(numEventsInWaitList, phEventWaitList);
  if (Result != UR_RESULT_SUCCESS) {
    return Result;
  }

  // libomptarget does not have a fill API so just create a buffer on host and
  // copy it the target
  std::vector<uint8_t> fillData(size);
  const size_t NumPatternRepetitions = size / patternSize;
  for (size_t i = 0; i < NumPatternRepetitions; i++) {
    std::memcpy(fillData.data() + (i * patternSize), pPattern, patternSize);
  }

  if (phEvent) {
    EventPtr =
        std::unique_ptr<ur_event_handle_t_>(ur_event_handle_t_::makeEvent(
            hQueue, hQueue->Context, UR_COMMAND_USM_FILL));
    EventPtr->start();
  }

  const auto OmpResult = __tgt_rtl_data_submit_async(
      hQueue->getDevice()->getID(), ptr, fillData.data(), fillData.size(),
      hQueue->AsyncInfo);
  if (OmpResult != OFFLOAD_SUCCESS) {
    return UR_RESULT_ERROR_ADAPTER_SPECIFIC;
  }

  if (phEvent) {
    Result = EventPtr->record();
    *phEvent = EventPtr.release();
  }

  return Result;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMMemcpy(
    ur_queue_handle_t hQueue, bool blocking, void *pDst, const void *pSrc,
    size_t size, uint32_t numEventsInWaitList,
    const ur_event_handle_t *phEventWaitList, ur_event_handle_t *phEvent) {

  ur_result_t Result = UR_RESULT_SUCCESS;
  std::unique_ptr<ur_event_handle_t_> EventPtr{nullptr};
  Result = waitForEvents(numEventsInWaitList, phEventWaitList);

  if (phEvent || blocking) {
    EventPtr =
        std::unique_ptr<ur_event_handle_t_>(ur_event_handle_t_::makeEvent(
            hQueue, hQueue->Context, UR_COMMAND_USM_MEMCPY));
    EventPtr->start();
  }

  USMAllocation DstAllocInfo;
  bool DstIsHost = false;
  if (hQueue->Context->findUSMAllocation(DstAllocInfo, pDst) !=
      UR_RESULT_SUCCESS) {
    DstIsHost = true;
  }

  USMAllocation SrcAllocInfo;
  bool SrcIsHost = false;
  if (hQueue->Context->findUSMAllocation(SrcAllocInfo, pSrc) !=
      UR_RESULT_SUCCESS) {
    SrcIsHost = true;
  }

  int32_t OmpResult = OFFLOAD_SUCCESS;
  if (SrcIsHost && DstIsHost) {
    std::memcpy(pDst, pSrc, size);
  }
  // from host to device
  else if (SrcIsHost && !DstIsHost) {
    OmpResult = __tgt_rtl_data_submit(hQueue->Device->getID(), pDst,
                                      const_cast<void *>(pSrc), size);
  }
  // from device to host
  else if (!SrcIsHost && DstIsHost) {
    OmpResult = __tgt_rtl_data_retrieve(SrcAllocInfo.Device->getID(), pDst,
                                        const_cast<void *>(pSrc), size);
  }
  // device to device
  else if (!SrcIsHost && !DstIsHost) {
    OmpResult = __tgt_rtl_data_exchange(
        SrcAllocInfo.Device->getID(), const_cast<void *>(pSrc),
        DstAllocInfo.Device->getID(), pDst, size);
  }

  if (OmpResult != OFFLOAD_SUCCESS) {
    return UR_RESULT_ERROR_ADAPTER_SPECIFIC;
  }

  if (phEvent || blocking) {
    Result = EventPtr->record();
  }

  if (blocking) {
    ur_event_handle_t WaitForEvent = EventPtr.get();
    Result = waitForEvents(1, &WaitForEvent);
  }

  if (phEvent) {
    *phEvent = EventPtr.release();
  }

  return Result;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMPrefetch(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] const void *pMem, [[maybe_unused]] size_t size,
    [[maybe_unused]] ur_usm_migration_flags_t flags,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMAdvise(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] const void *pMem, [[maybe_unused]] size_t size,
    [[maybe_unused]] ur_usm_advice_flags_t advice,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMFill2D(
    [[maybe_unused]] ur_queue_handle_t hQueue, [[maybe_unused]] void *pMem,
    [[maybe_unused]] size_t pitch, [[maybe_unused]] size_t patternSize,
    [[maybe_unused]] const void *pPattern, [[maybe_unused]] size_t width,
    [[maybe_unused]] size_t height,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueUSMMemcpy2D(
    [[maybe_unused]] ur_queue_handle_t hQueue, [[maybe_unused]] bool blocking,
    [[maybe_unused]] void *pDst, [[maybe_unused]] size_t dstPitch,
    [[maybe_unused]] const void *pSrc, [[maybe_unused]] size_t srcPitch,
    [[maybe_unused]] size_t width, [[maybe_unused]] size_t height,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL
urUSMGetMemAllocInfo(ur_context_handle_t hContext, const void *pMem,
                     ur_usm_alloc_info_t propName, size_t propSize,
                     void *pPropValue, size_t *pPropSizeRet) {

  UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);
  USMAllocation AllocInfo;
  if (hContext->findUSMAllocation(AllocInfo, pMem) != UR_RESULT_SUCCESS) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  switch (propName) {
  case UR_USM_ALLOC_INFO_TYPE:
    return ReturnValue(AllocInfo.alloc_type);
  case UR_USM_ALLOC_INFO_BASE_PTR:
    return ReturnValue(AllocInfo.usm_ptr);
  case UR_USM_ALLOC_INFO_SIZE:
    return ReturnValue(AllocInfo.alloc_size);
  case UR_USM_ALLOC_INFO_DEVICE:
    return ReturnValue(hContext->getDevice());
  case UR_USM_ALLOC_INFO_POOL: {
    auto UMFPool = umfPoolByPtr(pMem);
    if (!UMFPool) {
      return UR_RESULT_ERROR_INVALID_VALUE;
    }
    ur_usm_pool_handle_t Pool = hContext->getOwningURPool(UMFPool);
    if (!Pool) {
      return UR_RESULT_ERROR_INVALID_VALUE;
    }
    return ReturnValue(Pool);
  }

  default:
    return UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
  }

  return UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
}

UR_APIEXPORT ur_result_t UR_APICALL
urUSMImportExp([[maybe_unused]] ur_context_handle_t Context,
               [[maybe_unused]] void *HostPtr, [[maybe_unused]] size_t Size) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL
urUSMReleaseExp([[maybe_unused]] ur_context_handle_t Context,
                [[maybe_unused]] void *HostPtr) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL
urUSMPoolCreate(ur_context_handle_t hContext, ur_usm_pool_desc_t *pPoolDesc,
                ur_usm_pool_handle_t *ppPool) {
  ur_result_t Result = UR_RESULT_SUCCESS;
  try {
    auto Pool = std::make_unique<ur_usm_pool_handle_t_>(hContext, pPoolDesc);
    *ppPool = Pool.release();
  } catch (std::bad_alloc &) {
    Result = UR_RESULT_ERROR_OUT_OF_RESOURCES;
  } catch (...) {
    Result = UR_RESULT_ERROR_UNKNOWN;
  }
  return Result;
}

UR_APIEXPORT ur_result_t UR_APICALL
urUSMPoolRetain(ur_usm_pool_handle_t pPool) {
  UR_ASSERT(pPool->GetReferenceCount() > 0, UR_RESULT_ERROR_INVALID_MEM_OBJECT);
  pPool->IncrementReferenceCount();
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urUSMPoolRelease(ur_usm_pool_handle_t pPool) {
  ur_result_t Result = UR_RESULT_SUCCESS;
  try {
    if (pPool->DecrementReferenceCount() > 0) {
      Result = UR_RESULT_SUCCESS;
    } else {
      std::unique_ptr<ur_usm_pool_handle_t_> Pool(pPool);
    }
  } catch (...) {
    Result = UR_RESULT_ERROR_UNKNOWN;
  }
  return Result;
}

UR_APIEXPORT ur_result_t UR_APICALL
urUSMPoolGetInfo(ur_usm_pool_handle_t hPool, ur_usm_pool_info_t propName,
                 size_t propSize, void *pPropValue, size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);
  switch (propName) {
  case UR_USM_POOL_INFO_CONTEXT:
    return ReturnValue(hPool->Context);

  case UR_USM_POOL_INFO_REFERENCE_COUNT:
    return ReturnValue(hPool->GetReferenceCount());

  default:
    return UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
  }
  return UR_RESULT_SUCCESS;
}
