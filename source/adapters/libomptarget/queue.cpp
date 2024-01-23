//===--------- queue.cpp - Libomptarget Adapter ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

#include "queue.hpp"
#include "common.hpp"
#include "device.hpp"

// MISSING FUNCTIONALITY:
// With the CUDA plugin, each async_info struct contains a single CUstream.
// Because CUDA streams are in-order, we cannot support an out-of-order queue.
// We could copy the CUDA adapter and implement an out-of-order queue using
// lots of in-order queues - it's not clear that this would work well for other
// plugins, and generally the abstraction of libomptarget's async info does not
// seem well suited to that.
UR_APIEXPORT ur_result_t UR_APICALL urQueueCreate(
    ur_context_handle_t hContext, ur_device_handle_t hDevice,
    const ur_queue_properties_t *pProperties, ur_queue_handle_t *phQueue) {
  // The async_info struct contains the underlying queue-like resource (e.g.
  // CUstream for CUDA)
  __tgt_async_info *AsyncInfo;
  int32_t DeviceID = hDevice->getID();
  auto Result = __tgt_rtl_init_async_info(DeviceID, &AsyncInfo);
  if (Result != OFFLOAD_SUCCESS) {
    return UR_RESULT_ERROR_OUT_OF_RESOURCES;
  }

  // Save the flags so they can be queried later, but we don't actually do
  // anything meaningful with them
  auto Flags = pProperties ? pProperties->flags : 0;

  *phQueue = new ur_queue_handle_t_(hContext, hDevice, AsyncInfo, Flags);

  return UR_RESULT_SUCCESS;
}

// MISSING FUNCTIONALITY:
// * No obvious way to implement UR_QUEUE_INFO_SIZE
UR_APIEXPORT ur_result_t UR_APICALL urQueueGetInfo(ur_queue_handle_t hQueue,
                                                   ur_queue_info_t propName,
                                                   size_t propValueSize,
                                                   void *pPropValue,
                                                   size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propValueSize, pPropValue, pPropSizeRet);

  switch (propName) {
  case UR_QUEUE_INFO_CONTEXT:
    return ReturnValue(hQueue->Context);
  case UR_QUEUE_INFO_DEVICE:
    return ReturnValue(hQueue->Device);
  case UR_QUEUE_INFO_REFERENCE_COUNT:
    return ReturnValue(hQueue->getReferenceCount());
  case UR_QUEUE_INFO_FLAGS:
    return ReturnValue(hQueue->Flags);
  case UR_QUEUE_INFO_EMPTY: {
    // TODO: In theory the below can be used to query the queue progress, but
    // it clears the underlying CUstream upon completion, so it's not clear
    // what weird side effects it might have. Don't implement it for now.
    //__tgt_rtl_query_async(0, hQueue->AsyncInfo);
    return UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
  }
  default:
    return UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
  }

  return UR_RESULT_ERROR_INVALID_ENUMERATION;
}

UR_APIEXPORT ur_result_t UR_APICALL
urQueueGetNativeHandle([[maybe_unused]] ur_queue_handle_t hQueue,
                       [[maybe_unused]] ur_queue_native_desc_t *,
                       [[maybe_unused]] ur_native_handle_t *phNativeQueue) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueCreateWithNativeHandle(
    [[maybe_unused]] ur_native_handle_t hNativeQueue,
    [[maybe_unused]] ur_context_handle_t hContext,
    [[maybe_unused]] ur_device_handle_t hDevice,
    [[maybe_unused]] const ur_queue_native_properties_t *pProperties,
    [[maybe_unused]] ur_queue_handle_t *phQueue) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueFinish(ur_queue_handle_t hQueue) {
  int32_t DeviceID = hQueue->getDevice()->getID();
  auto Result = __tgt_rtl_synchronize(DeviceID, hQueue->AsyncInfo);
  // If the queue completed, the underlying async_info resource is released.
  // Create it again so the queue can be reused. This may be very ineffecient
  // compared to reusing the same queue resource.
  if (Result == OFFLOAD_SUCCESS) {
    Result = __tgt_rtl_init_async_info(DeviceID, &hQueue->AsyncInfo);
  }
  return Result ? UR_RESULT_ERROR_OUT_OF_RESOURCES : UR_RESULT_SUCCESS;
}

// TODO: This is a nop on CUDA, need to check this is still the case with
// libomptarget.
UR_APIEXPORT ur_result_t UR_APICALL
urQueueFlush([[maybe_unused]] ur_queue_handle_t hQueue) {
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urQueueRetain(ur_queue_handle_t hQueue) {
  hQueue->incrementReferenceCount();
  return UR_RESULT_SUCCESS;
}

// MISSING FUNCTIONALITY:
// * No way to release the underlying async_info, so the queue may leak memory
//   if it was not finished when released
UR_APIEXPORT ur_result_t UR_APICALL urQueueRelease(ur_queue_handle_t hQueue) {
  if (hQueue->decrementReferenceCount() == 0) {
    delete hQueue;
  }
  return UR_RESULT_SUCCESS;
}
