// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: MIT

#include "fixtures.h"

using urEventReleaseTest = uur::event::urEventReferenceTest;

/* Check that urEventRelease returns Success */
TEST_P(urEventReleaseTest, Success) {
    ASSERT_NO_FATAL_FAILURE(CreateEvent());
    ASSERT_SUCCESS(urEventRelease(event));
    EXPECT_NO_FATAL_FAILURE(Cleanup());
}

/* Check that urEventRelease decrements the reference count*/
TEST_P(urEventReleaseTest, CheckReferenceCount) {
    ASSERT_NO_FATAL_FAILURE(CreateEvent());
    ASSERT_SUCCESS(urEventRetain(event));
    ASSERT_NO_FATAL_FAILURE(checkEventReferenceCount(2));
    ASSERT_SUCCESS(urEventRelease(event));
    ASSERT_NO_FATAL_FAILURE(checkEventReferenceCount(1));
    ASSERT_SUCCESS(urEventRelease(event));
    EXPECT_NO_FATAL_FAILURE(Cleanup());
}

TEST_P(urEventReleaseTest, InvalidNullHandle) {
    ASSERT_EQ(urEventRelease(nullptr), UR_RESULT_ERROR_INVALID_NULL_HANDLE);
}

TEST_P(urEventReleaseTest, InvalidEvent) {
    ur_event_handle_t event{};
    ASSERT_EQ(urEventRelease(event), UR_RESULT_ERROR_INVALID_EVENT);
}

UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(urEventReleaseTest);
