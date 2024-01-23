//===--------- enqueue.cpp - Libomptarget Adapter ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

#include "common.hpp"
#include "event.hpp"
#include "kernel.hpp"
#include "memory.hpp"
#include "queue.hpp"
#include <omptargetplugin.h>

// NOTE: The CUDA adapter equivalent has an optimization where it only waits on
// the newest event on each stream (because they're in-order). This doesn't
// bother with that - also it might not be portable across all libomptarget
// plugins.
ur_result_t enqueueEventsWait(ur_queue_handle_t Queue,
                              uint32_t NumEventsInWaitList,
                              const ur_event_handle_t *EventWaitList) {
  auto *AsyncInfo = Queue->AsyncInfo;
  int32_t DeviceID = Queue->getDevice()->getID();
  for (size_t Idx = 0; Idx < NumEventsInWaitList; Idx++) {
    auto *Event = EventWaitList[Idx];
    // This should enqueue a wait on the queue without blocking on it
    auto Res = __tgt_rtl_wait_event(DeviceID, Event->Event, AsyncInfo);
    // MISSING FUNCTIONALITY:
    // * The only possible error code is OFFLOAD_FAILURE. We have no idea if
    //   that means there's a problem with the events, the queue, or something
    //   else. Also, bailing out after enqueueing only some of the waits could
    //   leave things in a dodgy state, but there's not much else we can do.
    if (Res != OFFLOAD_SUCCESS) {
      return UR_RESULT_ERROR_INVALID_EVENT_WAIT_LIST;
    }
  }
  return UR_RESULT_SUCCESS;
}

