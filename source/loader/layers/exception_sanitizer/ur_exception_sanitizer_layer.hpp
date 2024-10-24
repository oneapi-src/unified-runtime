/*
 *
 * Copyright (C) 2023 Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file ur_exception_sanitizer_layer.h
 *
 */

#ifndef UR_EXCEPTION_SANITIZER_LAYER_H
#define UR_EXCEPTION_SANITIZER_LAYER_H 1

#include "ur_ddi.h"
#include "ur_proxy_layer.hpp"
#include "ur_util.hpp"

#define EXCEPTION_SANITIZER_COMP_NAME "exception sanitizer layer"

namespace ur_exception_sanitizer_layer {

///////////////////////////////////////////////////////////////////////////////
class __urdlllocal context_t : public proxy_layer_context_t,
                               public AtomicSingleton<context_t> {
  public:
    ur_dditable_t urDdiTable = {};
    codeloc_data codelocData;

    context_t() = default;
    ~context_t() = default;

    ur_result_t init(ur_dditable_t *dditable,
                     const std::set<std::string> &enabledLayerNames,
                     codeloc_data codelocData) override;
    ur_result_t tearDown() override { return UR_RESULT_SUCCESS; }
    static std::vector<std::string> getNames() {
        return {"UR_LAYER_EXCEPTION_SANITIZER"};
    }
};

context_t *getContext();

} // namespace ur_exception_sanitizer_layer

#endif /* UR_EXCEPTION_SANITIZER_LAYER_H */
