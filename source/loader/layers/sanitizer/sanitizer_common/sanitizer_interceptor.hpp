/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file stacktrace.hpp
 *
 */

#pragma once

#include "sanitizer_allocator.hpp"
#include "sanitizer_buffer.hpp"
#include "sanitizer_common.hpp"
#include "sanitizer_libdevice.hpp"

#include "ur_sanitizer_layer.hpp"

#include <memory>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

namespace ur_sanitizer_layer {

struct ShadowMemory;

struct AllocInfoList {
    std::vector<std::shared_ptr<AllocInfo>> List;
    ur_shared_mutex Mutex;
};

struct DeviceInfo {
    ur_device_handle_t Handle;

    DeviceType Type = DeviceType::UNKNOWN;
    size_t Alignment = 0;
    uptr ShadowOffset = 0;
    uptr ShadowOffsetEnd = 0;
    ShadowMemory *Shadow = nullptr;

    // Device features
    bool IsSupportSharedSystemUSM = false;

    ur_mutex Mutex;
    std::queue<std::shared_ptr<AllocInfo>> Quarantine;
    size_t QuarantineSize = 0;

    // Device handles are special and alive in the whole process lifetime,
    // so we needn't retain&release here.
    explicit DeviceInfo(ur_device_handle_t Device) : Handle(Device) {}

    ur_result_t allocShadowMemory(ur_context_handle_t Context);
};

struct QueueInfo {
    ur_queue_handle_t Handle;

    ur_shared_mutex Mutex;
    ur_event_handle_t LastEvent;

    explicit QueueInfo(ur_queue_handle_t Queue)
        : Handle(Queue), LastEvent(nullptr) {
        [[maybe_unused]] auto Result =
            getContext()->urDdiTable.Queue.pfnRetain(Queue);
        assert(Result == UR_RESULT_SUCCESS);
    }

    ~QueueInfo() {
        [[maybe_unused]] auto Result =
            getContext()->urDdiTable.Queue.pfnRelease(Handle);
        assert(Result == UR_RESULT_SUCCESS);
    }
};

struct KernelInfo {
    ur_kernel_handle_t Handle;
    ur_shared_mutex Mutex;
    std::atomic<int32_t> RefCount = 1;
    std::unordered_map<uint32_t, std::shared_ptr<MemBuffer>> BufferArgs;
    std::unordered_map<uint32_t, std::pair<const void *, StackTrace>>
        PointerArgs;

    // Need preserve the order of local arguments
    std::map<uint32_t, LocalArgsInfo> LocalArgs;

    explicit KernelInfo(ur_kernel_handle_t Kernel) : Handle(Kernel) {
        [[maybe_unused]] auto Result =
            getContext()->urDdiTable.Kernel.pfnRetain(Kernel);
        assert(Result == UR_RESULT_SUCCESS);
    }

    ~KernelInfo() {
        [[maybe_unused]] auto Result =
            getContext()->urDdiTable.Kernel.pfnRelease(Handle);
        assert(Result == UR_RESULT_SUCCESS);
    }
};

struct ContextInfo {
    ur_context_handle_t Handle;

    std::vector<ur_device_handle_t> DeviceList;
    std::unordered_map<ur_device_handle_t, AllocInfoList> AllocInfosMap;

    explicit ContextInfo(ur_context_handle_t Context) : Handle(Context) {
        [[maybe_unused]] auto Result =
            getContext()->urDdiTable.Context.pfnRetain(Context);
        assert(Result == UR_RESULT_SUCCESS);
    }

    ~ContextInfo() {
        [[maybe_unused]] auto Result =
            getContext()->urDdiTable.Context.pfnRelease(Handle);
        assert(Result == UR_RESULT_SUCCESS);
    }

