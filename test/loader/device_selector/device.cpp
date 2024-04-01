// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "device_selector/device.hpp"
#include "common.hpp"

using namespace std::string_view_literals;
using namespace ur::device_selector;

TEST(DeviceMatcher, Empty) {
    DeviceMatcher matcher;
    auto result = matcher.init("");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ("empty device"sv, result->message);
}

TEST(DeviceMatcher, Invalid) {
    DeviceMatcher matcher;
    auto result = matcher.init("NotADeviceName");
    ASSERT_TRUE(result.has_value());
    ASSERT_STREQ("invalid device: 'NotADeviceName'", result->message.c_str());
}

TEST(DeviceMatcher, InvalidStar) {
    DeviceMatcher matcher;
    auto result = matcher.init("*garbage");
    ASSERT_TRUE(result.has_value());
    ASSERT_STREQ("invalid device: '*garbage'", result->message.c_str());
}

TEST(DeviceMatcher, Star) {
    DeviceMatcher matcher;
    auto result = matcher.init("*");
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    std::array<uint32_t, 2> indicies = {0, 0xFFFFFFFF};
    for (auto index : indicies) {
        ASSERT_EQ(DESCRIPTOR(HIP, ALL, index), matcher);
        ASSERT_EQ(DESCRIPTOR(HIP, GPU, index), matcher);
        ASSERT_EQ(DESCRIPTOR(HIP, CPU, index), matcher);
        ASSERT_EQ(DESCRIPTOR(HIP, FPGA, index), matcher);
        ASSERT_EQ(DESCRIPTOR(HIP, MCA, index), matcher);
        ASSERT_EQ(DESCRIPTOR(HIP, VPU, index), matcher);
    }
}

TEST(DeviceMatcher, GPU) {
    DeviceMatcher matcher;
    auto result = matcher.init("gpu");
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_EQ(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, CPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, FPGA, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, MCA, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, VPU, 0), matcher);
}

TEST(DeviceMatcher, CPU) {
    DeviceMatcher matcher;
    auto result = matcher.init("cpu");
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(NATIVE_CPU, CPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, FPGA, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, MCA, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, VPU, 0), matcher);
}

TEST(DeviceMatcher, FPGA) {
    DeviceMatcher matcher;
    auto result = matcher.init("fpga");
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, CPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, FPGA, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, MCA, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, VPU, 0), matcher);
}

TEST(DeviceMatcher, MCA) {
    DeviceMatcher matcher;
    auto result = matcher.init("mca");
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, CPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, FPGA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, MCA, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, VPU, 0), matcher);
}

TEST(DeviceMatcher, NPU) {
    DeviceMatcher matcher;
    auto result = matcher.init("npu");
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, CPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, FPGA, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, MCA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, VPU, 0), matcher);
}

TEST(DeviceMatcher, StarDotStar) {
    DeviceMatcher matcher;
    auto result = matcher.init("*.*");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_TRUE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    std::array<uint32_t, 2> indicies = {0, 0xFFFFFFFF};
    for (auto index : indicies) {
        for (auto subIndex : indicies) {
            // Valid matches
            ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, ALL, index, subIndex), matcher);
            ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, index, subIndex), matcher);
            ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, CPU, index, subIndex), matcher);
            ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, FPGA, index, subIndex), matcher);
            ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, MCA, index, subIndex), matcher);
            ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, VPU, index, subIndex), matcher);
            // Invalid matches, the descriptor has no subIndex
            ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, ALL, index), matcher);
            ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, index), matcher);
            ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, CPU, index), matcher);
            ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, FPGA, index), matcher);
            ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, MCA, index), matcher);
            ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, VPU, index), matcher);
        }
    }
}

TEST(DeviceMatcher, InvalidNum) {
    DeviceMatcher matcher;
    auto result = matcher.init("0c");
    ASSERT_TRUE(result.has_value());
    ASSERT_STREQ("invalid number: '0c'", result->message.c_str());
}

TEST(DeviceMatcher, Num) {
    DeviceMatcher matcher;
    auto result = matcher.init("0");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_FALSE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_EQ(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 1), matcher);
}

