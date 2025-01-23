//===--------- event.hpp - CUDA Adapter -----------------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

#include <cuda.h>
#include <ur/ur.hpp>

#include "common.hpp"
#include "queue.hpp"

/// UR Event mapping to CUevent
///
struct ur_event_handle_t_ : ur_handle_t_ {
public:
  using native_type = CUevent;

  ur_result_t record();

  ur_result_t wait();

  ur_result_t start();

  native_type get() const noexcept { return EvEnd; };

  ur_queue_handle_t getQueue() const noexcept { return Queue; }

  CUstream getStream() const noexcept { return Stream; }

  uint32_t getComputeStreamToken() const noexcept { return StreamToken; }

  ur_command_t getCommandType() const noexcept { return CommandType; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }

  bool isRecorded() const noexcept { return IsRecorded; }

  bool isStarted() const noexcept { return IsStarted; }

  bool isCompleted() const noexcept;

  bool isInterop() const noexcept { return IsInterop; };

  uint32_t getExecutionStatus() const noexcept {

    if (!isRecorded()) {
      return UR_EVENT_STATUS_SUBMITTED;
    }

    if (!isCompleted()) {
      return UR_EVENT_STATUS_RUNNING;
    }
    return UR_EVENT_STATUS_COMPLETE;
  }

  bool isTimestampEvent() const noexcept {
    return getCommandType() == UR_COMMAND_TIMESTAMP_RECORDING_EXP;
  }

  ur_context_handle_t getContext() const noexcept { return Context; };

  uint32_t incrementReferenceCount() { return ++RefCount; }

  uint32_t decrementReferenceCount() { return --RefCount; }

  uint32_t getEventID() const noexcept { return EventID; }

  bool backendHasOwnership() const noexcept { return HasOwnership; }

  // Returns the counter time when the associated command(s) were enqueued
  //
  uint64_t getQueuedTime() const;

  // Returns the counter time when the associated command(s) started execution
  //
  uint64_t getStartTime() const;

  // Returns the counter time when the associated command(s) completed
  //
  uint64_t getEndTime() const;

  // construct a native CUDA. This maps closely to the underlying CUDA event.
  static ur_event_handle_t
  makeNative(ur_command_t Type, ur_queue_handle_t Queue, CUstream Stream,
             uint32_t StreamToken = std::numeric_limits<uint32_t>::max()) {
    const bool RequiresTimings =
        Queue->URFlags & UR_QUEUE_FLAG_PROFILING_ENABLE ||
        Type == UR_COMMAND_TIMESTAMP_RECORDING_EXP;
    if (RequiresTimings) {
      Queue->createHostSubmitTimeStream();
    }
    native_type EvEnd = nullptr, EvQueued = nullptr, EvStart = nullptr;
    UR_CHECK_ERROR(cuEventCreate(
        &EvEnd, RequiresTimings ? CU_EVENT_DEFAULT : CU_EVENT_DISABLE_TIMING));

    if (RequiresTimings) {
      UR_CHECK_ERROR(cuEventCreate(&EvQueued, CU_EVENT_DEFAULT));
      UR_CHECK_ERROR(cuEventCreate(&EvStart, CU_EVENT_DEFAULT));
    }
    return new ur_event_handle_t_(Type, Queue->getContext(), Queue, EvEnd,
                                  EvQueued, EvStart, Stream, StreamToken);
  }

  static ur_event_handle_t makeWithNative(ur_context_handle_t context,
                                          CUevent eventNative) {
    return new ur_event_handle_t_(context, eventNative);
  }

  ur_result_t release();

  ~ur_event_handle_t_();

private:
  // This constructor is private to force programmers to use the makeNative /
  // make_user static members in order to create a pi_event for CUDA.
  ur_event_handle_t_(ur_command_t Type, ur_context_handle_t Context,
                     ur_queue_handle_t Queue, native_type EvEnd,
                     native_type EvQueued, native_type EvStart, CUstream Stream,
                     uint32_t StreamToken);

  // This constructor is private to force programmers to use the
  // makeWithNative for event interop
  ur_event_handle_t_(ur_context_handle_t Context, CUevent EventNative);

  ur_command_t CommandType; // The type of command associated with event.

  std::atomic_uint32_t RefCount; // Event reference count.

  bool HasOwnership; // Signifies if event owns the native type.

  bool HasBeenWaitedOn; // Signifies whether the event has been waited
                        // on through a call to wait(), which implies
                        // that it has completed.

  bool IsRecorded; // Signifies wether a native CUDA event has been recorded
                   // yet.
  bool IsStarted;  // Signifies wether the operation associated with the
                   // UR event has started or not

  const bool IsInterop{false}; // Made with urEventCreateWithNativeHandle

  uint32_t StreamToken;
  uint32_t EventID; // Queue identifier of the event.

  native_type EvEnd; // CUDA event handle. If this ur_event_handle_t represents
                     // a user event, this will be nullptr.

  native_type EvStart; // CUDA event handle associated with the start

  native_type EvQueued; // CUDA event handle associated with the time
                        // the command was enqueued

  ur_queue_handle_t Queue; // ur_queue_handle_t associated with the event. If
                           // this is a user event, this will be nullptr.

  CUstream Stream; // CUstream associated with the event. If this is a user
                   // event, this will be uninitialized.

  ur_context_handle_t Context; // ur_context_handle_t associated with the event.
                               // If this is a native event, this will be the
                               // same context associated with the queue member.
};

// Iterate over `event_wait_list` and apply the given callback `f` to the
// latest event on each queue therein. The callback must take a single
// ur_event_handle_t argument and return a ur_result_t. If the callback returns
// an error, the iteration terminates and the error is returned.
template <typename Func>
ur_result_t forLatestEvents(const ur_event_handle_t *EventWaitList,
                            std::size_t NumEventsInWaitList, Func &&F) {

  if (EventWaitList == nullptr || NumEventsInWaitList == 0) {
    return UR_RESULT_ERROR_INVALID_EVENT_WAIT_LIST;
  }

  // Fast path if we only have a single event
  if (NumEventsInWaitList == 1) {
    return F(EventWaitList[0]);
  }

  std::vector<ur_event_handle_t> Events{EventWaitList,
                                        EventWaitList + NumEventsInWaitList};
  std::sort(Events.begin(), Events.end(),
            [](ur_event_handle_t Event0, ur_event_handle_t Event1) {
              // Tiered sort creating sublists of streams (smallest value first)
              // in which the corresponding events are sorted into a sequence of
              // newest first.
              return Event0->getStream() < Event1->getStream() ||
                     (Event0->getStream() == Event1->getStream() &&
                      Event0->getEventID() > Event1->getEventID());
            });

  CUstream LastSeenStream = 0;
  for (size_t i = 0; i < Events.size(); i++) {
    auto Event = Events[i];
    if (!Event || (i != 0 && !Event->isInterop() &&
                   Event->getStream() == LastSeenStream)) {
      continue;
    }

    LastSeenStream = Event->getStream();

    auto Result = F(Event);
    if (Result != UR_RESULT_SUCCESS) {
      return Result;
    }
  }

  return UR_RESULT_SUCCESS;
}
