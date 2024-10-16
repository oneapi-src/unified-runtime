// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <uur/fixtures.h>

using urL0EnqueueAllocTest = uur::urQueueTest;
UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(urL0EnqueueAllocTest);

TEST_P(urL0EnqueueAllocTest, SuccessHostAlloc) {
    ur_device_usm_access_capability_flags_t hostUSMSupport = 0;
    ASSERT_SUCCESS(uur::GetDeviceUSMHostSupport(device, hostUSMSupport));
    if (!hostUSMSupport) {
        GTEST_SKIP() << "Host USM is not supported.";
    }

    void *Ptr = nullptr;
    size_t allocSize = sizeof(int);
    ur_event_handle_t AllocEvent = nullptr;
    ASSERT_SUCCESS(urEnqueueUSMHostAllocExp(queue, nullptr, allocSize, nullptr,
                                            0, nullptr, &Ptr, &AllocEvent));
    ASSERT_SUCCESS(urQueueFinish(queue));
    ASSERT_NE(Ptr, nullptr);
    ASSERT_NE(AllocEvent, nullptr);

    *(int *)Ptr = 0xC0FFEE;

    ur_event_handle_t FreeEvent = nullptr;
    ASSERT_SUCCESS(
        urEnqueueUSMFreeExp(queue, nullptr, Ptr, 0, nullptr, &FreeEvent));
    ASSERT_SUCCESS(urQueueFinish(queue));
    ASSERT_NE(FreeEvent, nullptr);
}