TEST(DeviceMatcher, NumDotInvalidNum) {
    DeviceMatcher matcher;
    {
        auto result = matcher.init("1.crap");
        ASSERT_TRUE(result.has_value());
        ASSERT_STREQ("invalid number: 'crap' in '1.crap'",
                     result->message.c_str());
    }
    {
        auto result = matcher.init("1.");
        ASSERT_TRUE(result.has_value());
        ASSERT_STREQ("invalid number: '' in '1.'", result->message.c_str());
    }
}

TEST(DeviceMatcher, NumDotNum) {
    DeviceMatcher matcher;
    auto result = matcher.init("0.0");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_TRUE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 1), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 1, 0), matcher);
}

TEST(DeviceMatcher, NumDotStar) {
    DeviceMatcher matcher;
    auto result = matcher.init("0.*");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_TRUE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0xFFFFFFFF), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 1, 0), matcher);
}

TEST(DeviceMatcher, NumDotNumDotInvalidNum) {
    DeviceMatcher matcher;
    auto result = matcher.init("1.0.garbage");
    ASSERT_TRUE(result.has_value());
    ASSERT_STREQ("invalid number: 'garbage' in '1.0.garbage'",
                 result->message.c_str());
}

TEST(DeviceMatcher, NumDotNumDotNum) {
    DeviceMatcher matcher;
    auto result = matcher.init("0.0.0");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_TRUE(matcher.isSubDeviceMatcher());
    ASSERT_TRUE(matcher.isSubSubDeviceMatcher());
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0, 1), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 1, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 1, 0, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 1, 1), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 1, 0, 1), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 1, 1, 1), matcher);
}

TEST(DeviceMatcher, NumDotNumDotStar) {
    DeviceMatcher matcher;
    auto result = matcher.init("0.0.*");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_TRUE(matcher.isSubDeviceMatcher());
    ASSERT_TRUE(matcher.isSubSubDeviceMatcher());
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0, 0xFFFFFFFF), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 1, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 1, 0, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 1, 1), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 1, 0, 1), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 1, 1, 1), matcher);
}

TEST(DeviceMatcher, StarDotStarDotStar) {
    DeviceMatcher matcher;
    auto result = matcher.init("*.*.*");
    ASSERT_FALSE(result.has_value()) << result->message;
    ASSERT_TRUE(matcher.isSubDeviceMatcher());
    ASSERT_TRUE(matcher.isSubSubDeviceMatcher());
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0, 0xFFFFFFFF), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0xFFFFFFFF, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0xFFFFFFFF, 0, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0, 0xFFFFFFFF, 0xFFFFFFFF), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0xFFFFFFFF, 0, 0xFFFFFFFF), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF),
              matcher);
}

TEST(DevicesMatcher, InvalidBefore) {
    DevicesMatcher matcher;
    auto result = matcher.init(",gpu");
    ASSERT_TRUE(result.has_value());
    ASSERT_STREQ("empty device in: ',gpu'", result->message.c_str());
}

TEST(DevicesMatcher, InvalidAfter) {
    DevicesMatcher matcher;
    auto result = matcher.init("gpu,");
    ASSERT_TRUE(result.has_value());
    ASSERT_STREQ("empty device in: 'gpu,'", result->message.c_str());
}

TEST(DevicesMatcher, GPUCommaCPU) {
    DevicesMatcher matcher;
    auto result = matcher.init("gpu,cpu");
    ASSERT_FALSE(result.has_value()) << result.value().message;
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, CPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, FPGA, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, MCA, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, VPU, 0), matcher);
}

TEST(DevicesMatcher, OneCommaTwo) {
    DevicesMatcher matcher;
    auto result = matcher.init("0,2");
    ASSERT_FALSE(result.has_value()) << result.value().message;
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 1), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 2), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 3), matcher);
}

TEST(DevicesMatcher, StarCommaStarDotStar) {
    DevicesMatcher matcher;
    auto result = matcher.init("*,*.*");
    ASSERT_FALSE(result.has_value()) << result.value().message;
    ASSERT_TRUE(matcher.isSubDeviceMatcher());
}
