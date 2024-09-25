/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file asan_interceptor.hpp
 *
 */

#pragma once

#include "sanitizer_common/sanitizer_interceptor.hpp"

#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace ur_sanitizer_layer {

class Quarantine;

class AsanInterceptor : public SanitizerInterceptor {
  public:
    explicit AsanInterceptor();

    ~AsanInterceptor();

    ur_result_t allocateMemory(ur_context_handle_t Context,
                               ur_device_handle_t Device,
                               const ur_usm_desc_t *Properties,
                               ur_usm_pool_handle_t Pool, size_t Size,
                               AllocType Type, void **ResultPtr) override;
    ur_result_t releaseMemory(ur_context_handle_t Context, void *Ptr) override;

    ur_result_t registerDeviceGlobals(ur_context_handle_t Context,
                                      ur_program_handle_t Program) override;

    ur_result_t preLaunchKernel(ur_kernel_handle_t Kernel,
                                ur_queue_handle_t Queue,
                                USMLaunchInfo &LaunchInfo) override;

    ur_result_t postLaunchKernel(ur_kernel_handle_t Kernel,
                                 ur_queue_handle_t Queue,
                                 USMLaunchInfo &LaunchInfo) override;

    ur_result_t insertContext(ur_context_handle_t Context,
                              std::shared_ptr<ContextInfo> &CI) override;
    ur_result_t eraseContext(ur_context_handle_t Context) override;

    ur_result_t insertDevice(ur_device_handle_t Device,
                             std::shared_ptr<DeviceInfo> &CI) override;
    ur_result_t eraseDevice(ur_device_handle_t Device) override;

    ur_result_t insertKernel(ur_kernel_handle_t Kernel) override;
    ur_result_t eraseKernel(ur_kernel_handle_t Kernel) override;

    ur_result_t insertMemBuffer(std::shared_ptr<MemBuffer> MemBuffer) override;
    ur_result_t eraseMemBuffer(ur_mem_handle_t MemHandle) override;
    std::shared_ptr<MemBuffer> getMemBuffer(ur_mem_handle_t MemHandle) override;

    ur_result_t holdAdapter(ur_adapter_handle_t Adapter) override {
        std::scoped_lock<ur_shared_mutex> Guard(m_AdaptersMutex);
        if (m_Adapters.find(Adapter) != m_Adapters.end()) {
            return UR_RESULT_SUCCESS;
        }
        UR_CALL(getContext()->urDdiTable.Global.pfnAdapterRetain(Adapter));
        m_Adapters.insert(Adapter);
        return UR_RESULT_SUCCESS;
    }

    std::optional<AllocationIterator>
    findAllocInfoByAddress(uptr Address) override;

    std::shared_ptr<ContextInfo>
    getContextInfo(ur_context_handle_t Context) override {
        std::shared_lock<ur_shared_mutex> Guard(m_ContextMapMutex);
        assert(m_ContextMap.find(Context) != m_ContextMap.end());
        return m_ContextMap[Context];
    }

    std::shared_ptr<DeviceInfo>
    getDeviceInfo(ur_device_handle_t Device) override {
        std::shared_lock<ur_shared_mutex> Guard(m_DeviceMapMutex);
        assert(m_DeviceMap.find(Device) != m_DeviceMap.end());
        return m_DeviceMap[Device];
    }

    std::shared_ptr<KernelInfo>
    getKernelInfo(ur_kernel_handle_t Kernel) override {
        std::shared_lock<ur_shared_mutex> Guard(m_KernelMapMutex);
        assert(m_KernelMap.find(Kernel) != m_KernelMap.end());
        return m_KernelMap[Kernel];
    }

  private:
    ur_result_t updateShadowMemory(std::shared_ptr<ContextInfo> &ContextInfo,
                                   std::shared_ptr<DeviceInfo> &DeviceInfo,
                                   ur_queue_handle_t Queue);
    ur_result_t enqueueAllocInfo(ur_context_handle_t Context,
                                 std::shared_ptr<DeviceInfo> &DeviceInfo,
                                 ur_queue_handle_t Queue,
                                 std::shared_ptr<AllocInfo> &AI);

    /// Initialize Global Variables & Kernel Name at first Launch
    ur_result_t prepareLaunch(ur_context_handle_t Context,
                              std::shared_ptr<DeviceInfo> &DeviceInfo,
                              ur_queue_handle_t Queue,
                              ur_kernel_handle_t Kernel,
                              USMLaunchInfo &LaunchInfo);

    ur_result_t allocShadowMemory(ur_context_handle_t Context,
                                  std::shared_ptr<DeviceInfo> &DeviceInfo);

  private:
    std::unordered_map<ur_context_handle_t, std::shared_ptr<ContextInfo>>
        m_ContextMap;
    ur_shared_mutex m_ContextMapMutex;
    std::unordered_map<ur_device_handle_t, std::shared_ptr<DeviceInfo>>
        m_DeviceMap;
    ur_shared_mutex m_DeviceMapMutex;

    std::unordered_map<ur_kernel_handle_t, std::shared_ptr<KernelInfo>>
        m_KernelMap;
    ur_shared_mutex m_KernelMapMutex;

    std::unordered_map<ur_mem_handle_t, std::shared_ptr<MemBuffer>>
        m_MemBufferMap;
    ur_shared_mutex m_MemBufferMapMutex;

    /// Assumption: all USM chunks are allocated in one VA
    AllocationMap m_AllocationMap;
    ur_shared_mutex m_AllocationMapMutex;

    std::unique_ptr<Quarantine> m_Quarantine;

    std::unordered_set<ur_adapter_handle_t> m_Adapters;
    ur_shared_mutex m_AdaptersMutex;
};

} // namespace ur_sanitizer_layer
