# Copyright (C) 2022 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(ur_device_selector INTERFACE)

add_library(${PROJECT_NAME}::device_selector ALIAS ur_device_selector)

target_include_directories(ur_device_selector INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ur_device_selector INTERFACE
    ${PROJECT_NAME}::headers
    ${PROJECT_NAME}::common
)
