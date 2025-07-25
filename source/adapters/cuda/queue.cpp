//===--------- queue.cpp - CUDA Adapter -----------------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "queue.hpp"
#include "common.hpp"
#include "context.hpp"
#include "event.hpp"

#include <cassert>
#include <cuda.h>

template <>
void cuda_stream_queue::computeStreamWaitForBarrierIfNeeded(CUstream Stream,
                                                            uint32_t StreamI) {
  if (BarrierEvent && !ComputeAppliedBarrier[StreamI]) {
    UR_CHECK_ERROR(cuStreamWaitEvent(Stream, BarrierEvent, 0));
    ComputeAppliedBarrier[StreamI] = true;
  }
}

template <>
void cuda_stream_queue::transferStreamWaitForBarrierIfNeeded(CUstream Stream,
                                                             uint32_t StreamI) {
  if (BarrierEvent && !TransferAppliedBarrier[StreamI]) {
    UR_CHECK_ERROR(cuStreamWaitEvent(Stream, BarrierEvent, 0));
    TransferAppliedBarrier[StreamI] = true;
  }
}

template <>
ur_queue_handle_t cuda_stream_queue::getEventQueue(const ur_event_handle_t e) {
  return e->getQueue();
}

template <>
uint32_t
cuda_stream_queue::getEventComputeStreamToken(const ur_event_handle_t e) {
  return e->getComputeStreamToken();
}

template <>
CUstream cuda_stream_queue::getEventStream(const ur_event_handle_t e) {
  return e->getStream();
}

