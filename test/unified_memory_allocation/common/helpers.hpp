// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: MIT

#ifndef UR_UMA_TEST_HELPERS_H
#define UR_UMA_TEST_HELPERS_H

#include <gtest/gtest.h>

#include <uma/memory_pool.h>
#include <uma/memory_provider.h>

#include "pool.h"
#include "provider.h"
#include "uma_helpers.hpp"

#include <memory>

namespace uma {

auto wrapPoolUnique(uma_memory_pool_handle_t hPool) {
    return ur_memory_pool_handle_unique(hPool, &umaPoolDestroy);
}

auto wrapProviderUnique(uma_memory_provider_handle_t hProvider) {
    return ur_memory_provider_handle_unique(hProvider, &umaMemoryProviderDestroy);
}

struct test : ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

// accepts function that creates memory pool (gtest disallows passing unique_ptrs here)
struct poolTest : test, ::testing::WithParamInterface<std::function<ur_memory_pool_handle_unique()>> {
    poolTest() : pool(nullptr, nullptr) {}

    void SetUp() {
        test::SetUp();
        this->pool = this->GetParam()();
    }

    void TearDown() override {
        test::TearDown();
    }

    ur_memory_pool_handle_unique pool;
};

} // namespace uma

#endif // UR_UMA_TEST_HELPERS_H
