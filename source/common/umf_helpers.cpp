/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include "umf_helpers.hpp"
#include "ur_pool_manager.hpp"

namespace umf {
umf_result_t USMSharedMemoryProvider::initialize(usm::pool_descriptor Desc) {
    Context = Desc.hContext;
    Device = Desc.hDevice;
    usmDesc.pNext = &hostDesc;
    hostDesc.flags = Desc.hostFlags;
    hostDesc.pNext = &deviceDesc;
    deviceDesc.flags = Desc.deviceFlags;

    if (Desc.allocLocation.has_value()) {
        allocLocation = Desc.allocLocation.value();
        deviceDesc.pNext = &allocLocation;
    }

    return UMF_RESULT_SUCCESS;
}

umf_result_t USMDeviceMemoryProvider::initialize(usm::pool_descriptor Desc) {
    Context = Desc.hContext;
    Device = Desc.hDevice;
    usmDesc.pNext = &deviceDesc;
    deviceDesc.flags = Desc.deviceFlags;

    if (Desc.allocLocation.has_value()) {
        allocLocation = Desc.allocLocation.value();
        deviceDesc.pNext = &allocLocation;
    }

    return UMF_RESULT_SUCCESS;
}

umf_result_t USMHostMemoryProvider::initialize(usm::pool_descriptor Desc) {
    Context = Desc.hContext;
    usmDesc.pNext = &hostDesc;
    hostDesc.flags = Desc.hostFlags;

    if (Desc.allocLocation.has_value()) {
        allocLocation = Desc.allocLocation.value();
        hostDesc.pNext = &allocLocation;
    }

    return UMF_RESULT_SUCCESS;
}
} // namespace umf
