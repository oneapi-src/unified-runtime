/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file sanitizer_libdevice.hpp
 *
 */

#pragma once

#include <cstdint>

namespace ur_sanitizer_layer {

enum class DeviceType : uint32_t { UNKNOWN = 0, CPU, GPU_PVC, GPU_DG2 };

inline const char *ToString(DeviceType Type) {
    switch (Type) {
    case DeviceType::UNKNOWN:
        return "UNKNOWN";
    case DeviceType::CPU:
        return "CPU";
    case DeviceType::GPU_PVC:
        return "PVC";
    case DeviceType::GPU_DG2:
        return "DG2";
    default:
        return "UNKNOWN";
    }
}

} // namespace ur_sanitizer_layer
