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
#include "program.hpp"

#include <vector>

struct ur_kernel_handle_t_ {
  using native_type = cl_kernel;
  native_type Kernel;
  ur_program_handle_t Program;
  ur_context_handle_t Context;
  std::atomic<uint32_t> RefCount = 0;

  ur_kernel_handle_t_(native_type Kernel, ur_program_handle_t Program,
                      ur_context_handle_t Context)
      : Kernel(Kernel), Program(Program), Context(Context) {
    RefCount = 1;
    urProgramRetain(Program);
    urContextRetain(Context);
  }

  ~ur_kernel_handle_t_() {
    clReleaseKernel(Kernel);
    urProgramRelease(Program);
    urContextRelease(Context);
  }

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }

  static ur_result_t makeWithNative(native_type NativeKernel,
                                    ur_program_handle_t Program,
                                    ur_context_handle_t Context,
                                    ur_kernel_handle_t &Kernel) {
    if (!Program || !Context) {
      return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    try {
      cl_context CLContext;
      CL_RETURN_ON_FAILURE(clGetKernelInfo(NativeKernel, CL_KERNEL_CONTEXT,
                                           sizeof(CLContext), &CLContext,
                                           nullptr));
      cl_program CLProgram;
      CL_RETURN_ON_FAILURE(clGetKernelInfo(NativeKernel, CL_KERNEL_PROGRAM,
                                           sizeof(CLProgram), &CLProgram,
                                           nullptr));

      if (Context->get() != CLContext) {
        return UR_RESULT_ERROR_INVALID_CONTEXT;
      }
      if (Program->get() != CLProgram) {
        return UR_RESULT_ERROR_INVALID_PROGRAM;
      }
      auto URKernel =
          std::make_unique<ur_kernel_handle_t_>(NativeKernel, Program, Context);
      Kernel = URKernel.release();
    } catch (std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }

    return UR_RESULT_SUCCESS;
  }

  native_type get() { return Kernel; }

  ur_platform_handle_t getPlatform() { return Context->Devices[0]->Platform; }
};
