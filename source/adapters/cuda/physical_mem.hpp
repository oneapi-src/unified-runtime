//===---------- physical_mem.hpp - CUDA Adapter ---------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

#include <ur/ur.hpp>

#include <cuda.h>

#include "adapter.hpp"
#include "common/ur_ref_count.hpp"
#include "device.hpp"
#include "platform.hpp"

/// UR queue mapping on physical memory allocations used in virtual memory
/// management.
///
struct ur_physical_mem_handle_t_ : ur::cuda::handle_base {
  using native_type = CUmemGenericAllocationHandle;

  ur::RefCount RefCount;
  native_type PhysicalMem;
  ur_context_handle_t_ *Context;
  ur_device_handle_t Device;
  size_t Size;
  ur_physical_mem_properties_t Properties;

  ur_physical_mem_handle_t_(native_type PhysMem, ur_context_handle_t_ *Ctx,
                            ur_device_handle_t Device, size_t Size,
                            ur_physical_mem_properties_t Properties)
      : handle_base(), PhysicalMem(PhysMem), Context(Ctx), Device(Device),
        Size(Size), Properties(Properties) {
    urContextRetain(Context);
  }

  ~ur_physical_mem_handle_t_() { urContextRelease(Context); }

  native_type get() const noexcept { return PhysicalMem; }

  ur_context_handle_t_ *getContext() const noexcept { return Context; }

  ur_device_handle_t_ *getDevice() const noexcept { return Device; }

  size_t getSize() const noexcept { return Size; }

  ur_physical_mem_properties_t getProperties() const noexcept {
    return Properties;
  }
};
