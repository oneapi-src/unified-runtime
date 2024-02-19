// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <uur/fixtures.h>

using urMemImageCreateWithNativeHandleTest = uur::urContextTest<>;
UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(urMemImageCreateWithNativeHandleTest);

TEST_P(urMemImageCreateWithNativeHandleTest, Success) {

    ur_image_format_t image_format = {
        /*.channelOrder =*/UR_IMAGE_CHANNEL_ORDER_ARGB,
        /*.channelType =*/UR_IMAGE_CHANNEL_TYPE_UNORM_INT8,
    };
    ur_image_desc_t image_desc = {
        /*.stype =*/UR_STRUCTURE_TYPE_IMAGE_DESC,
        /*.pNext =*/nullptr,
        /*.type =*/UR_MEM_TYPE_IMAGE2D,
        /*.width =*/16,
        /*.height =*/16,
        /*.depth =*/1,
        /*.arraySize =*/1,
        /*.rowPitch =*/16 * sizeof(char[4]),
        /*.slicePitch =*/16 * 16 * sizeof(char[4]),
        /*.numMipLevel =*/0,
        /*.numSamples =*/0,
    };
    ur_mem_handle_t image = nullptr;

    ur_native_handle_t native_handle = nullptr;
    if (urMemGetNativeHandle(image, device, &native_handle)) {
        GTEST_SKIP();
    }

    ur_mem_handle_t mem = nullptr;
    ASSERT_EQ_RESULT(
        UR_RESULT_ERROR_INVALID_NULL_HANDLE,
        urMemImageCreateWithNativeHandle(native_handle, context, &image_format,
                                         &image_desc, nullptr, &mem));
    ASSERT_NE(nullptr, mem);

    ur_context_handle_t mem_context = nullptr;
    ASSERT_SUCCESS(urMemGetInfo(mem, UR_MEM_INFO_CONTEXT,
                                sizeof(ur_context_handle_t), &mem_context,
                                nullptr));
    ASSERT_EQ(context, mem_context);
}
