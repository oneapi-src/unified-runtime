//===--------- command_buffer.cpp - OpenCL Adapter ---------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "command_buffer.hpp"
#include "common.hpp"
#include "context.hpp"
#include "event.hpp"
#include "kernel.hpp"
#include "memory.hpp"
#include "platform.hpp"
#include "queue.hpp"

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferCreateExp(
    ur_context_handle_t hContext, ur_device_handle_t hDevice,
    [[maybe_unused]] const ur_exp_command_buffer_desc_t *pCommandBufferDesc,
    ur_exp_command_buffer_handle_t *phCommandBuffer) {

  ur_queue_handle_t Queue = nullptr;
  UR_RETURN_ON_FAILURE(urQueueCreate(hContext, hDevice, nullptr, &Queue));

  ur_platform_handle_t Platform = hDevice->Platform;
  cl_ext::clCreateCommandBufferKHR_fn clCreateCommandBufferKHR =
      Platform->ExtFuncPtr->clCreateCommandBufferKHRCache;
  UR_RETURN_ON_FAILURE(Platform->getExtFunc(&clCreateCommandBufferKHR,
                                            cl_ext::CreateCommandBufferName,
                                            "cl_khr_command_buffer"));

  cl_int Res = 0;
  cl_command_queue CLQueue = Queue->get();
  auto CLCommandBuffer = clCreateCommandBufferKHR(1, &CLQueue, nullptr, &Res);
  CL_RETURN_ON_FAILURE_AND_SET_NULL(Res, phCommandBuffer);

  try {
    auto URCommandBuffer = std::make_unique<ur_exp_command_buffer_handle_t_>(
        Queue, hContext, CLCommandBuffer);
    *phCommandBuffer = URCommandBuffer.release();
  } catch (std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_RESOURCES;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  CL_RETURN_ON_FAILURE(Res);
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urCommandBufferRetainExp(ur_exp_command_buffer_handle_t hCommandBuffer) {
  UR_RETURN_ON_FAILURE(urQueueRetain(hCommandBuffer->hInternalQueue));

  ur_platform_handle_t Platform = hCommandBuffer->getPlatform();
  cl_ext::clRetainCommandBufferKHR_fn clRetainCommandBuffer =
      Platform->ExtFuncPtr->clRetainCommandBufferKHRCache;
  UR_RETURN_ON_FAILURE(Platform->getExtFunc(&clRetainCommandBuffer,
                                            cl_ext::RetainCommandBufferName,
                                            "cl_khr_command_buffer"));

  CL_RETURN_ON_FAILURE(clRetainCommandBuffer(hCommandBuffer->CLCommandBuffer));
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urCommandBufferReleaseExp(ur_exp_command_buffer_handle_t hCommandBuffer) {
  UR_RETURN_ON_FAILURE(urQueueRelease(hCommandBuffer->hInternalQueue));

  ur_platform_handle_t Platform = hCommandBuffer->getPlatform();
  cl_ext::clReleaseCommandBufferKHR_fn clReleaseCommandBufferKHR =
      Platform->ExtFuncPtr->clReleaseCommandBufferKHRCache;
  UR_RETURN_ON_FAILURE(Platform->getExtFunc(&clReleaseCommandBufferKHR,
                                            cl_ext::ReleaseCommandBufferName,
                                            "cl_khr_command_buffer"));

  CL_RETURN_ON_FAILURE(
      clReleaseCommandBufferKHR(hCommandBuffer->CLCommandBuffer));
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urCommandBufferFinalizeExp(ur_exp_command_buffer_handle_t hCommandBuffer) {
  ur_platform_handle_t Platform = hCommandBuffer->getPlatform();
  cl_ext::clFinalizeCommandBufferKHR_fn clFinalizeCommandBufferKHR =
      Platform->ExtFuncPtr->clFinalizeCommandBufferKHRCache;
  UR_RETURN_ON_FAILURE(Platform->getExtFunc(&clFinalizeCommandBufferKHR,
                                            cl_ext::FinalizeCommandBufferName,
                                            "cl_khr_command_buffer"));

  CL_RETURN_ON_FAILURE(
      clFinalizeCommandBufferKHR(hCommandBuffer->CLCommandBuffer));
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferAppendKernelLaunchExp(
    ur_exp_command_buffer_handle_t hCommandBuffer, ur_kernel_handle_t hKernel,
    uint32_t workDim, const size_t *pGlobalWorkOffset,
    const size_t *pGlobalWorkSize, const size_t *pLocalWorkSize,
    uint32_t numSyncPointsInWaitList,
    const ur_exp_command_buffer_sync_point_t *pSyncPointWaitList,
    ur_exp_command_buffer_sync_point_t *pSyncPoint,
    ur_exp_command_buffer_command_handle_t *) {

  ur_platform_handle_t Platform = hCommandBuffer->getPlatform();
  cl_ext::clCommandNDRangeKernelKHR_fn clCommandNDRangeKernelKHR =
      Platform->ExtFuncPtr->clCommandNDRangeKernelKHRCache;
  UR_RETURN_ON_FAILURE(Platform->getExtFunc(&clCommandNDRangeKernelKHR,
                                            cl_ext::CommandNRRangeKernelName,
                                            "cl_khr_command_buffer"));

  CL_RETURN_ON_FAILURE(clCommandNDRangeKernelKHR(
      hCommandBuffer->CLCommandBuffer, nullptr, nullptr, hKernel->get(),
      workDim, pGlobalWorkOffset, pGlobalWorkSize, pLocalWorkSize,
      numSyncPointsInWaitList, pSyncPointWaitList, pSyncPoint, nullptr));

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferAppendUSMMemcpyExp(
    [[maybe_unused]] ur_exp_command_buffer_handle_t hCommandBuffer,
    [[maybe_unused]] void *pDst, [[maybe_unused]] const void *pSrc,
    [[maybe_unused]] size_t size,
    [[maybe_unused]] uint32_t numSyncPointsInWaitList,
    [[maybe_unused]] const ur_exp_command_buffer_sync_point_t
        *pSyncPointWaitList,
    [[maybe_unused]] ur_exp_command_buffer_sync_point_t *pSyncPoint) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferAppendUSMFillExp(
    [[maybe_unused]] ur_exp_command_buffer_handle_t hCommandBuffer,
    [[maybe_unused]] void *pMemory, [[maybe_unused]] const void *pPattern,
    [[maybe_unused]] size_t patternSize, [[maybe_unused]] size_t size,
    [[maybe_unused]] uint32_t numSyncPointsInWaitList,
    [[maybe_unused]] const ur_exp_command_buffer_sync_point_t
        *pSyncPointWaitList,
    [[maybe_unused]] ur_exp_command_buffer_sync_point_t *pSyncPoint) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferAppendMemBufferCopyExp(
    ur_exp_command_buffer_handle_t hCommandBuffer, ur_mem_handle_t hSrcMem,
    ur_mem_handle_t hDstMem, size_t srcOffset, size_t dstOffset, size_t size,
    uint32_t numSyncPointsInWaitList,
    const ur_exp_command_buffer_sync_point_t *pSyncPointWaitList,
    ur_exp_command_buffer_sync_point_t *pSyncPoint) {

  ur_platform_handle_t Platform = hCommandBuffer->getPlatform();
  cl_ext::clCommandCopyBufferKHR_fn clCommandCopyBufferKHR =
      Platform->ExtFuncPtr->clCommandCopyBufferKHRCache;
  UR_RETURN_ON_FAILURE(Platform->getExtFunc(&clCommandCopyBufferKHR,
                                            cl_ext::CommandCopyBufferName,
                                            "cl_khr_command_buffer"));

  CL_RETURN_ON_FAILURE(clCommandCopyBufferKHR(
      hCommandBuffer->CLCommandBuffer, nullptr, hSrcMem->get(), hDstMem->get(),
      srcOffset, dstOffset, size, numSyncPointsInWaitList, pSyncPointWaitList,
      pSyncPoint, nullptr));

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferAppendMemBufferCopyRectExp(
    [[maybe_unused]] ur_exp_command_buffer_handle_t hCommandBuffer,
    [[maybe_unused]] ur_mem_handle_t hSrcMem,
    [[maybe_unused]] ur_mem_handle_t hDstMem,
    [[maybe_unused]] ur_rect_offset_t srcOrigin,
    [[maybe_unused]] ur_rect_offset_t dstOrigin,
    [[maybe_unused]] ur_rect_region_t region,
    [[maybe_unused]] size_t srcRowPitch, [[maybe_unused]] size_t srcSlicePitch,
    [[maybe_unused]] size_t dstRowPitch, [[maybe_unused]] size_t dstSlicePitch,
    [[maybe_unused]] uint32_t numSyncPointsInWaitList,
    [[maybe_unused]] const ur_exp_command_buffer_sync_point_t
        *pSyncPointWaitList,
    [[maybe_unused]] ur_exp_command_buffer_sync_point_t *pSyncPoint) {

  size_t OpenCLOriginRect[3]{srcOrigin.x, srcOrigin.y, srcOrigin.z};
  size_t OpenCLDstRect[3]{dstOrigin.x, dstOrigin.y, dstOrigin.z};
  size_t OpenCLRegion[3]{region.width, region.height, region.depth};

  ur_platform_handle_t Platform = hCommandBuffer->getPlatform();
  cl_ext::clCommandCopyBufferRectKHR_fn clCommandCopyBufferRectKHR =
      Platform->ExtFuncPtr->clCommandCopyBufferRectKHRCache;
  UR_RETURN_ON_FAILURE(Platform->getExtFunc(&clCommandCopyBufferRectKHR,
                                            cl_ext::CommandCopyBufferRectName,
                                            "cl_khr_command_buffer"));

  CL_RETURN_ON_FAILURE(clCommandCopyBufferRectKHR(
      hCommandBuffer->CLCommandBuffer, nullptr, hSrcMem->get(), hDstMem->get(),
      OpenCLOriginRect, OpenCLDstRect, OpenCLRegion, srcRowPitch, srcSlicePitch,
      dstRowPitch, dstSlicePitch, numSyncPointsInWaitList, pSyncPointWaitList,
      pSyncPoint, nullptr));

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT
ur_result_t UR_APICALL urCommandBufferAppendMemBufferWriteExp(
    [[maybe_unused]] ur_exp_command_buffer_handle_t hCommandBuffer,
    [[maybe_unused]] ur_mem_handle_t hBuffer, [[maybe_unused]] size_t offset,
    [[maybe_unused]] size_t size, [[maybe_unused]] const void *pSrc,
    [[maybe_unused]] uint32_t numSyncPointsInWaitList,
    [[maybe_unused]] const ur_exp_command_buffer_sync_point_t
        *pSyncPointWaitList,
    [[maybe_unused]] ur_exp_command_buffer_sync_point_t *pSyncPoint) {

  cl_adapter::die("Experimental Command-buffer feature is not "
                  "implemented for OpenCL adapter.");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT
ur_result_t UR_APICALL urCommandBufferAppendMemBufferReadExp(
    [[maybe_unused]] ur_exp_command_buffer_handle_t hCommandBuffer,
    [[maybe_unused]] ur_mem_handle_t hBuffer, [[maybe_unused]] size_t offset,
    [[maybe_unused]] size_t size, [[maybe_unused]] void *pDst,
    [[maybe_unused]] uint32_t numSyncPointsInWaitList,
    [[maybe_unused]] const ur_exp_command_buffer_sync_point_t
        *pSyncPointWaitList,
    [[maybe_unused]] ur_exp_command_buffer_sync_point_t *pSyncPoint) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT
ur_result_t UR_APICALL urCommandBufferAppendMemBufferWriteRectExp(
    [[maybe_unused]] ur_exp_command_buffer_handle_t hCommandBuffer,
    [[maybe_unused]] ur_mem_handle_t hBuffer,
    [[maybe_unused]] ur_rect_offset_t bufferOffset,
    [[maybe_unused]] ur_rect_offset_t hostOffset,
    [[maybe_unused]] ur_rect_region_t region,
    [[maybe_unused]] size_t bufferRowPitch,
    [[maybe_unused]] size_t bufferSlicePitch,
    [[maybe_unused]] size_t hostRowPitch,
    [[maybe_unused]] size_t hostSlicePitch, [[maybe_unused]] void *pSrc,
    [[maybe_unused]] uint32_t numSyncPointsInWaitList,
    [[maybe_unused]] const ur_exp_command_buffer_sync_point_t
        *pSyncPointWaitList,
    [[maybe_unused]] ur_exp_command_buffer_sync_point_t *pSyncPoint) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT
ur_result_t UR_APICALL urCommandBufferAppendMemBufferReadRectExp(
    [[maybe_unused]] ur_exp_command_buffer_handle_t hCommandBuffer,
    [[maybe_unused]] ur_mem_handle_t hBuffer,
    [[maybe_unused]] ur_rect_offset_t bufferOffset,
    [[maybe_unused]] ur_rect_offset_t hostOffset,
    [[maybe_unused]] ur_rect_region_t region,
    [[maybe_unused]] size_t bufferRowPitch,
    [[maybe_unused]] size_t bufferSlicePitch,
    [[maybe_unused]] size_t hostRowPitch,
    [[maybe_unused]] size_t hostSlicePitch, [[maybe_unused]] void *pDst,
    [[maybe_unused]] uint32_t numSyncPointsInWaitList,
    [[maybe_unused]] const ur_exp_command_buffer_sync_point_t
        *pSyncPointWaitList,
    [[maybe_unused]] ur_exp_command_buffer_sync_point_t *pSyncPoint) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferAppendMemBufferFillExp(
    ur_exp_command_buffer_handle_t hCommandBuffer, ur_mem_handle_t hBuffer,
    const void *pPattern, size_t patternSize, size_t offset, size_t size,
    uint32_t numSyncPointsInWaitList,
    const ur_exp_command_buffer_sync_point_t *pSyncPointWaitList,
    ur_exp_command_buffer_sync_point_t *pSyncPoint) {

  ur_platform_handle_t Platform = hCommandBuffer->getPlatform();
  cl_ext::clCommandFillBufferKHR_fn clCommandFillBufferKHR =
      Platform->ExtFuncPtr->clCommandFillBufferKHRCache;
  UR_RETURN_ON_FAILURE(Platform->getExtFunc(&clCommandFillBufferKHR,
                                            cl_ext::CommandFillBufferName,
                                            "cl_khr_command_buffer"));

  CL_RETURN_ON_FAILURE(clCommandFillBufferKHR(
      hCommandBuffer->CLCommandBuffer, nullptr, hBuffer->get(), pPattern,
      patternSize, offset, size, numSyncPointsInWaitList, pSyncPointWaitList,
      pSyncPoint, nullptr));

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferAppendUSMPrefetchExp(
    ur_exp_command_buffer_handle_t hCommandBuffer, const void *mem, size_t size,
    ur_usm_migration_flags_t flags, uint32_t numSyncPointsInWaitList,
    const ur_exp_command_buffer_sync_point_t *pSyncPointWaitList,
    ur_exp_command_buffer_sync_point_t *pSyncPoint) {
  (void)hCommandBuffer;
  (void)mem;
  (void)size;
  (void)flags;
  (void)numSyncPointsInWaitList;
  (void)pSyncPointWaitList;
  (void)pSyncPoint;

  // Not implemented
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferAppendUSMAdviseExp(
    ur_exp_command_buffer_handle_t hCommandBuffer, const void *mem, size_t size,
    ur_usm_advice_flags_t advice, uint32_t numSyncPointsInWaitList,
    const ur_exp_command_buffer_sync_point_t *pSyncPointWaitList,
    ur_exp_command_buffer_sync_point_t *pSyncPoint) {
  (void)hCommandBuffer;
  (void)mem;
  (void)size;
  (void)advice;
  (void)numSyncPointsInWaitList;
  (void)pSyncPointWaitList;
  (void)pSyncPoint;

  // Not implemented
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferEnqueueExp(
    ur_exp_command_buffer_handle_t hCommandBuffer, ur_queue_handle_t hQueue,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {

  ur_platform_handle_t Platform = hCommandBuffer->getPlatform();
  cl_ext::clEnqueueCommandBufferKHR_fn clEnqueueCommandBufferKHR =
      Platform->ExtFuncPtr->clEnqueueCommandBufferKHRCache;
  UR_RETURN_ON_FAILURE(Platform->getExtFunc(&clEnqueueCommandBufferKHR,
                                            cl_ext::EnqueueCommandBufferName,
                                            "cl_khr_command_buffer"));

  const uint32_t NumberOfQueues = 1;
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  cl_command_queue CLQueue = hQueue->get();
  CL_RETURN_ON_FAILURE(clEnqueueCommandBufferKHR(
      NumberOfQueues, &CLQueue, hCommandBuffer->CLCommandBuffer,
      numEventsInWaitList, CLWaitEvents.data(), &Event));
  if (phEvent) {
    try {
      auto UREvent =
          std::make_unique<ur_event_handle_t_>(Event, hQueue->Context, hQueue);
      *phEvent = UREvent.release();
    } catch (std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferRetainCommandExp(
    [[maybe_unused]] ur_exp_command_buffer_command_handle_t hCommand) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferReleaseCommandExp(
    [[maybe_unused]] ur_exp_command_buffer_command_handle_t hCommand) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferUpdateKernelLaunchExp(
    [[maybe_unused]] ur_exp_command_buffer_command_handle_t hCommand,
    [[maybe_unused]] const ur_exp_command_buffer_update_kernel_launch_desc_t
        *pUpdateKernelLaunch) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferGetInfoExp(
    ur_exp_command_buffer_handle_t hCommandBuffer,
    ur_exp_command_buffer_info_t propName, size_t propSize, void *pPropValue,
    size_t *pPropSizeRet) {

  ur_platform_handle_t Platform = hCommandBuffer->getPlatform();
  cl_ext::clGetCommandBufferInfoKHR_fn clGetCommandBufferInfoKHR =
      Platform->ExtFuncPtr->clGetCommandBufferInfoKHRCache;
  UR_RETURN_ON_FAILURE(Platform->getExtFunc(&clGetCommandBufferInfoKHR,
                                            cl_ext::GetCommandBufferInfoName,
                                            "cl_khr_command_buffer"));

  if (propName != UR_EXP_COMMAND_BUFFER_INFO_REFERENCE_COUNT) {
    return UR_RESULT_ERROR_INVALID_ENUMERATION;
  }

  if (pPropSizeRet) {
    *pPropSizeRet = sizeof(cl_uint);
  }

  cl_uint ref_count;
  CL_RETURN_ON_FAILURE(clGetCommandBufferInfoKHR(
      hCommandBuffer->CLCommandBuffer, CL_COMMAND_BUFFER_REFERENCE_COUNT_KHR,
      sizeof(ref_count), &ref_count, nullptr));

  if (pPropValue) {
    if (propSize != sizeof(cl_uint)) {
      return UR_RESULT_ERROR_INVALID_SIZE;
    }
    static_assert(sizeof(cl_uint) == sizeof(uint32_t));
    *static_cast<uint32_t *>(pPropValue) = static_cast<uint32_t>(ref_count);
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urCommandBufferCommandGetInfoExp(
    [[maybe_unused]] ur_exp_command_buffer_command_handle_t hCommand,
    [[maybe_unused]] ur_exp_command_buffer_command_info_t propName,
    [[maybe_unused]] size_t propSize, [[maybe_unused]] void *pPropValue,
    [[maybe_unused]] size_t *pPropSizeRet) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
