// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "fixtures.h"

// Tests non-kernel commands using ur events for synchronization work as expected
struct CommandEventSyncTest : uur::command_buffer::urCommandBufferExpTest {
    void SetUp() override {
        UUR_RETURN_ON_FATAL_FAILURE(urCommandBufferExpTest::SetUp());

        ur_bool_t event_support = false;
        ASSERT_SUCCESS(urDeviceGetInfo(
            device, UR_DEVICE_INFO_COMMAND_BUFFER_EVENT_SUPPORT_EXP,
            sizeof(ur_bool_t), &event_support, nullptr));
        if (!event_support) {
            GTEST_SKIP() << "External event sync is not supported by device.";
        }

        ur_queue_flags_t flags = UR_QUEUE_FLAG_SUBMISSION_BATCHED;
        ur_queue_properties_t props = {
            /*.stype =*/UR_STRUCTURE_TYPE_QUEUE_PROPERTIES,
            /*.pNext =*/nullptr,
            /*.flags =*/flags,
        };
        ASSERT_SUCCESS(urQueueCreate(context, device, &props, &queue));
        ASSERT_NE(queue, nullptr);

        for (auto &device_ptr : device_ptrs) {
            ASSERT_SUCCESS(urUSMDeviceAlloc(context, device, nullptr, nullptr,
                                            allocation_size, &device_ptr));
            ASSERT_NE(device_ptr, nullptr);
        }

        for (auto &buffer : buffers) {
            ASSERT_SUCCESS(urMemBufferCreate(context, UR_MEM_FLAG_READ_WRITE,
                                             allocation_size, nullptr,
                                             &buffer));
            ASSERT_NE(buffer, nullptr);
        }
    }

    virtual void TearDown() override {
        for (auto &device_ptr : device_ptrs) {
            if (device_ptr) {
                EXPECT_SUCCESS(urUSMFree(context, device_ptr));
            }
        }

        for (auto &event : external_events) {
            if (event) {
                EXPECT_SUCCESS(urEventRelease(event));
            }
        }

        for (auto &buffer : buffers) {
            if (buffer) {
                EXPECT_SUCCESS(urMemRelease(buffer));
            }
        }

        if (queue) {
            ASSERT_SUCCESS(urQueueRelease(queue));
        }

        UUR_RETURN_ON_FATAL_FAILURE(urCommandBufferExpTest::TearDown());
    }

    std::array<void *, 2> device_ptrs = {nullptr, nullptr};
    std::array<ur_mem_handle_t, 2> buffers = {nullptr, nullptr};
    std::array<ur_event_handle_t, 6> external_events = {
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    std::array<ur_exp_command_buffer_sync_point_t, 2> sync_points = {0, 0};
    ur_queue_handle_t queue = nullptr;
    static constexpr size_t elements = 64;
    static constexpr size_t allocation_size = sizeof(uint32_t) * elements;
};

UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(CommandEventSyncTest);

TEST_P(CommandEventSyncTest, USMMemcpyExp) {
    // Get wait event from queue fill on ptr 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, device_ptrs[0], sizeof(patternX),
                                    &patternX, allocation_size, 0, nullptr,
                                    &external_events[0]));

    // Command to fill ptr 1
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urCommandBufferAppendUSMFillExp(
        cmd_buf_handle, device_ptrs[1], &patternY, sizeof(patternY),
        allocation_size, 0, nullptr, 0, nullptr, &sync_points[0], nullptr,
        nullptr));

    // Test command overwriting ptr 1 with ptr 0 command based on queue event
    ASSERT_SUCCESS(urCommandBufferAppendUSMMemcpyExp(
        cmd_buf_handle, device_ptrs[1], device_ptrs[0], allocation_size, 1,
        &sync_points[0], 1, &external_events[0], nullptr, &external_events[1],
        nullptr));
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(cmd_buf_handle));
    ASSERT_SUCCESS(
        urCommandBufferEnqueueExp(cmd_buf_handle, queue, 0, nullptr, nullptr));

    // Queue read ptr 1 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, host_enqueue_ptr.data(),
                                      device_ptrs[1], allocation_size, 1,
                                      &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternX);
    }
}

