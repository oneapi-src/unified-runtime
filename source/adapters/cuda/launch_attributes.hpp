//===--------- launch_attributes.hpp - CUDA Adapter -----------------------===//
//
// Copyright (C) 2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

#include <ur/ur.hpp>

#include "common.hpp"

struct ur_exp_launch_attribute_handle_t_ {
private:
  using native_type = CUlaunchAttribute;

  native_type CuLaunchAttribute;

public:
  ur_exp_launch_attribute_handle_t_() {}

  ~ur_exp_launch_attribute_handle_t_() {}

  native_type get() const noexcept { return CuLaunchAttribute; };
};
// todo check attribute req here?
