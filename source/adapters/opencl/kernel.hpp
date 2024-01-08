//===--------- kernel.hpp - OpenCL Adapter ---------------------------===//
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

struct ur_kernel_handle_t_ {
  using native_type = cl_kernel;
  native_type Kernel;
  ur_program_handle_t Program;
  ur_context_handle_t Context;

  ur_kernel_handle_t_(native_type Kernel, ur_program_handle_t Program,
                      ur_context_handle_t Context)
      : Kernel(Kernel), Program(Program), Context(Context) {}

  ~ur_kernel_handle_t_() {}

  static ur_result_t makeWithNative(native_type NativeKernel,
                                    ur_program_handle_t Program,
                                    ur_context_handle_t Context,
                                    ur_kernel_handle_t &Kernel) {
    auto URKernel =
        std::make_unique<ur_kernel_handle_t_>(NativeKernel, Program, Context);
    if (!Program) {
      cl_program CLProgram;
      CL_RETURN_ON_FAILURE(clGetKernelInfo(NativeKernel, CL_KERNEL_PROGRAM,
                                           sizeof(CLProgram), &CLProgram,
                                           nullptr));
      ur_native_handle_t NativeProgram =
          reinterpret_cast<ur_native_handle_t>(CLProgram);
      UR_RETURN_ON_FAILURE(urProgramCreateWithNativeHandle(
          NativeProgram, nullptr, nullptr, &(URKernel->Program)));
    }
    cl_context CLContext;
    CL_RETURN_ON_FAILURE(clGetKernelInfo(NativeKernel, CL_KERNEL_CONTEXT,
                                         sizeof(CLContext), &CLContext,
                                         nullptr));
    if (!Context) {
      ur_native_handle_t NativeContext =
          reinterpret_cast<ur_native_handle_t>(CLContext);
      UR_RETURN_ON_FAILURE(urContextCreateWithNativeHandle(
          NativeContext, 0, nullptr, nullptr, &(URKernel->Context)));
    } else if (Context->get() != CLContext) {
      return UR_RESULT_ERROR_INVALID_CONTEXT;
    }
    Kernel = URKernel.release();
    return UR_RESULT_SUCCESS;
  }

  native_type get() { return Kernel; }
};
