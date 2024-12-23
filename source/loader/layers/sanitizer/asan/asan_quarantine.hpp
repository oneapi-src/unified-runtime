/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file asan_quarantine.hpp
 *
 */

#pragma once

#include "asan_allocator.hpp"

#include <atomic>
#include <queue>
#include <unordered_map>
#include <vector>

namespace ur_sanitizer_layer {
namespace asan {

class QuarantineCache {
  public:
    using ElementT = std::shared_ptr<AllocInfo>;
    using QueueT = std::queue<ElementT>;

    // The following methods are not thread safe, use this lock
    ur_mutex Mutex;

    // Total memory used, including internal accounting.
    uptr size() const { return m_Size; }

    void enqueue(ElementT &AI) {
        m_Queue.push(AI);
        m_Size += AI->AllocSize;
    }

    std::optional<ElementT> dequeue() {
        if (m_Queue.empty()) {
            return std::optional<ElementT>{};
        }
        auto &AI = m_Queue.front();
        m_Queue.pop();
        m_Size -= AI->AllocSize;
        return AI;
    }

    void remove(ElementT &AI) {
        m_Size -= AI->AllocSize;
        // remove the element from the queue
        QueueT newQueue;
        while (!m_Queue.empty()) {
            auto currentElement = m_Queue.front();
            m_Queue.pop();
            if (currentElement != AI) {
                newQueue.push(currentElement);
            }
        }
        std::swap(m_Queue, newQueue);
    }

  private:
    QueueT m_Queue;
    std::atomic_uintptr_t m_Size = 0;
};

class Quarantine {
  public:
    explicit Quarantine(size_t MaxQuarantineSize)
        : m_MaxQuarantineSize(MaxQuarantineSize) {}

    std::vector<std::shared_ptr<AllocInfo>> put(std::shared_ptr<AllocInfo> &AI);
    void remove(std::shared_ptr<AllocInfo> &AI);

  private:
    QuarantineCache &getCache(ur_device_handle_t Device) {
        std::scoped_lock<ur_mutex> Guard(m_Mutex);
        return m_Map[Device];
    }

    std::unordered_map<ur_device_handle_t, QuarantineCache> m_Map;
    ur_mutex m_Mutex;
    size_t m_MaxQuarantineSize;
};

} // namespace asan
} // namespace ur_sanitizer_layer