TEST_P(CommandEventSyncTest, USMFillExp) {
    // Get wait event from queue fill on ptr 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, device_ptrs[0], sizeof(patternX),
                                    &patternX, allocation_size, 0, nullptr,
                                    &external_events[0]));

    // Test fill command overwriting ptr 0 waiting on queue event
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urCommandBufferAppendUSMFillExp(
        cmd_buf_handle, device_ptrs[0], &patternY, sizeof(patternY),
        allocation_size, 0, nullptr, 1, &external_events[0], nullptr,
        &external_events[1], nullptr));
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(cmd_buf_handle));
    ASSERT_SUCCESS(
        urCommandBufferEnqueueExp(cmd_buf_handle, queue, 0, nullptr, nullptr));

    // Queue read ptr 0 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, host_enqueue_ptr.data(),
                                      device_ptrs[0], allocation_size, 1,
                                      &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }
}

TEST_P(CommandEventSyncTest, MemBufferCopyExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Command to fill buffer 1
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferFillExp(
        cmd_buf_handle, buffers[1], &patternY, sizeof(patternY), 0,
        allocation_size, 0, nullptr, 0, nullptr, &sync_points[0], nullptr,
        nullptr));

    // Test command overwriting buffer 1 with buffer 0 command based on queue event
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferCopyExp(
        cmd_buf_handle, buffers[0], buffers[1], 0, 0, allocation_size, 1,
        &sync_points[0], 1, &external_events[0], nullptr, &external_events[1],
        nullptr));
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(cmd_buf_handle));
    ASSERT_SUCCESS(
        urCommandBufferEnqueueExp(cmd_buf_handle, queue, 0, nullptr, nullptr));

    // Queue read buffer 1 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[1], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternX);
    }
}

TEST_P(CommandEventSyncTest, MemBufferCopyRectExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Command to fill buffer 1
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferFillExp(
        cmd_buf_handle, buffers[1], &patternY, sizeof(patternY), 0,
        allocation_size, 0, nullptr, 0, nullptr, &sync_points[0], nullptr,
        nullptr));

    // Test command overwriting buffer 1 with buffer 0 command based on queue event
    ur_rect_offset_t src_origin{0, 0, 0};
    ur_rect_offset_t dst_origin{0, 0, 0};
    constexpr size_t rect_buffer_row_size = 16;
    ur_rect_region_t region{rect_buffer_row_size, rect_buffer_row_size, 1};
    size_t src_row_pitch = rect_buffer_row_size;
    size_t src_slice_pitch = allocation_size;
    size_t dst_row_pitch = rect_buffer_row_size;
    size_t dst_slice_pitch = allocation_size;
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferCopyRectExp(
        cmd_buf_handle, buffers[0], buffers[1], src_origin, dst_origin, region,
        src_row_pitch, src_slice_pitch, dst_row_pitch, dst_slice_pitch, 1,
        &sync_points[0], 1, &external_events[0], nullptr, &external_events[1],
        nullptr));
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(cmd_buf_handle));
    ASSERT_SUCCESS(
        urCommandBufferEnqueueExp(cmd_buf_handle, queue, 0, nullptr, nullptr));

    // Queue read buffer 1 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[1], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternX);
    }
}

TEST_P(CommandEventSyncTest, MemBufferReadExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Test command reading buffer 0 based on queue event
    std::array<uint32_t, elements> host_command_ptr{};
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferReadExp(
        cmd_buf_handle, buffers[0], 0, allocation_size, host_command_ptr.data(),
        0, nullptr, 1, &external_events[0], nullptr, &external_events[1],
        nullptr));
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(cmd_buf_handle));
    ASSERT_SUCCESS(
        urCommandBufferEnqueueExp(cmd_buf_handle, queue, 0, nullptr, nullptr));

    // Overwrite buffer 0 based on event returned from command-buffer command,
    // then read back to verify ordering
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(
        queue, buffers[0], &patternY, sizeof(patternY), 0, allocation_size, 1,
        &external_events[1], &external_events[2]));
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[2], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_command_ptr[i], patternX);
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }
}

