# Copyright (C) 2022-2023 Intel Corporation
# SPDX-License-Identifier: MIT

add_library(common INTERFACE)
add_library(${PROJECT_NAME}::common ALIAS common)

target_include_directories(common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(unified_memory_allocation)
target_link_libraries(common INTERFACE unified_memory_allocation)

add_subdirectory(uma_pools)
target_include_directories(disjoint_pool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(common INTERFACE disjoint_pool)
