//===--------- launch_attributes.cpp - CUDA Adapter------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "launch_attributes.cpp"
#include "common.hpp"
#include "context.hpp"

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetLaunchAttributeExp(
    ur_exp_launch_attribute_handle_t *launchAttr,
    ur_exp_launch_attribute_id_t attrID, size_t attrSize, void *pAttrValue) {
  try {
    if (launchAttr == NULL) {
      return UR_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    switch (attrID) {
    case UR_LAUNCH_ATTRIBUTE_CLUSTER_DIMENSION: {
      if (sizeof(attrSize) != sizeof(uint32_t) * 3) {
        return UR_RESULT_ERROR_INVALID_SIZE;
      }
      if (pAttrValue == NULL) {
        return UR_RESULT_ERROR_INVALID_NULL_POINTER;
      }
      uint32_t *attrValues = reinterpret_cast<uint32_t *>(pAttrValue);
      CUlaunchAttribute launch_attribute = launchAttr[0]->get();
      launch_attribute.id = CU_LAUNCH_ATTRIBUTE_CLUSTER_DIMENSION;
      launch_attribute.value.clusterDim.x = attrValues[0];
      launch_attribute.value.clusterDim.y = attrValues[1];
      launch_attribute.value.clusterDim.z = attrValues[2];
      break;
    }
    default: {
      return UR_RESULT_ERROR_INVALID_ENUMERATION;
    }
    }
  } catch (ur_result_t err) {
    return err;
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueKernelLaunchCustomExp(
    ur_queue_handle_t hQueue, ur_kernel_handle_t hKernel, uint32_t workDim,
    const size_t *pGlobalWorkSize, const size_t *pLocalWorkSize,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    uint32_t numAttrsInLaunchAttrList,
    ur_exp_launch_attribute_handle_t *launchAttrList,
    ur_event_handle_t *phEvent) {
  // Preconditions
  UR_ASSERT(hQueue->getContext() == hKernel->getContext(),
            UR_RESULT_ERROR_INVALID_KERNEL);
  UR_ASSERT(workDim > 0, UR_RESULT_ERROR_INVALID_WORK_DIMENSION);
  UR_ASSERT(workDim < 4, UR_RESULT_ERROR_INVALID_WORK_DIMENSION);

  if (*pGlobalWorkSize == 0) {
    return urEnqueueEventsWaitWithBarrier(hQueue, numEventsInWaitList,
                                          phEventWaitList, phEvent);
  }

  // Set the number of threads per block to the number of threads per warp
  // by default unless user has provided a better number
  size_t ThreadsPerBlock[3] = {32u, 1u, 1u};
  size_t BlocksPerGrid[3] = {1u, 1u, 1u};

  uint32_t LocalSize = hKernel->getLocalSize();
  ur_result_t Result = UR_RESULT_SUCCESS;
  CUfunction CuFunc = hKernel->get();

  Result = setKernelParams(hQueue->getContext(), hQueue->Device, workDim, null,
                           pGlobalWorkSize, pLocalWorkSize, hKernel, CuFunc,
                           ThreadsPerBlock, BlocksPerGrid);
  if (Result != UR_RESULT_SUCCESS) {
    return Result;
  }

  try {
    std::unique_ptr<ur_event_handle_t_> RetImplEvent{nullptr};

    uint32_t StreamToken;
    ur_stream_guard_ Guard;
    CUstream CuStream = hQueue->getNextComputeStream(
        numEventsInWaitList, phEventWaitList, Guard, &StreamToken);

    Result = enqueueEventsWait(hQueue, CuStream, numEventsInWaitList,
                               phEventWaitList);

    if (phEvent) {
      RetImplEvent =
          std::unique_ptr<ur_event_handle_t_>(ur_event_handle_t_::makeNative(
              UR_COMMAND_KERNEL_LAUNCH, hQueue, CuStream, StreamToken));
      UR_CHECK_ERROR(RetImplEvent->start());
    }

    auto &ArgIndices = hKernel->getArgIndices();

    CUlaunchConfig launch_config;
    launch_config.gridDimX = grid_dims.x;
    launch_config.gridDimY = grid_dims.y;
    launch_config.gridDimZ = grid_dims.z;
    launch_config.blockDimX = block_dims.x;
    launch_config.blockDimY = block_dims.y;
    launch_config.blockDimZ = block_dims.z;

    launch_config.sharedMemBytes = LocalSize;
    launch_config.hStream = CuStream;
    launch_config.attrs = &(launchAttrList[0]->get());
    launch_config.numAttrs = numAttrsInLaunchAttrList;

    UR_CHECK_ERROR(cuLaunchKernelEx(&launch_config, CuFunc,
                                    const_cast<void **>(ArgIndices.data()),
                                    nullptr));

    if (LocalSize != 0)
      hKernel->clearLocalSize();

    if (phEvent) {
      UR_CHECK_ERROR(RetImplEvent->record());
      *phEvent = RetImplEvent.release();
    }

  } catch (ur_result_t Err) {
    Result = Err;
  }
  return Result;
}
