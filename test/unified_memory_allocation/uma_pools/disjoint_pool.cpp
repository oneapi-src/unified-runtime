// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: MIT

#include "disjoint_pool.hpp"
#include "memoryPoolOps.hpp"

static DisjointPool::Config poolConfig() {
    DisjointPool::Config config{};
    config.SlabMinSize = 4096;
    config.MaxPoolableSize = 4096;
    config.Capacity = 4;
    config.MinBucketSize = 64;
    return config;
}

using poolTest = poolTest;

INSTANTIATE_TEST_SUITE_P(disjointPoolTests,
                         poolTest,
                         ::testing::Values(
                             [] { return uma::poolMakeUnique<DisjointPool>(
                                      uma::wrapProviderUnique(mallocProviderCreate()), poolConfig()); }));
