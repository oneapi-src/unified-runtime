// Copyright (C) 2022-2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef UR_CONFORMANCE_INCLUDE_FIXTURES_PLATFORM_H_INCLUDED
#define UR_CONFORMANCE_INCLUDE_FIXTURES_PLATFORM_H_INCLUDED

namespace uur {

struct urTest : ::testing::Test {

    void SetUp() override {
        loader_config = uur::PlatformEnvironment::instance->loader_config;
        adapters = uur::PlatformEnvironment::instance->adapters;
    }

    ur_loader_config_handle_t loader_config = nullptr;
    std::vector<ur_adapter_handle_t> adapters;
};

struct urPlatformsTest : urTest {

    void SetUp() override {
        UUR_RETURN_ON_FATAL_FAILURE(urTest::SetUp());
        platforms = uur::PlatformEnvironment::instance->platforms;
    }

    std::vector<ur_platform_handle_t> platforms;
};

struct urPlatformTest : urPlatformsTest {

    void SetUp() override {
        UUR_RETURN_ON_FATAL_FAILURE(urPlatformsTest::SetUp());
        platform = uur::PlatformEnvironment::instance->platform;
    }

    ur_platform_handle_t platform;
};
} // namespace uur

#endif // UR_CONFORMANCE_INCLUDE_FIXTURES_PLATFORM_H_INCLUDED
