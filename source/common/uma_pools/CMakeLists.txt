# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: MIT

add_library(uma_usm_pool STATIC
    usm_allocator_config.cpp
    usm_allocator.cpp
)

add_library(${PROJECT_NAME}::uma_usm_pool ALIAS uma_usm_pool)

target_link_libraries(uma_usm_pool PRIVATE unified_memory_allocation)
target_include_directories(uma_usm_pool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
