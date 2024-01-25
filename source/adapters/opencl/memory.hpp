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
#include "context.hpp"

#include <vector>

struct ur_mem_handle_t_ {
  using native_type = cl_mem;
  native_type Memory;
  ur_context_handle_t Context;
  std::atomic<uint32_t> RefCount = 0;

  ur_mem_handle_t_(native_type Mem, ur_context_handle_t Ctx)
      : Memory(Mem), Context(Ctx) {
    RefCount = 1;
    urContextRetain(Context);
  }

  ~ur_mem_handle_t_() {
    clReleaseMemObject(Memory);
    urContextRelease(Context);
  }

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }

  static ur_result_t makeWithNative(native_type NativeMem,
                                    ur_context_handle_t Ctx,
                                    ur_mem_handle_t &Mem) {
    if (!Ctx) {
      return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    try {
      cl_context CLContext;
      CL_RETURN_ON_FAILURE(clGetMemObjectInfo(
          NativeMem, CL_MEM_CONTEXT, sizeof(CLContext), &CLContext, nullptr));

      if (Ctx->get() != CLContext) {
        return UR_RESULT_ERROR_INVALID_CONTEXT;
      }
      auto URMem = std::make_unique<ur_mem_handle_t_>(NativeMem, Ctx);
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
