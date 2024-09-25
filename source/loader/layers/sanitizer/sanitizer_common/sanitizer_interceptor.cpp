//===----------------------------------------------------------------------===//
/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file asan_interceptor.cpp
 *
 */

#include "sanitizer_interceptor.hpp"
#include "sanitizer_utils.hpp"
#include "ur_api.h"

namespace ur_sanitizer_layer {

ur_result_t SanitizerInterceptor::allocateMemory(
    ur_context_handle_t Context, ur_device_handle_t Device,
    const ur_usm_desc_t *Properties, ur_usm_pool_handle_t Pool, size_t Size,
    AllocType Type, void **ResultPtr) {
    return UR_RESULT_SUCCESS;
}
ur_result_t SanitizerInterceptor::releaseMemory(ur_context_handle_t Context,
                                                void *Ptr) {
    return UR_RESULT_SUCCESS;
}

ur_result_t
SanitizerInterceptor::registerDeviceGlobals(ur_context_handle_t Context,
                                            ur_program_handle_t Program) {
    return UR_RESULT_SUCCESS;
}

ur_result_t SanitizerInterceptor::preLaunchKernel(ur_kernel_handle_t Kernel,
                                                  ur_queue_handle_t Queue,
                                                  USMLaunchInfo &LaunchInfo) {
    return UR_RESULT_SUCCESS;
}

ur_result_t SanitizerInterceptor::postLaunchKernel(ur_kernel_handle_t Kernel,
                                                   ur_queue_handle_t Queue,
                                                   USMLaunchInfo &LaunchInfo) {
    return UR_RESULT_SUCCESS;
}

ur_result_t
SanitizerInterceptor::insertContext(ur_context_handle_t Context,
                                    std::shared_ptr<ContextInfo> &CI) {
    return UR_RESULT_SUCCESS;
}
ur_result_t SanitizerInterceptor::eraseContext(ur_context_handle_t Context) {
    return UR_RESULT_SUCCESS;
}

ur_result_t
SanitizerInterceptor::insertDevice(ur_device_handle_t Device,
                                   std::shared_ptr<DeviceInfo> &CI) {
    return UR_RESULT_SUCCESS;
}
ur_result_t SanitizerInterceptor::eraseDevice(ur_device_handle_t Device) {
    return UR_RESULT_SUCCESS;
}

ur_result_t SanitizerInterceptor::insertKernel(ur_kernel_handle_t Kernel) {
    return UR_RESULT_SUCCESS;
}
ur_result_t SanitizerInterceptor::eraseKernel(ur_kernel_handle_t Kernel) {
    return UR_RESULT_SUCCESS;
}

ur_result_t
SanitizerInterceptor::insertMemBuffer(std::shared_ptr<MemBuffer> MemBuffer) {
    return UR_RESULT_SUCCESS;
}
ur_result_t SanitizerInterceptor::eraseMemBuffer(ur_mem_handle_t MemHandle) {
    return UR_RESULT_SUCCESS;
}
std::shared_ptr<MemBuffer>
SanitizerInterceptor::getMemBuffer(ur_mem_handle_t MemHandle) {
    return nullptr;
}

ur_result_t SanitizerInterceptor::holdAdapter(ur_adapter_handle_t Adapter) {
    return UR_RESULT_SUCCESS;
}

std::optional<AllocationIterator>
SanitizerInterceptor::findAllocInfoByAddress(uptr Address) {
    return std::nullopt;
}

std::shared_ptr<ContextInfo>
SanitizerInterceptor::getContextInfo(ur_context_handle_t Context) {
    return nullptr;
}

std::shared_ptr<DeviceInfo>
SanitizerInterceptor::getDeviceInfo(ur_device_handle_t Device) {
    return nullptr;
}

std::shared_ptr<KernelInfo>
SanitizerInterceptor::getKernelInfo(ur_kernel_handle_t Kernel) {
    return nullptr;
}

} // namespace ur_sanitizer_layer
