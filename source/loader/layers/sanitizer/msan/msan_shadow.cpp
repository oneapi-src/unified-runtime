/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file asan_shadow.cpp
 *
 */

#include "msan_shadow.hpp"

#include "sanitizer_common/sanitizer_common.hpp"
#include "sanitizer_common/sanitizer_interceptor.hpp"
#include "sanitizer_common/sanitizer_utils.hpp"

#include "ur_api.h"
#include "ur_sanitizer_layer.hpp"

namespace ur_sanitizer_layer {

ur_result_t MsanShadowMemoryCPU::Setup() {
    static uptr Begin = 0, End = 0;
    static ur_result_t Result = [this]() {
        size_t ShadowSize = GetShadowSize();
        Begin = MmapNoReserve(0, ShadowSize);
        if (Begin == 0) {
            return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        }
        // DontCoredumpRange(Begin, ShadowSize);
        End = Begin + ShadowSize;

        ShadowBegin = Begin;
        ShadowEnd = End;

        // Set shadow memory for null pointer
        // auto URes = EnqueuePoisonShadow({}, 0, 1, kNullPointerRedzoneMagic);
        // if (URes != UR_RESULT_SUCCESS) {
        //     getContext()->logger.error("EnqueuePoisonShadow(NullPointerRZ): {}",
        //                                URes);
        //     return URes;
        // }
        // return URes;
        return UR_RESULT_SUCCESS;
    }();
    assert(Begin != 0 && End != 0);
    ShadowBegin = Begin;
    ShadowEnd = End;
    return Result;
}

ur_result_t MsanShadowMemoryCPU::Destory() {
    if (ShadowBegin == 0) {
        return UR_RESULT_SUCCESS;
    }
    static ur_result_t Result = [this]() {
        if (!Munmap(ShadowBegin, GetShadowSize())) {
            return UR_RESULT_ERROR_UNKNOWN;
        }
        return UR_RESULT_SUCCESS;
    }();
    return Result;
}

uptr MsanShadowMemoryCPU::MemToShadow(uptr Ptr) {
    return ShadowBegin + (Ptr >> 1);
}

ur_result_t MsanShadowMemoryCPU::EnqueuePoisonShadow(ur_queue_handle_t,
                                                     uptr Ptr, uptr Size,
                                                     u8 Value) {
    if (Size == 0) {
        return UR_RESULT_SUCCESS;
    }

    uptr ShadowBegin = MemToShadow(Ptr);
    uptr ShadowEnd = MemToShadow(Ptr + Size - 1);
    assert(ShadowBegin <= ShadowEnd);
    getContext()->logger.debug(
        "EnqueuePoisonShadow(addr={}, count={}, value={})", (void *)ShadowBegin,
        ShadowEnd - ShadowBegin + 1, (void *)(size_t)Value);
    memset((void *)ShadowBegin, Value, ShadowEnd - ShadowBegin + 1);

    return UR_RESULT_SUCCESS;
}

ur_result_t MsanShadowMemoryGPU::Setup() {
    static uptr Begin = 0, End = 0;
    // Currently, Level-Zero doesn't create independent VAs for each contexts, if we reserve
    // shadow memory for each contexts, this will cause out-of-resource error when user uses
    // multiple contexts. Therefore, we just create one shadow memory here.
    static ur_result_t Result = [this]() {
        size_t ShadowSize = GetShadowSize();
        // TODO: Protect Bad Zone
        auto Result = getContext()->urDdiTable.VirtualMem.pfnReserve(
            Context, nullptr, ShadowSize, (void **)&Begin);
        if (Result != UR_RESULT_SUCCESS) {
            getContext()->logger.error("Failed to reserve shadow memory: {}",
                                       Result);
            die("Failed to reserve shadow memory");
        }

        // Retain the context which reserves shadow memory
        Result = getContext()->urDdiTable.Context.pfnRetain(Context);
        assert(Result == UR_RESULT_SUCCESS && "Failed to retain context");

        End = Begin + ShadowSize;
        ShadowBegin = Begin;
        ShadowEnd = End;

        return Result;
    }();
    assert(Begin != 0 && End != 0);
    return Result;
}

ur_result_t MsanShadowMemoryGPU::Destory() {
    if (ShadowBegin == 0) {
        return UR_RESULT_SUCCESS;
    }
    static ur_result_t Result = [this]() {
        auto Result = getContext()->urDdiTable.VirtualMem.pfnFree(
            Context, (const void *)ShadowBegin, GetShadowSize());
        getContext()->urDdiTable.Context.pfnRelease(Context);
        return Result;
    }();
    return Result;
}

ur_result_t MsanShadowMemoryGPU::EnqueuePoisonShadow(ur_queue_handle_t Queue,
                                                     uptr Ptr, uptr Size,
                                                     u8 Value) {
    if (Size == 0) {
        return UR_RESULT_SUCCESS;
    }

    uptr ShadowBegin = MemToShadow(Ptr);
    uptr ShadowEnd = MemToShadow(Ptr + Size - 1);
    assert(ShadowBegin <= ShadowEnd);
    {
        static const size_t PageSize =
            GetVirtualMemGranularity(Context, Device);

        ur_physical_mem_properties_t Desc{
            UR_STRUCTURE_TYPE_PHYSICAL_MEM_PROPERTIES, nullptr, 0};

        // Make sure [Ptr, Ptr + Size] is mapped to physical memory
        for (auto MappedPtr = RoundDownTo(ShadowBegin, PageSize);
             MappedPtr <= ShadowEnd; MappedPtr += PageSize) {
            std::scoped_lock<ur_mutex> Guard(VirtualMemMapsMutex);
            if (VirtualMemMaps.find(MappedPtr) == VirtualMemMaps.end()) {
                ur_physical_mem_handle_t PhysicalMem{};
                auto URes = getContext()->urDdiTable.PhysicalMem.pfnCreate(
                    Context, Device, PageSize, &Desc, &PhysicalMem);
                if (URes != UR_RESULT_SUCCESS) {
                    getContext()->logger.error("urPhysicalMemCreate(): {}",
                                               URes);
                    return URes;
                }

                URes = getContext()->urDdiTable.VirtualMem.pfnMap(
                    Context, (void *)MappedPtr, PageSize, PhysicalMem, 0,
                    UR_VIRTUAL_MEM_ACCESS_FLAG_READ_WRITE);
                if (URes != UR_RESULT_SUCCESS) {
                    getContext()->logger.error("urVirtualMemMap({}, {}): {}",
                                               (void *)MappedPtr, PageSize,
                                               URes);
                    return URes;
                }

                getContext()->logger.debug("urVirtualMemMap: {} ~ {}",
                                           (void *)MappedPtr,
                                           (void *)(MappedPtr + PageSize - 1));

                // Initialize to zero
                URes = EnqueueUSMBlockingSet(Queue, (void *)MappedPtr, 0,
                                             PageSize);
                if (URes != UR_RESULT_SUCCESS) {
                    getContext()->logger.error("EnqueueUSMBlockingSet(): {}",
                                               URes);
                    return URes;
                }

                VirtualMemMaps[MappedPtr].first = PhysicalMem;
            }

            // We don't need to record virtual memory map for null pointer,
            // since it doesn't have an alloc info.
            if (Ptr == 0) {
                continue;
            }

            auto AllocInfoIt =
                getContext()->interceptor->findAllocInfoByAddress(Ptr);
            assert(AllocInfoIt);
            VirtualMemMaps[MappedPtr].second.insert((*AllocInfoIt)->second);
        }
    }

    auto URes = EnqueueUSMBlockingSet(Queue, (void *)ShadowBegin, Value,
                                      ShadowEnd - ShadowBegin + 1);
    getContext()->logger.debug(
        "EnqueuePoisonShadow (addr={}, count={}, value={}): {}",
        (void *)ShadowBegin, ShadowEnd - ShadowBegin + 1, (void *)(size_t)Value,
        URes);
    if (URes != UR_RESULT_SUCCESS) {
        getContext()->logger.error("EnqueueUSMBlockingSet(): {}", URes);
        return URes;
    }

    return UR_RESULT_SUCCESS;
}

ur_result_t MsanShadowMemoryGPU::ReleaseShadow(std::shared_ptr<AllocInfo> AI) {
    uptr ShadowBegin = MemToShadow(AI->AllocBegin);
    uptr ShadowEnd = MemToShadow(AI->AllocBegin + AI->AllocSize);
    assert(ShadowBegin <= ShadowEnd);

    static const size_t PageSize = GetVirtualMemGranularity(Context, Device);

    for (auto MappedPtr = RoundDownTo(ShadowBegin, PageSize);
         MappedPtr <= ShadowEnd; MappedPtr += PageSize) {
        std::scoped_lock<ur_mutex> Guard(VirtualMemMapsMutex);
        if (VirtualMemMaps.find(MappedPtr) == VirtualMemMaps.end()) {
            continue;
        }
        VirtualMemMaps[MappedPtr].second.erase(AI);
        if (VirtualMemMaps[MappedPtr].second.empty()) {
            UR_CALL(getContext()->urDdiTable.VirtualMem.pfnUnmap(
                Context, (void *)MappedPtr, PageSize));
            UR_CALL(getContext()->urDdiTable.PhysicalMem.pfnRelease(
                VirtualMemMaps[MappedPtr].first));
            getContext()->logger.debug("urVirtualMemUnmap: {} ~ {}",
                                       (void *)MappedPtr,
                                       (void *)(MappedPtr + PageSize - 1));
        }
    }

    return UR_RESULT_SUCCESS;
}

uptr MsanShadowMemoryPVC::MemToShadow(uptr Ptr) {
    assert(Ptr & 0xFF00'0000'0000'0000ULL && "device USM only");
    return ShadowBegin + ((Ptr & 0x00FF'FFFF'FFFFULL) >> 1);
}

uptr MsanShadowMemoryDG2::MemToShadow(uptr Ptr) {
    if (Ptr & 0xFFFF000000000000ULL) { // Device USM
        return ShadowBegin + 0x80000000000ULL +
               ((Ptr & 0x7FFFFFFFFFFFULL) >> 1);
    } else { // Host/Shared USM
        return ShadowBegin + (Ptr >> 1);
    }
}

} // namespace ur_sanitizer_layer