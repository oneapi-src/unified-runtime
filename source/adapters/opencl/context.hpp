//===--------- context.hpp - OpenCL Adapter ---------------------------===//
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

namespace cl_adapter {
ur_result_t
getDevicesFromContext(ur_context_handle_t hContext,
                      std::unique_ptr<std::vector<cl_device_id>> &DevicesInCtx);
}

// struct ur_context_handle_t_ {
//     using native_type = cl_context;
//     native_type Context;
//     std::atomic_uint32_t RefCount;
//     ur_platform_handle_t Platform;

//     ur_context_handle_t_(native_type Ctx):Context(Ctx) {}

//     ~ur_context_handle_t_() {}

//     native_type get() { return Context; }

//     uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

//     uint32_t decrementReferenceCount() noexcept { return --RefCount; }

//     uint32_t getReferenceCount() const noexcept { return RefCount; }
// };
