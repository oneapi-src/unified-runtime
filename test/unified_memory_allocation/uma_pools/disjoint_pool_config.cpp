// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: MIT

#include "helpers.hpp"
#include "usm_pool_config.hpp"

using PoolDescriptor = usm_pool::PoolDescriptor;

enum DESCRIPTOR_TYPES { HOST,
                        DEVICE,
                        SHARED,
                        READ_ONLY_SHARED };

const std::map<DESCRIPTOR_TYPES, PoolDescriptor>
    DESCRIPTORS = {{HOST, PoolDescriptor(UR_USM_TYPE_HOST)},
                   {DEVICE, PoolDescriptor(UR_USM_TYPE_DEVICE)},
                   {SHARED, PoolDescriptor(UR_USM_TYPE_SHARED)},
                   {READ_ONLY_SHARED, PoolDescriptor(UR_USM_TYPE_SHARED, true)}};

class disjointPoolConfigTests : public testing::TestWithParam<std::string> {
  protected:
    const int hostMaxPoolableSizeDefault = 2 * 1024 * 1024;
    const int hostCapacityDefault = 4;
    const int hostSlabMinSizeDefault = 64 * 1024;

    const int deviceMaxPoolableSizeDefault = 4 * 1024 * 1024;
    const int deviceCapacityDefault = 4;
    const int deviceSlabMinSizeDefault = 64 * 1024;

    const int sharedMaxPoolableSizeDefault = 0;
    const int sharedCapacityDefault = 0;
    const int sharedSlabMinSizeDefault = 2 * 1024 * 1024;

    const int readOnlySharedMaxPoolableSizeDefault = 4 * 1024 * 1024;
    const int readOnlySharedCapacityDefault = 4;
    const int readOnlySharedSlabMinSizeDefault = 2 * 1024 * 1024;

    const int maxSizeDefault = 16 * 1024 * 1024;
};

// invalid configs for which descriptors' parameters should be set to default values
INSTANTIATE_TEST_SUITE_P(configsForDefaultValues,
                         disjointPoolConfigTests,
                         testing::Values("",
                                         "ab12cdefghi34jk56lmn78opr910",
                                         "1;32M;foo:0,3,2m;device:1M,4,64k",
                                         "132M;host:4m,3,2m,4m;device:1M,4,64k",
                                         "1;32M;abdc123;;;device:1M,4,64k",
                                         "1;32M;host:0,3,2m,4;device:1M,4,64k,5;host:1,8,4m,100",
                                         "32M;1;host:1M,4,64k;device:1m,4,64K;shared:0,3,1M"));

TEST_F(disjointPoolConfigTests, disjointPoolConfigTest) {
    // test if the returned unordered map contains only the valid configs
    usm_pool::PoolConfigurations<typename DisjointPool::Config> configsMap =
        usm_pool::getPoolConfigurationsFor<DisjointPool>();

    // check if all the valid configs are present
    for (const auto &descriptor : DESCRIPTORS) {
        ASSERT_EQ(configsMap.count(descriptor.second), 1);
    }

    // check if there are no more configs in the unordered map
    ASSERT_EQ(configsMap.size(), 4);
}

TEST_F(disjointPoolConfigTests, disjointPoolConfigStringEnabledBuffersTest) {
    // test for valid string with enabled buffers-- (values to configs should be
    // parsed from string)
    std::string config = "1;32M;host:1M,3,32k;device:1m,2,32m;shared:2m,3,1M;"
                         "read_only_shared:0,0,3M";
    usm_pool::PoolConfigurations<typename DisjointPool::Config> configsMap =
        usm_pool::getPoolConfigurationsFor<DisjointPool>(config);

    // test for host
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).MaxPoolableSize, 1 * 1024 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).Capacity, 3);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).SlabMinSize, 32 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).limits->MaxSize, 32 * 1024 * 1024);

    // test for device
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).MaxPoolableSize, 1 * 1024 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).Capacity, 2);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).SlabMinSize, 32 * 1024 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).limits->MaxSize, 32 * 1024 * 1024);

    // test for shared
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).MaxPoolableSize, 2 * 1024 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).Capacity, 3);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).SlabMinSize, 1 * 1024 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).limits->MaxSize, 32 * 1024 * 1024);

    // test for read-only shared
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).MaxPoolableSize, 0);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).Capacity, 0);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).SlabMinSize, 3 * 1024 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).limits->MaxSize, 32 * 1024 * 1024);
}

TEST_F(disjointPoolConfigTests, disjointPoolConfigStringImpartialEnabledBuffersTest) {
    // test for valid impartial string with enabled buffers-- (values to configs should be
    // parsed from string if present, the rest should be default
    // set by getConfigurationsFor)
    std::string config = "1;32M;host:1M,2,16k;shared:64k,3,1M;";
    usm_pool::PoolConfigurations<typename DisjointPool::Config> configsMap =
        usm_pool::getPoolConfigurationsFor<DisjointPool>(config);

    // test for host
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).MaxPoolableSize, 1 * 1024 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).Capacity, 2);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).SlabMinSize, 16 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).limits->MaxSize, 32 * 1024 * 1024);

    // test for device
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).MaxPoolableSize, deviceMaxPoolableSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).Capacity, deviceCapacityDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).SlabMinSize, deviceSlabMinSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).limits->MaxSize, 32 * 1024 * 1024);

    // test for shared
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).MaxPoolableSize, 64 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).Capacity, 3);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).SlabMinSize, 1 * 1024 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).limits->MaxSize, 32 * 1024 * 1024);

    // test for read-only shared
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).MaxPoolableSize, readOnlySharedMaxPoolableSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).Capacity, readOnlySharedCapacityDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).SlabMinSize, readOnlySharedSlabMinSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).limits->MaxSize, 32 * 1024 * 1024);
}

