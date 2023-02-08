// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: MIT

#include "fixtures.h"

using urEventRetainTest = uur::event::urEventReferenceTest;

/* Check that urEventRetain returns Success */
TEST_P(urEventRetainTest, Success) {
    ASSERT_NO_FATAL_FAILURE(CreateEvent());
    ASSERT_SUCCESS(urEventRetain(event));
    ASSERT_SUCCESS(urEventRelease(event));
    ASSERT_SUCCESS(urEventRelease(event));
    EXPECT_NO_FATAL_FAILURE(Cleanup());
}

/* Check that urEventRetain increments the reference count */
TEST_P(urEventRetainTest, CheckReferenceCount) {

    ASSERT_NO_FATAL_FAILURE(CreateEvent());
    ASSERT_NO_FATAL_FAILURE(checkEventReferenceCount(1));
    ASSERT_SUCCESS(urEventRetain(event));
    ASSERT_NO_FATAL_FAILURE(checkEventReferenceCount(2));
    ASSERT_SUCCESS(urEventRelease(event));
    ASSERT_SUCCESS(urEventRelease(event));
    EXPECT_NO_FATAL_FAILURE(Cleanup());
}

TEST_P(urEventRetainTest, InvalidNullHandle) {
    ASSERT_EQ(urEventRetain(nullptr), UR_RESULT_ERROR_INVALID_NULL_HANDLE);
}

TEST_P(urEventRetainTest, InvalidEvent) {
    ur_event_handle_t event{};
    ASSERT_EQ(urEventRetain(event), UR_RESULT_ERROR_INVALID_EVENT);
}

UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(urEventRetainTest);
