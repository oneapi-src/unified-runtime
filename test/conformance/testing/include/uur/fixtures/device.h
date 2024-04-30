// Copyright (C) 2022-2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef UR_CONFORMANCE_INCLUDE_FIXTURES_DEVICE_H_INCLUDED
#define UR_CONFORMANCE_INCLUDE_FIXTURES_DEVICE_H_INCLUDED

#include <uur/fixtures/helpers.h>
#include <uur/fixtures/platform.h>

namespace uur {

inline std::pair<bool, std::vector<ur_device_handle_t>>
GetDevices(ur_platform_handle_t platform) {
    uint32_t count = 0;
    if (urDeviceGet(platform, UR_DEVICE_TYPE_ALL, 0, nullptr, &count)) {
        return {false, {}};
    }
    if (count == 0) {
        return {false, {}};
    }
    std::vector<ur_device_handle_t> devices(count);
    if (urDeviceGet(platform, UR_DEVICE_TYPE_ALL, count, devices.data(),
                    nullptr)) {
        return {false, {}};
    }
    return {true, devices};
}

inline bool hasDevicePartitionSupport(ur_device_handle_t device,
                                      const ur_device_partition_t property) {
    std::vector<ur_device_partition_t> properties;
    uur::GetDevicePartitionProperties(device, properties);
    return std::find(properties.begin(), properties.end(), property) !=
           properties.end();
}

struct urAllDevicesTest : urPlatformTest {
    void SetUp() override {
        UUR_RETURN_ON_FATAL_FAILURE(urPlatformTest::SetUp());
        auto devicesPair = GetDevices(platform);
        if (!devicesPair.first) {
            FAIL() << "Failed to get devices";
        }
        devices = std::move(devicesPair.second);
    }

    void TearDown() override {
        for (auto &device : devices) {
            EXPECT_SUCCESS(urDeviceRelease(device));
        }
        UUR_RETURN_ON_FATAL_FAILURE(urPlatformTest::TearDown());
    }

    std::vector<ur_device_handle_t> devices;
};

struct urDeviceTest : urPlatformTest,
                      ::testing::WithParamInterface<ur_device_handle_t> {
    void SetUp() override {
        UUR_RETURN_ON_FATAL_FAILURE(urPlatformTest::SetUp());
        device = GetParam();
    }

    void TearDown() override {
        EXPECT_SUCCESS(urDeviceRelease(device));
        UUR_RETURN_ON_FATAL_FAILURE(urPlatformTest::TearDown());
    }

    ur_device_handle_t device;
};

template <class T>
struct urDeviceTestWithParam
    : urPlatformTest,
      ::testing::WithParamInterface<std::tuple<ur_device_handle_t, T>> {
    void SetUp() override {
        UUR_RETURN_ON_FATAL_FAILURE(urPlatformTest::SetUp());
        device = std::get<0>(this->GetParam());
    }
    // TODO - I don't like the confusion with GetParam();
    const T &getParam() const { return std::get<1>(this->GetParam()); }
    ur_device_handle_t device;
};
} // namespace uur

#endif // UR_CONFORMANCE_INCLUDE_FIXTURES_DEVICE_H_INCLUDED
