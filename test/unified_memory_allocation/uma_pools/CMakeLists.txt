# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: MIT

add_uma_test(usm_pool usm_pool.cpp)
target_link_libraries(uma_test-usm_pool PRIVATE uma_usm_pool)
