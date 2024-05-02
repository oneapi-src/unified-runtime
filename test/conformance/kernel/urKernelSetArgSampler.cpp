// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <uur/fixtures.h>

struct urKernelSetArgSamplerTestWithParam
    : uur::urBaseKernelTestWithParam<uur::SamplerCreateParamT> {
    void SetUp() {
        program_name = "image_sampled_copy";
        UUR_RETURN_ON_FATAL_FAILURE(
            uur::urBaseKernelTestWithParam<uur::SamplerCreateParamT>::SetUp());
        UUR_RETURN_ON_FATAL_FAILURE(
            uur::urBaseKernelTestWithParam<uur::SamplerCreateParamT>::Build());

        ur_bool_t imageSupported = false;
        ASSERT_SUCCESS(
            urDeviceGetInfo(this->device, UR_DEVICE_INFO_IMAGE_SUPPORTED,
                            sizeof(ur_bool_t), &imageSupported, nullptr));
        if (!imageSupported) {
            GTEST_SKIP();
        }

        const auto param = getParam();
        const auto normalized = std::get<0>(param);
        const auto addr_mode = std::get<1>(param);
        const auto filter_mode = std::get<2>(param);

        ur_sampler_desc_t sampler_desc = {
            UR_STRUCTURE_TYPE_SAMPLER_DESC, /* sType */
            nullptr,                        /* pNext */
            normalized,                     /* normalizedCoords */
            addr_mode,                      /* addressingMode */
            filter_mode                     /* filterMode */
        };
        ASSERT_SUCCESS(urSamplerCreate(context, &sampler_desc, &sampler));

        ASSERT_SUCCESS(urMemImageCreate(context, UR_MEM_FLAG_READ_WRITE,
                                        &image_format, &image_desc, nullptr,
                                        &image_in));

        ASSERT_SUCCESS(urMemImageCreate(context, UR_MEM_FLAG_READ_WRITE,
                                        &image_format, &image_desc, nullptr,
                                        &image_out));
        ASSERT_SUCCESS(urQueueCreate(context, device, nullptr, &queue));
    }

    void TearDown() {
        if (sampler) {
            ASSERT_SUCCESS(urSamplerRelease(sampler));
        }
        if (image_in) {
            ASSERT_SUCCESS(urMemRelease(image_in));
        }
        if (image_out) {
            ASSERT_SUCCESS(urMemRelease(image_out));
        }
        if (queue) {
            ASSERT_SUCCESS(urQueueRelease(queue));
        }
        UUR_RETURN_ON_FATAL_FAILURE(uur::urBaseKernelTestWithParam<
                                    uur::SamplerCreateParamT>::TearDown());
    }

    ur_sampler_handle_t sampler = nullptr;
    ur_queue_handle_t queue = nullptr;

    uint32_t width = 3;

    ur_image_format_t image_format = {
        /*.channelOrder =*/UR_IMAGE_CHANNEL_ORDER_RGBA,
        /*.channelType =*/UR_IMAGE_CHANNEL_TYPE_FLOAT,
    };
    ur_image_desc_t image_desc = {
        /*.stype =*/UR_STRUCTURE_TYPE_IMAGE_DESC,
        /*.pNext =*/nullptr,
        /*.type =*/UR_MEM_TYPE_IMAGE1D,
        /*.width =*/width,
        /*.height =*/1,
        /*.depth =*/1,
        /*.arraySize =*/1,
        /*.rowPitch =*/0,
        /*.slicePitch =*/0,
        /*.numMipLevel =*/0,
        /*.numSamples =*/0,
    };
    ur_rect_region_t region1D{width, 1, 1};
    ur_rect_offset_t origin{0, 0, 0};
    ur_mem_handle_t image_in = nullptr;
    ur_mem_handle_t image_out = nullptr;

    struct rgba_pixel {
        float data[4];
    };

    rgba_pixel pixel1 = {0.2f, 0.4f, 0.6f, 0.8f};
    rgba_pixel pixel2 = {0.6f, 0.4f, 0.2f, 0.0f};
    rgba_pixel pixel3 = {0.0f, 0.0f, 0.0f, 0.0f};
    rgba_pixel pixel4 = {0.4f, 0.4f, 0.4f, 0.4f};

    bool CheckEqual(rgba_pixel &output, rgba_pixel &expected) {
        bool is_equal = true;
        for (uint32_t pixel_idx = 0; pixel_idx < 4; pixel_idx++) {
            is_equal &= (fabs(output.data[pixel_idx] -
                              expected.data[pixel_idx]) < 0.01);
        }
        return is_equal;
    }

    void VerifyPixel(rgba_pixel &output) {
        const auto param = getParam();
        const auto addr_mode = std::get<1>(param);

        // UR_SAMPLER_ADDRESSING_MODE_NONE are not possible to guarantee its output as it is undefined behaviour when the read index is out-of-range.
        if (addr_mode != UR_SAMPLER_ADDRESSING_MODE_NONE) {
            // Since different adapters give different results, we could only guarantee that the result should be one of these values.
            ASSERT_TRUE(
                CheckEqual(output, pixel1) || CheckEqual(output, pixel2) ||
                CheckEqual(output, pixel3) || CheckEqual(output, pixel4));
        }
    };
};

