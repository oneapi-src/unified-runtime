# Copyright (C) 2024 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_library(benchmark_common INTERFACE fixtures.hpp)
target_link_libraries(benchmark_common INTERFACE
    ${PROJECT_NAME}::loader
    ${CMAKE_DL_LIBS}
    ${PROJECT_NAME}::headers
)
target_include_directories(benchmark_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${UR_CONFORMANCE_DEVICE_BINARIES_DIR}
)
target_compile_definitions(benchmark_common INTERFACE -DKERNEL_IL_PATH="${UR_CONFORMANCE_DEVICE_BINARIES_DIR}")
add_dependencies(benchmark_common generate_device_binaries)