TEST_P(CommandEventSyncTest, MemBufferReadRectExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Test command reading buffer 0 based on queue event
    std::array<uint32_t, elements> host_command_ptr{};
    ur_rect_offset_t buffer_offset = {0, 0, 0};
    ur_rect_offset_t host_offset = {0, 0, 0};
    constexpr size_t rect_buffer_row_size = 16;
    ur_rect_region_t region = {rect_buffer_row_size, rect_buffer_row_size, 1};
    size_t buffer_row_pitch = rect_buffer_row_size;
    size_t buffer_slice_pitch = allocation_size;
    size_t host_row_pitch = rect_buffer_row_size;
    size_t host_slice_pitch = allocation_size;
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferReadRectExp(
        cmd_buf_handle, buffers[0], buffer_offset, host_offset, region,
        buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch,
        host_command_ptr.data(), 0, nullptr, 1, &external_events[0], nullptr,
        &external_events[1], nullptr));
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(cmd_buf_handle));
    ASSERT_SUCCESS(
        urCommandBufferEnqueueExp(cmd_buf_handle, queue, 0, nullptr, nullptr));

    // Overwrite buffer 0 based on event returned from command-buffer command,
    // then read back to verify ordering
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(
        queue, buffers[0], &patternY, sizeof(patternY), 0, allocation_size, 1,
        &external_events[1], &external_events[2]));
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[2], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_command_ptr[i], patternX);
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }
}

TEST_P(CommandEventSyncTest, MemBufferWriteExp) {

    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Test command overwriting buffer 0 based on queue event
    std::array<uint32_t, elements> host_command_ptr{};
    uint32_t patternY = 0xA;
    std::fill(host_command_ptr.begin(), host_command_ptr.end(), patternY);
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferWriteExp(
        cmd_buf_handle, buffers[0], 0, allocation_size, host_command_ptr.data(),
        0, nullptr, 1, &external_events[0], nullptr, &external_events[1],
        nullptr));
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(cmd_buf_handle));
    ASSERT_SUCCESS(
        urCommandBufferEnqueueExp(cmd_buf_handle, queue, 0, nullptr, nullptr));

    // Read back buffer 0 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY) << i;
    }
}

TEST_P(CommandEventSyncTest, MemBufferWriteRectExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Test command overwriting buffer 0 based on queue event
    std::array<uint32_t, elements> host_command_ptr{};
    uint32_t patternY = 0xA;
    std::fill(host_command_ptr.begin(), host_command_ptr.end(), patternY);

    ur_rect_offset_t buffer_offset = {0, 0, 0};
    ur_rect_offset_t host_offset = {0, 0, 0};
    constexpr size_t rect_buffer_row_size = 16;
    ur_rect_region_t region = {rect_buffer_row_size, rect_buffer_row_size, 1};
    size_t buffer_row_pitch = rect_buffer_row_size;
    size_t buffer_slice_pitch = allocation_size;
    size_t host_row_pitch = rect_buffer_row_size;
    size_t host_slice_pitch = allocation_size;
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferWriteRectExp(
        cmd_buf_handle, buffers[0], buffer_offset, host_offset, region,
        buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch,
        host_command_ptr.data(), 0, nullptr, 1, &external_events[0], nullptr,
        &external_events[1], nullptr));
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(cmd_buf_handle));
    ASSERT_SUCCESS(
        urCommandBufferEnqueueExp(cmd_buf_handle, queue, 0, nullptr, nullptr));

    // Read back buffer 0 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY) << i;
    }
}

TEST_P(CommandEventSyncTest, MemBufferFillExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Test fill command overwriting buffer 0 based on queue event
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferFillExp(
        cmd_buf_handle, buffers[0], &patternY, sizeof(patternY), 0,
        allocation_size, 0, nullptr, 1, &external_events[0], nullptr,
        &external_events[1], nullptr));
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(cmd_buf_handle));
    ASSERT_SUCCESS(
        urCommandBufferEnqueueExp(cmd_buf_handle, queue, 0, nullptr, nullptr));

    // Queue read buffer 0 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }
}

TEST_P(CommandEventSyncTest, USMPrefetchExp) {
    // Get wait event from queue fill on ptr 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, device_ptrs[0], sizeof(patternX),
                                    &patternX, allocation_size, 0, nullptr,
                                    &external_events[0]));

    // Test prefetch command waiting on queue event
    ASSERT_SUCCESS(urCommandBufferAppendUSMPrefetchExp(
        cmd_buf_handle, device_ptrs[1], allocation_size, 0 /* migration flags*/,
        0, nullptr, 1, &external_events[0], nullptr, &external_events[1],
        nullptr));
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(cmd_buf_handle));
    ASSERT_SUCCESS(
        urCommandBufferEnqueueExp(cmd_buf_handle, queue, 0, nullptr, nullptr));

    // Queue read ptr 0 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, host_enqueue_ptr.data(),
                                      device_ptrs[0], allocation_size, 1,
                                      &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternX);
    }
}

