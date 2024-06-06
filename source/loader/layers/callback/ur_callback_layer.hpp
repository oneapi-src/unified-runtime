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

namespace ur_callback_layer {

struct dummy_handle_t_ {
    dummy_handle_t_(size_t DataSize = 0)
        : MStorage(DataSize), MData(MStorage.data()) {}
    dummy_handle_t_(unsigned char *Data) : MData(Data) {}
    std::atomic<size_t> MRefCounter = 1;
    std::vector<unsigned char> MStorage;
    unsigned char *MData = nullptr;
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
    const size_t NewValue = --DummyHandlePtr->MRefCounter;
    if (NewValue == 0) {
        delete DummyHandlePtr;
    }
}

// Increment reference counter for the handle
template <class T> inline void retainDummyHandle(T Handle) {
    auto DummyHandlePtr = reinterpret_cast<dummy_handle_t>(Handle);
    ++DummyHandlePtr->MRefCounter;
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

    api_callbacks apiCallbacks = {};
    bool enableMock = false;

  private:
    const std::string name = "UR_LAYER_CALLBACK";
};

extern context_t context;
}; // namespace ur_callback_layer
