// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <uur/fixtures.h>

using urProgramBuildTest = uur::urProgramTest;
UUR_INSTANTIATE_KERNEL_TEST_SUITE_P(urProgramBuildTest);

TEST_P(urProgramBuildTest, Success) {
    ASSERT_SUCCESS(urProgramBuild(program, 1, &device, nullptr));
}

TEST_P(urProgramBuildTest, InvalidNullHandleProgram) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_NULL_HANDLE,
                     urProgramBuild(nullptr, 1, &device, nullptr));
}

TEST_P(urProgramBuildTest, InvalidNullPointerDevices) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_NULL_POINTER,
                     urProgramBuild(program, 1, nullptr, nullptr));
}

TEST_P(urProgramBuildTest, InvalidSizeNumDevices) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_SIZE,
                     urProgramBuild(program, 0, &device, nullptr));
}
