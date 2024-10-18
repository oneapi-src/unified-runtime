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
  native_type CLProgram;
  ur_context_handle_t Context;
  std::atomic<uint32_t> RefCount = 0;
  bool IsNativeHandleOwned = true;

  ur_program_handle_t_(native_type Prog, ur_context_handle_t Ctx)
      : CLProgram(Prog), Context(Ctx) {
    RefCount = 1;
    urContextRetain(Context);
  }

  ~ur_program_handle_t_() {
    urContextRelease(Context);
    if (IsNativeHandleOwned) {
      clReleaseProgram(CLProgram);
    }
  }

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }

  static ur_result_t makeWithNative(native_type NativeProg,
                                    ur_context_handle_t Context,
                                    ur_program_handle_t &Program);
};
