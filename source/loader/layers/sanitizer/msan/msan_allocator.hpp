/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file msan_allocator.hpp
 *
 */

#pragma once

#include "sanitizer_common/sanitizer_common.hpp"
#include "sanitizer_common/sanitizer_stacktrace.hpp"

namespace ur_sanitizer_layer {

enum class MsanAllocType : uint32_t {
    UNKNOWN,
    DEVICE_USM,
    SHARED_USM,
    HOST_USM,
    MEM_BUFFER,
    DEVICE_GLOBAL
};

struct MsanAllocInfo {
    uptr AllocBegin = 0;
    size_t AllocSize = 0;

    MsanAllocType Type = MsanAllocType::UNKNOWN;
    bool IsReleased = false;

    ur_context_handle_t Context = nullptr;
    ur_device_handle_t Device = nullptr;

    StackTrace AllocStack;
    StackTrace ReleaseStack;

    void print();
};

using MsanAllocationMap = std::map<uptr, std::shared_ptr<MsanAllocInfo>>;
using MsanAllocationIterator = MsanAllocationMap::iterator;

inline const char *ToString(MsanAllocType Type) {
    switch (Type) {
    case MsanAllocType::DEVICE_USM:
        return "Device USM";
    case MsanAllocType::HOST_USM:
        return "Host USM";
    case MsanAllocType::SHARED_USM:
        return "Shared USM";
    case MsanAllocType::MEM_BUFFER:
        return "Memory Buffer";
    case MsanAllocType::DEVICE_GLOBAL:
        return "Device Global";
    default:
        return "Unknown Type";
    }
}

} // namespace ur_sanitizer_layer
