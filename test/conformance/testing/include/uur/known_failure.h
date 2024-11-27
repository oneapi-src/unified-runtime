// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef UR_CONFORMANCE_INCLUDE_KNOWN_FAILURE_H_INCLUDED
#define UR_CONFORMANCE_INCLUDE_KNOWN_FAILURE_H_INCLUDED

#include "uur/utils.h"
#include <string>
#include <string_view>
#include <vector>

namespace uur {
struct DeviceMatcher {
    DeviceMatcher(uint32_t adapterVersion, ur_platform_backend_t backend,
                  std::vector<std::string> deviceNames)
        : adapterVersion(adapterVersion), backend(backend),
          deviceNames(deviceNames) {}

    uint32_t adapterVersion;
    ur_platform_backend_t backend;
    std::vector<std::string> deviceNames;
};

struct OpenCL : DeviceMatcher {
    OpenCL(std::initializer_list<std::string> il)
        : DeviceMatcher(1, UR_PLATFORM_BACKEND_OPENCL, {il.begin(), il.end()}) {
    }
};

struct LevelZero : DeviceMatcher {
    LevelZero(std::initializer_list<std::string> il)
        : DeviceMatcher(1, UR_PLATFORM_BACKEND_LEVEL_ZERO,
                        {il.begin(), il.end()}) {}
};

struct LevelZeroV2 : DeviceMatcher {
    LevelZeroV2(std::initializer_list<std::string> il)
        : DeviceMatcher(2, UR_PLATFORM_BACKEND_LEVEL_ZERO,
                        {il.begin(), il.end()}) {}
};

struct CUDA : DeviceMatcher {
    CUDA(std::initializer_list<std::string> il)
        : DeviceMatcher(1, UR_PLATFORM_BACKEND_CUDA, {il.begin(), il.end()}) {}
};

struct HIP : DeviceMatcher {
    HIP(std::initializer_list<std::string> il)
        : DeviceMatcher(1, UR_PLATFORM_BACKEND_HIP, {il.begin(), il.end()}) {}
};

struct NativeCPU : DeviceMatcher {
    NativeCPU(std::initializer_list<std::string> il)
        : DeviceMatcher(1, UR_PLATFORM_BACKEND_NATIVE_CPU,
                        {il.begin(), il.end()}) {}
};

inline bool isKnownFailureOn(ur_adapter_handle_t adapter,
                             ur_platform_handle_t platform,
                             ur_device_handle_t device,
                             std::vector<DeviceMatcher> deviceMatchers) {
    for (const auto &deviceMatcher : deviceMatchers) {
        uint32_t adapterVersion = 0;
        urAdapterGetInfo(adapter, UR_ADAPTER_INFO_VERSION,
                         sizeof(adapterVersion), &adapterVersion, nullptr);
        ur_platform_backend_t backend = UR_PLATFORM_BACKEND_UNKNOWN;
        uur::GetPlatformInfo<ur_platform_backend_t>(
            platform, UR_PLATFORM_INFO_BACKEND, backend);
        if (deviceMatcher.adapterVersion != adapterVersion &&
            deviceMatcher.backend != backend) {
            continue;
        }
        if (deviceMatcher.deviceNames.empty()) {
            return true;
        }
        std::string deviceName;
        uur::GetDeviceInfo<std::string>(device, UR_DEVICE_INFO_NAME,
                                        deviceName);
        for (const auto &unsupportedDeviceName : deviceMatcher.deviceNames) {
            if (deviceName.find(unsupportedDeviceName) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}
} // namespace uur

#define UUR_KNOWN_FAILURE_ON(...)                                              \
    if (uur::isKnownFailureOn(adapter, platform, device, {__VA_ARGS__})) {     \
        using namespace std::literals;                                         \
        std::string platformName;                                              \
        uur::GetPlatformInfo<std::string>(platform, UR_PLATFORM_INFO_NAME,     \
                                          platformName);                       \
        std::string deviceName;                                                \
        uur::GetDeviceInfo<std::string>(device, UR_DEVICE_INFO_NAME,           \
                                        deviceName);                           \
        const char *alsoRunKnownFailures =                                     \
            std::getenv("UR_CTS_ALSO_RUN_KNOWN_FAILURES");                     \
        if (alsoRunKnownFailures && (alsoRunKnownFailures == "1"sv ||          \
                                     alsoRunKnownFailures == "yes"sv ||        \
                                     alsoRunKnownFailures == "YES"sv ||        \
                                     alsoRunKnownFailures == "on"sv ||         \
                                     alsoRunKnownFailures == "ON"sv ||         \
                                     alsoRunKnownFailures == "true"sv ||       \
                                     alsoRunKnownFailures == "TRUE"sv)) {      \
            std::cerr << "Known failure on: " << platformName << ", "          \
                      << deviceName << "\n";                                   \
        } else {                                                               \
            GTEST_SKIP() << "Known failure on: " << platformName << ", "       \
                         << deviceName;                                        \
        }                                                                      \
    }

#endif // UR_CONFORMANCE_INCLUDE_KNOWN_FAILURE_H_INCLUDED
