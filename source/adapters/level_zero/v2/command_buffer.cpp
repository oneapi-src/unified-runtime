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

namespace {

// Checks whether zeCommandListImmediateAppendCommandListsExp can be used for a
// given Context and Device.
void checkImmediateAppendSupport(ur_context_handle_t Context,
                                 ur_device_handle_t Device) {
  // TODO The L0 driver is not reporting this extension yet. Once it does,
  // switch to using the variable zeDriverImmediateCommandListAppendFound.

  // Minimum version that supports zeCommandListImmediateAppendCommandListsExp.
  constexpr uint32_t MinDriverVersion = 30898;
  bool DriverSupportsImmediateAppend =
      Context->getPlatform()->isDriverVersionNewerOrSimilar(1, 3,
                                                            MinDriverVersion);

  // If this environment variable is:
  //   - Set to 1: the immediate append path will always be enabled as long the
  //   pre-requisites are met.
  //   - Set to 0: the immediate append path will always be disabled.
  //   - Not Defined: The default behaviour will be used which enables the
  //   immediate append path only for some devices when the pre-requisites are
  //   met.
  const char *AppendEnvVarName = "UR_L0_CMD_BUFFER_USE_IMMEDIATE_APPEND_PATH";
  const char *UrRet = std::getenv(AppendEnvVarName);

  if (!Device->ImmCommandListUsed) {
    logger::error("Adapter v2 is used but immediate command-lists are currently "
                  "disabled. Immediate command-lists are "
                  "required to use the adapter v2.");
    std::abort();
  }
  if (!DriverSupportsImmediateAppend) {
    logger::error("Adapter v2 is used but "
                  "the current driver does not support the "
                  "zeCommandListImmediateAppendCommandListsExp entrypoint. A "
                  "driver version of at least {} is required to use the "
                  "immediate append path.", MinDriverVersion);
    std::abort();
  }

  const bool EnableAppendPath = !UrRet || std::atoi(UrRet) == 1;
  if (!Device->isPVC() && !EnableAppendPath) {
    logger::error("Adapter v2 is used but "
                  "immediate append support is not enabled."
                  "Please set {}=1 to enable it.", AppendEnvVarName);
    std::abort();
  }

}

}

std::pair<ze_event_handle_t *, uint32_t>
ur_exp_command_buffer_handle_t_::getWaitListView(
    const ur_event_handle_t *phWaitEvents, uint32_t numWaitEvents) {

  waitList.resize(numWaitEvents);
  for (uint32_t i = 0; i < numWaitEvents; i++) {
    waitList[i] = phWaitEvents[i]->getZeEvent();
  }

  return {waitList.data(), static_cast<uint32_t>(numWaitEvents)};
}

ur_exp_command_buffer_handle_t_::ur_exp_command_buffer_handle_t_(
    ur_context_handle_t Context, ur_device_handle_t Device,
      ze_command_list_handle_t CommandList,
      const ur_exp_command_buffer_desc_t *Desc)
    : Context(Context), Device(Device), ZeCommandList(CommandList),
      IsUpdatable(Desc ? Desc->isUpdatable : false) {
  UR_CALL_THROWS(ur::level_zero::urContextRetain(Context));
  UR_CALL_THROWS(ur::level_zero::urDeviceRetain(Device));
}

void ur_exp_command_buffer_handle_t_::cleanupCommandBufferResources() {
  // Release the memory allocated to the Context stored in the command_buffer
  UR_CALL_THROWS(ur::level_zero::urContextRelease(Context));

  // Release the device
  UR_CALL_THROWS(ur::level_zero::urDeviceRelease(Device));

  // Release the memory allocated to the CommandList stored in the
  // command_buffer
  if (ZeCommandList) {
    ZE_CALL_NOCHECK(zeCommandListDestroy, (ZeCommandList));
  }

  for (auto &AssociatedKernel : KernelsList) {
    UR_CALL_THROWS(ur::level_zero::urKernelRelease(AssociatedKernel));
  }
}

