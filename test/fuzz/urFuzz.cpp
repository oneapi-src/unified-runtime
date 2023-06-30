// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "kernel_entry_points.h"
#include "ur_api.h"
#include "utils.hpp"

namespace fuzz {

int ur_platform_get(TestState &state) {
    ur_result_t res = urPlatformGet(
        state.adapters.data(), state.adapters.size(), state.num_entries,
        state.platforms.data(), &state.num_platforms);
    if (res != UR_RESULT_SUCCESS) {
        return -1;
    }
    if (state.platforms.size() != state.num_platforms) {
        state.platforms.resize(state.num_platforms);
    }

    return 0;
}

int ur_device_get(TestState &state) {
    if (state.platforms.empty() ||
        state.platform_num >= state.platforms.size() ||
        state.platforms[0] == nullptr) {
        return -1;
    }

    ur_result_t res = urDeviceGet(state.platforms[state.platform_num],
                                  state.device_type, state.num_entries,
                                  state.devices.data(), &state.num_devices);
    if (res != UR_RESULT_SUCCESS) {
        return -1;
    }
    if (state.devices.size() != state.num_devices) {
        state.devices.resize(state.num_devices);
    }

    return 0;
}

int ur_device_release(TestState &state) {
    if (state.devices.empty()) {
        return -1;
    }

    ur_result_t res = urDeviceRelease(state.devices.back());
    if (res == UR_RESULT_SUCCESS) {
        state.devices.pop_back();
    }

    return 0;
}

int ur_context_create(TestState &state) {
    if (!check_device_exists(&state)) {
        return -1;
    }

    ur_context_handle_t context;
    ur_result_t res = urContextCreate(state.devices.size(),
                                      state.devices.data(), nullptr, &context);
    if (res == UR_RESULT_SUCCESS) {
        state.contexts.push_back(context);
    }

    return 0;
}

int ur_context_release(TestState &state) {
    if (!check_context_exists(&state) ||
        state.contexts[state.context_num] == state.contexts.back()) {
        return -1;
    }

    ur_result_t res = urContextRelease(state.contexts.back());
    if (res == UR_RESULT_SUCCESS) {
        state.contexts.pop_back();
    }

    return 0;
}

int pool_create(
    TestState &state,
    std::map<ur_usm_pool_handle_t, std::vector<void *>> &pool_allocs) {
    if (!check_context_exists(&state)) {
        return -1;
    }

    ur_usm_pool_handle_t pool;
    ur_usm_pool_desc_t pool_desc{UR_STRUCTURE_TYPE_USM_POOL_DESC, nullptr,
                                 UR_USM_POOL_FLAG_ZERO_INITIALIZE_BLOCK};
    ur_result_t res =
        urUSMPoolCreate(state.contexts[state.context_num], &pool_desc, &pool);
    if (res == UR_RESULT_SUCCESS) {
        pool_allocs[pool] = {};
    }

    return 0;
}

int ur_usm_pool_create_host(TestState &state) {
    return pool_create(state, state.pool_host_allocs);
}

int ur_usm_pool_create_device(TestState &state) {
    return pool_create(state, state.pool_device_allocs);
}

int pool_release(
    TestState &state,
    std::map<ur_usm_pool_handle_t, std::vector<void *>> &pool_allocs) {
    if (!check_context_exists(&state)) {
        return -1;
    }

    uint8_t pool_num;
    if (get_next_input_data(&state.input, &pool_num) != 0) {
        return -1;
    }
    if (!check_pool_exists(&pool_allocs, pool_num)) {
        return -1;
    }

    auto &[pool, allocs] = *get_map_item(&pool_allocs, pool_num);
    if (allocs.empty()) {
        return -1;
    }

    ur_result_t res = urUSMPoolRelease(pool);
    if (res == UR_RESULT_SUCCESS) {
        pool_allocs.erase(pool);
    }

    return 0;
}

int ur_usm_pool_release_host(TestState &state) {
    return pool_release(state, state.pool_host_allocs);
}

int ur_usm_pool_release_device(TestState &state) {
    return pool_release(state, state.pool_device_allocs);
}

int alloc_setup(TestState &state, uint16_t &alloc_size) {
    if (!check_context_exists(&state)) {
        return -1;
    }

    if (get_next_input_data(&state.input, &alloc_size) != 0) {
        return -1;
    }

    return 0;
}

int host_alloc(TestState &state, ur_usm_pool_handle_t pool,
               std::vector<void *> &allocs) {
    void *ptr;
    uint16_t alloc_size;
    ur_result_t res = UR_RESULT_SUCCESS;

    int ret = alloc_setup(state, alloc_size);
    if (ret != 0) {
        return -1;
    }

    auto &context = state.contexts[state.context_num];
    res = urUSMHostAlloc(context, nullptr, pool, alloc_size, &ptr);
    if (res == UR_RESULT_SUCCESS) {
        allocs.push_back(ptr);
    }

    return 0;
}

int get_alloc_pool(
    TestState &state,
    std::map<ur_usm_pool_handle_t, std::vector<void *>> &pool_map,
    ur_usm_pool_handle_t pool, std::vector<void *> &allocs) {
    uint8_t pool_num;

    if (get_next_input_data(&state.input, &pool_num) != 0) {
        return -1;
    }
    if (!check_pool_exists(&pool_map, pool_num)) {
        return -1;
    }

    auto &[pool_tmp, allocs_tmp] = *get_map_item(&pool_map, pool_num);
    pool = pool_tmp;
    allocs = allocs_tmp;

    return 0;
}

int ur_usm_host_alloc_pool(TestState &state) {
    ur_usm_pool_handle_t pool = nullptr;
    std::vector<void *> allocs;

    get_alloc_pool(state, state.pool_host_allocs, pool, allocs);
    return host_alloc(state, pool, allocs);
}

int ur_usm_host_alloc_no_pool(TestState &state) {
    return host_alloc(state, nullptr, state.no_pool_host_allocs);
}

int device_alloc(TestState &state, ur_usm_pool_handle_t pool,
                 std::vector<void *> &allocs) {
    void *ptr;
    uint16_t alloc_size;
    ur_result_t res = UR_RESULT_SUCCESS;

    int ret = alloc_setup(state, alloc_size);
    if (ret != 0) {
        return -1;
    }

    if (!check_device_exists(&state)) {
        return -1;
    }

    auto &context = state.contexts[state.context_num];
    auto &device = state.devices[state.device_num];
    res = urUSMDeviceAlloc(context, device, nullptr, pool, alloc_size, &ptr);
    if (res == UR_RESULT_SUCCESS) {
        allocs.push_back(ptr);
    }

    return 0;
}

int ur_usm_device_alloc_pool(TestState &state) {
    ur_usm_pool_handle_t pool = nullptr;
    std::vector<void *> allocs;

    get_alloc_pool(state, state.pool_device_allocs, pool, allocs);
    return device_alloc(state, pool, allocs);
}

int ur_usm_device_alloc_no_pool(TestState &state) {
    return device_alloc(state, nullptr, state.no_pool_device_allocs);
}

int free_pool(TestState &state,
              std::map<ur_usm_pool_handle_t, std::vector<void *>> &pool_map) {
    if (pool_map.empty()) {
        return -1;
    }

    ur_usm_pool_handle_t pool = nullptr;
    std::vector<void *> allocs;

    int ret = get_alloc_pool(state, pool_map, pool, allocs);
    if (ret != 0 || allocs.empty()) {
        return -1;
    }

    urUSMFree(state.contexts[state.context_num], allocs.back());
    allocs.pop_back();

    return 0;
}

int ur_usm_free_host_pool(TestState &state) {
    return free_pool(state, state.pool_host_allocs);
}

int ur_usm_free_device_pool(TestState &state) {
    return free_pool(state, state.pool_device_allocs);
}

int free_no_pool(TestState &state, std::vector<void *> allocs) {
    if (allocs.empty()) {
        return -1;
    }

    urUSMFree(state.contexts[state.context_num], allocs.back());
    allocs.pop_back();

    return 0;
}

int ur_usm_free_host_no_pool(TestState &state) {
    return free_no_pool(state, state.no_pool_host_allocs);
}

int ur_usm_free_device_no_pool(TestState &state) {
    return free_no_pool(state, state.no_pool_device_allocs);
}

// TODO: Extract API calls to separate functions
int ur_program_create_with_il(TestState &state) {
    if (!check_context_exists(&state) || !check_device_exists(&state)) {
        return -1;
    }

    std::vector<char> il_bin;
    ur_program_handle_t program = nullptr;
    ur_kernel_handle_t kernel = nullptr;
    ur_queue_handle_t queue = nullptr;
    ur_event_handle_t event = nullptr;
    auto &context = state.contexts[state.context_num];
    auto &device = state.devices[state.device_num];
    std::string kernel_name =
        uur::device_binaries::program_kernel_map["bar"][0];

    load_kernel_source(il_bin);
    urProgramCreateWithIL(context, il_bin.data(), il_bin.size(), nullptr,
                          &program);
    urProgramBuild(context, program, nullptr);
    urKernelCreate(program, kernel_name.data(), &kernel);
    urQueueCreate(context, device, nullptr, &queue);

    const uint32_t nDim = 3;
    const size_t gWorkOffset[] = {0, 0, 0};
    const size_t gWorkSize[] = {128, 128, 128};

    urEnqueueKernelLaunch(queue, kernel, nDim, gWorkOffset, gWorkSize, nullptr,
                          0, nullptr, &event);

    urEventWait(1, &event);
    urEventRelease(event);
    urQueueFinish(queue);
    urQueueRelease(queue);
    urKernelRelease(kernel);
    urProgramRelease(program);

    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(uint8_t *data, size_t size) {
    int next_api_call;
    TestState test_state;
    int ret = -1;

    int (*api_wrappers[])(TestState &) = {
        ur_platform_get,
        ur_device_get,
        ur_device_release,
        ur_context_create,
        ur_context_release,
        ur_usm_pool_create_host,
        ur_usm_pool_create_device,
        ur_usm_pool_release_host,
        ur_usm_pool_release_device,
        ur_usm_host_alloc_pool,
        ur_usm_host_alloc_no_pool,
        ur_usm_device_alloc_pool,
        ur_usm_device_alloc_no_pool,
        ur_usm_free_host_pool,
        ur_usm_free_host_no_pool,
        ur_usm_free_device_pool,
        ur_usm_free_device_no_pool,
        ur_program_create_with_il,
    };

    test_state.input = {data, size};

    ret = init_random_data(&test_state);
    if (ret == -1) {
        return ret;
    }

    urLoaderConfigCreate(&test_state.config);
    urLoaderConfigEnableLayer(test_state.config, "UR_LAYER_FULL_VALIDATION");
    ur_result_t res = urInit(0, test_state.config);
    if (res != UR_RESULT_SUCCESS) {
        return -1;
    }

    test_state.adapters.resize(test_state.num_entries);
    res = urAdapterGet(test_state.num_entries, test_state.adapters.data(),
                       &test_state.num_adapters);
    if (res != UR_RESULT_SUCCESS || test_state.num_adapters == 0) {
        return -1;
    }

    while ((next_api_call = get_next_api_call(&test_state.input)) != -1) {
        ret = api_wrappers[next_api_call](test_state);
        if (ret) {
            cleanup(&test_state);
            return -1;
        }
    }

    cleanup(&test_state);
    return 0;
}
} // namespace fuzz
