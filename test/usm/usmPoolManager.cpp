// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: MIT

#include "../unified_memory_allocation/common/pool.hpp"
#include "../unified_memory_allocation/common/provider.hpp"
#include "ur_pool_manager.hpp"

#include <uur/fixtures.h>

#include <unordered_set>

struct urUsmPoolManagerTest : public uur::urMultiDeviceContextTest,
                              ::testing::WithParamInterface<ur_usm_pool_flags_t> {}; // TODO: extend params over which we test

struct dummy_pool : uma_test::pool_base {
    struct Config {};
    uma_result_t initialize(uma_memory_provider_handle_t, size_t, const Config &) noexcept { return UMA_RESULT_SUCCESS; }
};

struct dummy_provider : uma_test::provider_base {
    uma_result_t initialize(bool readOnly) noexcept { return UMA_RESULT_SUCCESS; }
};

TEST_F(urUsmPoolManagerTest, poolIsPerTypeAndDevice) {
    usm::PoolConfigurations<dummy_pool::Config> configs;
    auto &devices = uur::DevicesEnvironment::instance->devices;
    auto poolFlags = this->GetParam();

    usm::uma_pool_manager_t poolManager;
    poolManager.initialize<dummy_provider, dummy_pool>(this->context, devices.size(), devices.data(), configs, poolFlags);

    auto forEach = [&](auto &&cb) {
        for (auto &device : devices) {
            for (auto type : {UR_USM_TYPE_HOST, UR_USM_TYPE_DEVICE, UR_USM_TYPE_SHARED}) {
                for (int flags = 1; flags <= UR_USM_MEM_FLAG_DEVICE_READ_ONLY; flags *= 2) {
                    for (int advice = 1; advice <= UR_MEM_ADVICE_BIAS_UNCACHED; advice++) {
                        usm::allocation_descriptor_t desc;
                        desc.hDevice = device;
                        desc.type = type;
                        desc.flags = flags;
                        desc.advice = ur_mem_advice_t(advice);
                        cb(desc);
                    }
                }
            }
        }
    };

    std::unordered_set<uma_memory_pool_handle_t> poolHandles;
    forEach([&](usm::allocation_descriptor_t &desc) {
        auto handle = poolManager.poolForDescriptor(desc);
        ASSERT_NE(handle, nullptr);
        poolHandles.emplace(handle);
    });

    // Each device has pools for Host, Device, Shared, SharedReadOnly only
    ASSERT_EQ(poolHandles.size(), 4 * devices.size());
}
