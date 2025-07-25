//===----------- event.cpp - LLVM Offload Adapter  ------------------------===//
//
// Copyright (C) 2025 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <OffloadAPI.h>
#include <ur_api.h>

#include "event.hpp"
#include "queue.hpp"
#include "ur2offload.hpp"

UR_APIEXPORT ur_result_t UR_APICALL urEventGetInfo(ur_event_handle_t hEvent,
                                                   ur_event_info_t propName,
                                                   size_t propSize,
                                                   void *pPropValue,
                                                   size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);

  switch (propName) {
  case UR_EVENT_INFO_CONTEXT:
    return ReturnValue(hEvent->UrQueue->UrContext);
  case UR_EVENT_INFO_COMMAND_QUEUE:
    return ReturnValue(hEvent->UrQueue);
  case UR_EVENT_INFO_COMMAND_TYPE:
    return ReturnValue(hEvent->Type);
  case UR_EVENT_INFO_REFERENCE_COUNT:
    return ReturnValue(hEvent->RefCount.load());
  default:
    return UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventGetProfilingInfo(ur_event_handle_t,
                                                            ur_profiling_info_t,
                                                            size_t, void *,
                                                            size_t *) {
  // All variants are optional
  return UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
}

UR_APIEXPORT ur_result_t UR_APICALL
urEventWait(uint32_t numEvents, const ur_event_handle_t *phEventWaitList) {
  for (uint32_t i = 0; i < numEvents; i++) {
    if (phEventWaitList[i]->OffloadEvent) {
      OL_RETURN_ON_ERR(olSyncEvent(phEventWaitList[i]->OffloadEvent));
    }
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventRetain(ur_event_handle_t hEvent) {
  hEvent->RefCount++;

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventRelease(ur_event_handle_t hEvent) {
  if (--hEvent->RefCount == 0) {
    auto Res = olDestroyEvent(hEvent->OffloadEvent);
    if (Res) {
      return offloadResultToUR(Res);
    }
    delete hEvent;
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urEventGetNativeHandle(ur_event_handle_t, ur_native_handle_t *) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventCreateWithNativeHandle(
    ur_native_handle_t, ur_context_handle_t,
    const ur_event_native_properties_t *, ur_event_handle_t *) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
