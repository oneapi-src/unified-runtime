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

#include "asan_statistics.hpp"
#include "ur_sanitizer_layer.hpp"
#include <atomic>

namespace ur_sanitizer_layer {

std::atomic<size_t> MallocedSize;
std::atomic<size_t> RedZoneSize;
std::atomic<size_t> ShadowMemorySize;
double overhead = 0;

void print_result() {
    if (MallocedSize + ShadowMemorySize == 0) {
        std::cout << "Stats: " << overhead << "," << MallocedSize << ","
                  << RedZoneSize << "," << ShadowMemorySize << "";
        return;
    }

    auto new_overhead = (RedZoneSize + ShadowMemorySize) /
                        (double)(MallocedSize + ShadowMemorySize);
    if (new_overhead > overhead) {
        overhead = new_overhead;
        std::cout << "Stats: " << overhead * 100 << "% |" << MallocedSize << ","
                  << RedZoneSize << "," << ShadowMemorySize << "";
    }
}

void add_memory(size_t malloced, size_t redzone) {
    MallocedSize += malloced;
    RedZoneSize += redzone;
    print_result();
}

void del_memory(size_t malloced, size_t redzone) {
    MallocedSize -= malloced;
    RedZoneSize -= redzone;
    // print_result();
}

void add_shadow(size_t shadow) {
    ShadowMemorySize += shadow;
    print_result();
}

void AsanStats::Print() {
    getContext()->logger.always(
        "Stats: {}M malloced ({}M for red zones) by {} calls",
        usm_malloced >> 20, usm_malloced_redzones >> 20, usm_mallocs);
    getContext()->logger.always("Stats: {}M freed by {} calls", usm_freed >> 20,
                                usm_frees);
    getContext()->logger.always("Stats: {}M really freed by {} calls",
                                usm_really_freed >> 20, usm_real_frees);

    getContext()->logger.always(
        "Stats: shadow memory {}G reserved, {}M mapped; {} maps",
        shadow_reserved >> 30, shadow_mmaped >> 20, shadow_mmaps);
}

} // namespace ur_sanitizer_layer