UUR_TEST_SUITE_P(
    urKernelSetArgSamplerTestWithParam,
    ::testing::Combine(
        ::testing::Values(true, false),
        ::testing::Values(UR_SAMPLER_ADDRESSING_MODE_NONE,
                          UR_SAMPLER_ADDRESSING_MODE_CLAMP_TO_EDGE,
                          UR_SAMPLER_ADDRESSING_MODE_CLAMP,
                          UR_SAMPLER_ADDRESSING_MODE_REPEAT,
                          UR_SAMPLER_ADDRESSING_MODE_MIRRORED_REPEAT),
        ::testing::Values(UR_SAMPLER_FILTER_MODE_NEAREST,
                          UR_SAMPLER_FILTER_MODE_LINEAR)),
    uur::deviceTestWithParamPrinter<uur::SamplerCreateParamT>);

TEST_P(urKernelSetArgSamplerTestWithParam, Success) {
    std::vector<rgba_pixel> input = {pixel1, pixel2, pixel2};
    ASSERT_SUCCESS(urEnqueueMemImageWrite(queue, image_in, 0, {0, 0, 0},
                                          {width, 1, 1}, 0, 0, input.data(), 0,
                                          nullptr, nullptr));
    ASSERT_SUCCESS(urQueueFinish(queue));

    ur_kernel_arg_mem_obj_properties_t props{
        UR_STRUCTURE_TYPE_KERNEL_ARG_MEM_OBJ_PROPERTIES, nullptr,
        UR_MEM_FLAG_READ_WRITE};
    ASSERT_SUCCESS(urKernelSetArgMemObj(kernel, 0, &props, image_out));

    ASSERT_SUCCESS(urKernelSetArgMemObj(kernel, 1, &props, image_in));

    ASSERT_SUCCESS(urKernelSetArgSampler(kernel, 2, nullptr, sampler));

    size_t global_size = 1;
    size_t global_offset = 0;
    size_t n_dimensions = 1;
    ASSERT_SUCCESS(urEnqueueKernelLaunch(queue, kernel, n_dimensions,
                                         &global_offset, &global_size, nullptr,
                                         0, nullptr, nullptr));
    ASSERT_SUCCESS(urQueueFinish(queue));

    rgba_pixel output = {0, 0, 0, 0};
    ASSERT_SUCCESS(urEnqueueMemImageRead(queue, image_out, 0, {0, 0, 0},
                                         {1, 1, 1}, 0, 0, &output, 0, nullptr,
                                         nullptr));
    ASSERT_SUCCESS(urQueueFinish(queue));

    VerifyPixel(output);
}

struct urKernelSetArgSamplerTest : uur::urBaseKernelTest {
    void SetUp() {
        program_name = "image_sampled_copy";
        UUR_RETURN_ON_FATAL_FAILURE(urBaseKernelTest::SetUp());
        Build();
        ur_sampler_desc_t sampler_desc = {
            UR_STRUCTURE_TYPE_SAMPLER_DESC,   /* sType */
            nullptr,                          /* pNext */
            false,                            /* normalizedCoords */
            UR_SAMPLER_ADDRESSING_MODE_CLAMP, /* addressingMode */
            UR_SAMPLER_FILTER_MODE_NEAREST    /* filterMode */
        };
        ASSERT_SUCCESS(urSamplerCreate(context, &sampler_desc, &sampler));
    }

    void TearDown() {
        if (sampler) {
            ASSERT_SUCCESS(urSamplerRelease(sampler));
        }
        UUR_RETURN_ON_FATAL_FAILURE(urBaseKernelTest::TearDown());
    }

    ur_sampler_handle_t sampler = nullptr;
};

UUR_INSTANTIATE_KERNEL_TEST_SUITE_P(urKernelSetArgSamplerTest);

TEST_P(urKernelSetArgSamplerTest, SuccessWithProps) {
    ur_kernel_arg_sampler_properties_t props{
        UR_STRUCTURE_TYPE_KERNEL_ARG_SAMPLER_PROPERTIES, nullptr};
    size_t arg_index = 2;
    ASSERT_SUCCESS(urKernelSetArgSampler(kernel, arg_index, &props, sampler));
}

TEST_P(urKernelSetArgSamplerTest, InvalidNullHandleKernel) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_NULL_HANDLE,
                     urKernelSetArgSampler(nullptr, 2, nullptr, sampler));
}

TEST_P(urKernelSetArgSamplerTest, InvalidNullHandleArgValue) {
    ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_NULL_HANDLE,
                     urKernelSetArgSampler(kernel, 2, nullptr, nullptr));
}

TEST_P(urKernelSetArgSamplerTest, InvalidKernelArgumentIndex) {
    size_t num_kernel_args = 0;
    ASSERT_SUCCESS(urKernelGetInfo(kernel, UR_KERNEL_INFO_NUM_ARGS,
                                   sizeof(num_kernel_args), &num_kernel_args,
                                   nullptr));
    ASSERT_EQ_RESULT(
        UR_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_INDEX,
        urKernelSetArgSampler(kernel, num_kernel_args + 1, nullptr, sampler));
}
