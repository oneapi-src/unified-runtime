/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file sanitizer_shadow.hpp
 *
 */

#include "sanitizer_allocator.hpp"

#include <unordered_set>

namespace ur_sanitizer_layer {

struct ShadowMemory {
    ShadowMemory(ur_context_handle_t Context, ur_device_handle_t Device)
        : Context(Context), Device(Device) {}

    virtual ~ShadowMemory() {}

    virtual ur_result_t Setup() = 0;

    virtual ur_result_t Destory() = 0;

    virtual uptr MemToShadow(uptr Ptr) = 0;

    virtual ur_result_t EnqueuePoisonShadow(ur_queue_handle_t Queue, uptr Ptr,
                                            uptr Size, u8 Value) = 0;

    virtual ur_result_t ReleaseShadow(std::shared_ptr<AllocInfo>) {
        return UR_RESULT_SUCCESS;
    }

    virtual size_t GetShadowSize() = 0;

    ur_context_handle_t Context{};

    ur_device_handle_t Device{};

    uptr ShadowBegin = 0;

    uptr ShadowEnd = 0;
};

} // namespace ur_sanitizer_layer
