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
#include "context.hpp"

#include <vector>

struct ur_program_handle_t_ {
  using native_type = cl_program;
  native_type Program;
  ur_context_handle_t Context;
  std::atomic<uint32_t> RefCount = 0;

  ur_program_handle_t_(native_type Prog, ur_context_handle_t Ctx)
      : Program(Prog), Context(Ctx) {
    RefCount = 1;
    urContextRetain(Context);
  }

  ~ur_program_handle_t_() {
    clReleaseProgram(Program);
    urContextRelease(Context);
  }

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }

  static ur_result_t makeWithNative(native_type NativeProg,
                                    ur_context_handle_t Context,
                                    ur_program_handle_t &Program) {
    if (!Context) {
      return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    try {
      cl_context CLContext;
      CL_RETURN_ON_FAILURE(clGetProgramInfo(NativeProg, CL_PROGRAM_CONTEXT,
                                            sizeof(CLContext), &CLContext,
                                            nullptr));
      if (Context->get() != CLContext) {
        return UR_RESULT_ERROR_INVALID_CONTEXT;
      }
      auto URProgram =
          std::make_unique<ur_program_handle_t_>(NativeProg, Context);
      Program = URProgram.release();
    } catch (std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }

    return UR_RESULT_SUCCESS;
  }

  native_type get() { return Program; }
};
