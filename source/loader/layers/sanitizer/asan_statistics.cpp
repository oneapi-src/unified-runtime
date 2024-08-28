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
#include "asan_interceptor.hpp"
#include "ur_sanitizer_layer.hpp"

namespace ur_sanitizer_layer {

void AsanStats::Print() {
    getContext()->logger.always("Stats: peak memory overhead = {}%",
                                Overhead * 100);
}

void AsanStats::UpdateUSMMalloced(uptr MallocedSize, uptr RedzoneSize) {
    UsmMalloced += MallocedSize;
    UsmMallocedRedzones += RedzoneSize;
}

void AsanStats::UpdateUSMFreed(uptr FreedSize) { UsmFreed += FreedSize; }

void AsanStats::UpdateUSMRealFreed(uptr FreedSize, uptr RedzoneSize) {
    UsmMalloced -= FreedSize;
    UsmMallocedRedzones -= RedzoneSize;
    if (getContext()->interceptor->getOptions().MaxQuarantineSizeMB) {
        UsmFreed -= FreedSize;
    }
}

void AsanStats::UpdateShadowMmaped(uptr ShadowSize) {
    ShadowMmaped += ShadowSize;
}

void AsanStats::UpdateShadowMalloced(uptr ShadowSize) {
    ShadowMalloced += ShadowSize;
}

void AsanStats::UpdateShadowFreed(uptr ShadowSize) {
    ShadowMalloced -= ShadowSize;
}

void AsanStats::UpdateOverhead() {
    auto ShadowSize = ShadowMmaped + ShadowMalloced;
    auto TotalSize = UsmMalloced + ShadowSize;
    if (TotalSize == 0) {
        return;
    }
    auto NewOverhead = ShadowSize / (double)TotalSize;
    Overhead = std::max(Overhead, NewOverhead);
}

} // namespace ur_sanitizer_layer
