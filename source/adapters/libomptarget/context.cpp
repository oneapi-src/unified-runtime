//===--------- context.cpp - Libomptarget Adapter --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

#include "context.hpp"
#include "usm.hpp"
#include <algorithm>
#include <cuda.h>

void ur_context_handle_t_::addPool(ur_usm_pool_handle_t Pool) {
  std::lock_guard ContextLock(Mutex);
  PoolHandles.insert(Pool);
}

void ur_context_handle_t_::removePool(ur_usm_pool_handle_t Pool) {
  std::lock_guard ContextLock(Mutex);
  PoolHandles.erase(Pool);
}

void ur_context_handle_t_::addUSMAllocation(const USMAllocation &AllocInfo) {
  USMAllocations.push_back(AllocInfo);
}

ur_result_t ur_context_handle_t_::removeUSMAllocation(const void *Ptr) {
  std::remove_if(USMAllocations.begin(), USMAllocations.end(),
                 [Ptr](const auto &a) { return Ptr == a.usm_ptr; });
  return UR_RESULT_SUCCESS;
}

ur_result_t ur_context_handle_t_::findUSMAllocation(USMAllocation &AllocInfo,
                                                    const void *Ptr) {
  auto Result =
      std::find_if(std::begin(USMAllocations), std::end(USMAllocations),
                   [Ptr](const auto &a) { return Ptr == a.usm_ptr; });
  if (Result == USMAllocations.end()) {
    return UR_RESULT_ERROR_UNKNOWN;
  }
  AllocInfo = *Result;
  return UR_RESULT_SUCCESS;
}

ur_usm_pool_handle_t
ur_context_handle_t_::getOwningURPool(umf_memory_pool_t *UMFPool) {
  std::lock_guard ContextLock(Mutex);
  for (auto &Pool : PoolHandles) {
    if (Pool->hasUMFPool(UMFPool)) {
      return Pool;
    }
  }
  return nullptr;
}

UR_APIEXPORT ur_result_t UR_APICALL
urContextCreate([[maybe_unused]] uint32_t DeviceCount,
                [[maybe_unused]] const ur_device_handle_t *phDevices,
                [[maybe_unused]] const ur_context_properties_t *,
                [[maybe_unused]] ur_context_handle_t *phContext) {

  std::unique_ptr<ur_context_handle_t_> ContextPtr{nullptr};

  ContextPtr = std::make_unique<ur_context_handle_t_>(*phDevices);
  *phContext = ContextPtr.release();

  return UR_RESULT_SUCCESS;
}

/* This entrypoint is implemented using CUDA since libomptarget does not provide
 * equivalent functionality */
UR_APIEXPORT ur_result_t UR_APICALL urContextGetInfo(
    [[maybe_unused]] ur_context_handle_t hContext,
    [[maybe_unused]] ur_context_info_t propName,
    [[maybe_unused]] size_t propSize, [[maybe_unused]] void *pPropValue,
    [[maybe_unused]] size_t *pPropSizeRet) {

  UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);

  CUdevice NativeDevice;
  OMPT_RETURN_ON_FAILURE(
      cuDeviceGet(&NativeDevice, hContext->getDevice()->getID()));
  CUcontext Context;
  OMPT_RETURN_ON_FAILURE(cuDevicePrimaryCtxRetain(&Context, NativeDevice));
  OMPT_RETURN_ON_FAILURE(cuCtxSetCurrent(Context));

  switch (uint32_t{propName}) {
  case UR_CONTEXT_INFO_NUM_DEVICES:
    return ReturnValue(1);
  case UR_CONTEXT_INFO_DEVICES:
    return ReturnValue(hContext->getDevice());
  case UR_CONTEXT_INFO_REFERENCE_COUNT:
    return ReturnValue(hContext->getReferenceCount());
  case UR_CONTEXT_INFO_ATOMIC_MEMORY_ORDER_CAPABILITIES: {
    uint32_t Capabilities = UR_MEMORY_ORDER_CAPABILITY_FLAG_RELAXED |
                            UR_MEMORY_ORDER_CAPABILITY_FLAG_ACQUIRE |
                            UR_MEMORY_ORDER_CAPABILITY_FLAG_RELEASE |
                            UR_MEMORY_ORDER_CAPABILITY_FLAG_ACQ_REL;
    return ReturnValue(Capabilities);
  }
  case UR_CONTEXT_INFO_ATOMIC_MEMORY_SCOPE_CAPABILITIES: {
    int Major = 0;
    omptarget_adapter::assertion(
        cuDeviceGetAttribute(&Major,
                             CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR,
                             hContext->getDevice()->getID()) == CUDA_SUCCESS);
    uint32_t Capabilities =
        (Major >= 7) ? UR_MEMORY_SCOPE_CAPABILITY_FLAG_WORK_ITEM |
                           UR_MEMORY_SCOPE_CAPABILITY_FLAG_SUB_GROUP |
                           UR_MEMORY_SCOPE_CAPABILITY_FLAG_WORK_GROUP |
                           UR_MEMORY_SCOPE_CAPABILITY_FLAG_DEVICE |
                           UR_MEMORY_SCOPE_CAPABILITY_FLAG_SYSTEM
                     : UR_MEMORY_SCOPE_CAPABILITY_FLAG_WORK_ITEM |
                           UR_MEMORY_SCOPE_CAPABILITY_FLAG_SUB_GROUP |
                           UR_MEMORY_SCOPE_CAPABILITY_FLAG_WORK_GROUP |
                           UR_MEMORY_SCOPE_CAPABILITY_FLAG_DEVICE;
    return ReturnValue(Capabilities);
  }
  case UR_CONTEXT_INFO_USM_MEMCPY2D_SUPPORT:
    // 2D USM memcpy is supported.
    return ReturnValue(true);
  case UR_CONTEXT_INFO_USM_FILL2D_SUPPORT:
    // 2D USM operations currently not supported.
    return ReturnValue(false);

  default:
    break;
  }

  return UR_RESULT_ERROR_INVALID_ENUMERATION;
}

UR_APIEXPORT ur_result_t UR_APICALL
urContextRelease([[maybe_unused]] ur_context_handle_t hContext) {
  hContext->decrementReferenceCount();
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urContextRetain([[maybe_unused]] ur_context_handle_t hContext) {
  hContext->incrementReferenceCount();
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urContextGetNativeHandle([[maybe_unused]] ur_context_handle_t hContext,
                         [[maybe_unused]] ur_native_handle_t *phNativeContext) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urContextCreateWithNativeHandle(
    [[maybe_unused]] ur_native_handle_t hNativeContext,
    [[maybe_unused]] uint32_t, [[maybe_unused]] const ur_device_handle_t *,
    [[maybe_unused]] const ur_context_native_properties_t *,
    [[maybe_unused]] ur_context_handle_t *phContext) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urContextSetExtendedDeleter(
    [[maybe_unused]] ur_context_handle_t hContext,
    [[maybe_unused]] ur_context_extended_deleter_t pfnDeleter,
    [[maybe_unused]] void *pUserData) {
  hContext->setExtendedDeleter(pfnDeleter, pUserData);
  return UR_RESULT_SUCCESS;
}
