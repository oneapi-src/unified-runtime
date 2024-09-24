// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <uur/fixtures.h>

using urProgramCompileTest = uur::urProgramTest;
UUR_INSTANTIATE_KERNEL_TEST_SUITE_P(urProgramCompileTest);

TEST_P(urProgramCompileTest, Success) {
    ASSERT_SUCCESS(urProgramCompile(program, 1, &device, nullptr));
}

TEST_P(urProgramCompileTest, InvalidNullHandleProgram) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_NULL_HANDLE,
                     urProgramCompile(nullptr, 1, &device, nullptr));
}

TEST_P(urProgramCompileTest, InvalidNullPointerDevices) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_NULL_POINTER,
                     urProgramCompile(program, 1, nullptr, nullptr));
}

TEST_P(urProgramCompileTest, InvalidSizeNumDevices) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_SIZE,
                     urProgramCompile(program, 0, &device, nullptr));
}