namespace ur::level_zero {

/**
 * Creates a L0 command list
 * @param[in] Context The Context associated with the command-list
 * @param[in] Device  The Device associated with the command-list
 * @param[in] IsUpdatable Whether the command-list should be mutable.
 * @param[out] CommandList The L0 command-list created by this function.
 * @return UR_RESULT_SUCCESS or an error code on failure
 */
ur_result_t createMainCommandList(ur_context_handle_t Context,
                                  ur_device_handle_t Device,
                                  bool IsUpdatable,
                                  ze_command_list_handle_t &CommandList) {


  using queue_group_type = ur_device_handle_t_::queue_group_info_t::type;
  // that should be call to queue getZeOrdinal, 
  // but queue is not available while constructing buffer
  uint32_t QueueGroupOrdinal = Device->QueueGroup[queue_group_type::Compute].ZeOrdinal;

  ZeStruct<ze_command_list_desc_t> ZeCommandListDesc;
  ZeCommandListDesc.commandQueueGroupOrdinal = QueueGroupOrdinal;

  ZeCommandListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;

  ZeStruct<ze_mutable_command_list_exp_desc_t> ZeMutableCommandListDesc;
  if (IsUpdatable) {
    ZeMutableCommandListDesc.flags = 0;
    ZeCommandListDesc.pNext = &ZeMutableCommandListDesc;
  }

  ZE2UR_CALL(zeCommandListCreate, (Context->getZeHandle(), Device->ZeDevice,
                                   &ZeCommandListDesc, &CommandList));

  return UR_RESULT_SUCCESS;
}

ur_result_t
urCommandBufferCreateExp(ur_context_handle_t Context, ur_device_handle_t Device,
                         const ur_exp_command_buffer_desc_t *CommandBufferDesc,
                         ur_exp_command_buffer_handle_t *CommandBuffer) {
  bool IsUpdatable = CommandBufferDesc && CommandBufferDesc->isUpdatable;
  checkImmediateAppendSupport(Context, Device);

  if (IsUpdatable) {
    UR_ASSERT(Context->getPlatform()->ZeMutableCmdListExt.Supported,
              UR_RESULT_ERROR_UNSUPPORTED_FEATURE);
  }
  
  ze_command_list_handle_t ZeCommandList = nullptr;
  UR_CALL(createMainCommandList(Context, Device, IsUpdatable, ZeCommandList));
  try {
    *CommandBuffer = new ur_exp_command_buffer_handle_t_(
        Context, Device, ZeCommandList, CommandBufferDesc);
  } catch (const std::bad_alloc &) {
    return exceptionToResult(std::current_exception());
  }
  return UR_RESULT_SUCCESS;
}
ur_result_t
urCommandBufferRetainExp(ur_exp_command_buffer_handle_t hCommandBuffer) {
  hCommandBuffer->RefCount.increment();
  return UR_RESULT_SUCCESS;
}

ur_result_t
urCommandBufferReleaseExp(ur_exp_command_buffer_handle_t hCommandBuffer) {
  if (!hCommandBuffer->RefCount.decrementAndTest())
    return UR_RESULT_SUCCESS;

  try {
    hCommandBuffer->cleanupCommandBufferResources();
  } catch (...) {
    delete hCommandBuffer;
    return exceptionToResult(std::current_exception());
  }
  delete hCommandBuffer;
  return UR_RESULT_SUCCESS;
}

ur_result_t
urCommandBufferFinalizeExp(ur_exp_command_buffer_handle_t hCommandBuffer)  {
  UR_ASSERT(hCommandBuffer, UR_RESULT_ERROR_INVALID_NULL_POINTER);
  UR_ASSERT(!hCommandBuffer->IsFinalized, UR_RESULT_ERROR_INVALID_OPERATION);

  // It is not allowed to append to command list from multiple threads.
  std::scoped_lock<ur_shared_mutex> Guard(hCommandBuffer->Mutex);

  // Close the command lists and have them ready for dispatch.
  ZE2UR_CALL(zeCommandListClose, (hCommandBuffer->ZeCommandList));

  hCommandBuffer->IsFinalized = true;

  return UR_RESULT_SUCCESS;
}

ur_result_t urCommandBufferAppendKernelLaunchExp(
    ur_exp_command_buffer_handle_t commandBuffer, ur_kernel_handle_t hKernel,
    uint32_t workDim, const size_t *pGlobalWorkOffset,
    const size_t *pGlobalWorkSize, const size_t *pLocalWorkSize,
    uint32_t numKernelAlternatives, ur_kernel_handle_t *kernelAlternatives,
    uint32_t numSyncPointsInWaitList,
    const ur_exp_command_buffer_sync_point_t *syncPointWaitList,
    uint32_t numEventsInWaitList, const ur_event_handle_t *eventWaitList,
    ur_exp_command_buffer_sync_point_t *retSyncPoint, ur_event_handle_t *event,
    ur_exp_command_buffer_command_handle_t *command) {
  //Need to know semantics 
  // - should they be checked before kernel execution or before kernel appending to list
  // if latter then it is easy fix, if former then TODO
  std::ignore = numEventsInWaitList;
  std::ignore = eventWaitList;
  std::ignore = event;

  //sync mechanic can be ignored, because all lists are in-order
  std::ignore = numSyncPointsInWaitList;
  std::ignore = syncPointWaitList;
  std::ignore = retSyncPoint;

  //TODO
  std::ignore = numKernelAlternatives;
  std::ignore = kernelAlternatives;
  std::ignore = command;

  UR_ASSERT(hKernel, UR_RESULT_ERROR_INVALID_NULL_HANDLE);
  UR_ASSERT(hKernel->getProgramHandle(), UR_RESULT_ERROR_INVALID_NULL_POINTER);

  UR_ASSERT(workDim > 0, UR_RESULT_ERROR_INVALID_WORK_DIMENSION);
  UR_ASSERT(workDim < 4, UR_RESULT_ERROR_INVALID_WORK_DIMENSION);

  ze_kernel_handle_t hZeKernel = hKernel->getZeHandle(commandBuffer->Device);

  std::scoped_lock<ur_shared_mutex, ur_shared_mutex> Lock(commandBuffer->Mutex,
                                                          hKernel->Mutex);

  ze_group_count_t zeThreadGroupDimensions{1, 1, 1};
  uint32_t WG[3]{};
  UR_CALL(calculateKernelWorkDimensions(hZeKernel, commandBuffer->Device,
                                        zeThreadGroupDimensions, WG, workDim,
                                        pGlobalWorkSize, pLocalWorkSize));

  auto waitList = commandBuffer->getWaitListView(nullptr, 0);

  bool memoryMigrated = false;
  auto memoryMigrate = [&](void *src, void *dst, size_t size) {
    ZE2UR_CALL_THROWS(zeCommandListAppendMemoryCopy,
                      (commandBuffer->ZeCommandList, dst, src, size, nullptr,
                       waitList.second, waitList.first));
    memoryMigrated = true;
  };

  UR_CALL(hKernel->prepareForSubmission(commandBuffer->Context, commandBuffer->Device, pGlobalWorkOffset,
                                        workDim, WG[0], WG[1], WG[2],
                                        memoryMigrate));

  ZE2UR_CALL(zeCommandListAppendLaunchKernel,
             (commandBuffer->ZeCommandList, hZeKernel, &zeThreadGroupDimensions,
              nullptr, waitList.second, waitList.first));

  return UR_RESULT_SUCCESS;
}

ur_result_t urCommandBufferEnqueueExp(
    ur_exp_command_buffer_handle_t hCommandBuffer, ur_queue_handle_t hQueue,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {
  try {
    return hQueue->enqueueCommandBuffer(
        hCommandBuffer->ZeCommandList, phEvent, numEventsInWaitList, phEventWaitList);
  } catch (...) {
    return exceptionToResult(std::current_exception());
  }
}

ur_result_t
urCommandBufferGetInfoExp(ur_exp_command_buffer_handle_t hCommandBuffer,
                          ur_exp_command_buffer_info_t propName,
                          size_t propSize, void *pPropValue,
                          size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);

  switch (propName) {
  case UR_EXP_COMMAND_BUFFER_INFO_REFERENCE_COUNT:
    return ReturnValue(uint32_t{hCommandBuffer->RefCount.load()});
  case UR_EXP_COMMAND_BUFFER_INFO_DESCRIPTOR: {
    ur_exp_command_buffer_desc_t Descriptor{};
    Descriptor.stype = UR_STRUCTURE_TYPE_EXP_COMMAND_BUFFER_DESC;
    Descriptor.pNext = nullptr;
    Descriptor.isUpdatable = hCommandBuffer->IsUpdatable;
    Descriptor.isInOrder = true;
    Descriptor.enableProfiling = hCommandBuffer->IsProfilingEnabled;

    return ReturnValue(Descriptor);
  }
  default:
    assert(!"Command-buffer info request not implemented");
  }

  return UR_RESULT_ERROR_INVALID_ENUMERATION;
}

} // namespace ur::level_zero
