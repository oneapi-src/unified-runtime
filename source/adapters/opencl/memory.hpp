//===--------- memory.hpp - OpenCL Adapter ---------------------------===//
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

struct ur_mem_handle_t_ {
  using native_type = cl_mem;
  native_type Memory;
  ur_context_handle_t Context;

  ur_mem_handle_t_(native_type Mem, ur_context_handle_t Ctx)
      : Memory(Mem), Context(Ctx) {}

  ~ur_mem_handle_t_() {}

  static ur_result_t makeWithNative(native_type NativeMem,
                                    ur_context_handle_t Ctx,
                                    ur_mem_handle_t &Mem) {
    try {
      auto URMem = std::make_unique<ur_mem_handle_t_>(NativeMem, Ctx);
      if (!Ctx) {
        cl_context CLContext;
        CL_RETURN_ON_FAILURE(clGetMemObjectInfo(
            NativeMem, CL_MEM_CONTEXT, sizeof(CLContext), &CLContext, nullptr));
        ur_native_handle_t NativeContext =
            reinterpret_cast<ur_native_handle_t>(CLContext);
        UR_RETURN_ON_FAILURE(urContextCreateWithNativeHandle(
            NativeContext, 0, nullptr, nullptr, &(URMem->Context)));
      }
      Mem = URMem.release();
    } catch (std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }

    return UR_RESULT_SUCCESS;
  }

  native_type get() { return Memory; }
};
