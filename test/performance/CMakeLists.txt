# Copyright (C) 2024 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(FetchContent)

FetchContent_Declare(
    nanobench
    GIT_REPOSITORY https://github.com/martinus/nanobench.git
    GIT_TAG v4.3.11
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(nanobench)

add_subdirectory(common)

function(add_benchmark_adapter name adapter epochs iters)
    set(BENCH_NAME ${name}-${adapter})
    add_test(NAME ${BENCH_NAME}
        COMMAND ${name} ${epochs} ${iters}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    set_tests_properties(${BENCH_NAME} PROPERTIES
        ENVIRONMENT "UR_ADAPTERS_FORCE_LOAD=\"$<TARGET_FILE:ur_${adapter}>\""
        LABELS "benchmark;${adapter}"
    )
endfunction()

function(add_benchmark name epochs iters)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE
        nanobench
        benchmark_common
    )

    if(UR_BUILD_ADAPTER_L0 OR UR_BUILD_ADAPTER_ALL)
        add_benchmark_adapter(${name} adapter_level_zero ${epochs} ${iters})
    endif()
    if(UR_BUILD_ADAPTER_OPENCL OR UR_BUILD_ADAPTER_ALL)
        add_benchmark_adapter(${name} adapter_opencl ${epochs} ${iters})
    endif()
endfunction()

add_benchmark(enqueue_kernel 100 100)