TEST_P(CommandEventSyncTest, USMAdviseExp) {
    // Get wait event from queue fill on ptr 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, device_ptrs[0], sizeof(patternX),
                                    &patternX, allocation_size, 0, nullptr,
                                    &external_events[0]));

    // Test advise command waiting on queue event
    ASSERT_SUCCESS(urCommandBufferAppendUSMAdviseExp(
        cmd_buf_handle, device_ptrs[0], allocation_size, 0 /* advice flags*/, 0,
        nullptr, 1, &external_events[0], nullptr, &external_events[1],
        nullptr));
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(cmd_buf_handle));
    ASSERT_SUCCESS(
        urCommandBufferEnqueueExp(cmd_buf_handle, queue, 0, nullptr, nullptr));

    // Queue read ptr 0 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, host_enqueue_ptr.data(),
                                      device_ptrs[0], allocation_size, 1,
                                      &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternX);
    }
}

// Tests non-kernel commands using ur events for synchronization can be
// updated
struct CommandEventSyncUpdateTest : CommandEventSyncTest {
    void SetUp() override {
        UUR_RETURN_ON_FATAL_FAILURE(CommandEventSyncTest::SetUp());

        if (!updatable_command_buffer_support) {
            GTEST_SKIP() << "External event update is not supported by device.";
        }

        // Create a command-buffer with update enabled.
        ur_exp_command_buffer_desc_t desc{
            UR_STRUCTURE_TYPE_EXP_COMMAND_BUFFER_DESC, nullptr, true};

        ASSERT_SUCCESS(urCommandBufferCreateExp(context, device, &desc,
                                                &updatable_cmd_buf_handle));
        ASSERT_NE(updatable_cmd_buf_handle, nullptr);
    }

    virtual void TearDown() override {
        if (command_handle) {
            EXPECT_SUCCESS(urCommandBufferReleaseCommandExp(command_handle));
        }

        if (updatable_cmd_buf_handle) {
            EXPECT_SUCCESS(urCommandBufferReleaseExp(updatable_cmd_buf_handle));
        }

        UUR_RETURN_ON_FATAL_FAILURE(CommandEventSyncTest::TearDown());
    }

    ur_exp_command_buffer_handle_t updatable_cmd_buf_handle = nullptr;
    ur_exp_command_buffer_command_handle_t command_handle = nullptr;
};

UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(CommandEventSyncUpdateTest);

TEST_P(CommandEventSyncUpdateTest, USMMemcpyExp) {
    // Get wait event from queue fill on ptr 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, device_ptrs[0], sizeof(patternX),
                                    &patternX, allocation_size, 0, nullptr,
                                    &external_events[0]));

    // Command to fill ptr 1
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urCommandBufferAppendUSMFillExp(
        updatable_cmd_buf_handle, device_ptrs[1], &patternY, sizeof(patternY),
        allocation_size, 0, nullptr, 0, nullptr, &sync_points[0], nullptr,
        &command_handle));

    // Test command overwriting ptr 1 with ptr 0 command based on queue event
    ASSERT_SUCCESS(urCommandBufferAppendUSMMemcpyExp(
        updatable_cmd_buf_handle, device_ptrs[1], device_ptrs[0],
        allocation_size, 1, &sync_points[0], 1, &external_events[0], nullptr,
        &external_events[1], &command_handle));
    ASSERT_NE(nullptr, command_handle);
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(updatable_cmd_buf_handle));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    // Queue read ptr 1 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, host_enqueue_ptr.data(),
                                      device_ptrs[1], allocation_size, 1,
                                      &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternX);
    }

    uint32_t patternZ = 666;
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, device_ptrs[0], sizeof(patternZ),
                                    &patternZ, allocation_size, 0, nullptr,
                                    &external_events[2]));

    // Update command command-wait event to wait on fill of new value
    ASSERT_SUCCESS(urCommandBufferUpdateWaitEventsExp(command_handle, 1,
                                                      &external_events[2]));

    // Get a new signal event for command-buffer
    ASSERT_SUCCESS(urCommandBufferUpdateSignalEventExp(command_handle,
                                                       &external_events[3]));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, host_enqueue_ptr.data(),
                                      device_ptrs[1], allocation_size, 1,
                                      &external_events[3], nullptr));

    // Verify update
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternZ);
    }
}

