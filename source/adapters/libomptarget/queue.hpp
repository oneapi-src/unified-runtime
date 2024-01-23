//===--------- queue.hpp - Libomptarget Adapter ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//
#pragma once

#include <atomic>
#include <omptargetplugin.h>
#include <ur_api.h>

struct ur_queue_handle_t_ {
  ur_context_handle_t Context;
  ur_device_handle_t Device;
  __tgt_async_info *AsyncInfo;
  ur_queue_flags_t Flags;
  std::atomic_uint32_t RefCount;

  ur_queue_handle_t_(ur_context_handle_t Context, ur_device_handle_t Device,
                     __tgt_async_info *AsyncInfo, ur_queue_flags_t Flags)
      : Context(Context), Device(Device), AsyncInfo(AsyncInfo), Flags(Flags),
        RefCount(1) {
    urContextRetain(Context);
    urDeviceRetain(Device);
  }

  ~ur_queue_handle_t_() {
    urDeviceRelease(Device);
    urContextRelease(Context);
  }

  ur_device_handle_t getDevice() const noexcept { return Device; }

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }
};
