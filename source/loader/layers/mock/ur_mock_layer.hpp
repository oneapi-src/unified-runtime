/*
 *
 * Copyright (C) 2023-2024 Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file ur_layer.h
 *
 */
#pragma once
#include "ur_ddi.h"
#include "ur_proxy_layer.hpp"
#include "ur_util.hpp"

#include <atomic>

namespace ur_mock_layer {

struct dummy_handle_t_ {
    dummy_handle_t_(size_t dataSize = 0)
        : storage(dataSize), data(storage.data()) {}
    dummy_handle_t_(unsigned char *data) : data(data) {}
    std::atomic<size_t> refCounter = 1;
    std::vector<unsigned char> storage;
    unsigned char *data = nullptr;
};

using dummy_handle_t = dummy_handle_t_ *;

// Allocates a dummy handle of type T with support of reference counting.
// Takes optional 'Size' parameter which can be used to allocate additional
// memory. The handle has to be deallocated using 'releaseDummyHandle'.
template <class T> inline T createDummyHandle(size_t Size = 0) {
    dummy_handle_t DummyHandlePtr = new dummy_handle_t_(Size);
    return reinterpret_cast<T>(DummyHandlePtr);
}

// Decrement reference counter for the handle and deallocates it if the
// reference counter becomes zero
template <class T> inline void releaseDummyHandle(T Handle) {
    auto DummyHandlePtr = reinterpret_cast<dummy_handle_t>(Handle);
    const size_t NewValue = --DummyHandlePtr->refCounter;
    if (NewValue == 0) {
        delete DummyHandlePtr;
    }
}

// Increment reference counter for the handle
template <class T> inline void retainDummyHandle(T Handle) {
    auto DummyHandlePtr = reinterpret_cast<dummy_handle_t>(Handle);
    ++DummyHandlePtr->refCounter;
}

///////////////////////////////////////////////////////////////////////////////
class __urdlllocal context_t : public proxy_layer_context_t {
  public:
    ur_dditable_t urDdiTable = {};

    context_t() {}
    ~context_t() {}

    bool isAvailable() const override { return true; }
    std::vector<std::string> getNames() const override { return {name}; }
    ur_result_t init(ur_dditable_t *dditable,
                     const std::set<std::string> &enabledLayerNames,
                     codeloc_data codelocData,
                     api_callbacks apiCallbacks) override;
    ur_result_t tearDown() override { return UR_RESULT_SUCCESS; }

    api_callbacks apiCallbacks;

  private:
    const std::string name = "UR_LAYER_MOCK";
};

extern context_t context;
}; // namespace ur_mock_layer
