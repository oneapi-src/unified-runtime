//===----------- common.hpp - Native CPU Adapter ---------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include "ur/ur.hpp"

constexpr size_t MaxMessageSize = 256;

extern thread_local ur_result_t ErrorMessageCode;
extern thread_local char ErrorMessage[MaxMessageSize];

// Base class to store common data
struct _ur_object {
  ur_shared_mutex Mutex;
};

// Todo: replace this with a common helper once it is available
struct RefCounted {
  std::atomic_uint32_t _refCount;
  uint32_t incrementReferenceCount() { return ++_refCount; }
  uint32_t decrementReferenceCount() { return --_refCount; }
  RefCounted() : _refCount{1} {}
  uint32_t getReferenceCount() const { return _refCount; }
};

template <typename T> inline void decrementOrDelete(T *refC) {
  if (refC->decrementReferenceCount() == 0)
    delete refC;
}
