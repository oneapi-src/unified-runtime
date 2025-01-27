// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <uur/fixtures.h>
#include <uur/known_failure.h>

struct urProgramGetInfoTest : uur::urProgramTest {
  void SetUp() override {
    UUR_RETURN_ON_FATAL_FAILURE(urProgramTest::SetUp());
    // Some queries need the program to be built.
    ASSERT_SUCCESS(urProgramBuild(this->context, program, nullptr));
  }
};

UUR_INSTANTIATE_DEVICE_TEST_SUITE_P(urProgramGetInfoTest);

TEST_P(urProgramGetInfoTest, SuccessReferenceCount) {
  size_t property_size = 0;
  const ur_program_info_t property_name = UR_PROGRAM_INFO_REFERENCE_COUNT;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urProgramGetInfo(program, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_EQ(property_size, sizeof(uint32_t));

  uint32_t property_value = 0;
  ASSERT_SUCCESS(urProgramGetInfo(program, property_name, property_size,
                                  &property_value, nullptr));

  ASSERT_GT(property_value, 0U);
}

TEST_P(urProgramGetInfoTest, SuccessContext) {
  size_t property_size = 0;
  const ur_program_info_t property_name = UR_PROGRAM_INFO_CONTEXT;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urProgramGetInfo(program, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_EQ(property_size, sizeof(ur_context_handle_t));

  ur_context_handle_t property_value = nullptr;
  ASSERT_SUCCESS(urProgramGetInfo(program, property_name, property_size,
                                  &property_value, nullptr));

  ASSERT_EQ(property_value, context);
}

TEST_P(urProgramGetInfoTest, SuccessNumDevices) {
  size_t property_size = 0;
  const ur_program_info_t property_name = UR_PROGRAM_INFO_NUM_DEVICES;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urProgramGetInfo(program, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_EQ(property_size, sizeof(uint32_t));

  uint32_t property_value = 0;
  ASSERT_SUCCESS(urProgramGetInfo(program, property_name, property_size,
                                  &property_value, nullptr));

  ASSERT_GE(uur::DevicesEnvironment::instance->devices.size(), property_value);
}

TEST_P(urProgramGetInfoTest, SuccessDevices) {
  size_t property_size = 0;
  const ur_program_info_t property_name = UR_PROGRAM_INFO_DEVICES;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urProgramGetInfo(program, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_EQ(property_size, sizeof(ur_context_handle_t));

  std::vector<ur_device_handle_t> property_value(property_size);
  ASSERT_SUCCESS(urProgramGetInfo(program, property_name, property_size,
                                  property_value.data(), nullptr));
  ASSERT_EQ(property_size, property_value.size());

  size_t devices_count = property_size / sizeof(ur_device_handle_t);

  ASSERT_EQ(devices_count, 1);
  ASSERT_EQ(property_value[0], device);
}

TEST_P(urProgramGetInfoTest, SuccessIL) {
  size_t property_size = 0;
  const ur_program_info_t property_name = UR_PROGRAM_INFO_IL;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urProgramGetInfo(program, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_GE(property_size, 0);

  std::vector<char> property_value(property_size, 'x');
  // Some adapters only support ProgramCreateWithBinary, in those cases we
  // expect a return size of 0 and an empty return value for INFO_IL.
  if (property_size > 0) {
    ASSERT_SUCCESS(urProgramGetInfo(program, UR_PROGRAM_INFO_IL, property_size,
                                    property_value.data(), nullptr));
    ASSERT_EQ(property_size, property_value.size());
    EXPECT_NE(property_value[0], 'x');
    EXPECT_EQ(property_value[property_size - 1], '\0');
    ASSERT_EQ(property_value, *il_binary.get());
  } else {
    ASSERT_TRUE(property_value.empty());
  }
}

TEST_P(urProgramGetInfoTest, SuccessBinarySizes) {
  size_t property_size = 0;
  const ur_program_info_t property_name = UR_PROGRAM_INFO_BINARY_SIZES;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urProgramGetInfo(program, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_GT(property_size, 0);

  std::vector<size_t> property_value(property_size / sizeof(size_t));
  ASSERT_SUCCESS(urProgramGetInfo(program, UR_PROGRAM_INFO_BINARY_SIZES,
                                  property_size, property_value.data(),
                                  nullptr));
ASSERT_EQ(property_size, property_value.size());
  for (const auto &binary_size : property_value) {
    ASSERT_GT(binary_size, 0);
  }
}

TEST_P(urProgramGetInfoTest, SuccessBinaries) {
  size_t property_size = 0;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urProgramGetInfo(program, UR_PROGRAM_INFO_BINARY_SIZES, 0, nullptr,
                       &property_size),
      UR_PROGRAM_INFO_BINARY_SIZES);
  // Due to how the fixtures + env are set up we should only have one
  // device associated with program, so one binary.
  ASSERT_EQ(property_size / sizeof(size_t), 1);

  size_t binary_sizes[1] = {property_size};
  std::vector<char> property_value;
  ASSERT_SUCCESS(urProgramGetInfo(program, UR_PROGRAM_INFO_BINARY_SIZES,
                                  property_size, binary_sizes, nullptr));
  property_value.resize(binary_sizes[0]);
  char *binaries[1] = {property_value.data()};

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urProgramGetInfo(program, UR_PROGRAM_INFO_BINARIES, sizeof(binaries[0]),
                       binaries, nullptr),
      UR_PROGRAM_INFO_BINARIES);
}

TEST_P(urProgramGetInfoTest, SuccessNumKernels) {
  UUR_KNOWN_FAILURE_ON(uur::HIP{});

  size_t property_size = 0;
  const ur_program_info_t property_name = UR_PROGRAM_INFO_NUM_KERNELS;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urProgramGetInfo(program, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_EQ(property_size, sizeof(size_t));

  size_t property_value = 0;
  ASSERT_SUCCESS(urProgramGetInfo(program, property_name, property_size,
                                  &property_value, nullptr));

  ASSERT_GT(property_value, 0U);
}

TEST_P(urProgramGetInfoTest, SuccessKernelNames) {
  size_t property_size = 0;
  const ur_program_info_t property_name = UR_PROGRAM_INFO_KERNEL_NAMES;

  ASSERT_SUCCESS_OR_OPTIONAL_QUERY(
      urProgramGetInfo(program, property_name, 0, nullptr, &property_size),
      property_name);
  ASSERT_GT(property_size, 0);

  std::vector<char> property_value(property_size, 'x');
  ASSERT_SUCCESS(urProgramGetInfo(program, property_name, property_size,
                                  &property_value, nullptr));

  ASSERT_EQ(property_size, property_value.size());
  EXPECT_NE(property_value[0], 'x');
  EXPECT_EQ(property_value[property_size - 1], '\0');
}

TEST_P(urProgramGetInfoTest, InvalidNullHandleProgram) {
  uint32_t property_value = 0;
  ASSERT_EQ_RESULT(urProgramGetInfo(nullptr, UR_PROGRAM_INFO_REFERENCE_COUNT,
                                    sizeof(property_value), &property_value,
                                    nullptr),
                   UR_RESULT_ERROR_INVALID_NULL_HANDLE);
}

TEST_P(urProgramGetInfoTest, InvalidEnumeration) {
  size_t property_size = 0;
  ASSERT_EQ_RESULT(UR_RESULT_ERROR_INVALID_ENUMERATION,
                   urProgramGetInfo(program, UR_PROGRAM_INFO_FORCE_UINT32, 0,
                                    nullptr, &property_size));
}

TEST_P(urProgramGetInfoTest, InvalidSizeZero) {
  uint32_t property_value = 0;
  ASSERT_EQ_RESULT(urProgramGetInfo(program, UR_PROGRAM_INFO_REFERENCE_COUNT, 0,
                                    &property_value, nullptr),
                   UR_RESULT_ERROR_INVALID_SIZE);
}

TEST_P(urProgramGetInfoTest, InvalidSizeSmall) {
  uint32_t property_value = 0;
  ASSERT_EQ_RESULT(urProgramGetInfo(program, UR_PROGRAM_INFO_REFERENCE_COUNT,
                                    sizeof(property_value) - 1, &property_value,
                                    nullptr),
                   UR_RESULT_ERROR_INVALID_SIZE);
}

TEST_P(urProgramGetInfoTest, InvalidNullPointerPropValue) {
  ASSERT_EQ_RESULT(urProgramGetInfo(program, UR_PROGRAM_INFO_REFERENCE_COUNT,
                                    sizeof(uint32_t), nullptr, nullptr),
                   UR_RESULT_ERROR_INVALID_NULL_POINTER);
}

TEST_P(urProgramGetInfoTest, InvalidNullPointerPropValueRet) {
  ASSERT_EQ_RESULT(urProgramGetInfo(program, UR_PROGRAM_INFO_REFERENCE_COUNT, 0,
                                    nullptr, nullptr),
                   UR_RESULT_ERROR_INVALID_NULL_POINTER);
}

TEST_P(urProgramGetInfoTest, NumDevicesIsNonzero) {
  uint32_t property_value = 0;
  ASSERT_SUCCESS(urProgramGetInfo(program, UR_PROGRAM_INFO_NUM_DEVICES,
                                  sizeof(property_value), &property_value,
                                  nullptr));
  ASSERT_GE(property_value, 1);
}

TEST_P(urProgramGetInfoTest, NumDevicesMatchesDeviceArray) {
  uint32_t property_value = 0;
  ASSERT_SUCCESS(urProgramGetInfo(program, UR_PROGRAM_INFO_NUM_DEVICES,
                                  sizeof(property_value), &property_value,
                                  nullptr));

  size_t info_devices_size = 0;
  ASSERT_SUCCESS(urProgramGetInfo(program, UR_PROGRAM_INFO_DEVICES, 0, nullptr,
                                  &info_devices_size));
  ASSERT_EQ(property_value, info_devices_size / sizeof(ur_device_handle_t));
}

TEST_P(urProgramGetInfoTest, NumDevicesMatchesContextNumDevices) {
  uint32_t property_value = 0;
  ASSERT_SUCCESS(urProgramGetInfo(program, UR_PROGRAM_INFO_NUM_DEVICES,
                                  sizeof(property_value), &property_value,
                                  nullptr));

  // The device count either matches the number of devices in the context or
  // is 1, depending on how it was built
  uint32_t info_context_devices_count = 0;
  ASSERT_SUCCESS(urContextGetInfo(context, UR_CONTEXT_INFO_NUM_DEVICES,
                                  sizeof(uint32_t), &info_context_devices_count,
                                  nullptr));
  ASSERT_TRUE(property_value == 1 ||
              property_value == info_context_devices_count);
}
