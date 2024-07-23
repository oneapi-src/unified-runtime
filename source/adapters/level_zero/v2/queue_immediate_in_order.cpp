//===--------- queue_immediate_in_order.cpp - Level Zero Adapter ---------===//
//
// Copyright (C) 2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "queue_immediate_in_order.hpp"

#include "../kernel.hpp"
#include "../program.hpp"

namespace v2 {

ur_wait_list_t::ur_wait_list_t(const ur_event_handle_t *phWaitEvents,
                               uint32_t numWaitEvents,
                               ze_event_handle_t pExtraZeEvent) {
  auto TotalEvents = numWaitEvents + (pExtraZeEvent != nullptr);
  if (TotalEvents > 1) {
    std::vector<ze_event_handle_t> WaitListVec;
    WaitListVec.reserve(TotalEvents);

    for (uint32_t i = 0; i < numWaitEvents; i++) {
      WaitListVec.push_back(phWaitEvents[i]->getZeEvent());
    }

    if (pExtraZeEvent) {
      WaitListVec.push_back(pExtraZeEvent);
    }

    WaitList = std::move(WaitListVec);
  } else if (numWaitEvents == 1) {
    WaitList = phWaitEvents[0]->getZeEvent();
  } else if (pExtraZeEvent != nullptr) {
    WaitList = pExtraZeEvent;
  } else { // no events
    WaitList = std::monostate{};
  }
}

std::pair<ze_event_handle_t *, uint32_t> ur_wait_list_t::getView() {
  if (auto singleEvent = std::get_if<ze_event_handle_t>(&WaitList)) {
    return {singleEvent, 1};
  } else if (auto waitList =
                 std::get_if<std::vector<ze_event_handle_t>>(&WaitList)) {
    return {waitList->data(), waitList->size()};
  } else {
    return {nullptr, 0};
  }
}

static int32_t getZeOrdinal(ur_device_handle_t hDevice, queue_group_type type) {
  if (type == queue_group_type::MainCopy && hDevice->hasMainCopyEngine()) {
    return hDevice->QueueGroup[queue_group_type::MainCopy].ZeOrdinal;
  }
  return hDevice->QueueGroup[queue_group_type::Compute].ZeOrdinal;
}

static std::optional<int32_t> getZeIndex(const ur_queue_properties_t *pProps) {
  if (pProps && pProps->pNext) {
    const ur_base_properties_t *extendedDesc =
        reinterpret_cast<const ur_base_properties_t *>(pProps->pNext);
    if (extendedDesc->stype == UR_STRUCTURE_TYPE_QUEUE_INDEX_PROPERTIES) {
      const ur_queue_index_properties_t *indexProperties =
          reinterpret_cast<const ur_queue_index_properties_t *>(extendedDesc);
      return indexProperties->computeIndex;
    }
  }
  return std::nullopt;
}

static ze_command_queue_priority_t getZePriority(ur_queue_flags_t flags) {
  if ((flags & UR_QUEUE_FLAG_PRIORITY_LOW) != 0)
    return ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;
  if ((flags & UR_QUEUE_FLAG_PRIORITY_HIGH) != 0)
    return ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH;
  return ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
}

ur_command_list_handler_t::ur_command_list_handler_t(
    v2::ur_context_handle_t hContext, ur_device_handle_t hDevice,
    const ur_queue_properties_t *pProps, queue_group_type type,
    event_pool *eventPool)
    : commandList(hContext->commandListCache.getImmediateCommandList(
          hDevice->ZeDevice, true, getZeOrdinal(hDevice, type),
          ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
          getZePriority(pProps ? pProps->flags : ur_queue_flags_t{}),
          getZeIndex(pProps))),
      lastEvent(std::move(eventPool->getProvider()->allocate().borrow)) {}

ur_queue_immediate_in_order_t::ur_queue_immediate_in_order_t(
    v2::ur_context_handle_t hContext, ur_device_handle_t hDevice,
    const ur_queue_properties_t *pProps)
    : hContext(hContext), hDevice(hDevice),
      eventPool(hContext->eventPoolCache.borrow(hDevice->Id)),
      copyHandler(hContext, hDevice, pProps, queue_group_type::MainCopy,
                  eventPool.get()),
      computeHandler(hContext, hDevice, pProps, queue_group_type::Compute,
                     eventPool.get()),
      kernelEnqueueLatency(
          "ur_queue_immediate_in_order_t::enqueueKernelLaunch") {}

ur_command_list_handler_t *ur_queue_immediate_in_order_t::getCommandListHandler(
    CommandListPreference preference) {
  if (preference == CommandListPreference::Copy) {
    return &copyHandler;
  } else {
    return &computeHandler;
  }
}

ze_event_handle_t ur_queue_immediate_in_order_t::getSignalEvent(
    ur_command_list_handler_t *handler, ur_event_handle_t *hUserEvent) {
  if (!hUserEvent) {
    return handler->lastEvent.get();
  }

  *hUserEvent = eventPool->allocate();
  return (*hUserEvent)->getZeEvent();
}

ur_wait_list_t ur_queue_immediate_in_order_t::getWaitList(
    ur_command_list_handler_t *handler, uint32_t numWaitEvents,
    const ur_event_handle_t *phWaitEvents) {
  auto extraWaitEvent = (lastHandler && handler != lastHandler)
                            ? lastHandler->lastEvent.get()
                            : nullptr;
  return ur_wait_list_t(phWaitEvents, numWaitEvents, extraWaitEvent);
}

ur_result_t
ur_queue_immediate_in_order_t::queueGetInfo(ur_queue_info_t propName,
                                            size_t propSize, void *pPropValue,
                                            size_t *pPropSizeRet) {
  std::ignore = propName;
  std::ignore = propSize;
  std::ignore = pPropValue;
  std::ignore = pPropSizeRet;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::queueRetain() {
  RefCount.increment();
  return UR_RESULT_SUCCESS;
}

ur_result_t ur_queue_immediate_in_order_t::queueRelease() {
  if (!RefCount.decrementAndTest())
    return UR_RESULT_SUCCESS;

  delete this;
  return UR_RESULT_SUCCESS;
}

ur_result_t ur_queue_immediate_in_order_t::queueGetNativeHandle(
    ur_queue_native_desc_t *pDesc, ur_native_handle_t *phNativeQueue) {
  std::ignore = pDesc;
  std::ignore = phNativeQueue;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::queueFinish() {
  std::unique_lock<ur_shared_mutex> lock(this->Mutex);

  if (!lastHandler) {
    return UR_RESULT_SUCCESS;
  }

  auto lastCmdList = lastHandler->commandList.get();
  lock.unlock();

  ZE2UR_CALL(zeCommandListHostSynchronize, (lastCmdList, UINT64_MAX));

  // TODO: set lastHandler to NULL?

  return UR_RESULT_SUCCESS;
}

ur_result_t ur_queue_immediate_in_order_t::queueFlush() {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueKernelLaunch(
    ur_kernel_handle_t hKernel, uint32_t workDim,
    const size_t *pGlobalWorkOffset, const size_t *pGlobalWorkSize,
    const size_t *pLocalWorkSize, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  rolling_latency_tracker tracker(kernelEnqueueLatency);

  UR_ASSERT(hKernel->Program, UR_RESULT_ERROR_INVALID_NULL_POINTER);

  // Lock automatically releases when this goes out of scope.
  std::scoped_lock<ur_shared_mutex, ur_shared_mutex, ur_shared_mutex> Lock(
      hKernel->Mutex, hKernel->Program->Mutex, this->Mutex);

  if (pGlobalWorkOffset != NULL) {
    UR_CALL(setKernelGlobalOffset(hContext, hKernel, pGlobalWorkOffset));
  }

  // TODO: remove this
  // If there are any pending arguments set them now.
  // if (!Kernel->PendingArguments.empty()) {
  //   UR_CALL(setKernelPendingArguments(CommandBuffer, Kernel));
  // }

  ze_group_count_t zeThreadGroupDimensions{1, 1, 1};
  uint32_t WG[3];
  UR_CALL(calculateKernelWorkDimensions(hKernel, hDevice,
                                        zeThreadGroupDimensions, WG, workDim,
                                        pGlobalWorkSize, pLocalWorkSize));

  ZE2UR_CALL(zeKernelSetGroupSize, (hKernel->ZeKernel, WG[0], WG[1], WG[2]));

  auto v2WaitList =
      reinterpret_cast<const ur_event_handle_t *>(phEventWaitList);
  auto v2SignalEvent = reinterpret_cast<ur_event_handle_t *>(phEvent);

  auto handler = getCommandListHandler(CommandListPreference::Compute);
  auto signalEvent = getSignalEvent(handler, v2SignalEvent);

  auto waitList = getWaitList(handler, numEventsInWaitList, v2WaitList);
  auto [pWaitEvents, numWaitEvents] = waitList.getView();

  ze_kernel_handle_t zeKernel;
  UR_CALL(getZeKernel(hDevice->ZeDevice, hKernel, &zeKernel));

  ZE2UR_CALL(zeCommandListAppendLaunchKernel,
             (handler->commandList.get(), zeKernel, &zeThreadGroupDimensions,
              signalEvent, numWaitEvents, pWaitEvents));

  lastHandler = handler;

  return UR_RESULT_SUCCESS;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueEventsWait(
    uint32_t numEventsInWaitList, const ::ur_event_handle_t *phEventWaitList,
    ::ur_event_handle_t *phEvent) {
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueEventsWaitWithBarrier(
    uint32_t numEventsInWaitList, const ::ur_event_handle_t *phEventWaitList,
    ::ur_event_handle_t *phEvent) {
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueMemBufferRead(
    ur_mem_handle_t hBuffer, bool blockingRead, size_t offset, size_t size,
    void *pDst, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hBuffer;
  std::ignore = blockingRead;
  std::ignore = offset;
  std::ignore = size;
  std::ignore = pDst;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueMemBufferWrite(
    ur_mem_handle_t hBuffer, bool blockingWrite, size_t offset, size_t size,
    const void *pSrc, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hBuffer;
  std::ignore = blockingWrite;
  std::ignore = offset;
  std::ignore = size;
  std::ignore = pSrc;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueMemBufferReadRect(
    ur_mem_handle_t hBuffer, bool blockingRead, ur_rect_offset_t bufferOrigin,
    ur_rect_offset_t hostOrigin, ur_rect_region_t region, size_t bufferRowPitch,
    size_t bufferSlicePitch, size_t hostRowPitch, size_t hostSlicePitch,
    void *pDst, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hBuffer;
  std::ignore = blockingRead;
  std::ignore = bufferOrigin;
  std::ignore = hostOrigin;
  std::ignore = region;
  std::ignore = bufferRowPitch;
  std::ignore = bufferSlicePitch;
  std::ignore = hostRowPitch;
  std::ignore = hostSlicePitch;
  std::ignore = pDst;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueMemBufferWriteRect(
    ur_mem_handle_t hBuffer, bool blockingWrite, ur_rect_offset_t bufferOrigin,
    ur_rect_offset_t hostOrigin, ur_rect_region_t region, size_t bufferRowPitch,
    size_t bufferSlicePitch, size_t hostRowPitch, size_t hostSlicePitch,
    void *pSrc, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hBuffer;
  std::ignore = blockingWrite;
  std::ignore = bufferOrigin;
  std::ignore = hostOrigin;
  std::ignore = region;
  std::ignore = bufferRowPitch;
  std::ignore = bufferSlicePitch;
  std::ignore = hostRowPitch;
  std::ignore = hostSlicePitch;
  std::ignore = pSrc;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueMemBufferCopy(
    ur_mem_handle_t hBufferSrc, ur_mem_handle_t hBufferDst, size_t srcOffset,
    size_t dstOffset, size_t size, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hBufferSrc;
  std::ignore = hBufferDst;
  std::ignore = srcOffset;
  std::ignore = dstOffset;
  std::ignore = size;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueMemBufferCopyRect(
    ur_mem_handle_t hBufferSrc, ur_mem_handle_t hBufferDst,
    ur_rect_offset_t srcOrigin, ur_rect_offset_t dstOrigin,
    ur_rect_region_t region, size_t srcRowPitch, size_t srcSlicePitch,
    size_t dstRowPitch, size_t dstSlicePitch, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hBufferSrc;
  std::ignore = hBufferDst;
  std::ignore = srcOrigin;
  std::ignore = dstOrigin;
  std::ignore = region;
  std::ignore = srcRowPitch;
  std::ignore = srcSlicePitch;
  std::ignore = dstRowPitch;
  std::ignore = dstSlicePitch;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueMemBufferFill(
    ur_mem_handle_t hBuffer, const void *pPattern, size_t patternSize,
    size_t offset, size_t size, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hBuffer;
  std::ignore = pPattern;
  std::ignore = patternSize;
  std::ignore = offset;
  std::ignore = size;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueMemImageRead(
    ur_mem_handle_t hImage, bool blockingRead, ur_rect_offset_t origin,
    ur_rect_region_t region, size_t rowPitch, size_t slicePitch, void *pDst,
    uint32_t numEventsInWaitList, const ::ur_event_handle_t *phEventWaitList,
    ::ur_event_handle_t *phEvent) {
  std::ignore = hImage;
  std::ignore = blockingRead;
  std::ignore = origin;
  std::ignore = region;
  std::ignore = rowPitch;
  std::ignore = slicePitch;
  std::ignore = pDst;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueMemImageWrite(
    ur_mem_handle_t hImage, bool blockingWrite, ur_rect_offset_t origin,
    ur_rect_region_t region, size_t rowPitch, size_t slicePitch, void *pSrc,
    uint32_t numEventsInWaitList, const ::ur_event_handle_t *phEventWaitList,
    ::ur_event_handle_t *phEvent) {
  std::ignore = hImage;
  std::ignore = blockingWrite;
  std::ignore = origin;
  std::ignore = region;
  std::ignore = rowPitch;
  std::ignore = slicePitch;
  std::ignore = pSrc;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueMemImageCopy(
    ur_mem_handle_t hImageSrc, ur_mem_handle_t hImageDst,
    ur_rect_offset_t srcOrigin, ur_rect_offset_t dstOrigin,
    ur_rect_region_t region, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hImageSrc;
  std::ignore = hImageDst;
  std::ignore = srcOrigin;
  std::ignore = dstOrigin;
  std::ignore = region;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueMemBufferMap(
    ur_mem_handle_t hBuffer, bool blockingMap, ur_map_flags_t mapFlags,
    size_t offset, size_t size, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent,
    void **ppRetMap) {
  std::ignore = hBuffer;
  std::ignore = blockingMap;
  std::ignore = mapFlags;
  std::ignore = offset;
  std::ignore = size;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  std::ignore = ppRetMap;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueMemUnmap(
    ur_mem_handle_t hMem, void *pMappedPtr, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hMem;
  std::ignore = pMappedPtr;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueUSMFill(
    void *pMem, size_t patternSize, const void *pPattern, size_t size,
    uint32_t numEventsInWaitList, const ::ur_event_handle_t *phEventWaitList,
    ::ur_event_handle_t *phEvent) {
  std::ignore = pMem;
  std::ignore = patternSize;
  std::ignore = pPattern;
  std::ignore = size;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueUSMMemcpy(
    bool blocking, void *pDst, const void *pSrc, size_t size,
    uint32_t numEventsInWaitList, const ::ur_event_handle_t *phEventWaitList,
    ::ur_event_handle_t *phEvent) {
  std::ignore = blocking;
  std::ignore = pDst;
  std::ignore = pSrc;
  std::ignore = size;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueUSMPrefetch(
    const void *pMem, size_t size, ur_usm_migration_flags_t flags,
    uint32_t numEventsInWaitList, const ::ur_event_handle_t *phEventWaitList,
    ::ur_event_handle_t *phEvent) {
  std::ignore = pMem;
  std::ignore = size;
  std::ignore = flags;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t
ur_queue_immediate_in_order_t::enqueueUSMAdvise(const void *pMem, size_t size,
                                                ur_usm_advice_flags_t advice,
                                                ::ur_event_handle_t *phEvent) {
  std::ignore = pMem;
  std::ignore = size;
  std::ignore = advice;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueUSMFill2D(
    void *pMem, size_t pitch, size_t patternSize, const void *pPattern,
    size_t width, size_t height, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = pMem;
  std::ignore = pitch;
  std::ignore = patternSize;
  std::ignore = pPattern;
  std::ignore = width;
  std::ignore = height;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueUSMMemcpy2D(
    bool blocking, void *pDst, size_t dstPitch, const void *pSrc,
    size_t srcPitch, size_t width, size_t height, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = blocking;
  std::ignore = pDst;
  std::ignore = dstPitch;
  std::ignore = pSrc;
  std::ignore = srcPitch;
  std::ignore = width;
  std::ignore = height;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueDeviceGlobalVariableWrite(
    ur_program_handle_t hProgram, const char *name, bool blockingWrite,
    size_t count, size_t offset, const void *pSrc, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hProgram;
  std::ignore = name;
  std::ignore = blockingWrite;
  std::ignore = count;
  std::ignore = offset;
  std::ignore = pSrc;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueDeviceGlobalVariableRead(
    ur_program_handle_t hProgram, const char *name, bool blockingRead,
    size_t count, size_t offset, void *pDst, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hProgram;
  std::ignore = name;
  std::ignore = blockingRead;
  std::ignore = count;
  std::ignore = offset;
  std::ignore = pDst;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueReadHostPipe(
    ur_program_handle_t hProgram, const char *pipe_symbol, bool blocking,
    void *pDst, size_t size, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hProgram;
  std::ignore = pipe_symbol;
  std::ignore = blocking;
  std::ignore = pDst;
  std::ignore = size;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueWriteHostPipe(
    ur_program_handle_t hProgram, const char *pipe_symbol, bool blocking,
    void *pSrc, size_t size, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hProgram;
  std::ignore = pipe_symbol;
  std::ignore = blocking;
  std::ignore = pSrc;
  std::ignore = size;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::bindlessImagesImageCopyExp(
    const void *pSrc, void *pDst, const ur_image_desc_t *pSrcImageDesc,
    const ur_image_desc_t *pDstImageDesc,
    const ur_image_format_t *pSrcImageFormat,
    const ur_image_format_t *pDstImageFormat,
    ur_exp_image_copy_region_t *pCopyRegion,
    ur_exp_image_copy_flags_t imageCopyFlags, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = pDst;
  std::ignore = pSrc;
  std::ignore = pSrcImageDesc;
  std::ignore = pDstImageDesc;
  std::ignore = imageCopyFlags;
  std::ignore = pSrcImageFormat;
  std::ignore = pDstImageFormat;
  std::ignore = pCopyRegion;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t
ur_queue_immediate_in_order_t::bindlessImagesWaitExternalSemaphoreExp(
    ur_exp_interop_semaphore_handle_t hSemaphore, bool hasWaitValue,
    uint64_t waitValue, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hSemaphore;
  std::ignore = hasWaitValue;
  std::ignore = waitValue;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t
ur_queue_immediate_in_order_t::bindlessImagesSignalExternalSemaphoreExp(
    ur_exp_interop_semaphore_handle_t hSemaphore, bool hasSignalValue,
    uint64_t signalValue, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hSemaphore;
  std::ignore = hasSignalValue;
  std::ignore = signalValue;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueCooperativeKernelLaunchExp(
    ur_kernel_handle_t hKernel, uint32_t workDim,
    const size_t *pGlobalWorkOffset, const size_t *pGlobalWorkSize,
    const size_t *pLocalWorkSize, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = hKernel;
  std::ignore = workDim;
  std::ignore = pGlobalWorkOffset;
  std::ignore = pGlobalWorkSize;
  std::ignore = pLocalWorkSize;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueTimestampRecordingExp(
    bool blocking, uint32_t numEventsInWaitList,
    const ::ur_event_handle_t *phEventWaitList, ::ur_event_handle_t *phEvent) {
  std::ignore = blocking;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueKernelLaunchCustomExp(
    ur_kernel_handle_t hKernel, uint32_t workDim, const size_t *pGlobalWorkSize,
    const size_t *pLocalWorkSize, uint32_t numPropsInLaunchPropList,
    const ur_exp_launch_property_t *launchPropList,
    uint32_t numEventsInWaitList, const ::ur_event_handle_t *phEventWaitList,
    ::ur_event_handle_t *phEvent) {
  std::ignore = hKernel;
  std::ignore = workDim;
  std::ignore = pGlobalWorkSize;
  std::ignore = pLocalWorkSize;
  std::ignore = numPropsInLaunchPropList;
  std::ignore = launchPropList;
  std::ignore = numEventsInWaitList;
  std::ignore = phEventWaitList;
  std::ignore = phEvent;
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t ur_queue_immediate_in_order_t::enqueueNativeCommandExp(
    ur_exp_enqueue_native_command_function_t, void *, uint32_t,
    const ur_mem_handle_t *, const ur_exp_enqueue_native_command_properties_t *,
    uint32_t, const ::ur_event_handle_t *, ::ur_event_handle_t *) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
} // namespace v2
