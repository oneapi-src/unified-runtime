// Copyright (C) 2022-2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef UR_CONFORMANCE_INCLUDE_FIXTURES_HELPERS_H_INCLUDED
#define UR_CONFORMANCE_INCLUDE_FIXTURES_HELPERS_H_INCLUDED

#define UUR_RETURN_ON_FATAL_FAILURE(...)                                       \
    __VA_ARGS__;                                                               \
    if (this->HasFatalFailure() || this->IsSkipped()) {                        \
        return;                                                                \
    }                                                                          \
    (void)0

#define UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(FIXTURE)                           \
    INSTANTIATE_TEST_SUITE_P(                                                  \
        , FIXTURE,                                                             \
        ::testing::ValuesIn(uur::DevicesEnvironment::instance->devices),       \
        [](const ::testing::TestParamInfo<ur_device_handle_t> &info) {         \
            return uur::GetPlatformAndDeviceName(info.param);                  \
        })

#define UUR_INSTANTIATE_KERNEL_TEST_SUITE_P(FIXTURE)                           \
    INSTANTIATE_TEST_SUITE_P(                                                  \
        , FIXTURE,                                                             \
        ::testing::ValuesIn(uur::KernelsEnvironment::instance->devices),       \
        [](const ::testing::TestParamInfo<ur_device_handle_t> &info) {         \
            return uur::GetPlatformAndDeviceName(info.param);                  \
        })

#define UUR_RETURN_ON_FATAL_FAILURE(...)                                       \
    __VA_ARGS__;                                                               \
    if (this->HasFatalFailure() || this->IsSkipped()) {                        \
        return;                                                                \
    }                                                                          \
    (void)0

#define UUR_ASSERT_SUCCESS_OR_UNSUPPORTED(ret)                                 \
    auto status = ret;                                                         \
    if (status == UR_RESULT_ERROR_UNSUPPORTED_FEATURE) {                       \
        GTEST_SKIP();                                                          \
    } else {                                                                   \
        ASSERT_EQ(status, UR_RESULT_SUCCESS);                                  \
    }

#endif // UR_CONFORMANCE_INCLUDE_FIXTURES_HELPERS_H_INCLUDED
