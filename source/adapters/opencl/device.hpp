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
#include "platform.hpp"

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

  ur_result_t getDeviceVersion(oclv::OpenCLVersion &Version) {
    size_t DevVerSize = 0;
    CL_RETURN_ON_FAILURE(
        clGetDeviceInfo(Device, CL_DEVICE_VERSION, 0, nullptr, &DevVerSize));

    std::string DevVer(DevVerSize, '\0');
    CL_RETURN_ON_FAILURE(clGetDeviceInfo(Device, CL_DEVICE_VERSION, DevVerSize,
                                         DevVer.data(), nullptr));

    Version = oclv::OpenCLVersion(DevVer);
    if (!Version.isValid()) {
      return UR_RESULT_ERROR_INVALID_DEVICE;
    }

    return UR_RESULT_SUCCESS;
  }

  ur_result_t checkDeviceExtensions(const std::vector<std::string> &Exts,
                                    bool &Supported) {
    size_t ExtSize = 0;
    CL_RETURN_ON_FAILURE(
        clGetDeviceInfo(Device, CL_DEVICE_EXTENSIONS, 0, nullptr, &ExtSize));

    std::string ExtStr(ExtSize, '\0');

    CL_RETURN_ON_FAILURE(clGetDeviceInfo(Device, CL_DEVICE_EXTENSIONS, ExtSize,
                                         ExtStr.data(), nullptr));

    Supported = true;
    for (const std::string &Ext : Exts) {
      if (!(Supported = (ExtStr.find(Ext) != std::string::npos))) {
        break;
      }
    }

    return UR_RESULT_SUCCESS;
  }
};
