//===--------- adapter.cpp - Libomptarget Adapter --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

#include "adapter.hpp"

ur_adapter_handle_t_ adapter{};

UR_APIEXPORT ur_result_t UR_APICALL
urAdapterGet(uint32_t NumEntries, ur_adapter_handle_t *phAdapters,
             uint32_t *pNumAdapters) {
  if (NumEntries > 0 && phAdapters) {
    adapter.RefCount++;
    *phAdapters = &adapter;
  }

  if (pNumAdapters) {
    *pNumAdapters = 1;
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urAdapterRetain([[maybe_unused]] ur_adapter_handle_t) {
  std::lock_guard<std::mutex> Lock{adapter.Mutex};
  if (++adapter.RefCount == 1) {
    OMPT_RETURN_ON_FAILURE(__tgt_rtl_init_plugin());
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urAdapterRelease([[maybe_unused]] ur_adapter_handle_t) {
  std::lock_guard<std::mutex> Lock{adapter.Mutex};
  if (--adapter.RefCount == 0) {
    OMPT_RETURN_ON_FAILURE(__tgt_rtl_deinit_plugin());
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urAdapterGetLastError([[maybe_unused]] ur_adapter_handle_t,
                      const char **ppMessage, int32_t *pError) {
  *ppMessage = omptarget_adapter::ErrorMessage;
  *pError = omptarget_adapter::ErrorMessageCode;

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urAdapterGetInfo(
    [[maybe_unused]] ur_adapter_handle_t, ur_adapter_info_t propName,
    size_t propSize, void *pPropValue, size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);

  switch (propName) {
  case UR_ADAPTER_INFO_BACKEND:
    /* Using UNKNOWN since there is no libomptarget enum at the moment */
    return ReturnValue(UR_ADAPTER_BACKEND_CUDA);
  case UR_ADAPTER_INFO_REFERENCE_COUNT:
    return ReturnValue(adapter.RefCount.load());
  default:
    return UR_RESULT_ERROR_INVALID_ENUMERATION;
  }

  return UR_RESULT_SUCCESS;
}
