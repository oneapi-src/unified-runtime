//===--------- memory.hpp - Libomptarget Adapter ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once
#include "common.hpp"
#include "context.hpp"

struct ur_mem_handle_t_ {
  ur_context_handle_t Context;
  ur_mem_handle_t Parent;
  std::atomic_uint32_t ReferenceCount;
  ur_mem_flags_t MemFlags;
  enum class AllocMode {
    Classic,
    AllocHostPtr,
    AllocCopyHostPtr,
    UseHostPtr,
  } MemAllocMode;
  void *Ptr;
  void *HostPtr;
  size_t BufferSize;

  ur_mem_handle_t_(ur_context_handle_t Context, ur_mem_handle_t Parent,
                   ur_mem_flags_t MemFlags, AllocMode Mode, void *DevicePtr,
                   void *HostPtr, size_t Size)
      : Context{Context}, Parent{Parent}, ReferenceCount(1), MemFlags{MemFlags},
        MemAllocMode{Mode}, Ptr{DevicePtr}, HostPtr{HostPtr}, BufferSize{Size} {
    if (IsSubBuffer()) {
      urMemRetain(Parent);
    } else {
      urContextRetain(Context);
    }
  }

  ~ur_mem_handle_t_() {
    if (IsSubBuffer()) {
      urMemRelease(Parent);
      return;
    }

    if (MemAllocMode == AllocMode::Classic) {
      __tgt_rtl_data_delete(Context->getDevice()->getID(), Ptr,
                            TARGET_ALLOC_DEVICE);
    }

    if (MemAllocMode == AllocMode::AllocHostPtr ||
        MemAllocMode == AllocMode::AllocCopyHostPtr) {
      __tgt_rtl_data_delete(Context->getDevice()->getID(), Ptr,
                            TARGET_ALLOC_HOST);
    }

    urContextRelease(Context);
  }

  bool IsSubBuffer() { return Parent != nullptr; }
  uint32_t GetReferenceCount() { return ReferenceCount; }
  uint32_t IncrementReferenceCount() { return ++ReferenceCount; }
  uint32_t DecrementReferenceCount() { return --ReferenceCount; }
  size_t GetSize() { return BufferSize; }
  ur_context_handle_t GetContext() { return Context; }
};
