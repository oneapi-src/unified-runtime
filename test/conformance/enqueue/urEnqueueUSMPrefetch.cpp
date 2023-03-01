// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: MIT

#include <uur/fixtures.h>

using urEnqueueUSMPrefetchWithParamTest =
    uur::urUSMDeviceAllocTestWithParam<ur_usm_migration_flag_t>;

UUR_TEST_SUITE_P(urEnqueueUSMPrefetchWithParamTest,
                 ::testing::Values(UR_USM_MIGRATION_FLAG_DEFAULT),
                 uur::deviceTestWithParamPrinter<ur_usm_migration_flag_t>);

TEST_P(urEnqueueUSMPrefetchWithParamTest, Success) {
    ur_event_handle_t prefetch_event = nullptr;
    ASSERT_SUCCESS(urEnqueueUSMPrefetch(queue, ptr, sizeof(int),
                                        getParam(), 0, nullptr,
                                        &prefetch_event));

    ASSERT_SUCCESS(urQueueFlush(queue));
    ASSERT_SUCCESS(urEventWait(1, &prefetch_event));

    const auto event_status =
        uur::GetEventInfo<ur_event_status_t>(prefetch_event,
                                             UR_EVENT_INFO_COMMAND_EXECUTION_STATUS);
    ASSERT_TRUE(event_status.has_value());
    ASSERT_EQ(event_status.value(), UR_EVENT_STATUS_COMPLETE);
    ASSERT_SUCCESS(urEventRelease(prefetch_event));
}

using urEnqueueUSMPrefetchTest = uur::urUSMDeviceAllocTest;
UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(urEnqueueUSMPrefetchTest);

TEST_P(urEnqueueUSMPrefetchTest, InvalidNullHandleQueue) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_NULL_HANDLE,
                     urEnqueueUSMPrefetch(nullptr, ptr, sizeof(int),
                                          UR_MEM_ADVICE_DEFAULT, 0, nullptr,
                                          nullptr));
}

TEST_P(urEnqueueUSMPrefetchTest, InvalidNullPointerMem) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_NULL_POINTER,
                     urEnqueueUSMPrefetch(queue, nullptr, sizeof(int),
                                          UR_MEM_ADVICE_DEFAULT, 0, nullptr,
                                          nullptr));
}

TEST_P(urEnqueueUSMPrefetchTest, InvalidEnumeration) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_ENUMERATION,
                     urEnqueueUSMPrefetch(queue, ptr, sizeof(int),
                                          UR_MEM_ADVICE_FORCE_UINT32, 0,
                                          nullptr,
                                          nullptr));
}

TEST_P(urEnqueueUSMPrefetchTest, InvalidSizeZero) {
    ASSERT_EQ_RESULT(
        UR_RESULT_ERROR_INVALID_SIZE,
        urEnqueueUSMPrefetch(queue, ptr, 0, UR_MEM_ADVICE_DEFAULT, 0, nullptr,
                             nullptr));
}

TEST_P(urEnqueueUSMPrefetchTest, InvalidSizeTooLarge) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_SIZE,
                     urEnqueueUSMPrefetch(queue, ptr, sizeof(int) * 2,
                                          UR_MEM_ADVICE_DEFAULT, 0, nullptr,
                                          nullptr));
}

TEST_P(urEnqueueUSMPrefetchTest, InvalidEventWaitList) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_EVENT_WAIT_LIST,
                     urEnqueueUSMPrefetch(queue, ptr, sizeof(int),
                                          UR_MEM_ADVICE_DEFAULT, 1, nullptr,
                                          nullptr));

    ur_event_handle_t validEvent;
    ASSERT_SUCCESS(urEnqueueEventsWait(queue, 0, nullptr, &validEvent));

    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_EVENT_WAIT_LIST,
                     urEnqueueUSMPrefetch(queue, ptr, sizeof(int),
                                          UR_MEM_ADVICE_DEFAULT, 0, &validEvent,
                                          nullptr));
}
