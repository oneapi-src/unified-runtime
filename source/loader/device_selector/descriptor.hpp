// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "ur_api.h"
#include <optional>
#include <string>

namespace ur::device_selector {

struct Diagnostic {
    Diagnostic(std::string message) : message(message) {}
    std::string message;
};

struct Descriptor {
    Descriptor() = default;

    Descriptor(const Descriptor &other)
        : backend(other.backend), type(other.type), index(other.index),
          subIndex(other.subIndex), subSubIndex(other.subSubIndex) {}

    Descriptor(ur_platform_backend_t backend, ur_device_type_t type,
               uint32_t index)
        : backend(backend), type(type), index(index) {}
    Descriptor(ur_platform_backend_t backend, ur_device_type_t type,
               uint32_t index, uint32_t subDeviceIndex)
        : backend(backend), type(type), index(index), subIndex(subDeviceIndex) {
    }
    Descriptor(ur_platform_backend_t backend, ur_device_type_t type,
               uint32_t index, uint32_t subDeviceIndex,
               uint32_t subSubDeviceIndex)
        : backend(backend), type(type), index(index), subIndex(subDeviceIndex),
          subSubIndex(subSubDeviceIndex) {}

    ur_platform_backend_t backend;
    ur_device_type_t type;
    uint32_t index = 0;
    std::optional<uint32_t> subIndex;
    std::optional<uint32_t> subSubIndex;
};

} // namespace ur::device_selector
