/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file msan_libdevice.hpp
 *
 */

#pragma once

#include "sanitizer_common/sanitizer_libdevice.hpp"

namespace ur_sanitizer_layer {

enum class MsanErrorType : int32_t {
    UNKNOWN,
    USES_OF_UNINITIALIZED_VALUE,
};

enum class MsanMemoryType : int32_t {
    UNKNOWN,
    USM_DEVICE,
    USM_HOST,
    USM_SHARED,
    LOCAL,
    PRIVATE,
    MEM_BUFFER,
    DEVICE_GLOBAL,
};

struct MsanErrorReport {
    int Flag = 0;

    char File[256 + 1] = {};
    char Func[256 + 1] = {};

    int32_t Line = 0;

    uint64_t GID0 = 0;
    uint64_t GID1 = 0;
    uint64_t GID2 = 0;

    uint64_t LID0 = 0;
    uint64_t LID1 = 0;
    uint64_t LID2 = 0;

    uintptr_t Address = 0;
    bool IsWrite = false;
    uint32_t AccessSize = 0;
    MsanMemoryType MemoryType = MsanMemoryType::UNKNOWN;
    MsanErrorType ErrorType = MsanErrorType::UNKNOWN;

    bool IsRecover = false;
};

struct MsanLocalArgsInfo {
    uint64_t Size = 0;
    uint64_t SizeWithRedZone = 0;
};

constexpr std::size_t MSAN_MAX_NUM_REPORTS = 10;

struct MsanLaunchInfo {
    uintptr_t GlobalShadowOffset = 0;
    uintptr_t GlobalShadowOffsetEnd = 0;

    uintptr_t PrivateShadowOffset = 0;
    uintptr_t PrivateShadowOffsetEnd = 0;

    uintptr_t LocalShadowOffset = 0;
    uintptr_t LocalShadowOffsetEnd = 0;

    MsanLocalArgsInfo *LocalArgs = nullptr; // Ordered by ArgIndex
    uint32_t NumLocalArgs = 0;

    DeviceType DeviceTy = DeviceType::UNKNOWN;
    uint32_t Debug = 0;

    MsanErrorReport SanitizerReport[MSAN_MAX_NUM_REPORTS];
};

// Based on the observation, only the last 24 bits of the address of the private
// variable have changed
constexpr std::size_t MSAN_PRIVATE_SIZE = 0xffffffULL + 1;

constexpr auto kSPIR_MsanDeviceGlobalCount = "__MsanDeviceGlobalCount";
constexpr auto kSPIR_MsanDeviceGlobalMetadata = "__MsanDeviceGlobalMetadata";

inline const char *ToString(MsanMemoryType MemoryType) {
    switch (MemoryType) {
    case MsanMemoryType::USM_DEVICE:
        return "Device USM";
    case MsanMemoryType::USM_HOST:
        return "Host USM";
    case MsanMemoryType::USM_SHARED:
        return "Shared USM";
    case MsanMemoryType::LOCAL:
        return "Local Memory";
    case MsanMemoryType::PRIVATE:
        return "Private Memory";
    case MsanMemoryType::MEM_BUFFER:
        return "Memory Buffer";
    case MsanMemoryType::DEVICE_GLOBAL:
        return "Device Global";
    default:
        return "Unknown Memory";
    }
}

inline const char *ToString(MsanErrorType ErrorType) {
    switch (ErrorType) {
    case MsanErrorType::USES_OF_UNINITIALIZED_VALUE:
        return "out-of-uninitialized-value";
    default:
        return "unknown-error";
    }
}

} // namespace ur_sanitizer_layer
