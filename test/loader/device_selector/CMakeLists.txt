# Copyright (C) 2022 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_ur_executable(test-device-selector
    backend.cpp
    device.cpp
    filter.cpp
    matcher.cpp
)

target_link_libraries(test-device-selector PRIVATE
    ${PROJECT_NAME}::device_selector
    GTest::gtest_main
)
