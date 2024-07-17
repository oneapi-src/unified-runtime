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
    std::atomic<uptr> usm_mallocs;
    std::atomic<uptr> usm_malloced;
    std::atomic<uptr> usm_malloced_redzones;

    std::atomic<uptr> usm_frees;
    std::atomic<uptr> usm_freed;
    std::atomic<uptr> usm_real_frees;
    std::atomic<uptr> usm_really_freed;

    std::atomic<uptr> shadow_reserved;
    std::atomic<uptr> shadow_mmaps;
    std::atomic<uptr> shadow_mmaped;
    std::atomic<uptr> shadow_malloced;
    std::atomic<uptr> shadow_freed;

    // AsanStats();

    void Print(); // Prints formatted stats to stderr.
    void Clear();
    void MergeFrom(const AsanStats *stats);
};

} // namespace ur_sanitizer_layer
