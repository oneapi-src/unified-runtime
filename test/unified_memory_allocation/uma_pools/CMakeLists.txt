# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: MIT

add_uma_test(disjointPool disjoint_pool.cpp)
add_uma_test(disjointPoolConfig disjoint_pool_config.cpp)
target_include_directories(uma_test-disjointPool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(uma_test-disjointPool PRIVATE disjoint_pool ${PROJECT_NAME}::common)
