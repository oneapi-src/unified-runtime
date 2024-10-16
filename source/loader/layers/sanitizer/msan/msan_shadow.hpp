/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file asan_shadow.hpp
 *
 */

#pragma once

#include "sanitizer_common/sanitizer_shadow.hpp"

namespace ur_sanitizer_layer {

struct MsanShadowMemoryCPU final : public ShadowMemory {
    MsanShadowMemoryCPU(ur_context_handle_t Context, ur_device_handle_t Device)
        : ShadowMemory(Context, Device) {}

    ur_result_t Setup() override;

    ur_result_t Destory() override;

    uptr MemToShadow(uptr Ptr) override;

    ur_result_t EnqueuePoisonShadow(ur_queue_handle_t Queue, uptr Ptr,
                                    uptr Size, u8 Value) override;

    size_t GetShadowSize() override { return 0x80000000000ULL; }
};

struct MsanShadowMemoryGPU : public ShadowMemory {
    MsanShadowMemoryGPU(ur_context_handle_t Context, ur_device_handle_t Device)
        : ShadowMemory(Context, Device) {}

    ur_result_t Setup() override;

    ur_result_t Destory() override;
    ur_result_t EnqueuePoisonShadow(ur_queue_handle_t Queue, uptr Ptr,
                                    uptr Size, u8 Value) override final;

    ur_result_t ReleaseShadow(std::shared_ptr<AllocInfo> AI) override final;

    ur_mutex VirtualMemMapsMutex;

    std::unordered_map<
        uptr, std::pair<ur_physical_mem_handle_t,
                        std::unordered_set<std::shared_ptr<AllocInfo>>>>
        VirtualMemMaps;
};

/// Shadow Memory layout of GPU PVC device
///
/// USM Allocation Range (56 bits)
///   Host   USM : 0x0000_0000_0000_0000 ~ 0x00ff_ffff_ffff_ffff
///   Shared USM : 0x0000_0000_0000_0000 ~ 0x0000_7fff_ffff_ffff
///   Device USM : 0xff00_0000_0000_0000 ~ 0xff00_ffff_ffff_ffff
///
/// Shadow Memory Mapping (Device USM only)
///   Device USM      : 0x0800_0000_0000 ~ 0x17ff_ffff_ffff
///
struct MsanShadowMemoryPVC final : public MsanShadowMemoryGPU {
    MsanShadowMemoryPVC(ur_context_handle_t Context, ur_device_handle_t Device)
        : MsanShadowMemoryGPU(Context, Device) {}

    uptr MemToShadow(uptr Ptr) override;

    size_t GetShadowSize() override { return 0x6000'0000'0000ULL; }
};

/// Shadow Memory layout of GPU DG2 device
///
/// USM Allocation Range (48 bits)
///   Host/Shared USM : 0x0000_0000_0000_0000 ~ 0x0000_7fff_ffff_ffff
///   Device      USM : 0xffff_8000_0000_0000 ~ 0xffff_ffff_ffff_ffff
///
/// Shadow Memory Mapping (SHADOW_SCALE=4)
///   Host/Shared USM : 0x0              ~ 0x07ff_ffff_ffff
///   Device      USM : 0x0800_0000_0000 ~ 0x0fff_ffff_ffff
///
struct MsanShadowMemoryDG2 final : public MsanShadowMemoryGPU {
    MsanShadowMemoryDG2(ur_context_handle_t Context, ur_device_handle_t Device)
        : MsanShadowMemoryGPU(Context, Device) {}

    uptr MemToShadow(uptr Ptr) override;

    size_t GetShadowSize() override { return 0x100000000000ULL; }
};

} // namespace ur_sanitizer_layer
