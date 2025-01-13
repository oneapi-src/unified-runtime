//===--------- command_buffer.hpp - Level Zero Adapter ---------------===//
//
// Copyright (C) 2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

#include <unordered_set>
#include <ur/ur.hpp>
#include <ur_api.h>
#include <ze_api.h>
#include <zes_api.h>

#include "common.hpp"

#include "context.hpp"
#include "kernel.hpp"
#include "queue_api.hpp"

struct command_buffer_profiling_t {
  ur_exp_command_buffer_sync_point_t numEvents;
  ze_kernel_timestamp_result_t *timestamps;
};

struct ur_exp_command_buffer_handle_t_ : public _ur_object {
  ur_exp_command_buffer_handle_t_(ur_context_handle_t context,
                                  ur_device_handle_t device,
                                  ze_command_list_handle_t commandList,
                                  const ur_exp_command_buffer_desc_t *desc);
  ur_event_handle_t getSignalEvent(ur_event_handle_t *hUserEvent,
                                   ur_command_t commandType);

  std::pair<ze_event_handle_t *, uint32_t>
  getWaitListView(const ur_event_handle_t *phWaitEvents,
                  uint32_t numWaitEvents);
  // Releases the resources associated with the command-buffer before the
  // command-buffer object is destroyed.
  void cleanupCommandBufferResources();

  // UR context associated with this command-buffer
  ur_context_handle_t context;
  // Device associated with this command buffer
  ur_device_handle_t device;
  v2::raii::ze_command_list_handle_t zeCommandList;

  std::vector<ze_event_handle_t> waitList;
  // Indicates if command-buffer commands can be updated after it is closed.
  bool isUpdatable = false;
  // Indicates if command buffer was finalized.
  bool isFinalized = false;
  // Command-buffer profiling is enabled.
  bool isProfilingEnabled = false;
  // This list is needed to release all kernels retained by the
  // command_buffer.
  std::vector<ur_kernel_handle_t> kernelsList;
};

struct ur_exp_command_buffer_command_handle_t_ : public _ur_object {
  ur_exp_command_buffer_command_handle_t_(ur_exp_command_buffer_handle_t,
                                          uint64_t);

  ~ur_exp_command_buffer_command_handle_t_();

  // Command-buffer of this command.
  ur_exp_command_buffer_handle_t commandBuffer;
  // L0 command ID identifying this command
  uint64_t commandId;
};