// MISSING FUNCTIONALITY:
// * Work dimensions > 1 are NOT supported, which will prevent a huge range of
//   SYCL applications from working.
UR_APIEXPORT ur_result_t UR_APICALL
urEnqueueKernelLaunch([[maybe_unused]] ur_queue_handle_t hQueue,
                      [[maybe_unused]] ur_kernel_handle_t hKernel,
                      [[maybe_unused]] uint32_t workDim,
                      [[maybe_unused]] const size_t *pGlobalWorkOffset,
                      [[maybe_unused]] const size_t *pGlobalWorkSize,
                      [[maybe_unused]] const size_t *pLocalWorkSize,
                      [[maybe_unused]] uint32_t numEventsInWaitList,
                      [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
                      [[maybe_unused]] ur_event_handle_t *phEvent) {
  ur_result_t Result = UR_RESULT_SUCCESS;
  // TODO: check this doesn't filter out explict Nx1x1 sizes
  UR_ASSERT(workDim == 1, UR_RESULT_ERROR_INVALID_WORK_DIMENSION);

  if (*pGlobalWorkSize == 0) {
    return urEnqueueEventsWaitWithBarrier(hQueue, numEventsInWaitList,
                                          phEventWaitList, phEvent);
  }

  // Enqueue wait list before the kernel
  Result = enqueueEventsWait(hQueue, numEventsInWaitList, phEventWaitList);

  int64_t DeviceID = hQueue->getDevice()->getID();
  auto Function = hKernel->Function;
  auto NumArgs = hKernel->getNumArgs();

  // Set the implicit global offset parameter if kernel has offset variant
  if (hKernel->FunctionWithOffsetParam) {
    std::uint32_t CudaImplicitOffset[3] = {0, 0, 0};
    if (pGlobalWorkOffset) {
      for (size_t i = 0; i < workDim; i++) {
        CudaImplicitOffset[i] =
            static_cast<std::uint32_t>(pGlobalWorkOffset[i]);
        if (pGlobalWorkOffset[i] != 0) {
          Function = hKernel->FunctionWithOffsetParam;
        }
      }
    }
    NumArgs++;
    hKernel->setImplicitOffsetArg(sizeof(CudaImplicitOffset),
                                  CudaImplicitOffset);
  }

  // Not all the KernelArgs are used by the CUDA plugin; for now just populate
  // the ones that are used
  KernelArgsTy LaunchArgs{};
  LaunchArgs.DynCGroupMem = hKernel->getLocalSize();
  if (pLocalWorkSize) {
    // Use the user specified local size
    // TODO: Probably can do more to verify the sizes
    LaunchArgs.NumTeams[0] = pGlobalWorkSize[0] / pLocalWorkSize[0];
    LaunchArgs.ThreadLimit[0] = pLocalWorkSize[0];
    LaunchArgs.NumTeams[1] = 0;
    LaunchArgs.NumTeams[2] = 0;
  } else {
    // Let the libomptarget plugin choose the block size
    // NOTE: We don't know whether the plugin's heuristic is better or worse
    // than the cuda adapter
    LaunchArgs.ThreadLimit[0] = 0;
    LaunchArgs.NumTeams[0] = 0;
    LaunchArgs.NumTeams[1] = 0;
    LaunchArgs.NumTeams[2] = 0;
  }
  LaunchArgs.Tripcount = pGlobalWorkSize[0];
  LaunchArgs.ThreadLimit[1] = 0;
  LaunchArgs.ThreadLimit[2] = 0;
  LaunchArgs.NumArgs = NumArgs;

  // The libomptarget plugin takes offsets to apply to each argument - we don't
  // need to add offsets so we can just pass a vector of zeroes
  auto ArgOffsets = std::vector<ptrdiff_t>(LaunchArgs.NumArgs, 0ul);

  ur_event_handle_t Event;
  if (phEvent) {
    Event = ur_event_handle_t_::makeEvent(
        hQueue, hQueue->Context, ur_command_t::UR_COMMAND_KERNEL_LAUNCH);
    if (!Event) {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    }
    Event->start();
  }

  auto Res = __tgt_rtl_launch_kernel(
      DeviceID, Function, (void **)hKernel->Args.getStorage(),
      ArgOffsets.data(), &LaunchArgs, hQueue->AsyncInfo);

  // MISSING FUNCTIONALITY: We don't know what the actual underlying error is so
  // have to just pick one arbitrarily
  if (Res != OFFLOAD_SUCCESS) {
    return UR_RESULT_ERROR_INVALID_KERNEL;
  }

  if (phEvent) {
    Event->record();
    *phEvent = Event;
  }

  return Result;
}

UR_APIEXPORT ur_result_t UR_APICALL
urEnqueueEventsWait([[maybe_unused]] ur_queue_handle_t hQueue,
                    [[maybe_unused]] uint32_t numEventsInWaitList,
                    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
                    [[maybe_unused]] ur_event_handle_t *phEvent) {

  ur_event_handle_t Event = nullptr;
  if (phEvent) {
    Event = ur_event_handle_t_::makeEvent(hQueue, hQueue->Context,
                                          UR_COMMAND_EVENTS_WAIT);
    if (Event) {
      Event->start();
    } else {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    }
  }

  auto Res = enqueueEventsWait(hQueue, numEventsInWaitList, phEventWaitList);

  if (Res == UR_RESULT_SUCCESS && phEvent) {
    Event->record();
    *phEvent = Event;
  }

  return Res;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueEventsWaitWithBarrier(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  return urEnqueueEventsWait(hQueue, numEventsInWaitList, phEventWaitList,
                             phEvent);
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferRead(
    ur_queue_handle_t hQueue, ur_mem_handle_t hBuffer, bool blockingRead,
    size_t offset, size_t size, void *pDst, uint32_t numEventsInWaitList,
    const ur_event_handle_t *phEventWaitList, ur_event_handle_t *phEvent) {
  ur_result_t Result = UR_RESULT_SUCCESS;

  Result = enqueueEventsWait(hQueue, numEventsInWaitList, phEventWaitList);

  ur_event_handle_t Event = nullptr;
  if (phEvent) {
    Event = ur_event_handle_t_::makeEvent(hQueue, hQueue->Context,
                                          UR_COMMAND_MEM_BUFFER_READ);
    UR_ASSERT(Event, UR_RESULT_ERROR_OUT_OF_RESOURCES);
    Event->start();
  }

  if (blockingRead) {
    __tgt_rtl_data_retrieve(hQueue->getDevice()->getID(), pDst,
                            (char *)hBuffer->Ptr + offset, size);
  } else {
    auto AsyncInfo = hQueue->AsyncInfo;
    __tgt_rtl_data_retrieve_async(hQueue->getDevice()->getID(), pDst,
                                  (char *)hBuffer->Ptr + offset, size,
                                  AsyncInfo);
  }

  if (phEvent) {
    Event->record();
    *phEvent = Event;
  }

  return Result;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferWrite(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_mem_handle_t hBuffer,
    [[maybe_unused]] bool blockingWrite, [[maybe_unused]] size_t offset,
    [[maybe_unused]] size_t size, [[maybe_unused]] const void *pSrc,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  ur_result_t Result = UR_RESULT_SUCCESS;

  Result = enqueueEventsWait(hQueue, numEventsInWaitList, phEventWaitList);

  ur_event_handle_t Event = nullptr;
  if (phEvent) {
    Event = ur_event_handle_t_::makeEvent(hQueue, hQueue->Context,
                                          UR_COMMAND_MEM_BUFFER_READ);
    UR_ASSERT(Event, UR_RESULT_ERROR_OUT_OF_RESOURCES);
    Event->start();
  }

  if (blockingWrite) {
    __tgt_rtl_data_submit(hQueue->getDevice()->getID(),
                          (char *)hBuffer->Ptr + offset,
                          const_cast<void *>(pSrc), size);
  } else {
    auto AsyncInfo = hQueue->AsyncInfo;
    __tgt_rtl_data_submit_async(hQueue->getDevice()->getID(),
                                (char *)hBuffer->Ptr + offset,
                                const_cast<void *>(pSrc), size, AsyncInfo);
  }

  if (phEvent) {
    Event->record();
    *phEvent = Event;
  }

  return Result;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferReadRect(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_mem_handle_t hBuffer,
    [[maybe_unused]] bool blockingRead,
    [[maybe_unused]] ur_rect_offset_t bufferOrigin,
    [[maybe_unused]] ur_rect_offset_t hostOrigin,
    [[maybe_unused]] ur_rect_region_t region,
    [[maybe_unused]] size_t bufferRowPitch,
    [[maybe_unused]] size_t bufferSlicePitch,
    [[maybe_unused]] size_t hostRowPitch,
    [[maybe_unused]] size_t hostSlicePitch, [[maybe_unused]] void *pDst,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferWriteRect(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_mem_handle_t hBuffer,
    [[maybe_unused]] bool blockingWrite,
    [[maybe_unused]] ur_rect_offset_t bufferOrigin,
    [[maybe_unused]] ur_rect_offset_t hostOrigin,
    [[maybe_unused]] ur_rect_region_t region,
    [[maybe_unused]] size_t bufferRowPitch,
    [[maybe_unused]] size_t bufferSlicePitch,
    [[maybe_unused]] size_t hostRowPitch,
    [[maybe_unused]] size_t hostSlicePitch, [[maybe_unused]] void *pSrc,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferCopy(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_mem_handle_t hBufferSrc,
    [[maybe_unused]] ur_mem_handle_t hBufferDst,
    [[maybe_unused]] size_t srcOffset, [[maybe_unused]] size_t dstOffset,
    [[maybe_unused]] size_t size, [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  ur_result_t Result = UR_RESULT_SUCCESS;

  Result = enqueueEventsWait(hQueue, numEventsInWaitList, phEventWaitList);

  ur_event_handle_t Event = nullptr;
  if (phEvent) {
    Event = ur_event_handle_t_::makeEvent(hQueue, hQueue->Context,
                                          UR_COMMAND_MEM_BUFFER_COPY);
    UR_ASSERT(Event, UR_RESULT_ERROR_OUT_OF_RESOURCES);
    Event->start();
  }

  // auto AsyncInfo = hQueue->AsyncInfo;
  // auto DeviceID = hQueue->getDevice()->getID();
  // auto Err = __tgt_rtl_data_exchange(DeviceID, hBufferSrc->Ptr + srcOffset,
  // DeviceID,
  //                                    hBufferDst->Ptr + dstOffset, size);
  // auto Err = __tgt_rtl_data_submit_async(
  //     hQueue->getDevice()->getID(), (char *) hBuffer->Ptr + offset,
  //     const_cast<void *>(pSrc), size, AsyncInfo);

  if (phEvent) {
    Event->record();
    *phEvent = Event;
  }

  return Result;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferCopyRect(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_mem_handle_t hBufferSrc,
    [[maybe_unused]] ur_mem_handle_t hBufferDst,
    [[maybe_unused]] ur_rect_offset_t srcOrigin,
    [[maybe_unused]] ur_rect_offset_t dstOrigin,
    [[maybe_unused]] ur_rect_region_t region,
    [[maybe_unused]] size_t srcRowPitch, [[maybe_unused]] size_t srcSlicePitch,
    [[maybe_unused]] size_t dstRowPitch, [[maybe_unused]] size_t dstSlicePitch,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferFill(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_mem_handle_t hBuffer,
    [[maybe_unused]] const void *pPattern, [[maybe_unused]] size_t patternSize,
    [[maybe_unused]] size_t offset, [[maybe_unused]] size_t size,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemImageRead(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_mem_handle_t hImage, [[maybe_unused]] bool blockingRead,
    [[maybe_unused]] ur_rect_offset_t origin,
    [[maybe_unused]] ur_rect_region_t region, [[maybe_unused]] size_t rowPitch,
    [[maybe_unused]] size_t slicePitch, [[maybe_unused]] void *pDst,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemImageWrite(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_mem_handle_t hImage,
    [[maybe_unused]] bool blockingWrite,
    [[maybe_unused]] ur_rect_offset_t origin,
    [[maybe_unused]] ur_rect_region_t region, [[maybe_unused]] size_t rowPitch,
    [[maybe_unused]] size_t slicePitch, [[maybe_unused]] void *pSrc,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL
urEnqueueMemImageCopy([[maybe_unused]] ur_queue_handle_t hQueue,
                      [[maybe_unused]] ur_mem_handle_t hImageSrc,
                      [[maybe_unused]] ur_mem_handle_t hImageDst,
                      [[maybe_unused]] ur_rect_offset_t srcOrigin,
                      [[maybe_unused]] ur_rect_offset_t dstOrigin,
                      [[maybe_unused]] ur_rect_region_t region,
                      [[maybe_unused]] uint32_t numEventsInWaitList,
                      [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
                      [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemBufferMap(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_mem_handle_t hBuffer, [[maybe_unused]] bool blockingMap,
    [[maybe_unused]] ur_map_flags_t mapFlags, [[maybe_unused]] size_t offset,
    [[maybe_unused]] size_t size, [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent,
    [[maybe_unused]] void **ppRetMap) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueMemUnmap(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_mem_handle_t hMem, [[maybe_unused]] void *pMappedPtr,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueDeviceGlobalVariableWrite(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_program_handle_t hProgram,
    [[maybe_unused]] const char *name, [[maybe_unused]] bool blockingWrite,
    [[maybe_unused]] size_t count, [[maybe_unused]] size_t offset,
    [[maybe_unused]] const void *pSrc,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueDeviceGlobalVariableRead(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_program_handle_t hProgram,
    [[maybe_unused]] const char *name, [[maybe_unused]] bool blockingRead,
    [[maybe_unused]] size_t count, [[maybe_unused]] size_t offset,
    [[maybe_unused]] void *pDst, [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueReadHostPipe(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_program_handle_t hProgram,
    [[maybe_unused]] const char *pipe_symbol, [[maybe_unused]] bool blocking,
    [[maybe_unused]] void *pDst, [[maybe_unused]] size_t size,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urEnqueueWriteHostPipe(
    [[maybe_unused]] ur_queue_handle_t hQueue,
    [[maybe_unused]] ur_program_handle_t hProgram,
    [[maybe_unused]] const char *pipe_symbol, [[maybe_unused]] bool blocking,
    [[maybe_unused]] void *pSrc, [[maybe_unused]] size_t size,
    [[maybe_unused]] uint32_t numEventsInWaitList,
    [[maybe_unused]] const ur_event_handle_t *phEventWaitList,
    [[maybe_unused]] ur_event_handle_t *phEvent) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
