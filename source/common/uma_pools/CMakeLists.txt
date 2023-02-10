# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: MIT

add_library(disjoint_pool STATIC
    disjoint_pool.cpp
)

add_library(${PROJECT_NAME}::disjoint_pool ALIAS disjoint_pool)

target_link_libraries(disjoint_pool PRIVATE
    unified_memory_allocation
    ${PROJECT_NAME}::headers)

target_include_directories(disjoint_pool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
