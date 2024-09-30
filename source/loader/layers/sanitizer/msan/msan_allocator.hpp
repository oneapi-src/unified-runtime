/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file asan_interceptor.hpp
 *
 */

#include "sanitizer_common/sanitizer_allocator.hpp"
#include "sanitizer_common/sanitizer_common.hpp"
#include "sanitizer_common/sanitizer_stacktrace.hpp"

#include <map>
#include <memory>

namespace ur_sanitizer_layer {

struct MsanAllocInfo {
    uptr AllocBegin = 0;
    size_t AllocSize = 0;

    AllocType Type = AllocType::UNKNOWN;
    bool IsInitialized = false;

    ur_context_handle_t Context = nullptr;
    ur_device_handle_t Device = nullptr;

    // StackTrace AllocStack;
    // StackTrace ReleaseStack;

    // void print();
};

using MsanAllocationMap = std::map<uptr, std::shared_ptr<MsanAllocInfo>>;
using MsanAllocationIterator = MsanAllocationMap::iterator;

} // namespace ur_sanitizer_layer
