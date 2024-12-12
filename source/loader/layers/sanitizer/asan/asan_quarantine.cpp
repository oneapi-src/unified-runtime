/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file asan_quarantine.cpp
 *
 */

#include "asan_quarantine.hpp"

namespace ur_sanitizer_layer {
namespace asan {

std::vector<std::shared_ptr<AllocInfo>> Quarantine::put(std::shared_ptr<AllocInfo> &AI) {
    auto Device = AI->Device;
    auto AllocSize = AI->AllocSize;
    auto &Cache = getCache(Device);

    std::vector<std::shared_ptr<AllocInfo>> DequeueList;
    std::scoped_lock<ur_mutex> Guard(Cache.Mutex);
    while (Cache.size() + AllocSize > m_MaxQuarantineSize) {
        auto AIToFreeOp = Cache.dequeue();
        if (!AIToFreeOp) {
            break;
        }
        DequeueList.emplace_back(*AIToFreeOp);
    }
    Cache.enqueue(AI);
    return DequeueList;
}

void Quarantine::remove(std::shared_ptr<AllocInfo> &AI) {
    auto Device = AI->Device;
    auto &Cache = getCache(Device);

    std::scoped_lock<ur_mutex> Guard(Cache.Mutex);
    Cache.remove(AI);
}

} // namespace asan
} // namespace ur_sanitizer_layer