/// Creates a `ur_queue_handle_t` object on the CUDA backend.
/// Valid properties
/// * __SYCL_PI_CUDA_USE_DEFAULT_STREAM -> CU_STREAM_DEFAULT
/// * __SYCL_PI_CUDA_SYNC_WITH_DEFAULT -> CU_STREAM_NON_BLOCKING
UR_APIEXPORT ur_result_t UR_APICALL
urQueueCreate(ur_context_handle_t hContext, ur_device_handle_t hDevice,
              const ur_queue_properties_t *pProps, ur_queue_handle_t *phQueue) {
  try {
    std::unique_ptr<ur_queue_handle_t_> Queue{nullptr};

    if (std::find(hContext->getDevices().begin(), hContext->getDevices().end(),
                  hDevice) == hContext->getDevices().end()) {
      *phQueue = nullptr;
      return UR_RESULT_ERROR_INVALID_DEVICE;
    }

    ScopedContext Active(hDevice);
    unsigned int Flags = CU_STREAM_NON_BLOCKING;
    ur_queue_flags_t URFlags = 0;
    // '0' is the default priority, per CUDA Toolkit 12.2 and earlier
    int Priority = 0;
    bool IsOutOfOrder = false;
    if (pProps && pProps->stype == UR_STRUCTURE_TYPE_QUEUE_PROPERTIES) {
      URFlags = pProps->flags;
      if (URFlags == UR_QUEUE_FLAG_USE_DEFAULT_STREAM) {
        Flags = CU_STREAM_DEFAULT;
      } else if (URFlags == UR_QUEUE_FLAG_SYNC_WITH_DEFAULT_STREAM) {
        Flags = 0;
      }

      if (URFlags & UR_QUEUE_FLAG_OUT_OF_ORDER_EXEC_MODE_ENABLE) {
        IsOutOfOrder = true;
      }
      if (URFlags & UR_QUEUE_FLAG_PRIORITY_HIGH) {
        UR_CHECK_ERROR(cuCtxGetStreamPriorityRange(nullptr, &Priority));
      } else if (URFlags & UR_QUEUE_FLAG_PRIORITY_LOW) {
        ScopedContext Active(hDevice);
        UR_CHECK_ERROR(cuCtxGetStreamPriorityRange(&Priority, nullptr));
      }
    }

    Queue = std::unique_ptr<ur_queue_handle_t_>(new ur_queue_handle_t_{
        {}, {IsOutOfOrder, hContext, hDevice, Flags, URFlags, Priority}});

    *phQueue = Queue.release();

    return UR_RESULT_SUCCESS;
  } catch (ur_result_t Err) {
    return Err;
  } catch (std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueRetain(ur_queue_handle_t hQueue) {
  assert(hQueue->RefCount.getCount() > 0);

  hQueue->RefCount.retain();
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueRelease(ur_queue_handle_t hQueue) {
  if (!hQueue->RefCount.release()) {
    return UR_RESULT_SUCCESS;
  }

  try {
    std::unique_ptr<ur_queue_handle_t_> Queue(hQueue);

    if (!hQueue->backendHasOwnership())
      return UR_RESULT_SUCCESS;

    ScopedContext Active(hQueue->getDevice());

    hQueue->forEachStream([](CUstream S) {
      UR_CHECK_ERROR(cuStreamSynchronize(S));
      UR_CHECK_ERROR(cuStreamDestroy(S));
    });

    if (hQueue->getHostSubmitTimeStream() != CUstream{0}) {
      UR_CHECK_ERROR(cuStreamSynchronize(hQueue->getHostSubmitTimeStream()));
      UR_CHECK_ERROR(cuStreamDestroy(hQueue->getHostSubmitTimeStream()));
    }

    return UR_RESULT_SUCCESS;
  } catch (ur_result_t Err) {
    return Err;
  } catch (...) {
    return UR_RESULT_ERROR_OUT_OF_RESOURCES;
  }
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueFinish(ur_queue_handle_t hQueue) {
  ur_result_t Result = UR_RESULT_SUCCESS;

  try {
    ScopedContext active(hQueue->getDevice());

    hQueue->syncStreams</*ResetUsed=*/true>(
        [](CUstream s) { UR_CHECK_ERROR(cuStreamSynchronize(s)); });

  } catch (ur_result_t Err) {

    Result = Err;

  } catch (...) {

    Result = UR_RESULT_ERROR_OUT_OF_RESOURCES;
  }

  return Result;
}

// There is no CUDA counterpart for queue flushing and we don't run into the
// same problem of having to flush cross-queue dependencies as some of the
// other plugins, so it can be left as no-op.
UR_APIEXPORT ur_result_t UR_APICALL urQueueFlush(ur_queue_handle_t /*hQueue*/) {
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueGetNativeHandle(
    ur_queue_handle_t hQueue, ur_queue_native_desc_t * /*pDesc*/,
    ur_native_handle_t *phNativeQueue) {

  ScopedContext Active(hQueue->getDevice());
  *phNativeQueue =
      reinterpret_cast<ur_native_handle_t>(hQueue->getInteropStream());
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueCreateWithNativeHandle(
    ur_native_handle_t hNativeQueue, ur_context_handle_t hContext,
    ur_device_handle_t hDevice, const ur_queue_native_properties_t *pProperties,
    ur_queue_handle_t *phQueue) {
  if (!hDevice && hContext->getDevices().size() == 1)
    hDevice = hContext->getDevices().front();

  unsigned int CuFlags;
  CUstream CuStream = reinterpret_cast<CUstream>(hNativeQueue);

  UR_CHECK_ERROR(cuStreamGetFlags(CuStream, &CuFlags));

  ur_queue_flags_t Flags = 0;
  if (CuFlags == CU_STREAM_DEFAULT) {
    Flags = UR_QUEUE_FLAG_USE_DEFAULT_STREAM;
  } else if (CuFlags == CU_STREAM_NON_BLOCKING) {
    Flags = UR_QUEUE_FLAG_SYNC_WITH_DEFAULT_STREAM;
  } else {
    setErrorMessage("Incorrect native stream flags, expecting "
                    "CU_STREAM_DEFAULT or CU_STREAM_NON_BLOCKING",
                    UR_RESULT_ERROR_INVALID_VALUE);
    return UR_RESULT_ERROR_ADAPTER_SPECIFIC;
  }

  auto isNativeHandleOwned =
      pProperties ? pProperties->isNativeHandleOwned : false;

  // Create queue from a native stream
  *phQueue = new ur_queue_handle_t_{
      {}, {CuStream, hContext, hDevice, CuFlags, Flags, isNativeHandleOwned}};

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueGetInfo(ur_queue_handle_t hQueue,
                                                   ur_queue_info_t propName,
                                                   size_t propValueSize,
                                                   void *pPropValue,
                                                   size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propValueSize, pPropValue, pPropSizeRet);

  switch (propName) {
  case UR_QUEUE_INFO_CONTEXT:
    return ReturnValue(hQueue->Context);
  case UR_QUEUE_INFO_DEVICE:
    return ReturnValue(hQueue->Device);
  case UR_QUEUE_INFO_REFERENCE_COUNT:
    return ReturnValue(hQueue->RefCount.getCount());
  case UR_QUEUE_INFO_FLAGS:
    return ReturnValue(hQueue->URFlags);
  case UR_QUEUE_INFO_EMPTY: {
    try {
      bool IsReady = hQueue->allOf([](CUstream S) -> bool {
        const CUresult Ret = cuStreamQuery(S);
        if (Ret == CUDA_SUCCESS)
          return true;

        if (Ret == CUDA_ERROR_NOT_READY)
          return false;

        UR_CHECK_ERROR(Ret);
        return false;
      });
      return ReturnValue(IsReady);
    } catch (ur_result_t Err) {
      return Err;
    } catch (...) {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    }
  }
  case UR_QUEUE_INFO_DEVICE_DEFAULT:
  case UR_QUEUE_INFO_SIZE:
    return UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
  default:
    return UR_RESULT_ERROR_INVALID_ENUMERATION;
  }
}
