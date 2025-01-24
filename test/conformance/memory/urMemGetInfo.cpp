// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <array>
#include <map>
#include <uur/fixtures.h>
#include <uur/known_failure.h>

using urMemGetInfoTest = uur::urMemBufferTest;
UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(urMemGetInfoTest);

TEST_P(urMemGetInfoTest, SuccessSize) {
  UUR_KNOWN_FAILURE_ON(uur::NativeCPU{});

  const ur_mem_info_t property_name = UR_MEM_INFO_SIZE;
  size_t property_size = 0;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urMemGetInfo(buffer, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_EQ(property_size, sizeof(size_t));

  size_t property_value = 0;
  ASSERT_SUCCESS(urMemGetInfo(buffer, property_name, property_size,
                              &property_value, nullptr));

  ASSERT_GE(property_value, allocation_size);
}

TEST_P(urMemGetInfoTest, SuccessContext) {
  UUR_KNOWN_FAILURE_ON(uur::NativeCPU{});

  const ur_mem_info_t property_name = UR_MEM_INFO_CONTEXT;
  size_t property_size = 0;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urMemGetInfo(buffer, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_EQ(property_size, sizeof(ur_context_handle_t));

  ur_context_handle_t property_value = nullptr;
  ASSERT_SUCCESS(urMemGetInfo(buffer, property_name, property_size,
                              &property_value, nullptr));

  ASSERT_EQ(context, property_value);
}

TEST_P(urMemGetInfoTest, SuccessReferenceCount) {
  UUR_KNOWN_FAILURE_ON(uur::NativeCPU{});

  const ur_mem_info_t property_name = UR_MEM_INFO_REFERENCE_COUNT;
  size_t property_size = 0;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urMemGetInfo(buffer, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_EQ(property_size, sizeof(uint32_t));

  uint32_t property_value = 0;
  ASSERT_SUCCESS(urMemGetInfo(buffer, property_name, property_size,
                              &property_value, nullptr));

  ASSERT_GT(property_value, 0);
}

TEST_P(urMemGetInfoTest, InvalidNullHandleMemory) {
  size_t property_size = 0;
  ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_NULL_HANDLE,
                   urMemGetInfo(nullptr, UR_MEM_INFO_SIZE,
                                sizeof(property_size), &property_size,
                                nullptr));
}

TEST_P(urMemGetInfoTest, InvalidEnumerationMemInfoType) {
  size_t property_size = 0;
  ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_ENUMERATION,
                   urMemGetInfo(buffer, UR_MEM_INFO_FORCE_UINT32,
                                sizeof(property_size), &property_size,
                                nullptr));
}

TEST_P(urMemGetInfoTest, InvalidSizeZero) {
  size_t property_size = 0;
  ASSERT_EQ_RESULT(
      urMemGetInfo(buffer, UR_MEM_INFO_SIZE, 0, &property_size, nullptr),
      UR_RESULT_ERROR_INVALID_SIZE);
}

TEST_P(urMemGetInfoTest, InvalidSizeSmall) {
  UUR_KNOWN_FAILURE_ON(uur::NativeCPU{});

  size_t property_size = 0;
  ASSERT_EQ_RESULT(urMemGetInfo(buffer, UR_MEM_INFO_SIZE,
                                sizeof(property_size) - 1, &property_size,
                                nullptr),
                   UR_RESULT_ERROR_INVALID_SIZE);
}

TEST_P(urMemGetInfoTest, InvalidNullPointerParamValue) {
  size_t property_size = 0;
  ASSERT_EQ_RESULT(urMemGetInfo(buffer, UR_MEM_INFO_SIZE, sizeof(property_size),
                                nullptr, nullptr),
                   UR_RESULT_ERROR_INVALID_NULL_POINTER);
}

TEST_P(urMemGetInfoTest, InvalidNullPointerPropSizeRet) {
  ASSERT_EQ_RESULT(urMemGetInfo(buffer, UR_MEM_INFO_SIZE, 0, nullptr, nullptr),
                   UR_RESULT_ERROR_INVALID_NULL_POINTER);
}

struct urMemGetInfoImageTest : uur::urMemImageTest {
  void SetUp() override {
    UUR_KNOWN_FAILURE_ON(uur::LevelZeroV2{});
    uur::urMemImageTest::SetUp();
  }
};

UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(urMemGetInfoImageTest);

TEST_P(urMemGetInfoImageTest, SuccessSize) {
  UUR_KNOWN_FAILURE_ON(uur::LevelZero{}, uur::OpenCL{"UHD Graphics"});

  const ur_mem_info_t property_name = UR_MEM_INFO_SIZE;
  size_t property_size = 0;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urMemGetInfo(image, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_EQ(property_size, sizeof(size_t));

  size_t property_value = 0;
  ASSERT_SUCCESS(urMemGetInfo(image, property_name, property_size,
                              &property_value, nullptr));

  const size_t expected_pixel_size = sizeof(uint8_t) * 4;
  const size_t expected_image_size = expected_pixel_size *
                                     image_desc.arraySize * image_desc.width *
                                     image_desc.height * image_desc.depth;

  ASSERT_EQ(property_value, expected_image_size);
}

TEST_P(urMemGetInfoImageTest, SuccessContext) {
  const ur_mem_info_t property_name = UR_MEM_INFO_CONTEXT;
  size_t property_size = 0;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urMemGetInfo(image, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_EQ(property_size, sizeof(ur_context_handle_t));

  ur_context_handle_t property_value = nullptr;
  ASSERT_SUCCESS(urMemGetInfo(image, property_name, property_size,
                              &property_value, nullptr));

  ASSERT_EQ(context, property_value);
}

TEST_P(urMemGetInfoImageTest, SuccessReferenceCount) {
  const ur_mem_info_t property_name = UR_MEM_INFO_REFERENCE_COUNT;
  size_t property_size = 0;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urMemGetInfo(image, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_EQ(property_size, sizeof(uint32_t));

  uint32_t property_value = 0;
  ASSERT_SUCCESS(urMemGetInfo(image, property_name, property_size,
                              &property_value, nullptr));

  ASSERT_GT(property_value, 0);
}
