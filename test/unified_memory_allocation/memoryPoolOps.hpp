// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: MIT

#include "helpers.hpp"
#include <cstring>

#ifndef UR_UMA_MEMORY_POOL_OPS_H
#define UR_UMA_MEMORY_POOL_OPS_H

using poolTest = uma::poolTest;

TEST_P(poolTest, allocFree) {
    static constexpr size_t allocSize = 64;
    auto *ptr = umaPoolMalloc(pool.get(), allocSize);
    ASSERT_NE(ptr, nullptr);
    std::memset(ptr, 0, allocSize);
    umaPoolFree(pool.get(), ptr);
}

TEST_P(poolTest, pow2AlignedAlloc) {
    static constexpr size_t allocSize = 64;
    static constexpr size_t maxAlignment = (1u << 22);

    for (size_t alignment = 1; alignment <= maxAlignment; alignment <<= 1) {
        std::cout << alignment << std::endl;
        auto *ptr = umaPoolAlignedMalloc(pool.get(), allocSize, alignment);
        ASSERT_NE(ptr, nullptr);
        std::memset(ptr, 0, allocSize);
        umaPoolFree(pool.get(), ptr);
    }
}

#endif /* UR_UMA_MEMORY_POOL_OPS_H */
