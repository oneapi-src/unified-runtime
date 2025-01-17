// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "fixtures.h"
#include "uur/known_failure.h"

using urEventGetProfilingInfoTest =
    uur::event::urEventTestWithParam<ur_profiling_info_t>;

TEST_P(urEventGetProfilingInfoTest, Success) {

    ur_profiling_info_t info_type = getParam();
    size_t size;
    ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
        urEventGetProfilingInfo(event, info_type, 0, nullptr, &size),
        info_type);
    ASSERT_EQ(size, 8);

    uint64_t time = 0x12341234;
    ASSERT_SUCCESS(
        urEventGetProfilingInfo(event, info_type, size, &time, nullptr));

    // Note: In theory it's possible for this test to run when the counter happens to equal
    // this value, but I assume that's so unlikely as to not worry about it
    ASSERT_NE(time, 0x12341234);
}

UUR_DEVICE_TEST_SUITE_P(urEventGetProfilingInfoTest,
                        ::testing::Values(UR_PROFILING_INFO_COMMAND_QUEUED,
                                          UR_PROFILING_INFO_COMMAND_SUBMIT,
                                          UR_PROFILING_INFO_COMMAND_START,
                                          UR_PROFILING_INFO_COMMAND_END,
                                          UR_PROFILING_INFO_COMMAND_COMPLETE),
                        uur::deviceTestWithParamPrinter<ur_profiling_info_t>);

using urEventGetProfilingInfoWithTimingComparisonTest = uur::event::urEventTest;

TEST_P(urEventGetProfilingInfoWithTimingComparisonTest, Success) {
    // AMD devices may report a "start" time before the "submit" time
    UUR_KNOWN_FAILURE_ON(uur::HIP{});

    // If a and b are supported, asserts that a <= b
    auto test_timing = [=](ur_profiling_info_t a, ur_profiling_info_t b) {
        std::stringstream trace{"Profiling Info: "};
        trace << a << " <= " << b;
        SCOPED_TRACE(trace.str());
        uint64_t a_time;
        auto result =
            urEventGetProfilingInfo(event, a, sizeof(a_time), &a_time, nullptr);
        if (result == UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION) {
            return;
        }
        ASSERT_SUCCESS(result);

        uint64_t b_time;
        result =
            urEventGetProfilingInfo(event, b, sizeof(b_time), &b_time, nullptr);
        if (result == UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION) {
            return;
        }
        ASSERT_SUCCESS(result);

        // Note: This assumes that the counter doesn't overflow
        ASSERT_LE(a_time, b_time);
    };

    test_timing(UR_PROFILING_INFO_COMMAND_QUEUED,
                UR_PROFILING_INFO_COMMAND_SUBMIT);
    test_timing(UR_PROFILING_INFO_COMMAND_SUBMIT,
                UR_PROFILING_INFO_COMMAND_START);
    test_timing(UR_PROFILING_INFO_COMMAND_START, UR_PROFILING_INFO_COMMAND_END);
    test_timing(UR_PROFILING_INFO_COMMAND_END,
                UR_PROFILING_INFO_COMMAND_COMPLETE);
}

UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(
    urEventGetProfilingInfoWithTimingComparisonTest);

using urEventGetProfilingInfoNegativeTest = uur::event::urEventTest;

TEST_P(urEventGetProfilingInfoNegativeTest, InvalidNullHandle) {
    UUR_KNOWN_FAILURE_ON(uur::NativeCPU{});

    ur_profiling_info_t info_type = UR_PROFILING_INFO_COMMAND_QUEUED;
    size_t size;
    ASSERT_SUCCESS(
        urEventGetProfilingInfo(event, info_type, 0, nullptr, &size));
    ASSERT_NE(size, 0);
    std::vector<uint8_t> data(size);

    /* Invalid hEvent */
    ASSERT_EQ_RESULT(
        urEventGetProfilingInfo(nullptr, info_type, 0, nullptr, &size),
        UR_RESULT_ERROR_INVALID_NULL_HANDLE);
}

TEST_P(urEventGetProfilingInfoNegativeTest, InvalidEnumeration) {
    size_t size;
    ASSERT_EQ_RESULT(urEventGetProfilingInfo(event,
                                             UR_PROFILING_INFO_FORCE_UINT32, 0,
                                             nullptr, &size),
                     UR_RESULT_ERROR_INVALID_ENUMERATION);
}

TEST_P(urEventGetProfilingInfoNegativeTest, InvalidValue) {
    UUR_KNOWN_FAILURE_ON(uur::NativeCPU{});

    ur_profiling_info_t info_type = UR_PROFILING_INFO_COMMAND_QUEUED;
    size_t size;
    ASSERT_SUCCESS(
        urEventGetProfilingInfo(event, info_type, 0, nullptr, &size));
    ASSERT_NE(size, 0);
    std::vector<uint8_t> data(size);

    /* Invalid propSize */
    ASSERT_EQ_RESULT(
        urEventGetProfilingInfo(event, info_type, 0, data.data(), nullptr),
        UR_RESULT_ERROR_INVALID_VALUE);
}

UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(urEventGetProfilingInfoNegativeTest);

struct urEventGetProfilingInfoForWaitWithBarrier : uur::urProfilingQueueTest {
    void SetUp() override {
        UUR_RETURN_ON_FATAL_FAILURE(urProfilingQueueTest::SetUp());
        ASSERT_SUCCESS(urMemBufferCreate(context, UR_MEM_FLAG_WRITE_ONLY, size,
                                         nullptr, &buffer));

        input.assign(count, 42);
        ur_event_handle_t membuf_event = nullptr;
        ASSERT_SUCCESS(urEnqueueMemBufferWrite(queue, buffer, false, 0, size,
                                               input.data(), 0, nullptr,
                                               &membuf_event));

        ASSERT_SUCCESS(
            urEnqueueEventsWaitWithBarrier(queue, 1, &membuf_event, &event));
        ASSERT_SUCCESS(urQueueFinish(queue));
    }

    void TearDown() override {
        UUR_RETURN_ON_FATAL_FAILURE(urProfilingQueueTest::TearDown());
    }

    const size_t count = 1024;
    const size_t size = sizeof(uint32_t) * count;
    ur_mem_handle_t buffer = nullptr;
    ur_event_handle_t event = nullptr;
    std::vector<uint32_t> input;
};

UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(urEventGetProfilingInfoForWaitWithBarrier);

TEST_P(urEventGetProfilingInfoForWaitWithBarrier, Success) {
    std::vector<uint8_t> submit_data(size);
    ASSERT_SUCCESS(urEventGetProfilingInfo(event,
                                           UR_PROFILING_INFO_COMMAND_START,
                                           size, submit_data.data(), nullptr));
    auto start_timing = reinterpret_cast<size_t *>(submit_data.data());
    ASSERT_NE(*start_timing, 0);

    std::vector<uint8_t> complete_data(size);
    ASSERT_SUCCESS(urEventGetProfilingInfo(event, UR_PROFILING_INFO_COMMAND_END,
                                           size, complete_data.data(),
                                           nullptr));
    auto end_timing = reinterpret_cast<size_t *>(complete_data.data());
    ASSERT_NE(*end_timing, 0);

    ASSERT_GE(*end_timing, *start_timing);
}