TEST_P(CommandEventSyncUpdateTest, USMFillExp) {
    // Get wait event from queue fill on ptr 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, device_ptrs[0], sizeof(patternX),
                                    &patternX, allocation_size, 0, nullptr,
                                    &external_events[0]));

    // Test fill command overwriting ptr 0 waiting on queue event
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urCommandBufferAppendUSMFillExp(
        updatable_cmd_buf_handle, device_ptrs[0], &patternY, sizeof(patternY),
        allocation_size, 0, nullptr, 1, &external_events[0], nullptr,
        &external_events[1], &command_handle));
    ASSERT_NE(nullptr, command_handle);
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(updatable_cmd_buf_handle));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    // Queue read ptr 0 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, host_enqueue_ptr.data(),
                                      device_ptrs[0], allocation_size, 1,
                                      &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }

    uint32_t patternZ = 666;
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, device_ptrs[0], sizeof(patternZ),
                                    &patternZ, allocation_size, 0, nullptr,
                                    &external_events[2]));

    // Update command command-wait event to wait on fill of new value
    ASSERT_SUCCESS(urCommandBufferUpdateWaitEventsExp(command_handle, 1,
                                                      &external_events[2]));

    // Get a new signal event for command-buffer
    ASSERT_SUCCESS(urCommandBufferUpdateSignalEventExp(command_handle,
                                                       &external_events[3]));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, host_enqueue_ptr.data(),
                                      device_ptrs[0], allocation_size, 1,
                                      &external_events[3], nullptr));

    // Verify update
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }
}

TEST_P(CommandEventSyncUpdateTest, MemBufferCopyExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Command to fill buffer 1
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferFillExp(
        updatable_cmd_buf_handle, buffers[1], &patternY, sizeof(patternY), 0,
        allocation_size, 0, nullptr, 0, nullptr, &sync_points[0], nullptr,
        nullptr));

    // Test command overwriting buffer 1 with buffer 0 command based on queue event
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferCopyExp(
        updatable_cmd_buf_handle, buffers[0], buffers[1], 0, 0, allocation_size,
        1, &sync_points[0], 1, &external_events[0], nullptr,
        &external_events[1], &command_handle));
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(updatable_cmd_buf_handle));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    // Queue read buffer 1 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[1], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternX);
    }

    uint32_t patternZ = 666;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternZ,
                                          sizeof(patternZ), 0, allocation_size,
                                          0, nullptr, &external_events[2]));

    // Update command command-wait event to wait on fill of new value
    ASSERT_SUCCESS(urCommandBufferUpdateWaitEventsExp(command_handle, 1,
                                                      &external_events[2]));

    // Get a new signal event for command-buffer
    ASSERT_SUCCESS(urCommandBufferUpdateSignalEventExp(command_handle,
                                                       &external_events[3]));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[1], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[3], nullptr));

    // Verify update
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternZ);
    }
}

TEST_P(CommandEventSyncUpdateTest, MemBufferCopyRectExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Command to fill buffer 1
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferFillExp(
        updatable_cmd_buf_handle, buffers[1], &patternY, sizeof(patternY), 0,
        allocation_size, 0, nullptr, 0, nullptr, &sync_points[0], nullptr,
        nullptr));

    // Test command overwriting buffer 1 with buffer 0 command based on queue event
    ur_rect_offset_t src_origin{0, 0, 0};
    ur_rect_offset_t dst_origin{0, 0, 0};
    constexpr size_t rect_buffer_row_size = 16;
    ur_rect_region_t region{rect_buffer_row_size, rect_buffer_row_size, 1};
    size_t src_row_pitch = rect_buffer_row_size;
    size_t src_slice_pitch = allocation_size;
    size_t dst_row_pitch = rect_buffer_row_size;
    size_t dst_slice_pitch = allocation_size;
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferCopyRectExp(
        updatable_cmd_buf_handle, buffers[0], buffers[1], src_origin,
        dst_origin, region, src_row_pitch, src_slice_pitch, dst_row_pitch,
        dst_slice_pitch, 1, &sync_points[0], 1, &external_events[0], nullptr,
        &external_events[1], &command_handle));
    ASSERT_NE(nullptr, command_handle);
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(updatable_cmd_buf_handle));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    // Queue read buffer 1 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[1], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternX);
    }

    uint32_t patternZ = 666;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternZ,
                                          sizeof(patternZ), 0, allocation_size,
                                          0, nullptr, &external_events[2]));

    // Update command command-wait event to wait on fill of new value
    ASSERT_SUCCESS(urCommandBufferUpdateWaitEventsExp(command_handle, 1,
                                                      &external_events[2]));

    // Get a new signal event for command-buffer
    ASSERT_SUCCESS(urCommandBufferUpdateSignalEventExp(command_handle,
                                                       &external_events[3]));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[1], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[3], nullptr));

    // Verify update
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternZ);
    }
}

