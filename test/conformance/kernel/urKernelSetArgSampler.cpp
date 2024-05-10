// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <uur/fixtures.h>

struct urKernelSetArgSamplerTestWithParam
    : uur::urBaseKernelTestWithParam<uur::SamplerCreateParamT> {
    void SetUp() {
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

        program_name = "image_copy";
        UUR_RETURN_ON_FATAL_FAILURE(
            uur::urBaseKernelTestWithParam<uur::SamplerCreateParamT>::SetUp());

        auto ret = urSamplerCreate(context, &sampler_desc, &sampler);
        if (ret == UR_RESULT_ERROR_UNSUPPORTED_FEATURE ||
            ret == UR_RESULT_ERROR_UNINITIALIZED) {
            GTEST_SKIP() << "urSamplerCreate not supported";
        } else {
            ASSERT_SUCCESS(ret);
        }

        UUR_RETURN_ON_FATAL_FAILURE(
            uur::urBaseKernelTestWithParam<uur::SamplerCreateParamT>::Build());
    }

    void TearDown() {
        if (sampler) {
            ASSERT_SUCCESS(urSamplerRelease(sampler));
        }
        UUR_RETURN_ON_FATAL_FAILURE(uur::urBaseKernelTestWithParam<
                                    uur::SamplerCreateParamT>::TearDown());
    }

    ur_sampler_handle_t sampler = nullptr;
};

UUR_TEST_SUITE_P(urKernelSetArgSamplerTestWithParam, uur::sampler_values,
                 uur::deviceTestWithParamPrinter<uur::SamplerCreateParamT>);

