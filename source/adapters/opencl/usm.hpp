//===--------------------- usm.hpp - OpenCL Adapter -----------------------===//
//
// Copyright (C) 2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CL/cl_ext.h"
#include <CL/cl.h>

// This struct is intended to be used in conjunction with the below callback via
// clSetEventCallback to release temporary allocations created by the adapter to
// implement certain USM operations.
//
// Example usage:
//
// auto Info = new AllocDeleterCallbackInfo(USMFreeFuncPtr, Context,
// Allocation); clSetEventCallback(USMOpEvent, CL_COMPLETE,
// AllocDeleterCallback, Info);
struct AllocDeleterCallbackInfo {
  AllocDeleterCallbackInfo(clMemBlockingFreeINTEL_fn USMFree,
                           cl_context CLContext, void *Allocation)
      : USMFree(USMFree), CLContext(CLContext), Allocation(Allocation) {
    clRetainContext(CLContext);
  }
  ~AllocDeleterCallbackInfo() {
    USMFree(CLContext, Allocation);
    clReleaseContext(CLContext);
  }
  AllocDeleterCallbackInfo(const AllocDeleterCallbackInfo &) = delete;
  AllocDeleterCallbackInfo &
  operator=(const AllocDeleterCallbackInfo &) = delete;

  clMemBlockingFreeINTEL_fn USMFree;
  cl_context CLContext;
  void *Allocation;
};

void AllocDeleterCallback(cl_event event, cl_int, void *pUserData);
