/*
 *
 * Copyright (C) 2023-2024 Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file ur_layer.h
 *
 */
#pragma once
#include "ur_ddi.h"
#include "ur_proxy_layer.hpp"
#include "ur_util.hpp"

namespace ur_callback_layer {

///////////////////////////////////////////////////////////////////////////////
class __urdlllocal context_t : public proxy_layer_context_t {
  public:
    ur_dditable_t urDdiTable = {};

    context_t() {}
    ~context_t() {}

    bool isAvailable() const override { return true; }
    std::vector<std::string> getNames() const override { return {name}; }
    ur_result_t init(ur_dditable_t *dditable,
                     const std::set<std::string> &enabledLayerNames,
                     codeloc_data codelocData,
                     api_callbacks apiCallbacks) override;
    ur_result_t tearDown() override { return UR_RESULT_SUCCESS; }

    api_callbacks apiCallbacks = {};

  private:
    const std::string name = "UR_LAYER_CALLBACK";
};

extern context_t context;
}; // namespace ur_callback_layer