struct urKernelSetArgSamplerTest : uur::urBaseKernelTest {
    void SetUp() {
        program_name = "image_copy";
        UUR_RETURN_ON_FATAL_FAILURE(urBaseKernelTest::SetUp());

        ur_sampler_desc_t sampler_desc = {
            UR_STRUCTURE_TYPE_SAMPLER_DESC,   /* sType */
            nullptr,                          /* pNext */
            false,                            /* normalizedCoords */
            UR_SAMPLER_ADDRESSING_MODE_CLAMP, /* addressingMode */
            UR_SAMPLER_FILTER_MODE_NEAREST    /* filterMode */
        };

        auto ret = urSamplerCreate(context, &sampler_desc, &sampler);
        if (ret == UR_RESULT_ERROR_UNSUPPORTED_FEATURE ||
            ret == UR_RESULT_ERROR_UNINITIALIZED) {
            GTEST_SKIP() << "urSamplerCreate not supported";
        } else {
            ASSERT_SUCCESS(ret);
        }

        Build();
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

struct urKernelSetArgSamplerReadTestWithParam
    : uur::urBaseKernelExecutionTestWithParam<uur::SamplerCreateParamT> {

    void SetUp() {
        program_name = "sampler_read";
        UUR_RETURN_ON_FATAL_FAILURE(
            urBaseKernelExecutionTestWithParam::SetUp());
        UUR_RETURN_ON_FATAL_FAILURE(
            uur::urBaseKernelTestWithParam<uur::SamplerCreateParamT>::Build());

        // Images and samplers are not available on AMD
        ur_platform_backend_t backend;
        ASSERT_SUCCESS(urPlatformGetInfo(platform, UR_PLATFORM_INFO_BACKEND,
                                         sizeof(backend), &backend, nullptr));
        if (backend == UR_PLATFORM_BACKEND_HIP) {
            GTEST_SKIP() << "Samplers are not supported on hip.";
        }

        const auto param = getParam();
        normalized = std::get<0>(param);
        addr_mode = std::get<1>(param);
        filter_mode = std::get<2>(param);

        // This is an invalid combination
        if (!normalized &&
            addr_mode == UR_SAMPLER_ADDRESSING_MODE_MIRRORED_REPEAT) {
            GTEST_SKIP()
                << "Sampler can't use unnormalised repeat addressing mode";
        }

        ur_sampler_desc_t _sampler_desc = {
            UR_STRUCTURE_TYPE_SAMPLER_DESC, /* sType */
            nullptr,                        /* pNext */
            normalized,                     /* normalizedCoords */
            addr_mode,                      /* addressingMode */
            filter_mode                     /* filterMode */
        };
        ASSERT_SUCCESS(urSamplerCreate(context, &_sampler_desc, &sampler));
    }

    void TearDown() {
        if (sampler) {
            ASSERT_SUCCESS(urSamplerRelease(sampler));
        }
        UUR_RETURN_ON_FATAL_FAILURE(
            urBaseKernelExecutionTestWithParam::TearDown());
    }

    ur_sampler_handle_t sampler = nullptr;
    bool normalized;
    ur_sampler_addressing_mode_t addr_mode;
    ur_sampler_filter_mode_t filter_mode;
    std::array<size_t, 2> offset = {0, 0};
    std::array<size_t, 2> size = {1, 1};
};
UUR_TEST_SUITE_P(urKernelSetArgSamplerReadTestWithParam, uur::sampler_values,
                 uur::deviceTestWithParamPrinter<uur::SamplerCreateParamT>);

TEST_P(urKernelSetArgSamplerTestWithParam, Success) {
    uint32_t arg_index = 2;
    ASSERT_SUCCESS(urKernelSetArgSampler(kernel, arg_index, nullptr, sampler));
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
    uint32_t num_kernel_args = 0;
    ASSERT_SUCCESS(urKernelGetInfo(kernel, UR_KERNEL_INFO_NUM_ARGS,
                                   sizeof(num_kernel_args), &num_kernel_args,
                                   nullptr));
    ASSERT_EQ_RESULT(
        UR_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_INDEX,
        urKernelSetArgSampler(kernel, num_kernel_args + 1, nullptr, sampler));
}

namespace {
template <size_t W, size_t H, typename T>
using ImageDataArray = std::array<std::array<std::array<T, 4>, W>, H>;
}

TEST_P(urKernelSetArgSamplerReadTestWithParam, ReadSamplerFilter) {
    std::array<float, 4> S({0.0, 0.0, 0.0, 1.0});
    std::array<float, 4> E({0.0, 0.0, 0.0, 0.0});

    ImageDataArray<4, 4, float> input_image{{
        {S, S, S, S},
        {S, E, E, S},
        {S, E, E, S},
        {S, E, E, S},
    }};

    ur_mem_handle_t result = nullptr;
    AddBuffer1DArg(sizeof(float) * 1, &result);
    ur_mem_handle_t image = nullptr;
    AddInputFloatImage<4, 4>(input_image.data(), &image,
                             UR_IMAGE_CHANNEL_TYPE_FLOAT,
                             UR_IMAGE_CHANNEL_ORDER_RGBA);
    if (normalized) {
        AddPodArg(std::array<float, 2>({1.0 / 4.0, 2.0 / 4.0}));
    } else {
        AddPodArg(std::array<float, 2>({1.0, 2.0}));
    }
    ASSERT_SUCCESS(urKernelSetArgSampler(kernel, 4, nullptr, sampler));
    ASSERT_SUCCESS(urEnqueueKernelLaunch(queue, kernel, 2, offset.data(),
                                         size.data(), nullptr, 0, nullptr,
                                         nullptr));

    float result_read = -1.0;
    ASSERT_SUCCESS(urEnqueueMemBufferRead(queue, result, true, 0, sizeof(float),
                                          &result_read, 0, nullptr, nullptr));

    if (filter_mode == UR_SAMPLER_FILTER_MODE_LINEAR) {
        ASSERT_FLOAT_EQ(result_read, 0.5);
    } else {
        ASSERT_FLOAT_EQ(result_read, 0.0);
    }
}

TEST_P(urKernelSetArgSamplerReadTestWithParam, ReadSamplerAddressMode) {
    std::array<float, 4> S({0.0, 0.0, 0.0, 1.0});
    std::array<float, 4> G({0.0, 0.0, 0.0, 0.5});

    ImageDataArray<4, 4, float> input_image{{
        {S, S, G, G},
        {S, S, G, G},
        {G, G, S, S},
        {G, G, S, S},
    }};

    ur_mem_handle_t result = nullptr;
    AddBuffer1DArg(sizeof(float) * 1, &result);
    ur_mem_handle_t image = nullptr;
    AddInputFloatImage<4, 4>(input_image.data(), &image,
                             UR_IMAGE_CHANNEL_TYPE_FLOAT,
                             UR_IMAGE_CHANNEL_ORDER_RGBA);
    if (normalized) {
        AddPodArg(std::array<float, 2>({0.5 / 4.0, 4.5 / 4.0}));
    } else {
        AddPodArg(std::array<float, 2>({0.5, 4.5}));
    }
    ASSERT_SUCCESS(urKernelSetArgSampler(kernel, 4, nullptr, sampler));
    ASSERT_SUCCESS(urEnqueueKernelLaunch(queue, kernel, 2, offset.data(),
                                         size.data(), nullptr, 0, nullptr,
                                         nullptr));

    float result_read = -1.0;
    ASSERT_SUCCESS(urEnqueueMemBufferRead(queue, result, true, 0, sizeof(float),
                                          &result_read, 0, nullptr, nullptr));

    switch (addr_mode) {
    case UR_SAMPLER_ADDRESSING_MODE_CLAMP:
        // Border colour in OpenCL is fully transparent black
        ASSERT_FLOAT_EQ(result_read, 0.0);
        break;
    case UR_SAMPLER_ADDRESSING_MODE_CLAMP_TO_EDGE:
    case UR_SAMPLER_ADDRESSING_MODE_MIRRORED_REPEAT:
        ASSERT_FLOAT_EQ(result_read, 0.5);
        break;
    case UR_SAMPLER_ADDRESSING_MODE_REPEAT:
        ASSERT_FLOAT_EQ(result_read, 1.0);
    case UR_SAMPLER_ADDRESSING_MODE_NONE:
        // Behaviour unspecified
        break;
    case UR_SAMPLER_ADDRESSING_MODE_FORCE_UINT32:
        GTEST_FAIL() << "Unknown address mode";
        break;
    }
}
