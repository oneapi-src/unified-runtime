//===--------- event.cpp - Libomptarget Adapter ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

#include "event.hpp"
#include "common.hpp"
#include <omptargetplugin.h>

UR_APIEXPORT ur_result_t UR_APICALL urEventCreateWithNativeHandle(
    [[maybe_unused]] ur_native_handle_t hNativeEvent,
    [[maybe_unused]] ur_context_handle_t hContext,
    [[maybe_unused]] const ur_event_native_properties_t *pProperties,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL
urEventGetNativeHandle([[maybe_unused]] ur_event_handle_t hEvent,
                       [[maybe_unused]] ur_native_handle_t *phNativeEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventRelease(ur_event_handle_t hEvent) {
  if (hEvent->decrementReferenceCount() == 0) {
    int32_t DeviceID = hEvent->getQueue()->getDevice()->getID();
    __tgt_rtl_destroy_event(DeviceID, hEvent->Event);
    delete hEvent;
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEventRetain(ur_event_handle_t hEvent) {
  hEvent->incrementReferenceCount();
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urEventWait(uint32_t numEvents, const ur_event_handle_t *phEventWaitList) {
  return waitForEvents(numEvents, phEventWaitList);
}

UR_APIEXPORT ur_result_t UR_APICALL urEventGetInfo(ur_event_handle_t hEvent,
                                                   ur_event_info_t propName,
                                                   size_t propValueSize,
                                                   void *pPropValue,
                                                   size_t *pPropValueSizeRet) {
  UrReturnHelper ReturnValue(propValueSize, pPropValue, pPropValueSizeRet);

  switch (propName) {
  case UR_EVENT_INFO_COMMAND_QUEUE:
    return ReturnValue(hEvent->getQueue());
  case UR_EVENT_INFO_COMMAND_TYPE:
    return ReturnValue(hEvent->getCommandType());
  case UR_EVENT_INFO_REFERENCE_COUNT:
    return ReturnValue(hEvent->getReferenceCount());
  case UR_EVENT_INFO_COMMAND_EXECUTION_STATUS:
    return ReturnValue(hEvent->getExecutionStatus());
  case UR_EVENT_INFO_CONTEXT:
    return ReturnValue(hEvent->getContext());
  default:
    return UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
  }

  return UR_RESULT_ERROR_INVALID_ENUMERATION;
}

// NOTE: Not implemented for now. May or may not be possible.
UR_APIEXPORT ur_result_t UR_APICALL urEventGetProfilingInfo(
    [[maybe_unused]] ur_event_handle_t hEvent,
    [[maybe_unused]] ur_profiling_info_t propName,
    [[maybe_unused]] size_t propSize, [[maybe_unused]] void *pPropValue,
    [[maybe_unused]] size_t *pPropSizeRet) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

// NOTE: This isn't implemented in the CUDA adapter
UR_APIEXPORT ur_result_t UR_APICALL
urEventSetCallback([[maybe_unused]] ur_event_handle_t hEvent,
                   [[maybe_unused]] ur_execution_info_t execStatus,
                   [[maybe_unused]] ur_event_callback_t pfnNotify,
                   [[maybe_unused]] void *pUserData) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t waitForEvents(uint32_t NumEvents, const ur_event_handle_t *Events) {
  for (uint32_t EventNum = 0; EventNum < NumEvents; EventNum++) {
    auto *Event = Events[EventNum];
    if (!Event->isRecorded()) {
      return UR_RESULT_ERROR_INVALID_EVENT;
    }

    if (!Event->isComplete()) {
      // NOTE: The implementation of `isComplete` means we will never reach
      // this code, but putting this here in case the implementation changes in
      // the future.
      int32_t DeviceID = Event->getQueue()->getDevice()->getID();
      __tgt_rtl_sync_event(DeviceID, Event->Event);
      Event->HasBeenWaitedOn = true;
    }
  }

  return UR_RESULT_SUCCESS;
}
