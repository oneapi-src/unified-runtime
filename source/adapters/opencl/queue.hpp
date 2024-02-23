//===--------- queue.hpp - OpenCL Adapter ---------------------------===//
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
#include "context.hpp"
#include "device.hpp"

#include <vector>

struct ur_queue_handle_t_ {
  using native_type = cl_command_queue;
  native_type Queue;
  ur_context_handle_t Context;
  ur_device_handle_t Device;
  std::atomic<uint32_t> RefCount = 0;

  ur_queue_handle_t_(native_type Queue, ur_context_handle_t Ctx,
                     ur_device_handle_t Dev)
      : Queue(Queue), Context(Ctx), Device(Dev) {
    RefCount = 1;
    urDeviceRetain(Device);
    urContextRetain(Context);
  }

  static ur_result_t makeWithNative(native_type NativeQueue,
                                    ur_context_handle_t Context,
                                    ur_device_handle_t Device,
                                    ur_queue_handle_t &Queue) {
    if (!Context || !Device) {
      return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    try {
      cl_context CLContext;
      CL_RETURN_ON_FAILURE(clGetCommandQueueInfo(NativeQueue, CL_QUEUE_CONTEXT,
                                                 sizeof(CLContext), &CLContext,
                                                 nullptr));
      cl_device_id CLDevice;
      CL_RETURN_ON_FAILURE(clGetCommandQueueInfo(
          NativeQueue, CL_QUEUE_DEVICE, sizeof(CLDevice), &CLDevice, nullptr));
      if (Context->get() != CLContext) {
        return UR_RESULT_ERROR_INVALID_CONTEXT;
      }
      if (Device->get() != CLDevice) {
        return UR_RESULT_ERROR_INVALID_DEVICE;
      }
      auto URQueue =
          std::make_unique<ur_queue_handle_t_>(NativeQueue, Context, Device);
      Queue = URQueue.release();
    } catch (std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }
    return UR_RESULT_SUCCESS;
  }

  ~ur_queue_handle_t_() {
    clReleaseCommandQueue(Queue);
    urDeviceRelease(Device);
    urContextRelease(Context);
  }

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }

  native_type get() { return Queue; }

  ur_platform_handle_t getPlatform() { return Device->Platform; }
};
