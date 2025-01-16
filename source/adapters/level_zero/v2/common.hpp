//===--------- common.hpp - Level Zero Adapter ---------------------------===//
//
// Copyright (C) 2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <exception>
#include <ze_api.h>

#include "../common.hpp"
#include "../ur_interface_loader.hpp"
#include "logger/ur_logger.hpp"
namespace {
#define DECLARE_DESTROY_FUNCTION(name)                                         \
  template <typename ZeHandleT> ze_result_t name##_wrapped(ZeHandleT handle) { \
    return ZE_CALL_NOCHECK_NAME(name, (handle), #name);                        \
  }

#define HANDLE_WRAPPER_TYPE(handle, destroy)                                   \
  ze_handle_wrapper<handle, destroy##_wrapped<handle>>
} // namespace

namespace v2 {

DECLARE_DESTROY_FUNCTION(zeKernelDestroy);
DECLARE_DESTROY_FUNCTION(zeEventDestroy);
DECLARE_DESTROY_FUNCTION(zeEventPoolDestroy);
DECLARE_DESTROY_FUNCTION(zeContextDestroy);
DECLARE_DESTROY_FUNCTION(zeCommandListDestroy);
namespace raii {

template <typename ZeHandleT, ze_result_t (*destroy)(ZeHandleT)>
struct ze_handle_wrapper {
  ze_handle_wrapper(bool ownZeHandle = true)
      : handle(nullptr), ownZeHandle(ownZeHandle) {}

  ze_handle_wrapper(ZeHandleT handle, bool ownZeHandle = true)
      : handle(handle), ownZeHandle(ownZeHandle) {}

  ze_handle_wrapper(const ze_handle_wrapper &) = delete;
  ze_handle_wrapper &operator=(const ze_handle_wrapper &) = delete;

  ze_handle_wrapper(ze_handle_wrapper &&other)
      : handle(other.handle), ownZeHandle(other.ownZeHandle) {
    other.handle = nullptr;
  }

  ze_handle_wrapper &operator=(ze_handle_wrapper &&other) {
    if (this == &other) {
      return *this;
    }

    if (handle) {
      reset();
    }
    handle = other.handle;
    ownZeHandle = other.ownZeHandle;
    other.handle = nullptr;
    return *this;
  }

  ~ze_handle_wrapper() {
    try {
      reset();
    } catch (...) {
      // logging already done in reset
    }
  }

  void reset() {
    if (!handle) {
      return;
    }

    if (ownZeHandle) {
      auto zeResult = destroy(handle);
      // Gracefully handle the case that L0 was already unloaded.
      if (zeResult && zeResult != ZE_RESULT_ERROR_UNINITIALIZED)
        throw ze2urResult(zeResult);
    }

    handle = nullptr;
  }

  std::pair<ZeHandleT, bool> release() {
    auto handle = this->handle;
    this->handle = nullptr;
    return {handle, ownZeHandle};
  }

  ZeHandleT get() const { return handle; }

  ZeHandleT *ptr() { return &handle; }

private:
  ZeHandleT handle;
  bool ownZeHandle;
};

using ze_kernel_handle_t = HANDLE_WRAPPER_TYPE(::ze_kernel_handle_t,
                                               zeKernelDestroy);

using ze_event_handle_t = HANDLE_WRAPPER_TYPE(::ze_event_handle_t,
                                              zeEventDestroy);

using ze_event_pool_handle_t = HANDLE_WRAPPER_TYPE(::ze_event_pool_handle_t,
                                                   zeEventPoolDestroy);

using ze_context_handle_t = HANDLE_WRAPPER_TYPE(::ze_context_handle_t,
                                                zeContextDestroy);

using ze_command_list_handle_t = HANDLE_WRAPPER_TYPE(::ze_command_list_handle_t,
                                                     zeCommandListDestroy);

template <typename URHandle, ur_result_t (*retain)(URHandle),
          ur_result_t (*release)(URHandle)>
struct ref_counted {
  ref_counted(URHandle handle) : handle(handle) { retain(handle); }