TEST_P(CommandEventSyncUpdateTest, MemBufferReadExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Test command reading buffer 0 based on queue event
    std::array<uint32_t, elements> host_command_ptr{};
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferReadExp(
        updatable_cmd_buf_handle, buffers[0], 0, allocation_size,
        host_command_ptr.data(), 0, nullptr, 1, &external_events[0], nullptr,
        &external_events[1], &command_handle));
    ASSERT_NE(nullptr, command_handle);
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(updatable_cmd_buf_handle));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    // Overwrite buffer 0 based on event returned from command-buffer command,
    // then read back to verify ordering
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(
        queue, buffers[0], &patternY, sizeof(patternY), 0, allocation_size, 1,
        &external_events[1], &external_events[2]));
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[2], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_command_ptr[i], patternX);
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }

    uint32_t patternZ = 666;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternZ,
                                          sizeof(patternZ), 0, allocation_size,
                                          0, nullptr, &external_events[3]));

    // Update command command-wait event to wait on fill of new value
    ASSERT_SUCCESS(urCommandBufferUpdateWaitEventsExp(command_handle, 1,
                                                      &external_events[3]));

    // Get a new signal event for command-buffer
    ASSERT_SUCCESS(urCommandBufferUpdateSignalEventExp(command_handle,
                                                       &external_events[4]));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    uint32_t patternA = 0xF;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(
        queue, buffers[0], &patternA, sizeof(patternA), 0, allocation_size, 1,
        &external_events[4], &external_events[5]));
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[5], nullptr));

    // Verify update
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_command_ptr[i], patternZ);
        ASSERT_EQ(host_enqueue_ptr[i], patternA);
    }
}

TEST_P(CommandEventSyncUpdateTest, MemBufferReadRectExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Test command reading buffer 0 based on queue event
    std::array<uint32_t, elements> host_command_ptr{};
    ur_rect_offset_t buffer_offset = {0, 0, 0};
    ur_rect_offset_t host_offset = {0, 0, 0};
    constexpr size_t rect_buffer_row_size = 16;
    ur_rect_region_t region = {rect_buffer_row_size, rect_buffer_row_size, 1};
    size_t buffer_row_pitch = rect_buffer_row_size;
    size_t buffer_slice_pitch = allocation_size;
    size_t host_row_pitch = rect_buffer_row_size;
    size_t host_slice_pitch = allocation_size;
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferReadRectExp(
        updatable_cmd_buf_handle, buffers[0], buffer_offset, host_offset,
        region, buffer_row_pitch, buffer_slice_pitch, host_row_pitch,
        host_slice_pitch, host_command_ptr.data(), 0, nullptr, 1,
        &external_events[0], nullptr, &external_events[1], &command_handle));
    ASSERT_NE(nullptr, command_handle);
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(updatable_cmd_buf_handle));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    // Overwrite buffer 0 based on event returned from command-buffer command,
    // then read back to verify ordering
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(
        queue, buffers[0], &patternY, sizeof(patternY), 0, allocation_size, 1,
        &external_events[1], &external_events[2]));
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[2], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_command_ptr[i], patternX);
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }

    uint32_t patternZ = 666;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternZ,
                                          sizeof(patternZ), 0, allocation_size,
                                          0, nullptr, &external_events[3]));

    // Update command command-wait event to wait on fill of new value
    ASSERT_SUCCESS(urCommandBufferUpdateWaitEventsExp(command_handle, 1,
                                                      &external_events[3]));

    // Get a new signal event for command-buffer
    ASSERT_SUCCESS(urCommandBufferUpdateSignalEventExp(command_handle,
                                                       &external_events[4]));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    uint32_t patternA = 0xF;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(
        queue, buffers[0], &patternA, sizeof(patternA), 0, allocation_size, 1,
        &external_events[4], &external_events[5]));
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[5], nullptr));

    // Verify update
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_command_ptr[i], patternZ);
        ASSERT_EQ(host_enqueue_ptr[i], patternA);
    }
}

