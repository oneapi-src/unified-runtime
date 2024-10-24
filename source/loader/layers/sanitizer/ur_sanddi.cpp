/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file ur_sanddi.cpp
 *
 */

#include "asan/asan_ddi.hpp"
#include "asan/asan_interceptor.hpp"
#include "ur_sanitizer_layer.hpp"

#include <memory>

namespace ur_sanitizer_layer {

ur_result_t context_t::init(ur_dditable_t *dditable,
                            const std::set<std::string> &enabledLayerNames,
                            [[maybe_unused]] codeloc_data codelocData) {
    ur_result_t result = UR_RESULT_SUCCESS;

    if (enabledLayerNames.count("UR_LAYER_ASAN")) {
        enabledType = SanitizerType::AddressSanitizer;
    } else if (enabledLayerNames.count("UR_LAYER_MSAN")) {
        enabledType = SanitizerType::MemorySanitizer;
    } else if (enabledLayerNames.count("UR_LAYER_TSAN")) {
        enabledType = SanitizerType::ThreadSanitizer;
    }

    // Only support AddressSanitizer now
    if (enabledType != SanitizerType::AddressSanitizer) {
        return result;
    }

    urDdiTable = *dditable;

    initAsanInterceptor();
    result = asan_ddi_init(dditable);

    return result;
}

} // namespace ur_sanitizer_layer
