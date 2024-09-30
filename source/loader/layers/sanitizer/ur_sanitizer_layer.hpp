/*
 *
 * Copyright (C) 2023 Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file ur_sanitizer_layer.hpp
 *
 */

#pragma once

#include "ur/ur.hpp"
#include "ur_proxy_layer.hpp"

#include "logger/ur_logger.hpp"

#define SANITIZER_COMP_NAME "sanitizer layer"

namespace ur_sanitizer_layer {

enum class SanitizerType {
    None,
    AddressSanitizer,
    MemorySanitizer,
    ThreadSanitizer,
};

class SanitizerInterceptor;
struct AsanOptions;

///////////////////////////////////////////////////////////////////////////////
class __urdlllocal context_t : public proxy_layer_context_t,
                               public AtomicSingleton<context_t> {
  public:
    ur_dditable_t urDdiTable = {};
    logger::Logger logger;
    std::unique_ptr<SanitizerInterceptor> interceptor;
    SanitizerType enabledType = SanitizerType::None;
    std::unique_ptr<AsanOptions> asanOptions;

    context_t();
    ~context_t();

    static std::vector<std::string> getNames() {
        return {"UR_LAYER_ASAN", "UR_LAYER_MSAN", "UR_LAYER_TSAN"};
    }
    ur_result_t init(ur_dditable_t *dditable,
                     const std::set<std::string> &enabledLayerNames,
                     codeloc_data codelocData) override;

    ur_result_t tearDown() override;
};

context_t *getContext();
} // namespace ur_sanitizer_layer
