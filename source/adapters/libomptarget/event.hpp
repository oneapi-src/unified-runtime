//===--------- event.hpp - Libomptarget Adapter ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//
#pragma once

#include "device.hpp"
#include "queue.hpp"
#include <atomic>
#include <ur_api.h>

struct ur_event_handle_t_ {
  using NativeType = void *;

  NativeType Event;
  ur_queue_handle_t Queue;
  ur_context_handle_t Context;
  bool IsRecorded;
  bool IsStarted;
  bool HasBeenWaitedOn;
  ur_command_t Type;
  std::atomic_uint32_t RefCount;

  static ur_event_handle_t makeEvent(ur_queue_handle_t Queue,
                                     ur_context_handle_t Context,
                                     ur_command_t Type) {
    NativeType NativeEvent;
    int32_t DeviceID = Queue->getDevice()->getID();
    if (__tgt_rtl_create_event(DeviceID, &NativeEvent) != OFFLOAD_SUCCESS) {
      return nullptr;
    }
    return new ur_event_handle_t_(NativeEvent, Queue, Context, Type);
  }

  ur_event_handle_t_(NativeType Event, ur_queue_handle_t Queue,
                     ur_context_handle_t Context, ur_command_t Type)
      : Event(Event), Queue(Queue), Context(Context), IsRecorded(false),
        IsStarted(false), HasBeenWaitedOn(false), Type(Type), RefCount(1) {
    urQueueRetain(Queue);
  }

  ur_result_t start() {
    IsStarted = true;
    return UR_RESULT_SUCCESS;
  }

  ur_result_t record() {
    int32_t DeviceID = Queue->getDevice()->getID();
    auto Res = __tgt_rtl_record_event(DeviceID, Event, Queue->AsyncInfo);
    if (Res != OFFLOAD_SUCCESS) {
      // NOTE: We don't know what the specififc problem is, just pick an error
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    }
    IsRecorded = true;
    return UR_RESULT_SUCCESS;
  }

  // MISSING FUNCTIONALITY: There is no way to query the status of an event, so
  // we have no choice but to wait on an event if hasn't been already. This is
  // likely to cause weird performance.
  // The alternative to this would be returning whether or not an event has
  // been explicitly waited on. This would mean incorrectly returning false if
  // an event has completed asynchronously without a wait and would probably
  // cause incorrect behavior, so it is probably the worst option.
  bool isComplete() {
    if (!IsRecorded) {
      return false;
    }

    if (!HasBeenWaitedOn) {
      int32_t DeviceID = Queue->getDevice()->getID();
      __tgt_rtl_sync_event(DeviceID, Event);
      HasBeenWaitedOn = true;
    }

    return true;
  }

  bool isRecorded() const noexcept { return IsRecorded; }

  ~ur_event_handle_t_() { urQueueRelease(Queue); }

  ur_command_t getCommandType() const noexcept { return Type; }
  ur_queue_handle_t getQueue() const noexcept { return Queue; }
  ur_context_handle_t getContext() const noexcept { return Context; }
  uint32_t getExecutionStatus() noexcept {
    if (!IsRecorded) {
      return UR_EVENT_STATUS_SUBMITTED;
    }

    if (!isComplete()) {
      return UR_EVENT_STATUS_RUNNING;
    }
    return UR_EVENT_STATUS_COMPLETE;
  }

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }
};

ur_result_t waitForEvents(uint32_t NumEvents, const ur_event_handle_t *Events);
