# Copyright (C) 2022-2023 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/state.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utility/file.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utility/statistics.cpp)

function(add_benchmark name)
    add_executable(${name}
        ${COMMON_SOURCES}
        ${ARGN})

    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/utility
        ${CMAKE_CURRENT_SOURCE_DIR}/../conformance/testing/include) # for raii helpers

    target_link_libraries(${name} PRIVATE
        ${PROJECT_NAME}::loader
        ${PROJECT_NAME}::headers)
endfunction()

add_benchmark(submit_kernel benchmarks/api_overhead/submit_kernel.cpp)
