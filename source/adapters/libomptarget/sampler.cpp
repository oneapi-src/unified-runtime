//===--------- sampler.cpp - Libomptarget Adapter --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

#include "common.hpp"

ur_result_t urSamplerCreate([[maybe_unused]] ur_context_handle_t hContext,
                            [[maybe_unused]] const ur_sampler_desc_t *pDesc,
                            [[maybe_unused]] ur_sampler_handle_t *phSampler) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urSamplerGetInfo(
    [[maybe_unused]] ur_sampler_handle_t hSampler,
    [[maybe_unused]] ur_sampler_info_t propName,
    [[maybe_unused]] size_t propSize, [[maybe_unused]] void *pPropValue,
    [[maybe_unused]] size_t *pPropSizeRet) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL
urSamplerRetain([[maybe_unused]] ur_sampler_handle_t hSampler) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL
urSamplerRelease([[maybe_unused]] ur_sampler_handle_t hSampler) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL
urSamplerGetNativeHandle([[maybe_unused]] ur_sampler_handle_t hSampler,
                         [[maybe_unused]] ur_native_handle_t *phNativeSampler) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urSamplerCreateWithNativeHandle(
    [[maybe_unused]] ur_native_handle_t hNativeSampler,
    [[maybe_unused]] ur_context_handle_t,
    [[maybe_unused]] const ur_sampler_native_properties_t *,
    [[maybe_unused]] ur_sampler_handle_t *phSampler) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
