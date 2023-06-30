// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <cassert>
#include <cstring>
#include <vector>

#include "ur_api.h"

struct FuzzData {
    uint16_t platform_count;
    uint16_t device_count;
    uint8_t device_type;
    uint32_t device_alloc_size;
    uint32_t host_alloc_size;
    uint8_t usm_alloc_info_prop_name;
    uint16_t usm_alloc_info_prop_size;
    void *usm_alloc_info_prop_value;
    uint16_t num_subdevices;
};

int ParseData(uint8_t *data, size_t size, FuzzData *fuzz_data) {
    // Make sure fuzzer data size is sufficient to store all variables
    if (size < sizeof(*fuzz_data)) {
        return -1;
    }

    uint8_t *data_ptr = data;
    memcpy(&fuzz_data->platform_count, data_ptr,
           sizeof(fuzz_data->platform_count));
    // Limit the max number of platforms to avoid allocating too much memory for a vector
    if (fuzz_data->platform_count > 1024) {
        return -1;
    }
    data_ptr += sizeof(fuzz_data->platform_count);

    memcpy(&fuzz_data->device_count, data_ptr, sizeof(fuzz_data->device_count));
    data_ptr += sizeof(fuzz_data->device_count);

    memcpy(&fuzz_data->device_type, data_ptr, sizeof(fuzz_data->device_type));
    // Pass only integers which can be a valid device type
    if (fuzz_data->device_type > 7) {
        return -1;
    }
    data_ptr += sizeof(fuzz_data->device_type);

    memcpy(&fuzz_data->device_alloc_size, data_ptr,
           sizeof(fuzz_data->device_alloc_size));
    // Limit the max size of allocations
    if (fuzz_data->device_alloc_size > 1 * 1024 * 1024) {
        return -1;
    }
    data_ptr += sizeof(fuzz_data->device_alloc_size);

    memcpy(&fuzz_data->host_alloc_size, data_ptr,
           sizeof(fuzz_data->host_alloc_size));
    // Limit the max size of allocations
    if (fuzz_data->host_alloc_size > 1 * 1024 * 1024) {
        return -1;
    }
    data_ptr += sizeof(fuzz_data->host_alloc_size);

    memcpy(&fuzz_data->usm_alloc_info_prop_name, data_ptr,
           sizeof(fuzz_data->usm_alloc_info_prop_name));
    // Pass only integers which can be a valid memory alloc info property name
    if (fuzz_data->usm_alloc_info_prop_name > 4) {
        return -1;
    }
    data_ptr += sizeof(fuzz_data->usm_alloc_info_prop_name);

    memcpy(&fuzz_data->usm_alloc_info_prop_size, data_ptr,
           sizeof(fuzz_data->usm_alloc_info_prop_size));
    // Limit the max size of the allocation
    if (fuzz_data->usm_alloc_info_prop_size > UINT16_MAX) {
        return -1;
    }
    // Make sure fuzzer data size is sufficient to store property data
    if (size < sizeof(*fuzz_data) + fuzz_data->usm_alloc_info_prop_size) {
        return -1;
    }
    data_ptr += sizeof(fuzz_data->usm_alloc_info_prop_size);

    fuzz_data->usm_alloc_info_prop_value = data_ptr;
    data_ptr += fuzz_data->usm_alloc_info_prop_size;

    memcpy(&fuzz_data->num_subdevices, data_ptr,
           sizeof(fuzz_data->num_subdevices));
    // Limit the max number of subdevices to avoid allocating too much memory for a vector
    if (fuzz_data->num_subdevices > 1024) {
        return -1;
    }
    data_ptr += sizeof(fuzz_data->num_subdevices);

    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(uint8_t *data, size_t size) {
    FuzzData fuzz_data;
    int ret = ParseData(data, size, &fuzz_data);
    if (ret) {
        return ret;
    }

    //// API calls
    ur_result_t res = UR_RESULT_SUCCESS;

    res = urInit(0);
    if (res != UR_RESULT_SUCCESS) {
        return 0;
    }

    // Get valid platforms
    std::vector<ur_platform_handle_t> platforms;
    uint32_t platformCount = 0;

    res = urPlatformGet(fuzz_data.platform_count, nullptr, &platformCount);
    if (res != UR_RESULT_SUCCESS) {
        return 0;
    }
    platformCount =
        platformCount ? fuzz_data.platform_count % platformCount : 0;
    platforms.resize(platformCount);
    res = urPlatformGet(platformCount, platforms.data(), nullptr);
    if (res != UR_RESULT_SUCCESS || platformCount == 0) {
        return 0;
    }

    // Get valid devices of a random platform
    ur_platform_handle_t platform =
        platforms[fuzz_data.platform_count % platforms.size()];
    std::vector<ur_device_handle_t> devices;
    uint32_t deviceCount = 0;
    ur_device_type_t deviceType =
        static_cast<ur_device_type_t>(fuzz_data.device_type);

    res = urDeviceGet(platform, deviceType, fuzz_data.device_count, nullptr,
                      &deviceCount);
    if (res != UR_RESULT_SUCCESS) {
        return 0;
    }
    deviceCount = deviceCount ? fuzz_data.device_count % deviceCount : 0;
    devices.resize(deviceCount);
    res = urDeviceGet(platform, deviceType, devices.size(), devices.data(),
                      nullptr);
    if (res != UR_RESULT_SUCCESS || deviceCount == 0) {
        return 0;
    }

    // Test API
    ur_context_handle_t context;
    void *host_ptr = nullptr;
    void *device_ptr = nullptr;
    size_t usm_alloc_info_size = 0;
    ur_device_handle_t device =
        devices[fuzz_data.device_count % devices.size()];
    ur_usm_type_t device_type = UR_USM_TYPE_UNKNOWN;
    const char *msg = nullptr;
    int32_t error_code = -1;
    ur_usm_pool_handle_t pool;
    std::vector<ur_device_handle_t> subdevices(fuzz_data.num_subdevices);

    urContextCreate(devices.size(), devices.data(), nullptr, &context);
    urUSMPoolCreate(context, nullptr, &pool);
    urUSMHostAlloc(context, nullptr, pool, fuzz_data.host_alloc_size,
                   &host_ptr);
    if (fuzz_data.host_alloc_size != 0) {
        memset(host_ptr, 'H', fuzz_data.host_alloc_size);
    } else {
        assert(host_ptr == nullptr);
    }
    urUSMDeviceAlloc(context, device, nullptr, pool,
                     fuzz_data.device_alloc_size, &device_ptr);
    urUSMGetMemAllocInfo(
        context, host_ptr,
        static_cast<ur_usm_alloc_info_t>(fuzz_data.usm_alloc_info_prop_name),
        fuzz_data.usm_alloc_info_prop_size, fuzz_data.usm_alloc_info_prop_value,
        nullptr);

    urPlatformGetLastError(platform, &msg, &error_code);

    urDevicePartition(device, nullptr, subdevices.size(), subdevices.data(),
                      nullptr);

    urUSMFree(context, host_ptr);
    urUSMFree(context, device_ptr);
    urUSMPoolRelease(pool);
    for (auto &device : devices) {
        urDeviceRelease(device);
    }
    urContextRelease(context);
    return 0;
}
