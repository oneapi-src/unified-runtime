// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <uur/fixtures.h>

using urKernelGetInfoTest = uur::urKernelTestWithParam<ur_kernel_info_t>;

UUR_TEST_SUITE_P(
    urKernelGetInfoTest,
    ::testing::Values(UR_KERNEL_INFO_FUNCTION_NAME, UR_KERNEL_INFO_NUM_ARGS,
                      UR_KERNEL_INFO_REFERENCE_COUNT, UR_KERNEL_INFO_CONTEXT,
                      UR_KERNEL_INFO_PROGRAM, UR_KERNEL_INFO_ATTRIBUTES,
                      UR_KERNEL_INFO_NUM_REGS),
    uur::deviceTestWithParamPrinter<ur_kernel_info_t>);

TEST_P(urKernelGetInfoTest, Success) {
    auto property_name = getParam();
    size_t property_size = 0;
    auto Err =
        urKernelGetInfo(kernel, property_name, 0, nullptr, &property_size);
    if (Err == UR_RESULT_SUCCESS) {
        std::vector<char> property_value(property_size);
        ASSERT_SUCCESS(urKernelGetInfo(kernel, property_name, property_size,
                                       property_value.data(), nullptr));
    } else {
        ASSERT_EQ_RESULT(Err, UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION);
    }
}

TEST_P(urKernelGetInfoTest, InvalidNullHandleKernel) {
    size_t kernel_name_length = 0;
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_NULL_HANDLE,
                     urKernelGetInfo(nullptr, UR_KERNEL_INFO_FUNCTION_NAME, 0,
                                     nullptr, &kernel_name_length));
}

TEST_P(urKernelGetInfoTest, InvalidEnumeration) {
    size_t bad_enum_length = 0;
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_ENUMERATION,
                     urKernelGetInfo(kernel, UR_KERNEL_INFO_FORCE_UINT32, 0,
                                     nullptr, &bad_enum_length));
}

TEST_P(urKernelGetInfoTest, InvalidSizeZero) {
    size_t query_size = 0;
    ASSERT_SUCCESS(urKernelGetInfo(kernel, UR_KERNEL_INFO_NUM_ARGS, 0, nullptr,
                                   &query_size));
    std::vector<char> query_data(query_size);
    ASSERT_EQ_RESULT(urKernelGetInfo(kernel, UR_KERNEL_INFO_NUM_ARGS, 0,
                                     query_data.data(), nullptr),
                     UR_RESULT_ERROR_INVALID_SIZE);
}

TEST_P(urKernelGetInfoTest, InvalidSizeSmall) {
    size_t query_size = 0;
    ASSERT_SUCCESS(urKernelGetInfo(kernel, UR_KERNEL_INFO_NUM_ARGS, 0, nullptr,
                                   &query_size));
    std::vector<char> query_data(query_size);
    ASSERT_EQ_RESULT(urKernelGetInfo(kernel, UR_KERNEL_INFO_NUM_ARGS,
                                     query_data.size() - 1, query_data.data(),
                                     nullptr),
                     UR_RESULT_ERROR_INVALID_SIZE);
}

TEST_P(urKernelGetInfoTest, InvalidNullPointerPropValue) {
    size_t query_size = 0;
    ASSERT_SUCCESS(urKernelGetInfo(kernel, UR_KERNEL_INFO_NUM_ARGS, 0, nullptr,
                                   &query_size));
    ASSERT_EQ_RESULT(urKernelGetInfo(kernel, UR_KERNEL_INFO_NUM_ARGS,
                                     query_size, nullptr, nullptr),
                     UR_RESULT_ERROR_INVALID_NULL_POINTER);
}

TEST_P(urKernelGetInfoTest, InvalidNullPointerPropSizeRet) {
    ASSERT_EQ_RESULT(
        urKernelGetInfo(kernel, UR_KERNEL_INFO_NUM_ARGS, 0, nullptr, nullptr),
        UR_RESULT_ERROR_INVALID_NULL_POINTER);
}
