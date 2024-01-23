//===--------- context.hpp - Libomptarget Adapter --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//
#pragma once

#include "common.hpp"
#include "device.hpp"
#include "usm.hpp"
#include <set>
#include <umf/memory_pool.h>

struct ur_context_handle_t_ {

public:
  struct deleter_data {
    ur_context_extended_deleter_t Function;
    void *UserData;

    void operator()() { Function(UserData); }
  };

  ur_context_handle_t_(ur_device_handle_t_ *Device)
      : Device{Device}, RefCount{1} {
    urDeviceRetain(Device);
  };

  void setExtendedDeleter(ur_context_extended_deleter_t Function,
                          void *UserData) {
    std::lock_guard<std::mutex> Guard(Mutex);
    ExtendedDeleters.emplace_back(deleter_data{Function, UserData});
  }

  ur_device_handle_t getDevice() const noexcept { return Device; }

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }

  void addPool(ur_usm_pool_handle_t Pool);

  void removePool(ur_usm_pool_handle_t Pool);

  ur_usm_pool_handle_t getOwningURPool(umf_memory_pool_t *UMFPool);

  void addUSMAllocation(const USMAllocation &AllocInfo);

  ur_result_t removeUSMAllocation(const void *Ptr);

  ur_result_t findUSMAllocation(USMAllocation &AllocInfo, const void *Ptr);

private:
  std::mutex Mutex;
  std::vector<deleter_data> ExtendedDeleters;
  ur_device_handle_t Device;
  std::atomic_uint32_t RefCount;
  std::set<ur_usm_pool_handle_t> PoolHandles;
  std::vector<USMAllocation> USMAllocations;
};