  ~ref_counted() { release(handle); }

  operator URHandle() const { return handle; }
  URHandle operator->() const { return handle; }

  ref_counted(const ref_counted &) = delete;
  ref_counted &operator=(const ref_counted &) = delete;

  ref_counted(ref_counted &&other) {
    handle = other.handle;
    other.handle = nullptr;
  }

  ref_counted &operator=(ref_counted &&other) {
    if (this == &other) {
      return *this;
    }

    if (handle) {
      release(handle);
    }

    handle = other.handle;
    other.handle = nullptr;
    return *this;
  }

  URHandle get() const { return handle; }

private:
  URHandle handle;
};

// This version of ref_counted does not call retain/release functions.
// It is used to avoid circular references, most notably to ur_context_handle_t.
// This is equivalent to just using URHandle but makes it clear that no ref
// counting is expected.
template <typename URHandle> struct weak {
  template <ur_result_t (*retain)(URHandle), ur_result_t (*release)(URHandle)>
  weak(const ref_counted<URHandle, retain, release> &handle)
      : handle(handle.get()) {}

  operator URHandle() const { return handle; }
  URHandle operator->() const { return handle; }

  weak(const weak &) = default;
  weak &operator=(const weak &) = default;

  weak(weak &&other) = default;
  weak &operator=(weak &&other) = default;

  URHandle get() const { return handle; }

private:
  URHandle handle;
};

template <typename URHandle> struct ref_counted_traits;

#define DECLARE_REF_COUNTER_TRAITS(URHandle, retainFn, releaseFn)              \
  template <> struct ref_counted_traits<URHandle> {                            \
    static ur_result_t retain(URHandle handle) {                               \
      assert(handle);                                                          \
      return retainFn(handle);                                                 \
    }                                                                          \
    static ur_result_t release(URHandle handle) {                              \
      assert(handle);                                                          \
      return releaseFn(handle);                                                \
    }                                                                          \
    static ur_result_t validate([[maybe_unused]] URHandle handle) {            \
      assert(handle);                                                          \
      assert(reinterpret_cast<_ur_object *>(handle)->RefCount.load() != 0);    \
      return UR_RESULT_SUCCESS;                                                \
    }                                                                          \
  };

// This version of ref_counted calls retain/release functions.
template <typename URHandle>
using rc = ref_counted<URHandle, ref_counted_traits<URHandle>::retain,
                       ref_counted_traits<URHandle>::release>;

// This version of ref_counted validates that the ref count is not zero on every
// release and retain in debug mode, and does nothing in the release mode.
// Used for types that should always be alibe during the adapter lifetime (e.g.
// devices).
template <typename URHandle>
using rc_val_only =
    ref_counted<URHandle, ref_counted_traits<URHandle>::validate,
                ref_counted_traits<URHandle>::validate>;

DECLARE_REF_COUNTER_TRAITS(::ur_device_handle_t, ur::level_zero::urDeviceRetain,
                           ur::level_zero::urDeviceRelease);
DECLARE_REF_COUNTER_TRAITS(::ur_context_handle_t,
                           ur::level_zero::urContextRetain,
                           ur::level_zero::urContextRelease);
DECLARE_REF_COUNTER_TRAITS(::ur_mem_handle_t, ur::level_zero::urMemRetain,
                           ur::level_zero::urMemRelease);
DECLARE_REF_COUNTER_TRAITS(::ur_program_handle_t,
                           ur::level_zero::urProgramRetain,
                           ur::level_zero::urProgramRelease);
DECLARE_REF_COUNTER_TRAITS(::ur_queue_handle_t, ur::level_zero::urQueueRetain,
                           ur::level_zero::urQueueRelease);

} // namespace raii

} // namespace v2
