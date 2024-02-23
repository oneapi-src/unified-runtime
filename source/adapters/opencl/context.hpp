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
  std::atomic<uint32_t> RefCount = 0;

  ur_context_handle_t_(native_type Ctx, uint32_t DevCount,
                       const ur_device_handle_t *phDevices)
      : Context(Ctx), DeviceCount(DevCount) {
    for (uint32_t i = 0; i < DeviceCount; i++) {
      Devices.emplace_back(phDevices[i]);
      urDeviceRetain(phDevices[i]);
    }
    RefCount = 1;
  }

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }

  static ur_result_t makeWithNative(native_type Ctx, uint32_t DevCount,
                                    const ur_device_handle_t *phDevices,
                                    ur_context_handle_t &Context) {
    if (!phDevices) {
      return UR_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    try {
      uint32_t CLDeviceCount;
      CL_RETURN_ON_FAILURE(clGetContextInfo(Ctx, CL_CONTEXT_NUM_DEVICES,
                                            sizeof(CLDeviceCount),
                                            &CLDeviceCount, nullptr));
      std::vector<cl_device_id> CLDevices(CLDeviceCount);
      CL_RETURN_ON_FAILURE(clGetContextInfo(Ctx, CL_CONTEXT_DEVICES,
                                            sizeof(CLDevices), CLDevices.data(),
                                            nullptr));
      if (DevCount != CLDeviceCount) {
        return UR_RESULT_ERROR_INVALID_CONTEXT;
      }
      for (uint32_t i = 0; i < DevCount; i++) {
        if (phDevices[i]->get() != CLDevices[i]) {
          return UR_RESULT_ERROR_INVALID_CONTEXT;
        }
      }
      auto URContext =
          std::make_unique<ur_context_handle_t_>(Ctx, DevCount, phDevices);
      Context = URContext.release();
    } catch (std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }

    return UR_RESULT_SUCCESS;
  }

  ~ur_context_handle_t_() {
    for (uint32_t i = 0; i < DeviceCount; i++) {
      urDeviceRelease(Devices[i]);
    }
    clReleaseContext(Context);
  }

  native_type get() { return Context; }

  ur_platform_handle_t getPlatform() { return Devices[0]->Platform; }

  const std::vector<ur_device_handle_t> &getDevices() { return Devices; }
};
