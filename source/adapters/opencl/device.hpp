//===--------- device.hpp - OpenCL Adapter ---------------------------===//
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

namespace cl_adapter {
ur_result_t getDeviceVersion(cl_device_id Dev, oclv::OpenCLVersion &Version);

ur_result_t checkDeviceExtensions(cl_device_id Dev,
                                  const std::vector<std::string> &Exts,
                                  bool &Supported);
} // namespace cl_adapter

struct ur_device_handle_t_ {
  using native_type = cl_device_id;
  native_type Device;
  ur_platform_handle_t Platform;
  cl_device_type Type = 0;
  ur_device_handle_t ParentDevice = nullptr;

  ur_device_handle_t_(native_type Dev, ur_platform_handle_t Plat,
                      ur_device_handle_t Parent)
      : Device(Dev), Platform(Plat), ParentDevice(Parent) {
    if (Parent) {
      Type = Parent->Type;
    } else {
      clGetDeviceInfo(Device, CL_DEVICE_TYPE, sizeof(cl_device_type), &Type,
                      nullptr);
    }
  }

  ~ur_device_handle_t_() {}

  native_type get() { return Device; }
};