TEST_P(CommandEventSyncUpdateTest, MemBufferWriteExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Test command overwriting buffer 0 based on queue event
    std::array<uint32_t, elements> host_command_ptr{};
    uint32_t patternY = 0xA;
    std::fill(host_command_ptr.begin(), host_command_ptr.end(), patternY);
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferWriteExp(
        updatable_cmd_buf_handle, buffers[0], 0, allocation_size,
        host_command_ptr.data(), 0, nullptr, 1, &external_events[0], nullptr,
        &external_events[1], &command_handle));
    ASSERT_NE(nullptr, command_handle);
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(updatable_cmd_buf_handle));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    // Read back buffer 0 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY) << i;
    }

    uint32_t patternZ = 666;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternZ,
                                          sizeof(patternZ), 0, allocation_size,
                                          0, nullptr, &external_events[2]));

    // Update command command-wait event to wait on fill of new value
    ASSERT_SUCCESS(urCommandBufferUpdateWaitEventsExp(command_handle, 1,
                                                      &external_events[2]));

    // Get a new signal event for command-buffer
    ASSERT_SUCCESS(urCommandBufferUpdateSignalEventExp(command_handle,
                                                       &external_events[3]));

    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[3], nullptr));

    // Verify update
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }
}

TEST_P(CommandEventSyncUpdateTest, MemBufferWriteRectExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Test command overwriting buffer 0 based on queue event
    std::array<uint32_t, elements> host_command_ptr{};
    uint32_t patternY = 0xA;
    std::fill(host_command_ptr.begin(), host_command_ptr.end(), patternY);

    ur_rect_offset_t buffer_offset = {0, 0, 0};
    ur_rect_offset_t host_offset = {0, 0, 0};
    constexpr size_t rect_buffer_row_size = 16;
    ur_rect_region_t region = {rect_buffer_row_size, rect_buffer_row_size, 1};
    size_t buffer_row_pitch = rect_buffer_row_size;
    size_t buffer_slice_pitch = allocation_size;
    size_t host_row_pitch = rect_buffer_row_size;
    size_t host_slice_pitch = allocation_size;
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferWriteRectExp(
        updatable_cmd_buf_handle, buffers[0], buffer_offset, host_offset,
        region, buffer_row_pitch, buffer_slice_pitch, host_row_pitch,
        host_slice_pitch, host_command_ptr.data(), 0, nullptr, 1,
        &external_events[0], nullptr, &external_events[1], &command_handle));
    ASSERT_NE(nullptr, command_handle);
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(updatable_cmd_buf_handle));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    // Read back buffer 0 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY) << i;
    }

    uint32_t patternZ = 666;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternZ,
                                          sizeof(patternZ), 0, allocation_size,
                                          0, nullptr, &external_events[2]));

    // Update command command-wait event to wait on fill of new value
    ASSERT_SUCCESS(urCommandBufferUpdateWaitEventsExp(command_handle, 1,
                                                      &external_events[2]));

    // Get a new signal event for command-buffer
    ASSERT_SUCCESS(urCommandBufferUpdateSignalEventExp(command_handle,
                                                       &external_events[3]));

    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[3], nullptr));

    // Verify update
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }
}

TEST_P(CommandEventSyncUpdateTest, MemBufferFillExp) {
    // Get wait event from queue fill on buffer 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternX,
                                          sizeof(patternX), 0, allocation_size,
                                          0, nullptr, &external_events[0]));

    // Test fill command overwriting buffer 0 based on queue event
    uint32_t patternY = 0xA;
    ASSERT_SUCCESS(urCommandBufferAppendMemBufferFillExp(
        updatable_cmd_buf_handle, buffers[0], &patternY, sizeof(patternY), 0,
        allocation_size, 0, nullptr, 1, &external_events[0], nullptr,
        &external_events[1], &command_handle));
    ASSERT_NE(nullptr, command_handle);
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(updatable_cmd_buf_handle));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    // Queue read buffer 0 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }

    uint32_t patternZ = 666;
    ASSERT_SUCCESS(urEnqueueMemBufferFill(queue, buffers[0], &patternZ,
                                          sizeof(patternZ), 0, allocation_size,
                                          0, nullptr, &external_events[2]));

    // Update command command-wait event to wait on fill of new value
    ASSERT_SUCCESS(urCommandBufferUpdateWaitEventsExp(command_handle, 1,
                                                      &external_events[2]));

    // Get a new signal event for command-buffer
    ASSERT_SUCCESS(urCommandBufferUpdateSignalEventExp(command_handle,
                                                       &external_events[3]));

    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));
    ASSERT_SUCCESS(urEnqueueMemBufferRead(
        queue, buffers[0], false, 0, allocation_size, host_enqueue_ptr.data(),
        1, &external_events[3], nullptr));

    // Verify update
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }
}

