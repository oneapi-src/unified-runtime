// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: MIT

#include "usm_pool.hpp"

#include "helpers.h"
#include "provider.h"

#include <uma/base.h>

#include <cstring>

TEST_F(umaTest, usmPoolBasic) {
    auto provider = uma::wrapProviderUnique(mallocProviderCreate());
    auto usmParams = USMAllocatorParameters{};

    usmParams.SlabMinSize = 4096;
    usmParams.MaxPoolableSize = 4096;
    usmParams.Capacity = 4;
    usmParams.MinBucketSize = 64;

    auto pool = uma::poolMakeUnique<UsmPool>(std::move(provider), usmParams);

    static constexpr size_t allocSize = 64;
    auto *ptr = umaPoolMalloc(pool.get(), allocSize);

    std::memset(ptr, 0, allocSize);

    umaPoolFree(pool.get(), ptr);
}
