// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "device_selector/matcher.hpp"
#include "common.hpp"
#include <gtest/gtest.h>

using namespace std::string_view_literals;

// ONEAPI_DEVICE_SELECTOR=opencl:*
// Only the OpenCL devices are available
TEST(Matcher, Example_01) {
    ur::device_selector::Matcher matcher;
    auto result = matcher.init("opencl:*");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, CPU, 1), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, FPGA, 2), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, CPU, 0), matcher);
}

// ONEAPI_DEVICE_SELECTOR=level_zero:gpu
// Only GPU devices on the Level Zero platform are available.
TEST(Matcher, Example_02) {
    ur::device_selector::Matcher matcher;
    auto result = matcher.init("level_zero:gpu");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, CPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, CPU, 0), matcher);
}

// ONEAPI_DEVICE_SELECTOR="opencl:gpu;level_zero:gpu"
// GPU devices from both Level Zero and OpenCL are available. Note that
// escaping (like quotation marks) will likely be needed when using semi-colon
// separated entries.
TEST(Matcher, Example_03) {
    ur::device_selector::Matcher matcher;
    auto result = matcher.init("opencl:gpu;level_zero:gpu");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 1), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 1), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, CPU, 0), matcher);
}

// ONEAPI_DEVICE_SELECTOR=opencl:gpu,cpu
// Only CPU and GPU devices on the OpenCL platform are available.
TEST(Matcher, Example_04) {
    ur::device_selector::Matcher matcher;
    auto result = matcher.init("opencl:gpu,cpu");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 1), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, CPU, 2), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, FPGA, 3), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, CPU, 0), matcher);
}

// ONEAPI_DEVICE_SELECTOR=opencl:0
// Only the device with index 0 on the OpenCL backend is available.
TEST(Matcher, Example_05) {
    ur::device_selector::Matcher matcher;
    auto result = matcher.init("opencl:0");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 1), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, CPU, 0), matcher);
}

// ONEAPI_DEVICE_SELECTOR=hip:0,2
// Only devices with indices of 0 and 2 from the HIP backend are available.
TEST(Matcher, Example_06) {
    ur::device_selector::Matcher matcher;
    auto result = matcher.init("hip:0,2");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_EQ(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 1), matcher);
    ASSERT_EQ(DESCRIPTOR(HIP, GPU, 2), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 3), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, CPU, 0), matcher);
}

// ONEAPI_DEVICE_SELECTOR=opencl:0.*
// All the sub-devices from the OpenCL device with index 0 are exposed as SYCL
// root devices. No other devices are available.
TEST(Matcher, Example_07) {
    ur::device_selector::Matcher matcher;
    auto result = matcher.init("opencl:0.*");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 0, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 0, 1), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 0, 2), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 0, 3), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0, 0), matcher);
}

// ONEAPI_DEVICE_SELECTOR=opencl:0.2
// The third sub-device (2 in zero-based counting) of the OpenCL device with
// index 0 will be the sole device available.
TEST(Matcher, Example_08) {
    ur::device_selector::Matcher matcher;
    auto result = matcher.init("opencl:0.2");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0, 1), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 0, 2), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0, 3), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 2), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0, 2), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0, 2), matcher);
}

// ONEAPI_DEVICE_SELECTOR=level_zero:*,*.*
// Exposes Level Zero devices to the application in two different ways. Each
// device (aka “card”) is exposed as a SYCL root device and each sub-device is
// also exposed as a SYCL root device.
TEST(Matcher, Example_09) {
    ur::device_selector::Matcher matcher;
    auto result = matcher.init("level_zero:*,*.*");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 1), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 1), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 1, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 1, 1), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 2), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 2, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 2, 1), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 3), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 3, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 3, 1), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0, 1), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0), matcher);
}

// ONEAPI_DEVICE_SELECTOR="opencl:*;!opencl:0"
// All OpenCL devices except for the device with index 0 are available.
TEST(Matcher, Example_10) {
    ur::device_selector::Matcher matcher;
    auto result = matcher.init("opencl:*;!opencl:0");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 1), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 9), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 1), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 1), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 1), matcher);
}

// ONEAPI_DEVICE_SELECTOR="!*:cpu"
// All devices except for CPU devices are available.
TEST(Matcher, Example_11) {
    ur::device_selector::Matcher matcher;
    auto result = matcher.init("!*:cpu");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, FPGA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, CPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, CPU, 0), matcher);
}
