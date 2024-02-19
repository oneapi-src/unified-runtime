// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <uur/fixtures.h>

using urSamplerGetInfoTest = uur::urSamplerTest<ur_sampler_info_t>;
UUR_TEST_SUITE_P(urSamplerGetInfoTest,
                 ::testing::Values(UR_SAMPLER_INFO_REFERENCE_COUNT,
                                   UR_SAMPLER_INFO_CONTEXT,
                                   UR_SAMPLER_INFO_NORMALIZED_COORDS,
                                   UR_SAMPLER_INFO_ADDRESSING_MODE,
                                   UR_SAMPLER_INFO_FILTER_MODE),
                 uur::deviceTestPrinter<ur_sampler_info_t>);

TEST_P(urSamplerGetInfoTest, Success) {
    size_t size = 0;
    ur_sampler_info_t info = getParam();
    ASSERT_SUCCESS(urSamplerGetInfo(sampler, info, 0, nullptr, &size));
    ASSERT_NE(size, 0);
    std::vector<uint8_t> infoData(size);
    ASSERT_SUCCESS(
        urSamplerGetInfo(sampler, info, size, infoData.data(), nullptr));

    switch (info) {
    case UR_SAMPLER_INFO_REFERENCE_COUNT: {
        auto *value = reinterpret_cast<uint32_t *>(infoData.data());
        ASSERT_TRUE(*value > 0);
        break;
    }
    case UR_SAMPLER_INFO_CONTEXT: {
        auto *value = reinterpret_cast<ur_context_handle_t *>(infoData.data());
        ASSERT_EQ(*value, this->context);
        break;
    }
    case UR_SAMPLER_INFO_NORMALIZED_COORDS: {
        auto *value = reinterpret_cast<ur_bool_t *>(infoData.data());
        ASSERT_EQ(*value, sampler_desc.normalizedCoords);
        break;
    }
    case UR_SAMPLER_INFO_ADDRESSING_MODE: {
        auto *value =
            reinterpret_cast<ur_sampler_addressing_mode_t *>(infoData.data());
        ASSERT_EQ(*value, sampler_desc.addressingMode);
        break;
    }
    case UR_SAMPLER_INFO_FILTER_MODE: {
        auto *value =
            reinterpret_cast<ur_sampler_filter_mode_t *>(infoData.data());
        ASSERT_EQ(*value, sampler_desc.filterMode);
    }
    default:
        break;
    }
}

using urSamplerGetInfoNegativeTest = uur::urSamplerTest<>;
UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(urSamplerGetInfoNegativeTest);

TEST_P(urSamplerGetInfoNegativeTest, InvalidNullHandleSampler) {
    uint32_t refcount = 0;
    ASSERT_EQ_RESULT(urSamplerGetInfo(nullptr, UR_SAMPLER_INFO_REFERENCE_COUNT,
                                      sizeof(refcount), &refcount, nullptr),
                     UR_RESULT_ERROR_INVALID_NULL_HANDLE);
}

TEST_P(urSamplerGetInfoNegativeTest, InvalidEnumerationInfo) {
    size_t size = 0;
    ASSERT_EQ_RESULT(urSamplerGetInfo(sampler, UR_SAMPLER_INFO_FORCE_UINT32, 0,
                                      nullptr, &size),
                     UR_RESULT_ERROR_INVALID_ENUMERATION);
}

TEST_P(urSamplerGetInfoNegativeTest, InvalidNullPointerPropSizeRet) {
    ASSERT_EQ_RESULT(urSamplerGetInfo(sampler, UR_SAMPLER_INFO_ADDRESSING_MODE,
                                      0, nullptr, nullptr),
                     UR_RESULT_ERROR_INVALID_NULL_POINTER);
}

TEST_P(urSamplerGetInfoNegativeTest, InvalidNullPointerPropValue) {
    ASSERT_EQ_RESULT(urSamplerGetInfo(sampler, UR_SAMPLER_INFO_ADDRESSING_MODE,
                                      sizeof(ur_sampler_addressing_mode_t),
                                      nullptr, nullptr),
                     UR_RESULT_ERROR_INVALID_NULL_POINTER);
}

TEST_P(urSamplerGetInfoNegativeTest, InvalidSizePropSizeZero) {
    ur_sampler_addressing_mode_t mode;
    ASSERT_EQ_RESULT(urSamplerGetInfo(sampler, UR_SAMPLER_INFO_ADDRESSING_MODE,
                                      0, &mode, nullptr),
                     UR_RESULT_ERROR_INVALID_SIZE);
}

TEST_P(urSamplerGetInfoNegativeTest, InvalidSizePropSizeSmall) {
    ur_sampler_addressing_mode_t mode;
    ASSERT_EQ_RESULT(urSamplerGetInfo(sampler, UR_SAMPLER_INFO_ADDRESSING_MODE,
                                      sizeof(mode) - 1, &mode, nullptr),
                     UR_RESULT_ERROR_INVALID_SIZE);
}
