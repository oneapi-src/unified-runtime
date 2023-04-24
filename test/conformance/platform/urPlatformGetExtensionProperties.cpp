// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: MIT

#include "fixtures.h"

using urPlatformGetExtensionPropertiesTest = uur::platform::urPlatformTest;

TEST_F(urPlatformGetExtensionPropertiesTest, Success) {
    uint32_t n_extensions = 0;
    ASSERT_SUCCESS(
        urPlatformGetExtensionProperties(platform, 0, nullptr, &n_extensions));

    if (n_extensions == 0) {
        GTEST_SKIP() << "Adapter does not support any extensions.\n";
    }

    std::vector<ur_extension_properties_t> extensions(n_extensions);
    ASSERT_SUCCESS(urPlatformGetExtensionProperties(
        platform, n_extensions, extensions.data(), nullptr));
}

TEST_F(urPlatformGetExtensionPropertiesTest, InvalidNullHandlePlatform) {
    uint32_t n_extensions = 0;
    ASSERT_EQ_RESULT(
        UR_RESULT_ERROR_INVALID_NULL_HANDLE,
        urPlatformGetExtensionProperties(nullptr, 0, nullptr, &n_extensions));
}