TEST_P(CommandEventSyncUpdateTest, USMPrefetchExp) {
    // Get wait event from queue fill on ptr 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, device_ptrs[0], sizeof(patternX),
                                    &patternX, allocation_size, 0, nullptr,
                                    &external_events[0]));

    // Test prefetch command waiting on queue event
    ASSERT_SUCCESS(urCommandBufferAppendUSMPrefetchExp(
        updatable_cmd_buf_handle, device_ptrs[1], allocation_size,
        0 /* migration flags*/, 0, nullptr, 1, &external_events[0], nullptr,
        &external_events[1], &command_handle));
    ASSERT_NE(nullptr, command_handle);
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(updatable_cmd_buf_handle));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    // Queue read ptr 0 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, host_enqueue_ptr.data(),
                                      device_ptrs[0], allocation_size, 1,
                                      &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternX);
    }

    uint32_t patternY = 42;
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, device_ptrs[0], sizeof(patternY),
                                    &patternY, allocation_size, 0, nullptr,
                                    &external_events[2]));

    // Update command command-wait event to wait on fill of new value
    ASSERT_SUCCESS(urCommandBufferUpdateWaitEventsExp(command_handle, 1,
                                                      &external_events[2]));

    // Get a new signal event for command-buffer
    ASSERT_SUCCESS(urCommandBufferUpdateSignalEventExp(command_handle,
                                                       &external_events[3]));

    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));
    ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, host_enqueue_ptr.data(),
                                      device_ptrs[0], allocation_size, 1,
                                      &external_events[3], nullptr));

    // Verify update
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }
}

TEST_P(CommandEventSyncUpdateTest, USMAdviseExp) {
    // Get wait event from queue fill on ptr 0
    uint32_t patternX = 42;
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, device_ptrs[0], sizeof(patternX),
                                    &patternX, allocation_size, 0, nullptr,
                                    &external_events[0]));

    // Test advise command waiting on queue event
    ASSERT_SUCCESS(urCommandBufferAppendUSMAdviseExp(
        updatable_cmd_buf_handle, device_ptrs[0], allocation_size,
        0 /* advice flags*/, 0, nullptr, 1, &external_events[0], nullptr,
        &external_events[1], &command_handle));
    ASSERT_NE(nullptr, command_handle);
    ASSERT_SUCCESS(urCommandBufferFinalizeExp(updatable_cmd_buf_handle));
    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));

    // Queue read ptr 0 based on event returned from command-buffer command
    std::array<uint32_t, elements> host_enqueue_ptr{};
    ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, host_enqueue_ptr.data(),
                                      device_ptrs[0], allocation_size, 1,
                                      &external_events[1], nullptr));

    // Verify
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternX);
    }

    uint32_t patternY = 42;
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, device_ptrs[0], sizeof(patternY),
                                    &patternY, allocation_size, 0, nullptr,
                                    &external_events[2]));

    // Update command command-wait event to wait on fill of new value
    ASSERT_SUCCESS(urCommandBufferUpdateWaitEventsExp(command_handle, 1,
                                                      &external_events[2]));

    // Get a new signal event for command-buffer
    ASSERT_SUCCESS(urCommandBufferUpdateSignalEventExp(command_handle,
                                                       &external_events[3]));

    ASSERT_SUCCESS(urCommandBufferEnqueueExp(updatable_cmd_buf_handle, queue, 0,
                                             nullptr, nullptr));
    ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, host_enqueue_ptr.data(),
                                      device_ptrs[0], allocation_size, 1,
                                      &external_events[3], nullptr));

    // Verify update
    ASSERT_SUCCESS(urQueueFinish(queue));
    for (size_t i = 0; i < elements; i++) {
        ASSERT_EQ(host_enqueue_ptr[i], patternY);
    }
}