    void insertAllocInfo(const std::vector<ur_device_handle_t> &Devices,
                         std::shared_ptr<AllocInfo> &AI) {
        for (auto Device : Devices) {
            auto &AllocInfos = AllocInfosMap[Device];
            std::scoped_lock<ur_shared_mutex> Guard(AllocInfos.Mutex);
            AllocInfos.List.emplace_back(AI);
        }
    }
};

struct LaunchInfo;

struct USMLaunchInfo {
    LaunchInfo *Data = nullptr;

    ur_context_handle_t Context = nullptr;
    ur_device_handle_t Device = nullptr;
    const size_t *GlobalWorkSize = nullptr;
    const size_t *GlobalWorkOffset = nullptr;
    std::vector<size_t> LocalWorkSize;
    uint32_t WorkDim = 0;

    USMLaunchInfo(ur_context_handle_t Context, ur_device_handle_t Device,
                  const size_t *GlobalWorkSize, const size_t *LocalWorkSize,
                  const size_t *GlobalWorkOffset, uint32_t WorkDim)
        : Context(Context), Device(Device), GlobalWorkSize(GlobalWorkSize),
          GlobalWorkOffset(GlobalWorkOffset), WorkDim(WorkDim) {
        if (LocalWorkSize) {
            this->LocalWorkSize =
                std::vector<size_t>(LocalWorkSize, LocalWorkSize + WorkDim);
        }
    }
    ~USMLaunchInfo();

    ur_result_t initialize();
    ur_result_t updateKernelInfo(const KernelInfo &KI);
};

struct DeviceGlobalInfo {
    uptr Size;
    uptr SizeWithRedZone;
    uptr Addr;
};

class SanitizerInterceptor {
  public:
    virtual ur_result_t allocateMemory(ur_context_handle_t Context,
                                       ur_device_handle_t Device,
                                       const ur_usm_desc_t *Properties,
                                       ur_usm_pool_handle_t Pool, size_t Size,
                                       AllocType Type, void **ResultPtr);
    virtual ur_result_t releaseMemory(ur_context_handle_t Context, void *Ptr);

    virtual ur_result_t registerDeviceGlobals(ur_context_handle_t Context,
                                              ur_program_handle_t Program);

    virtual ur_result_t preLaunchKernel(ur_kernel_handle_t Kernel,
                                        ur_queue_handle_t Queue,
                                        USMLaunchInfo &LaunchInfo);

    virtual ur_result_t postLaunchKernel(ur_kernel_handle_t Kernel,
                                         ur_queue_handle_t Queue,
                                         USMLaunchInfo &LaunchInfo);

    virtual ur_result_t insertContext(ur_context_handle_t Context,
                                      std::shared_ptr<ContextInfo> &CI);
    virtual ur_result_t eraseContext(ur_context_handle_t Context);

    virtual ur_result_t insertDevice(ur_device_handle_t Device,
                                     std::shared_ptr<DeviceInfo> &CI);
    virtual ur_result_t eraseDevice(ur_device_handle_t Device);

    virtual ur_result_t insertKernel(ur_kernel_handle_t Kernel);
    virtual ur_result_t eraseKernel(ur_kernel_handle_t Kernel);

    virtual ur_result_t insertMemBuffer(std::shared_ptr<MemBuffer> MemBuffer);
    virtual ur_result_t eraseMemBuffer(ur_mem_handle_t MemHandle);
    virtual std::shared_ptr<MemBuffer> getMemBuffer(ur_mem_handle_t MemHandle);

    virtual ur_result_t holdAdapter(ur_adapter_handle_t Adapter);

    virtual std::optional<AllocationIterator>
    findAllocInfoByAddress(uptr Address);

    virtual std::shared_ptr<ContextInfo>
    getContextInfo(ur_context_handle_t Context);

    virtual std::shared_ptr<DeviceInfo>
    getDeviceInfo(ur_device_handle_t Device);

    virtual std::shared_ptr<KernelInfo>
    getKernelInfo(ur_kernel_handle_t Kernel);
};

} // namespace ur_sanitizer_layer
