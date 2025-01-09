// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <uur/fixtures.h>
#include <vector>

using T = uint32_t;

struct urAsyncAllocTest : uur::urQueueTest {
    void SetUp() {
        UUR_RETURN_ON_FATAL_FAILURE(uur::urQueueTest::SetUp());

        host_vec = std::vector<T>(global_size, 0);
        ASSERT_EQ(host_vec.size(), global_size);
    }
    void TearDown() {
        UUR_RETURN_ON_FATAL_FAILURE(uur::urQueueTest::TearDown());
        if (pool) {
            UUR_RETURN_ON_FATAL_FAILURE(urUSMPoolRelease(pool));
        }
    }
    static constexpr T val = 42;
    static constexpr uint32_t global_size = 1e7;
    std::vector<T> host_vec;
    void *dev_ptr = nullptr;
    static constexpr size_t allocation_size = sizeof(val) * global_size;
    static constexpr size_t pool_size = 400;
    static constexpr ur_usm_pool_limits_desc_t limits_desc{
        UR_STRUCTURE_TYPE_USM_POOL_LIMITS_DESC, nullptr, pool_size, 1};
    ur_usm_pool_handle_t pool{0};
};

UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(urAsyncAllocTest);

#define SUCCESS_TEST(ALLOC_TYPE)                                               \
    TEST_P(urAsyncAllocTest, SuccessNoPool##ALLOC_TYPE) {                      \
        ur_event_handle_t alloc_ev;                                            \
        ASSERT_SUCCESS(urEnqueueUSM##ALLOC_TYPE##AllocExp(                     \
            queue, nullptr, 1, nullptr, 0, nullptr, &dev_ptr, &alloc_ev));     \
        ASSERT_SUCCESS(urEnqueueUSMFreeExp(queue, nullptr, dev_ptr, 1,         \
                                           &alloc_ev, nullptr));               \
    }

SUCCESS_TEST(Device)
SUCCESS_TEST(Shared)
SUCCESS_TEST(Host)

TEST_P(urAsyncAllocTest, SuccessWithPoolDevice) {
    ur_event_handle_t alloc_ev;
    ur_usm_pool_desc_t pool_desc{UR_STRUCTURE_TYPE_USM_POOL_DESC, &limits_desc,
                                 UR_USM_POOL_FLAG_USE_NATIVE_MEMORY_POOL_EXP};
    // Use device specific mem pool creation
    ASSERT_SUCCESS(urUSMPoolCreateExp(context, device, &pool_desc, &pool));

    ASSERT_SUCCESS(urEnqueueUSMDeviceAllocExp(queue, pool, 1, nullptr, 0,
                                              nullptr, &dev_ptr, &alloc_ev));
    ASSERT_SUCCESS(
        urEnqueueUSMFreeExp(queue, pool, dev_ptr, 1, &alloc_ev, nullptr));
}

#define SUCCESS_TEST_POOL(ALLOC_TYPE)                                          \
    TEST_P(urAsyncAllocTest, SuccessWithPool##) {                              \
        ur_event_handle_t alloc_ev;                                            \
        limits_desc->next = TTOD;                                              \
        ur_usm_pool_desc_t pool_desc{                                          \
            UR_STRUCTURE_TYPE_USM_POOL_DESC, &limits_desc,                     \
            UR_USM_POOL_FLAG_USE_NATIVE_MEMORY_POOL_EXP};                      \
        ASSERT_SUCCESS(urUSMPoolCreate(context, &pool_desc, &pool));           \
        ASSERT_SUCCESS(urEnqueueUSMDeviceAllocExp(                             \
            queue, pool, 1, nullptr, 0, nullptr, &dev_ptr, &alloc_ev));        \
        ASSERT_SUCCESS(                                                        \
            urEnqueueUSMFreeExp(queue, pool, dev_ptr, 1, &alloc_ev, nullptr)); \
    }
