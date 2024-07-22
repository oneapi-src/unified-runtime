// Copyright (C) 2022-2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once
#include <chrono>
#include <cstdio>
#if defined(__ARM_ARCH)
#include <sse2neon.h>
#else
#include <emmintrin.h>
#endif

class Timer {
  public:
    using Clock = std::chrono::high_resolution_clock;

    void measureStart() {
        // make sure that any pending instructions are done and all memory transactions committed.
        _mm_mfence();
        _mm_lfence();
        startTime = Clock::now();
    }

    void measureEnd() {
        // make sure that any pending instructions are done and all memory transactions committed.
        _mm_mfence();
        _mm_lfence();
        endTime = Clock::now();
    }

    Clock::duration get() const { return endTime - startTime; }

  private:
    bool markTimers = false;
    Clock::time_point startTime;
    Clock::time_point endTime;
};