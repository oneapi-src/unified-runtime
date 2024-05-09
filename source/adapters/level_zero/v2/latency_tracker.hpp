//===--------- ur_latency_tracker.cpp - common ---------------------------===//
//
// Copyright (C) 2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>

#include "logger/ur_logger.hpp"

namespace v2 {

static inline bool trackLatency = []() {
  return std::getenv("UR_ENABLE_LATENCY_TRACKING") != nullptr;
}();

class rolling_stats {
public:
  rolling_stats(const char *name) : name(name) {}

  ~rolling_stats() {
    if (trackLatency) {
      logger::info("[{}] average latency: {}ns", name, estimate());
      logger::info("[{}] number of samples: {}", name, count());
    }
  }

  // track latency by taking the value of duration directly.
  void trackValue(double value) {
    auto ratio = static_cast<double>(cnt) / (cnt + 1);
    avg *= ratio;
    ++cnt;
    avg += value / cnt;
  }

  // Return the rolling average.
  uint64_t estimate() { return static_cast<uint64_t>(avg); }

  // Number of samples tracked.
  uint64_t count() { return cnt; }

private:
  const char *name;
  double avg{0};
  uint64_t cnt{0};
};

class rolling_latency_tracker {
public:
  explicit rolling_latency_tracker(rolling_stats &stats)
      : stats_(trackLatency ? &stats : nullptr), begin_() {
    if (trackLatency) {
      begin_ = std::chrono::steady_clock::now();
    }
  }
  rolling_latency_tracker() {}
  ~rolling_latency_tracker() {
    if (stats_) {
      auto tp = std::chrono::steady_clock::now();
      auto diffNanos =
          std::chrono::duration_cast<std::chrono::nanoseconds>(tp - begin_)
              .count();
      stats_->trackValue(static_cast<double>(diffNanos));
    }
  }

  rolling_latency_tracker(const rolling_latency_tracker &) = delete;
  rolling_latency_tracker &operator=(const rolling_latency_tracker &) = delete;

  rolling_latency_tracker(rolling_latency_tracker &&rhs) noexcept
      : stats_(rhs.stats_), begin_(rhs.begin_) {
    rhs.stats_ = nullptr;
  }

  rolling_latency_tracker &operator=(rolling_latency_tracker &&rhs) noexcept {
    if (this != &rhs) {
      this->~rolling_latency_tracker();
      new (this) rolling_latency_tracker(std::move(rhs));
    }
    return *this;
  }

private:
  rolling_stats *stats_{nullptr};
  std::chrono::time_point<std::chrono::steady_clock> begin_;
};

} // namespace v2
