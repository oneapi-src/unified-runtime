//===--------- context.hpp - OpenCL Adapter ---------------------------===//
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

struct ur_context_handle_t_ {
  using native_type = cl_context;
  native_type Context;
  std::vector<ur_device_handle_t> Devices;
  uint32_t DeviceCount;

  ur_context_handle_t_(native_type Ctx, uint32_t DevCount,
                       const ur_device_handle_t *phDevices)
      : Context(Ctx), DeviceCount(DevCount) {
    for (uint32_t i = 0; i < DeviceCount; i++) {
      Devices.emplace_back(phDevices[i]);
    }
  }

  static ur_result_t makeWithNative(native_type Ctx, uint32_t DevCount,
                                    const ur_device_handle_t *phDevices,
                                    ur_context_handle_t &Context) {
    try {
      auto URContext =
          std::make_unique<ur_context_handle_t_>(Ctx, DevCount, phDevices);
      native_type &NativeContext = URContext->Context;
      uint32_t &DeviceCount = URContext->DeviceCount;
      if (!DeviceCount) {
        CL_RETURN_ON_FAILURE(
            clGetContextInfo(NativeContext, CL_CONTEXT_NUM_DEVICES,
                             sizeof(DeviceCount), &DeviceCount, nullptr));
        std::vector<cl_device_id> CLDevices(DeviceCount);
        CL_RETURN_ON_FAILURE(clGetContextInfo(NativeContext, CL_CONTEXT_DEVICES,
                                              sizeof(CLDevices),
                                              CLDevices.data(), nullptr));
        URContext->Devices.resize(DeviceCount);
        for (uint32_t i = 0; i < DeviceCount; i++) {
          ur_native_handle_t NativeDevice =
              reinterpret_cast<ur_native_handle_t>(CLDevices[i]);
          UR_RETURN_ON_FAILURE(urDeviceCreateWithNativeHandle(
              NativeDevice, nullptr, nullptr, &(URContext->Devices[i])));
        }
      }
      Context = URContext.release();
    } catch (std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }

    return UR_RESULT_SUCCESS;
  }

  ~ur_context_handle_t_() {}

  native_type get() { return Context; }
};
