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

void AsanStats::Print(ur_context_handle_t Context) {
    getContext()->logger.always("Stats: Context {}", (void *)Context);
    getContext()->logger.always("Stats:   peak memory overhead: {}%",
                                Overhead * 100);
}

void AsanStats::UpdateUSMMalloced(uptr MallocedSize, uptr RedzoneSize) {
    UsmMalloced += MallocedSize;
    UsmMallocedRedzones += RedzoneSize;
    getContext()->logger.debug(
        "Stats: UpdateUSMMalloced(UsmMalloced={}, UsmMallocedRedzones={})",
        UsmMalloced, UsmMallocedRedzones);
    UpdateOverhead();
}

void AsanStats::UpdateUSMFreed(uptr FreedSize) {
    UsmFreed += FreedSize;
    getContext()->logger.debug("Stats: UpdateUSMFreed(UsmFreed={})", UsmFreed);
}

void AsanStats::UpdateUSMRealFreed(uptr FreedSize, uptr RedzoneSize) {
    UsmMalloced -= FreedSize;
    UsmMallocedRedzones -= RedzoneSize;
    if (getContext()->interceptor->getOptions().MaxQuarantineSizeMB) {
        UsmFreed -= FreedSize;
    }
    getContext()->logger.debug(
        "Stats: UpdateUSMRealFreed(UsmMalloced={}, UsmMallocedRedzones={})",
        UsmMalloced, UsmMallocedRedzones);
    UpdateOverhead();
}

void AsanStats::UpdateShadowMmaped(uptr ShadowSize) {
    ShadowMmaped += ShadowSize;
    getContext()->logger.debug("Stats: UpdateShadowMmaped(ShadowMmaped={})",
                               ShadowMmaped);
    UpdateOverhead();
}

void AsanStats::UpdateShadowMalloced(uptr ShadowSize) {
    ShadowMalloced += ShadowSize;
    getContext()->logger.debug("Stats: UpdateShadowMalloced(ShadowMalloced={})",
                               ShadowMalloced);
    UpdateOverhead();
}

void AsanStats::UpdateShadowFreed(uptr ShadowSize) {
    ShadowMalloced -= ShadowSize;
    getContext()->logger.debug("Stats: UpdateShadowFreed(ShadowMalloced={})",
                               ShadowMalloced);
    UpdateOverhead();
}

void AsanStats::UpdateOverhead() {
    auto ShadowSize = ShadowMmaped + ShadowMalloced;
    auto TotalSize = UsmMalloced + ShadowSize;
    if (TotalSize == 0) {
        return;
    }
    auto NewOverhead = (ShadowSize + UsmMallocedRedzones) / (double)TotalSize;
    Overhead = std::max(Overhead, NewOverhead);
}

} // namespace ur_sanitizer_layer
