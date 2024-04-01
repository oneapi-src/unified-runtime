// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "device_selector/filter.hpp"
#include "common.hpp"

using namespace std::string_view_literals;
using namespace ur::device_selector;

TEST(FilterMatcher, InvalidNoColon) {
    FilterMatcher matcher;
    auto result = matcher.init("level_zero0");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ("invalid filter: 'level_zero0'"sv, result.value().message);
}

TEST(FilterMatcher, InvalidBackend) {
    FilterMatcher matcher;
    auto result = matcher.init("InvalidBackend:0");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ("invalid backend: 'InvalidBackend'"sv, result.value().message);
}

TEST(FilterMatcher, InvalidDevice) {
    FilterMatcher matcher;
    auto result = matcher.init("level_zero:InvalidDevice");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ("invalid device: 'InvalidDevice'"sv, result.value().message);
}

TEST(FilterMatcher, LevelZeroStar) {
    FilterMatcher matcher;
    auto result = matcher.init("level_zero:*");
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, CPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, FPGA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, MCA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(LEVEL_ZERO, VPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, GPU, 0), matcher);
}

TEST(FilterMatcher, OpenCLStar) {
    FilterMatcher matcher;
    auto result = matcher.init("opencl:*");
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_EQ(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, CPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, FPGA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, MCA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(OPENCL, VPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, GPU, 0), matcher);
}

TEST(FilterMatcher, CUDAStar) {
    FilterMatcher matcher;
    auto result = matcher.init("cuda:*");
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_EQ(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(CUDA, CPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(CUDA, FPGA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(CUDA, MCA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(CUDA, VPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, GPU, 0), matcher);
}

TEST(FilterMatcher, HIPStar) {
    FilterMatcher matcher;
    auto result = matcher.init("hip:*");
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_EQ(DESCRIPTOR(HIP, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(HIP, CPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(HIP, FPGA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(HIP, MCA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(HIP, VPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(NATIVE_CPU, GPU, 0), matcher);
}

TEST(FilterMatcher, NativeCPUStar) {
    FilterMatcher matcher;
    auto result = matcher.init("native_cpu:*");
    ASSERT_FALSE(result.has_value());
    ASSERT_FALSE(matcher.isSubDeviceMatcher());
    ASSERT_FALSE(matcher.isSubSubDeviceMatcher());
    ASSERT_EQ(DESCRIPTOR(NATIVE_CPU, GPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(NATIVE_CPU, CPU, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(NATIVE_CPU, FPGA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(NATIVE_CPU, MCA, 0), matcher);
    ASSERT_EQ(DESCRIPTOR(NATIVE_CPU, VPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(LEVEL_ZERO, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(OPENCL, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(CUDA, GPU, 0), matcher);
    ASSERT_NE(DESCRIPTOR(HIP, GPU, 0), matcher);
}
