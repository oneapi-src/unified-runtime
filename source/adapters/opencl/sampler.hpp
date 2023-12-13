//===--------- sampler.hpp - OpenCL Adapter ---------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

#include "common.hpp"

#include <vector>

struct ur_sampler_handle_t_ {
  using native_type = cl_sampler;
  native_type Sampler;
  ur_context_handle_t Context;

  ur_sampler_handle_t_(native_type Sampler, ur_context_handle_t Ctx)
      : Sampler(Sampler), Context(Ctx) {}

  ~ur_sampler_handle_t_() {}

  native_type get() { return Sampler; }
};
