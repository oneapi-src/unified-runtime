// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "state.hpp"
#include <stdexcept>
#include <vector>

urState::urState() {
    ur_device_init_flags_t device_flags = 0;
    auto initResult = urLoaderInit(device_flags, nullptr);
    if (initResult != UR_RESULT_SUCCESS) {
        throw std::runtime_error(
            "urLoaderInit() failed to initialize the loader.");
    }

    std::vector<ur_adapter_handle_t> adapters;

    uint32_t adapter_count = 0;
    urAdapterGet(0, nullptr, &adapter_count);

    if (adapter_count != 1) {
        throw std::runtime_error("Detected " + std::to_string(adapter_count) +
                                 " adapters, expected 1.");
    }

    urAdapterGet(adapter_count, adapter.ptr(), nullptr);

    uint32_t count = 0;
    if (urPlatformGet(adapters.data(), adapter_count, 0, nullptr, &count)) {
        throw std::runtime_error(
            "urPlatformGet() failed to get number of platforms.");
    }

    if (count != 1) {
        throw std::runtime_error("Detected " + std::to_string(count) +
                                 " platforms, expected 1.");
    }

    ur_platform_handle_t platform;
    if (urPlatformGet(adapters.data(), adapter_count, count, &platform,
                      nullptr)) {
        throw std::runtime_error("urPlatformGet failed to get platforms.");
    }

    count = 0;
    if (urDeviceGet(platform, UR_DEVICE_TYPE_ALL, 0, nullptr, &count)) {
        throw std::runtime_error(
            "urDevicesGet() failed to get number of devices.");
    }
    if (count < 1) {
        throw std::runtime_error("Detected " + std::to_string(count) +
                                 " devices, expected > 0");
    }

    std::vector<ur_device_handle_t> devices(count);

    if (urDeviceGet(platform, UR_DEVICE_TYPE_ALL, count, devices.data(),
                    nullptr)) {
        throw std::runtime_error(
            "urDevicesGet() failed to get number of devices.");
    }

    *device.ptr() = devices[0];

    if (urContextCreate(1, device.ptr(), nullptr, context.ptr())) {
        throw std::runtime_error(
            "urContextCreate() failed to create a context.");
    }
}