TEST_F(disjointPoolConfigTests, disjointPoolConfigStringDisabledBuffersTest) {
    // test for valid string with disabled buffers-- (the map should be empty)
    std::string config = "0;32M;host:1M,4,64k;device:1m,4,64K;shared:0,3,2M";
    usm_pool::PoolConfigurations<typename DisjointPool::Config> configsMap =
        usm_pool::getPoolConfigurationsFor<DisjointPool>(config);

    ASSERT_TRUE(configsMap.empty());
}

TEST_P(disjointPoolConfigTests, disjointPoolConfigInvalid) {
    std::string config = GetParam();
    usm_pool::PoolConfigurations<typename DisjointPool::Config> configsMap =
        usm_pool::getPoolConfigurationsFor<DisjointPool>(config);

    // test for host
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).MaxPoolableSize, hostMaxPoolableSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).Capacity, hostCapacityDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).SlabMinSize, hostSlabMinSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).limits->MaxSize, maxSizeDefault);

    // test for device
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).MaxPoolableSize, deviceMaxPoolableSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).Capacity, deviceCapacityDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).SlabMinSize, deviceSlabMinSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).limits->MaxSize, maxSizeDefault);

    // test for shared
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).MaxPoolableSize, sharedMaxPoolableSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).Capacity, sharedCapacityDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).SlabMinSize, sharedSlabMinSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).limits->MaxSize, maxSizeDefault);

    // test for read-only shared
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).MaxPoolableSize, readOnlySharedMaxPoolableSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).Capacity, readOnlySharedCapacityDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).SlabMinSize, readOnlySharedSlabMinSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).limits->MaxSize, maxSizeDefault);
}

TEST_F(disjointPoolConfigTests, disjointPoolConfigStringInvalidEnabledBuffers) {
    // test for a string with invalid parameter for EnabledBuffers--
    // (it should be set to a default)
    std::string config = "-5;32M;host:0,3,32k,4m;device:1M,2,32m";
    usm_pool::PoolConfigurations<typename DisjointPool::Config> configsMap =
        usm_pool::getPoolConfigurationsFor<DisjointPool>(config);

    // test for host
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).MaxPoolableSize, 0);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).Capacity, 3);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).SlabMinSize, 32 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).limits->MaxSize, 32 * 1024 * 1024);

    // test for device
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).MaxPoolableSize, 1 * 1024 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).Capacity, 2);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).SlabMinSize, 32 * 1024 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).limits->MaxSize, 32 * 1024 * 1024);

    // test for shared
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).MaxPoolableSize, sharedMaxPoolableSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).Capacity, sharedCapacityDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).SlabMinSize, sharedSlabMinSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).limits->MaxSize, 32 * 1024 * 1024);

    // test for read-only shared
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).MaxPoolableSize, readOnlySharedMaxPoolableSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).Capacity, readOnlySharedCapacityDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).SlabMinSize, readOnlySharedSlabMinSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).limits->MaxSize, 32 * 1024 * 1024);
}

TEST_F(disjointPoolConfigTests, disjointPoolConfigStringTooManyParameters) {
    // test for when too many parameters are passed-- (the extra parameters
    // should be ignored)
    std::string config = "1;32M;host:0,3,2m,4;device:1M,5,32k,5";
    usm_pool::PoolConfigurations<typename DisjointPool::Config> configsMap =
        usm_pool::getPoolConfigurationsFor<DisjointPool>(config);

    // test for host
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).MaxPoolableSize, 0);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).Capacity, 3);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).SlabMinSize, 2 * 1024 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(HOST)).limits->MaxSize, 32 * 1024 * 1024);

    // test for device
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).MaxPoolableSize, 1 * 1024 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).Capacity, 5);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).SlabMinSize, 32 * 1024);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(DEVICE)).limits->MaxSize, 32 * 1024 * 1024);

    // test for shared
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).MaxPoolableSize, sharedMaxPoolableSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).Capacity, sharedCapacityDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).SlabMinSize, sharedSlabMinSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(SHARED)).limits->MaxSize, 32 * 1024 * 1024);

    // test for read-only shared
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).MaxPoolableSize, readOnlySharedMaxPoolableSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).Capacity, readOnlySharedCapacityDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).SlabMinSize, readOnlySharedSlabMinSizeDefault);
    ASSERT_EQ(configsMap.at(DESCRIPTORS.at(READ_ONLY_SHARED)).limits->MaxSize, 32 * 1024 * 1024);
}
