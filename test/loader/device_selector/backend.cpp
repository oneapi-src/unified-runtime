// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "device_selector/backend.hpp"
#include "common.hpp"

using namespace std::string_view_literals;

TEST(BackendMatcher, Empty) {
    ur::device_selector::BackendMatcher matcher;
    auto result = matcher.init("");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ("empty backend"sv, result->message);
}

TEST(BackendMatcher, Invalid) {
    ur::device_selector::BackendMatcher matcher;
    auto result = matcher.init("NotABackendName");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ("invalid backend: 'NotABackendName'"sv, result->message);
}

TEST(BackendMatcher, Star) {
    ur::device_selector::BackendMatcher matcher;
    auto result = matcher.init("*");
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(matcher, DESCRIPTOR(LEVEL_ZERO, GPU, 0));
    ASSERT_EQ(matcher, DESCRIPTOR(OPENCL, GPU, 0));
    ASSERT_EQ(matcher, DESCRIPTOR(CUDA, GPU, 0));
    ASSERT_EQ(matcher, DESCRIPTOR(HIP, GPU, 0));
    ASSERT_EQ(matcher, DESCRIPTOR(NATIVE_CPU, CPU, 0));
}

TEST(BackendMatcher, OpenCL) {
    ur::device_selector::BackendMatcher matcher;
    auto result = matcher.init("OpenCL");
    ASSERT_FALSE(result.has_value());
    ASSERT_NE(matcher, DESCRIPTOR(LEVEL_ZERO, GPU, 0));
    ASSERT_EQ(matcher, DESCRIPTOR(OPENCL, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(CUDA, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(HIP, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(NATIVE_CPU, CPU, 0));
}

TEST(BackendMatcher, LevelZero) {
    ur::device_selector::BackendMatcher matcher;
    auto result = matcher.init("level_zero");
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(matcher, DESCRIPTOR(LEVEL_ZERO, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(OPENCL, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(CUDA, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(HIP, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(NATIVE_CPU, CPU, 0));
}

TEST(BackendMatcher, CUDA) {
    ur::device_selector::BackendMatcher matcher;
    auto result = matcher.init("CUDA");
    ASSERT_FALSE(result.has_value());
    ASSERT_NE(matcher, DESCRIPTOR(LEVEL_ZERO, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(OPENCL, GPU, 0));
    ASSERT_EQ(matcher, DESCRIPTOR(CUDA, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(HIP, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(NATIVE_CPU, CPU, 0));
}

TEST(BackendMatcher, HIP) {
    ur::device_selector::BackendMatcher matcher;
    auto result = matcher.init("Hip");
    ASSERT_FALSE(result.has_value());
    ASSERT_NE(matcher, DESCRIPTOR(LEVEL_ZERO, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(OPENCL, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(CUDA, GPU, 0));
    ASSERT_EQ(matcher, DESCRIPTOR(HIP, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(NATIVE_CPU, CPU, 0));
}

TEST(BackendMatcher, NativeCPU) {
    ur::device_selector::BackendMatcher matcher;
    auto result = matcher.init("Native_CPU");
    ASSERT_FALSE(result.has_value());
    ASSERT_NE(matcher, DESCRIPTOR(LEVEL_ZERO, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(OPENCL, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(CUDA, GPU, 0));
    ASSERT_NE(matcher, DESCRIPTOR(HIP, GPU, 0));
    ASSERT_EQ(matcher, DESCRIPTOR(NATIVE_CPU, CPU, 0));
}
