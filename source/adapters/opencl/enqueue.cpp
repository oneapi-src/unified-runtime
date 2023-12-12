//===--------- enqueue.cpp - OpenCL Adapter --------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "common.hpp"
#include "context.hpp"
#include "event.hpp"
#include "memory.hpp"
#include "program.hpp"
#include "queue.hpp"

cl_map_flags convertURMapFlagsToCL(ur_map_flags_t URFlags) {
  cl_map_flags CLFlags = 0;
  if (URFlags & UR_MAP_FLAG_READ) {
    CLFlags |= CL_MAP_READ;
  }
  if (URFlags & UR_MAP_FLAG_WRITE) {
    CLFlags |= CL_MAP_WRITE;
  }
  if (URFlags & UR_MAP_FLAG_WRITE_INVALIDATE_REGION) {
    CLFlags |= CL_MAP_WRITE_INVALIDATE_REGION;
  }

  return CLFlags;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueKernelLaunch(
    ur_queue_handle_t hQueue, ur_kernel_handle_t hKernel, uint32_t workDim,
    const size_t *pGlobalWorkOffset, const size_t *pGlobalWorkSize,
    const size_t *pLocalWorkSize, uint32_t numEventsInWaitList,
    const ur_event_handle_t *phEventWaitList, ur_event_handle_t *phEvent) {
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueNDRangeKernel(
      hQueue->get(), cl_adapter::cast<cl_kernel>(hKernel), workDim,
      pGlobalWorkOffset, pGlobalWorkSize, pLocalWorkSize, numEventsInWaitList,
      CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueEventsWait(
    ur_queue_handle_t hQueue, uint32_t numEventsInWaitList,
    const ur_event_handle_t *phEventWaitList, ur_event_handle_t *phEvent) {
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueMarkerWithWaitList(
      hQueue->get(), numEventsInWaitList, CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueEventsWaitWithBarrier(
    ur_queue_handle_t hQueue, uint32_t numEventsInWaitList,
    const ur_event_handle_t *phEventWaitList, ur_event_handle_t *phEvent) {
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueBarrierWithWaitList(
      hQueue->get(), numEventsInWaitList, CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferRead(
    ur_queue_handle_t hQueue, ur_mem_handle_t hBuffer, bool blockingRead,
    size_t offset, size_t size, void *pDst, uint32_t numEventsInWaitList,
    const ur_event_handle_t *phEventWaitList, ur_event_handle_t *phEvent) {
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueReadBuffer(
      hQueue->get(), hBuffer->get(), blockingRead, offset, size, pDst,
      numEventsInWaitList, CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferWrite(
    ur_queue_handle_t hQueue, ur_mem_handle_t hBuffer, bool blockingWrite,
    size_t offset, size_t size, const void *pSrc, uint32_t numEventsInWaitList,
    const ur_event_handle_t *phEventWaitList, ur_event_handle_t *phEvent) {
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueWriteBuffer(
      hQueue->get(), hBuffer->get(), blockingWrite, offset, size, pSrc,
      numEventsInWaitList, CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferReadRect(
    ur_queue_handle_t hQueue, ur_mem_handle_t hBuffer, bool blockingRead,
    ur_rect_offset_t bufferOrigin, ur_rect_offset_t hostOrigin,
    ur_rect_region_t region, size_t bufferRowPitch, size_t bufferSlicePitch,
    size_t hostRowPitch, size_t hostSlicePitch, void *pDst,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {
  const size_t BufferOrigin[3] = {bufferOrigin.x, bufferOrigin.y,
                                  bufferOrigin.z};
  const size_t HostOrigin[3] = {hostOrigin.x, hostOrigin.y, hostOrigin.z};
  const size_t Region[3] = {region.width, region.height, region.depth};
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueReadBufferRect(
      hQueue->get(), hBuffer->get(), blockingRead, BufferOrigin, HostOrigin,
      Region, bufferRowPitch, bufferSlicePitch, hostRowPitch, hostSlicePitch,
      pDst, numEventsInWaitList, CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferWriteRect(
    ur_queue_handle_t hQueue, ur_mem_handle_t hBuffer, bool blockingWrite,
    ur_rect_offset_t bufferOrigin, ur_rect_offset_t hostOrigin,
    ur_rect_region_t region, size_t bufferRowPitch, size_t bufferSlicePitch,
    size_t hostRowPitch, size_t hostSlicePitch, void *pSrc,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {
  const size_t BufferOrigin[3] = {bufferOrigin.x, bufferOrigin.y,
                                  bufferOrigin.z};
  const size_t HostOrigin[3] = {hostOrigin.x, hostOrigin.y, hostOrigin.z};
  const size_t Region[3] = {region.width, region.height, region.depth};
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueWriteBufferRect(
      hQueue->get(), hBuffer->get(), blockingWrite, BufferOrigin, HostOrigin,
      Region, bufferRowPitch, bufferSlicePitch, hostRowPitch, hostSlicePitch,
      pSrc, numEventsInWaitList, CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferCopy(
    ur_queue_handle_t hQueue, ur_mem_handle_t hBufferSrc,
    ur_mem_handle_t hBufferDst, size_t srcOffset, size_t dstOffset, size_t size,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueCopyBuffer(
      hQueue->get(), hBufferSrc->get(), hBufferDst->get(), srcOffset, dstOffset,
      size, numEventsInWaitList, CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferCopyRect(
    ur_queue_handle_t hQueue, ur_mem_handle_t hBufferSrc,
    ur_mem_handle_t hBufferDst, ur_rect_offset_t srcOrigin,
    ur_rect_offset_t dstOrigin, ur_rect_region_t region, size_t srcRowPitch,
    size_t srcSlicePitch, size_t dstRowPitch, size_t dstSlicePitch,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {
  const size_t SrcOrigin[3] = {srcOrigin.x, srcOrigin.y, srcOrigin.z};
  const size_t DstOrigin[3] = {dstOrigin.x, dstOrigin.y, dstOrigin.z};
  const size_t Region[3] = {region.width, region.height, region.depth};
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueCopyBufferRect(
      hQueue->get(), hBufferSrc->get(), hBufferDst->get(), SrcOrigin, DstOrigin,
      Region, srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch,
      numEventsInWaitList, CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferFill(
    ur_queue_handle_t hQueue, ur_mem_handle_t hBuffer, const void *pPattern,
    size_t patternSize, size_t offset, size_t size,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {
  // CL FillBuffer only allows pattern sizes up to the largest CL type:
  // long16/double16
  if (patternSize <= 128) {
    cl_event Event;
    std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
    for (uint32_t i = 0; i < numEventsInWaitList; i++) {
      CLWaitEvents[i] = phEventWaitList[i]->get();
    }
    CL_RETURN_ON_FAILURE(clEnqueueFillBuffer(
        hQueue->get(), hBuffer->get(), pPattern, patternSize, offset, size,
        numEventsInWaitList, CLWaitEvents.data(), &Event));
    if (phEvent) {
      *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
    }
    return UR_RESULT_SUCCESS;
  }

  auto NumValues = size / sizeof(uint64_t);
  auto HostBuffer = new uint64_t[NumValues];
  auto NumChunks = patternSize / sizeof(uint64_t);
  for (size_t i = 0; i < NumValues; i++) {
    HostBuffer[i] = static_cast<const uint64_t *>(pPattern)[i % NumChunks];
  }

  cl_event WriteEvent = nullptr;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  auto ClErr = clEnqueueWriteBuffer(
      hQueue->get(), hBuffer->get(), false, offset, size, HostBuffer,
      numEventsInWaitList, CLWaitEvents.data(), &WriteEvent);
  if (ClErr != CL_SUCCESS) {
    delete[] HostBuffer;
    CL_RETURN_ON_FAILURE(ClErr);
  }

  auto DeleteCallback = [](cl_event, cl_int, void *pUserData) {
    delete[] static_cast<uint64_t *>(pUserData);
  };
  ClErr =
      clSetEventCallback(WriteEvent, CL_COMPLETE, DeleteCallback, HostBuffer);
  if (ClErr != CL_SUCCESS) {
    // We can attempt to recover gracefully by attempting to wait for the write
    // to finish and deleting the host buffer.
    clWaitForEvents(1, &WriteEvent);
    delete[] HostBuffer;
    clReleaseEvent(WriteEvent);
    CL_RETURN_ON_FAILURE(ClErr);
  }

  if (phEvent) {
    *phEvent = new ur_event_handle_t_(WriteEvent, hQueue->Context, hQueue);
  } else {
    CL_RETURN_ON_FAILURE(clReleaseEvent(WriteEvent));
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemImageRead(
    ur_queue_handle_t hQueue, ur_mem_handle_t hImage, bool blockingRead,
    ur_rect_offset_t origin, ur_rect_region_t region, size_t rowPitch,
    size_t slicePitch, void *pDst, uint32_t numEventsInWaitList,
    const ur_event_handle_t *phEventWaitList, ur_event_handle_t *phEvent) {
  const size_t Origin[3] = {origin.x, origin.y, origin.z};
  const size_t Region[3] = {region.width, region.height, region.depth};
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueReadImage(
      hQueue->get(), hImage->get(), blockingRead, Origin, Region, rowPitch,
      slicePitch, pDst, numEventsInWaitList, CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemImageWrite(
    ur_queue_handle_t hQueue, ur_mem_handle_t hImage, bool blockingWrite,
    ur_rect_offset_t origin, ur_rect_region_t region, size_t rowPitch,
    size_t slicePitch, void *pSrc, uint32_t numEventsInWaitList,
    const ur_event_handle_t *phEventWaitList, ur_event_handle_t *phEvent) {
  const size_t Origin[3] = {origin.x, origin.y, origin.z};
  const size_t Region[3] = {region.width, region.height, region.depth};
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueWriteImage(
      hQueue->get(), hImage->get(), blockingWrite, Origin, Region, rowPitch,
      slicePitch, pSrc, numEventsInWaitList, CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemImageCopy(
    ur_queue_handle_t hQueue, ur_mem_handle_t hImageSrc,
    ur_mem_handle_t hImageDst, ur_rect_offset_t srcOrigin,
    ur_rect_offset_t dstOrigin, ur_rect_region_t region,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {
  const size_t SrcOrigin[3] = {srcOrigin.x, srcOrigin.y, srcOrigin.z};
  const size_t DstOrigin[3] = {dstOrigin.x, dstOrigin.y, dstOrigin.z};
  const size_t Region[3] = {region.width, region.height, region.depth};
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueCopyImage(
      hQueue->get(), hImageSrc->get(), hImageDst->get(), SrcOrigin, DstOrigin,
      Region, numEventsInWaitList, CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferMap(
    ur_queue_handle_t hQueue, ur_mem_handle_t hBuffer, bool blockingMap,
    ur_map_flags_t mapFlags, size_t offset, size_t size,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent, void **ppRetMap) {
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  cl_int Err;
  *ppRetMap = clEnqueueMapBuffer(hQueue->get(), hBuffer->get(), blockingMap,
                                 convertURMapFlagsToCL(mapFlags), offset, size,
                                 numEventsInWaitList, CLWaitEvents.data(),
                                 &Event, &Err);
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return mapCLErrorToUR(Err);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemUnmap(
    ur_queue_handle_t hQueue, ur_mem_handle_t hMem, void *pMappedPtr,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  CL_RETURN_ON_FAILURE(clEnqueueUnmapMemObject(hQueue->get(), hMem->get(),
                                               pMappedPtr, numEventsInWaitList,
                                               CLWaitEvents.data(), &Event));
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueDeviceGlobalVariableWrite(
    ur_queue_handle_t hQueue, ur_program_handle_t hProgram, const char *name,
    bool blockingWrite, size_t count, size_t offset, const void *pSrc,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {

  cl_context Ctx = hQueue->Context->get();

  cl_ext::clEnqueueWriteGlobalVariable_fn F = nullptr;
  cl_int Res = cl_ext::getExtFuncFromContext<decltype(F)>(
      Ctx, cl_ext::ExtFuncPtrCache->clEnqueueWriteGlobalVariableCache,
      cl_ext::EnqueueWriteGlobalVariableName, &F);

  if (!F || Res != CL_SUCCESS)
    return UR_RESULT_ERROR_INVALID_OPERATION;
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  Res = F(hQueue->get(), hProgram->get(), name, blockingWrite, count, offset,
          pSrc, numEventsInWaitList, CLWaitEvents.data(), &Event);
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return mapCLErrorToUR(Res);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueDeviceGlobalVariableRead(
    ur_queue_handle_t hQueue, ur_program_handle_t hProgram, const char *name,
    bool blockingRead, size_t count, size_t offset, void *pDst,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {

  cl_context Ctx = hQueue->Context->get();

  cl_ext::clEnqueueReadGlobalVariable_fn F = nullptr;
  cl_int Res = cl_ext::getExtFuncFromContext<decltype(F)>(
      Ctx, cl_ext::ExtFuncPtrCache->clEnqueueReadGlobalVariableCache,
      cl_ext::EnqueueReadGlobalVariableName, &F);

  if (!F || Res != CL_SUCCESS)
    return UR_RESULT_ERROR_INVALID_OPERATION;
  cl_event Event;
  std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
  for (uint32_t i = 0; i < numEventsInWaitList; i++) {
    CLWaitEvents[i] = phEventWaitList[i]->get();
  }
  Res = F(hQueue->get(), hProgram->get(), name, blockingRead, count, offset,
          pDst, numEventsInWaitList, CLWaitEvents.data(), &Event);
  if (phEvent) {
    *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
  }
  return mapCLErrorToUR(Res);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueReadHostPipe(
    ur_queue_handle_t hQueue, ur_program_handle_t hProgram,
    const char *pipe_symbol, bool blocking, void *pDst, size_t size,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {

  cl_context CLContext = hQueue->Context->get();

  cl_ext::clEnqueueReadHostPipeINTEL_fn FuncPtr = nullptr;
  ur_result_t RetVal =
      cl_ext::getExtFuncFromContext<cl_ext::clEnqueueReadHostPipeINTEL_fn>(
          CLContext, cl_ext::ExtFuncPtrCache->clEnqueueReadHostPipeINTELCache,
          cl_ext::EnqueueReadHostPipeName, &FuncPtr);

  if (FuncPtr) {
    cl_event Event;
    std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
    for (uint32_t i = 0; i < numEventsInWaitList; i++) {
      CLWaitEvents[i] = phEventWaitList[i]->get();
    }
    RetVal = mapCLErrorToUR(FuncPtr(hQueue->get(), hProgram->get(), pipe_symbol,
                                    blocking, pDst, size, numEventsInWaitList,
                                    CLWaitEvents.data(), &Event));
    if (phEvent) {
      *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
    }
  }

  return RetVal;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueWriteHostPipe(
    ur_queue_handle_t hQueue, ur_program_handle_t hProgram,
    const char *pipe_symbol, bool blocking, void *pSrc, size_t size,
    uint32_t numEventsInWaitList, const ur_event_handle_t *phEventWaitList,
    ur_event_handle_t *phEvent) {

  cl_context CLContext = hQueue->Context->get();

  cl_ext::clEnqueueWriteHostPipeINTEL_fn FuncPtr = nullptr;
  ur_result_t RetVal =
      cl_ext::getExtFuncFromContext<cl_ext::clEnqueueWriteHostPipeINTEL_fn>(
          CLContext, cl_ext::ExtFuncPtrCache->clEnqueueWriteHostPipeINTELCache,
          cl_ext::EnqueueWriteHostPipeName, &FuncPtr);

  if (FuncPtr) {
    cl_event Event;
    std::vector<cl_event> CLWaitEvents(numEventsInWaitList);
    for (uint32_t i = 0; i < numEventsInWaitList; i++) {
      CLWaitEvents[i] = phEventWaitList[i]->get();
    }
    RetVal = mapCLErrorToUR(FuncPtr(hQueue->get(), hProgram->get(), pipe_symbol,
                                    blocking, pSrc, size, numEventsInWaitList,
                                    CLWaitEvents.data(), &Event));
    if (phEvent) {
      *phEvent = new ur_event_handle_t_(Event, hQueue->Context, hQueue);
    }
  }

  return RetVal;
}
