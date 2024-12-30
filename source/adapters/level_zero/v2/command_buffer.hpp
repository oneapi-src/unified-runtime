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

#include <ur/ur.hpp>
#include <ur_api.h>
#include <ze_api.h>
#include <zes_api.h>
#include <unordered_set>

#include "common.hpp"

#include "context.hpp"
#include "kernel.hpp"
#include "queue_api.hpp"

struct command_buffer_profiling_t {
  ur_exp_command_buffer_sync_point_t NumEvents;
  ze_kernel_timestamp_result_t *Timestamps;
};

struct ur_exp_command_buffer_handle_t_ : public _ur_object {
  ur_exp_command_buffer_handle_t_(
      ur_context_handle_t Context, ur_device_handle_t Device,
      ze_command_list_handle_t CommandList,
      const ur_exp_command_buffer_desc_t *Desc
  );
  ur_event_handle_t getSignalEvent(ur_event_handle_t *hUserEvent,
                                              ur_command_t commandType);

  std::pair<ze_event_handle_t *, uint32_t>
  getWaitListView(const ur_event_handle_t *phWaitEvents,
                  uint32_t numWaitEvents);
  // Releases the resources associated with the command-buffer before the
  // command-buffer object is destroyed.
  void cleanupCommandBufferResources();

  // UR context associated with this command-buffer
  ur_context_handle_t Context;
  // Device associated with this command buffer
  ur_device_handle_t Device;
  ze_command_list_handle_t ZeCommandList;

  std::vector<ze_event_handle_t> waitList;
  // Indicates if command-buffer commands can be updated after it is closed.
  bool IsUpdatable = false;
  // Indicates if command buffer was finalized.
  bool IsFinalized = false;
  // Command-buffer profiling is enabled.
  bool IsProfilingEnabled = false;
  // This list is needed to release all kernels retained by the
  // command_buffer.
  std::vector<ur_kernel_handle_t> KernelsList;
};

struct ur_exp_command_buffer_command_handle_t_ : public _ur_object {
  ur_exp_command_buffer_command_handle_t_(ur_exp_command_buffer_handle_t,
                                          uint64_t);

  virtual ~ur_exp_command_buffer_command_handle_t_();

  // Command-buffer of this command.
  ur_exp_command_buffer_handle_t CommandBuffer;
  // L0 command ID identifying this command
  uint64_t CommandId;
};

// struct kernel_command_handle : public ur_exp_command_buffer_command_handle_t_ {
//   kernel_command_handle(ur_exp_command_buffer_handle_t CommandBuffer,
//                         ur_kernel_handle_t Kernel, uint64_t CommandId,
//                         uint32_t WorkDim, bool UserDefinedLocalSize,
//                         uint32_t NumKernelAlternatives,
//                         ur_kernel_handle_t *KernelAlternatives);

//   ~kernel_command_handle();

//   // Work-dimension the command was originally created with.
//   uint32_t WorkDim;
//   // Set to true if the user set the local work size on command creation.
//   bool UserDefinedLocalSize;
//   // Currently active kernel handle
//   ur_kernel_handle_t Kernel;
//   // Storage for valid kernel alternatives for this command.
//   std::unordered_set<ur_kernel_handle_t> ValidKernelHandles;
// };
