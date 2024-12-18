//===--------- command_buffer.cpp - Level Zero Adapter ---------------===//
//
// Copyright (C) 2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "command_buffer.hpp"
#include "../helpers/kernel_helpers.hpp"
#include "logger/ur_logger.hpp"
#include "../ur_interface_loader.hpp"

namespace ur::level_zero {

ur_result_t
urCommandBufferCreateExp(ur_context_handle_t Context, ur_device_handle_t Device,
                         const ur_exp_command_buffer_desc_t *CommandBufferDesc,
                         ur_exp_command_buffer_handle_t *CommandBuffer) {
  return UR_RESULT_SUCCESS;
}

ur_result_t urCommandBufferRetainCommandExp(
    ur_exp_command_buffer_command_handle_t Command) {
  return UR_RESULT_SUCCESS;
}

ur_result_t
urCommandBufferReleaseExp(ur_exp_command_buffer_handle_t CommandBuffer) {
  return UR_RESULT_SUCCESS;
}

ur_result_t
urCommandBufferFinalizeExp(ur_exp_command_buffer_handle_t CommandBuffer) {
  return UR_RESULT_SUCCESS;
}

ur_result_t urCommandBufferAppendKernelLaunchExp(
    ur_exp_command_buffer_handle_t CommandBuffer, ur_kernel_handle_t Kernel,
    uint32_t WorkDim, const size_t *GlobalWorkOffset,
    const size_t *GlobalWorkSize, const size_t *LocalWorkSize,
    uint32_t NumKernelAlternatives, ur_kernel_handle_t *KernelAlternatives,
    uint32_t NumSyncPointsInWaitList,
    const ur_exp_command_buffer_sync_point_t *SyncPointWaitList,
    uint32_t NumEventsInWaitList, const ur_event_handle_t *EventWaitList,
    ur_exp_command_buffer_sync_point_t *RetSyncPoint, ur_event_handle_t *Event,
    ur_exp_command_buffer_command_handle_t *Command) {
  return UR_RESULT_SUCCESS;
}

ur_result_t urCommandBufferEnqueueExp(
    ur_exp_command_buffer_handle_t CommandBuffer, ur_queue_handle_t UrQueue,
    uint32_t NumEventsInWaitList, const ur_event_handle_t *EventWaitList,
    ur_event_handle_t *Event) {
  return UR_RESULT_SUCCESS;
}
ur_result_t
urCommandBufferGetInfoExp(ur_exp_command_buffer_handle_t hCommandBuffer,
                          ur_exp_command_buffer_info_t propName,
                          size_t propSize, void *pPropValue,
                          size_t *pPropSizeRet) {
  return UR_RESULT_SUCCESS;
}

} // namespace ur::level_zero
