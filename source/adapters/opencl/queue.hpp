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
    if (Device) {
      urDeviceRetain(Device);
    }
    if (Context) {
      urContextRetain(Context);
    }
  }

  static ur_result_t makeWithNative(native_type NativeQueue,
                                    ur_context_handle_t Context,
                                    ur_device_handle_t Device,
                                    ur_queue_handle_t &Queue) {
    try {
      auto URQueue =
          std::make_unique<ur_queue_handle_t_>(NativeQueue, Context, Device);
      if (!Context) {
        cl_context CLContext;
        CL_RETURN_ON_FAILURE(
            clGetCommandQueueInfo(NativeQueue, CL_QUEUE_CONTEXT,
                                  sizeof(CLContext), &CLContext, nullptr));
        ur_native_handle_t NativeContext =
            reinterpret_cast<ur_native_handle_t>(CLContext);
        UR_RETURN_ON_FAILURE(urContextCreateWithNativeHandle(
            NativeContext, 0, nullptr, nullptr, &(URQueue->Context)));
        UR_RETURN_ON_FAILURE(urContextRetain(URQueue->Context));
      }
      if (!Device) {
        cl_device_id CLDevice;
        CL_RETURN_ON_FAILURE(clGetCommandQueueInfo(NativeQueue, CL_QUEUE_DEVICE,
                                                   sizeof(CLDevice), &CLDevice,
                                                   nullptr));
        ur_native_handle_t NativeDevice =
            reinterpret_cast<ur_native_handle_t>(CLDevice);
        UR_RETURN_ON_FAILURE(urDeviceCreateWithNativeHandle(
            NativeDevice, nullptr, nullptr, &(URQueue->Device)));
        UR_RETURN_ON_FAILURE(urDeviceRetain(URQueue->Device));
      }
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
    if (Device) {
      urDeviceRelease(Device);
    }
    if (Context) {
      urContextRelease(Context);
    }
  }

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }

  native_type get() { return Queue; }
};
