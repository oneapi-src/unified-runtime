/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file asan_report.hpp
 *
 */

#pragma once

#include "common.hpp"
#include <atomic>

namespace ur_sanitizer_layer {

struct AsanStats {
    void UpdateUSMMalloced(uptr MallocedSize, uptr RedzoneSize);
    void UpdateUSMFreed(uptr FreedSize, uptr RedzoneSize);

    void UpdateShadowMmaped(uptr ShadowSize);
    void UpdateShadowMalloced(uptr ShadowSize);
    void UpdateShadowFreed(uptr ShadowSize);

    void Print();

  private:
    std::atomic<uptr> UsmMalloced;
    std::atomic<uptr> UsmMallocedRedzones;

    std::atomic<uptr> ShadowMmaped;
    std::atomic<uptr> ShadowMalloced;

    double Overhead = 0.0;

    void UpdateOverhead();
};

} // namespace ur_sanitizer_layer
