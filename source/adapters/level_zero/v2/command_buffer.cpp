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
// given context.
void checkImmediateAppendSupport(ur_context_handle_t context) {
  bool DriverSupportsImmediateAppend =
      context->getPlatform()->ZeCommandListImmediateAppendExt.Supported;

  if (!DriverSupportsImmediateAppend) {
    logger::error("Adapter v2 is used but "
                  "the current driver does not support the "
                  "zeCommandListImmediateAppendCommandListsExp entrypoint.");
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
    ur_context_handle_t context, ur_device_handle_t device,
      ze_command_list_handle_t commandList,
      const ur_exp_command_buffer_desc_t *desc)
    : context(context), device(device), zeCommandList(commandList),
      isUpdatable(desc ? desc->isUpdatable : false) {
  UR_CALL_THROWS(ur::level_zero::urContextRetain(context));
  UR_CALL_THROWS(ur::level_zero::urDeviceRetain(device));
}

void ur_exp_command_buffer_handle_t_::cleanupCommandBufferResources() {
  // Release the memory allocated to the Context stored in the command_buffer
  UR_CALL_THROWS(ur::level_zero::urContextRelease(context));

  // Release the device
  UR_CALL_THROWS(ur::level_zero::urDeviceRelease(device));

  for (auto &associatedKernel : kernelsList) {
    UR_CALL_THROWS(ur::level_zero::urKernelRelease(associatedKernel));
  }
}

namespace ur::level_zero {

/**
 * Creates a L0 command list
 * @param[in] context The Context associated with the command-list
 * @param[in] device  The Device associated with the command-list
 * @param[in] isUpdatable Whether the command-list should be mutable.
 * @param[out] commandList The L0 command-list created by this function.
 * @return UR_RESULT_SUCCESS or an error code on failure
 */
ur_result_t createMainCommandList(ur_context_handle_t context,
                                  ur_device_handle_t device,
                                  bool isUpdatable,
                                  ze_command_list_handle_t &commandList) {


  using queue_group_type = ur_device_handle_t_::queue_group_info_t::type;
  // that should be call to queue getZeOrdinal, 
  // but queue is not available while constructing buffer
  uint32_t queueGroupOrdinal = device->QueueGroup[queue_group_type::Compute].ZeOrdinal;

  ZeStruct<ze_command_list_desc_t> zeCommandListDesc;
  zeCommandListDesc.commandQueueGroupOrdinal = queueGroupOrdinal;

  zeCommandListDesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;

  ZeStruct<ze_mutable_command_list_exp_desc_t> zeMutableCommandListDesc;
  if (isUpdatable) {
    zeMutableCommandListDesc.flags = 0;
    zeCommandListDesc.pNext = &zeMutableCommandListDesc;
  }

  ZE2UR_CALL(zeCommandListCreate, (context->getZeHandle(), device->ZeDevice,
                                   &zeCommandListDesc, &commandList));

  return UR_RESULT_SUCCESS;
}

ur_result_t
urCommandBufferCreateExp(ur_context_handle_t context, ur_device_handle_t device,
                         const ur_exp_command_buffer_desc_t *commandBufferDesc,
                         ur_exp_command_buffer_handle_t *commandBuffer) {
  try {
    bool isUpdatable = commandBufferDesc && commandBufferDesc->isUpdatable;
    checkImmediateAppendSupport(context);

    if (isUpdatable) {
      UR_ASSERT(context->getPlatform()->ZeMutableCmdListExt.Supported,
                UR_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
    
    ze_command_list_handle_t zeCommandList = nullptr;
    UR_CALL(createMainCommandList(context, device, isUpdatable, zeCommandList));
    *commandBuffer = new ur_exp_command_buffer_handle_t_(
        context, device, zeCommandList, commandBufferDesc);
  } catch (const std::bad_alloc &) {
    return exceptionToResult(std::current_exception());
  }
  return UR_RESULT_SUCCESS;
}
ur_result_t
urCommandBufferRetainExp(ur_exp_command_buffer_handle_t hCommandBuffer) {
  try {
    hCommandBuffer->RefCount.increment();
  } catch (const std::bad_alloc &) {
    return exceptionToResult(std::current_exception());
  }
  return UR_RESULT_SUCCESS;
}

ur_result_t
urCommandBufferReleaseExp(ur_exp_command_buffer_handle_t hCommandBuffer) {
  try {
    if (!hCommandBuffer->RefCount.decrementAndTest())
      return UR_RESULT_SUCCESS;

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
  try {
    UR_ASSERT(hCommandBuffer, UR_RESULT_ERROR_INVALID_NULL_POINTER);
    UR_ASSERT(!hCommandBuffer->isFinalized, UR_RESULT_ERROR_INVALID_OPERATION);

    // It is not allowed to append to command list from multiple threads.
    std::scoped_lock<ur_shared_mutex> guard(hCommandBuffer->Mutex);

    // Close the command lists and have them ready for dispatch.
    ZE2UR_CALL(zeCommandListClose, (hCommandBuffer->zeCommandList.get()));

    hCommandBuffer->isFinalized = true;
  } catch (...) {
    return exceptionToResult(std::current_exception());
  }
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
  try {
    UR_ASSERT(hKernel, UR_RESULT_ERROR_INVALID_NULL_HANDLE);
    UR_ASSERT(hKernel->getProgramHandle(), UR_RESULT_ERROR_INVALID_NULL_POINTER);

    UR_ASSERT(workDim > 0, UR_RESULT_ERROR_INVALID_WORK_DIMENSION);
    UR_ASSERT(workDim < 4, UR_RESULT_ERROR_INVALID_WORK_DIMENSION);

    ze_kernel_handle_t hZeKernel = hKernel->getZeHandle(commandBuffer->device);

    std::scoped_lock<ur_shared_mutex, ur_shared_mutex> lock(commandBuffer->Mutex,
                                                            hKernel->Mutex);

    ze_group_count_t zeThreadGroupDimensions{1, 1, 1};
    uint32_t wg[3]{};
    UR_CALL(calculateKernelWorkDimensions(hZeKernel, commandBuffer->device,
                                          zeThreadGroupDimensions, wg, workDim,
                                          pGlobalWorkSize, pLocalWorkSize));

    auto waitList = commandBuffer->getWaitListView(nullptr, 0);

    bool memoryMigrated = false;
    auto memoryMigrate = [&](void *src, void *dst, size_t size) {
      ZE2UR_CALL_THROWS(zeCommandListAppendMemoryCopy,
                        (commandBuffer->zeCommandList.get(), dst, src, size, nullptr,
                        waitList.second, waitList.first));
      memoryMigrated = true;
    };

    UR_CALL(hKernel->prepareForSubmission(commandBuffer->context, commandBuffer->device, pGlobalWorkOffset,
                                          workDim, wg[0], wg[1], wg[2],
                                          memoryMigrate));

    ZE2UR_CALL(zeCommandListAppendLaunchKernel,
              (commandBuffer->zeCommandList.get(), hZeKernel, &zeThreadGroupDimensions,
                nullptr, waitList.second, waitList.first));
  } catch (...) {
    return exceptionToResult(std::current_exception());
  }

  return UR_RESULT_SUCCESS;
}

ur_result_t urCommandBufferEnqueueExp(
    ur_exp_command_buffer_handle_t hCommandBuffer, ur_queue_handle_t hQueue,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {
  try {
    return hQueue->enqueueCommandBuffer(
        hCommandBuffer->zeCommandList.get(), phEvent, numEventsInWaitList, phEventWaitList);
  } catch (...) {
    return exceptionToResult(std::current_exception());
  }
}

ur_result_t
urCommandBufferGetInfoExp(ur_exp_command_buffer_handle_t hCommandBuffer,
                          ur_exp_command_buffer_info_t propName,
                          size_t propSize, void *pPropValue,
                          size_t *pPropSizeRet) {
  try {
    UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);

    switch (propName) {
    case UR_EXP_COMMAND_BUFFER_INFO_REFERENCE_COUNT:
      return ReturnValue(uint32_t{hCommandBuffer->RefCount.load()});
    case UR_EXP_COMMAND_BUFFER_INFO_DESCRIPTOR: {
      ur_exp_command_buffer_desc_t Descriptor{};
      Descriptor.stype = UR_STRUCTURE_TYPE_EXP_COMMAND_BUFFER_DESC;
      Descriptor.pNext = nullptr;
      Descriptor.isUpdatable = hCommandBuffer->isUpdatable;
      Descriptor.isInOrder = true;
      Descriptor.enableProfiling = hCommandBuffer->isProfilingEnabled;

      return ReturnValue(Descriptor);
    }
    default:
      assert(!"Command-buffer info request not implemented");
    }
  } catch (...) {
    return exceptionToResult(std::current_exception());
  }
  return UR_RESULT_ERROR_INVALID_ENUMERATION;
}

} // namespace ur::level_zero
