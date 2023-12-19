//===--------- program.hpp - OpenCL Adapter ---------------------------===//
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

struct ur_program_handle_t_ {
  using native_type = cl_program;
  native_type Program;
  ur_context_handle_t Context;

  ur_program_handle_t_(native_type Prog, ur_context_handle_t Ctx)
      : Program(Prog), Context(Ctx) {}

  ~ur_program_handle_t_() {}

  ur_result_t initWithNative() {
    if (!Context) {
      cl_context CLContext;
      CL_RETURN_ON_FAILURE(clGetProgramInfo(
          Program, CL_PROGRAM_CONTEXT, sizeof(CLContext), &CLContext, nullptr));
      ur_native_handle_t NativeContext =
          reinterpret_cast<ur_native_handle_t>(CLContext);
      UR_RETURN_ON_FAILURE(urContextCreateWithNativeHandle(
          NativeContext, 0, nullptr, nullptr, &Context));
    }
    return UR_RESULT_SUCCESS;
  }

  native_type get() { return Program; }
};
