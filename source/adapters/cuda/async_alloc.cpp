//===--------- async_alloc.cpp - CUDA Adapter -----------------------------===//
//
// Copyright (C) 2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <ur_api.h>

#include "context.hpp"
#include "enqueue.hpp"
#include "event.hpp"
#include "queue.hpp"
#include "usm.hpp"

UR_APIEXPORT ur_result_t urEnqueueUSMDeviceAllocExp(
    ur_queue_handle_t hQueue, ur_usm_pool_handle_t pPool, const size_t size,
    const ur_exp_async_usm_alloc_properties_t *, uint32_t numEventsInWaitList,
    const ur_event_handle_t *phEventWaitList, void **ppMem,
    ur_event_handle_t *phEvent) {
  try {
    std::unique_ptr<ur_event_handle_t_> RetImplEvent{nullptr};

    ScopedContext Active(hQueue->getDevice());
    uint32_t StreamToken;
    ur_stream_guard_ Guard;
    CUstream CuStream = hQueue->getNextComputeStream(
        numEventsInWaitList, phEventWaitList, Guard, &StreamToken);

    UR_CHECK_ERROR(enqueueEventsWait(hQueue, CuStream, numEventsInWaitList,
                                     phEventWaitList));

    if (phEvent) {
      RetImplEvent =
          std::unique_ptr<ur_event_handle_t_>(ur_event_handle_t_::makeNative(
              UR_COMMAND_KERNEL_LAUNCH, hQueue, CuStream, StreamToken));
      UR_CHECK_ERROR(RetImplEvent->start());
    }

    if (pPool) {
      assert(pPool->usesCudaPool());

    } else {
      UR_CHECK_ERROR(cuMemAllocAsync(reinterpret_cast<CUdeviceptr *>(ppMem),
                                     size, CuStream));
    }

    if (phEvent) {
      UR_CHECK_ERROR(RetImplEvent->record());
      *phEvent = RetImplEvent.release();
    }

  } catch (ur_result_t Err) {
    return Err;
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t urEnqueueUSMSharedAllocExp(
    ur_queue_handle_t, ur_usm_pool_handle_t, const size_t,
    const ur_exp_async_usm_alloc_properties_t *, uint32_t,
    const ur_event_handle_t *, void **, ur_event_handle_t *) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t urEnqueueUSMHostAllocExp(
    ur_queue_handle_t, ur_usm_pool_handle_t, const size_t,
    const ur_exp_async_usm_alloc_properties_t *, uint32_t,
    const ur_event_handle_t *, void **, ur_event_handle_t *) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t urEnqueueUSMFreeExp(ur_queue_handle_t,
                                             ur_usm_pool_handle_t, void *,
                                             uint32_t,
                                             const ur_event_handle_t *,
                                             ur_event_handle_t *) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
/*

UR_APIEXPORT ur_result_t urEnqueueUSMSharedAllocExp(
    ur_queue_handle_t hQueue, ur_usm_pool_handle_t pPool, const size_t size,
    const ur_exp_async_usm_alloc_properties_t *pProperties,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    void **ppMem, ur_event_handle_t *phEvent) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t urEnqueueUSMHostAllocExp(
    ur_queue_handle_t hQueue, ur_usm_pool_handle_t pPool, const size_t size,
    const ur_exp_async_usm_alloc_properties_t *pProperties,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    void **ppMem, ur_event_handle_t *phEvent) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t urEnqueueUSMFreeExp(
    ur_queue_handle_t hQueue, ur_usm_pool_handle_t pPool, void *pMem,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
*/
