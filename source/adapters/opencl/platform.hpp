//===--------- platform.hpp - OpenCL Adapter ---------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

#include "common.hpp"
#include "device.hpp"

#include <vector>

namespace cl_adapter {
ur_result_t getPlatformVersion(cl_platform_id Plat,
                               oclv::OpenCLVersion &Version);
} // namespace cl_adapter

struct ur_platform_handle_t_ {
  using native_type = cl_platform_id;
  native_type Platform = nullptr;
  std::vector<std::unique_ptr<ur_device_handle_t_>> Devices;

  ur_platform_handle_t_(native_type Plat) : Platform(Plat) {}

  ~ur_platform_handle_t_() {}

  template <typename T>
  ur_result_t getExtFunc(T CachedExtFunc, const char *FuncName, T *Fptr) {
    if (!CachedExtFunc) {
      // TODO: check that the function is available
      CachedExtFunc = reinterpret_cast<T>(
          clGetExtensionFunctionAddressForPlatform(Platform, FuncName));
      if (!CachedExtFunc) {
        return UR_RESULT_ERROR_INVALID_VALUE;
      }
    }
    *Fptr = CachedExtFunc;
    return UR_RESULT_SUCCESS;
  }

  native_type get() { return Platform; }

  ur_result_t InitDevices() {
    if (Devices.empty()) {
      cl_uint DeviceNum = 0;
      CL_RETURN_ON_FAILURE(
          clGetDeviceIDs(Platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &DeviceNum));

      std::vector<cl_device_id> CLDevices(DeviceNum);
      CL_RETURN_ON_FAILURE(clGetDeviceIDs(
          Platform, CL_DEVICE_TYPE_ALL, DeviceNum, CLDevices.data(), nullptr));

      Devices.resize(DeviceNum);
      for (size_t i = 0; i < DeviceNum; i++) {
        Devices[i] =
            std::make_unique<ur_device_handle_t_>(CLDevices[i], this, nullptr);
      }
    }

    return UR_RESULT_SUCCESS;
  }
};
