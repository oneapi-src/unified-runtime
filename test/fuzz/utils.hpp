// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <vector>

namespace fuzz {

enum FuzzerAPICall {
    UR_PLATFORM_GET,
    UR_DEVICE_GET,
    UR_DEVICE_RELEASE,
    UR_CONTEXT_CREATE,
    UR_CONTEXT_RELEASE,
    UR_USM_POOL_CREATE_HOST,
    UR_USM_POOL_CREATE_DEVICE,
    UR_USM_POOL_RELEASE_HOST,
    UR_USM_POOL_RELEASE_DEVICE,
    UR_USM_HOST_ALLOC_POOL,
    UR_USM_HOST_ALLOC_NO_POOL,
    UR_USM_DEVICE_ALLOC_POOL,
    UR_USM_DEVICE_ALLOC_NO_POOL,
    UR_USM_FREE_HOST_POOL,
    UR_USM_FREE_HOST_NO_POOL,
    UR_USM_FREE_DEVICE_POOL,
    UR_USM_FREE_DEVICE_NO_POOL,
    UR_PROGRAM_CREATE_WITH_IL,
    UR_MAX_FUZZER_API_CALL,
};

typedef struct FuzzerInput {
    uint8_t *data_ptr;
    size_t data_size;
} FuzzerInput;

typedef struct TestState {
    static constexpr uint32_t num_entries = 1;

    FuzzerInput input;
    ur_loader_config_handle_t config;

    std::vector<ur_adapter_handle_t> adapters;
    std::vector<ur_platform_handle_t> platforms;
    std::vector<ur_device_handle_t> devices;
    std::vector<ur_context_handle_t> contexts;
    std::map<ur_usm_pool_handle_t, std::vector<void *>> pool_host_allocs;
    std::map<ur_usm_pool_handle_t, std::vector<void *>> pool_device_allocs;
    std::vector<void *> no_pool_host_allocs;
    std::vector<void *> no_pool_device_allocs;
    ur_device_type_t device_type = UR_DEVICE_TYPE_ALL;

    uint32_t num_adapters;
    uint32_t num_platforms;
    uint32_t num_devices;

    uint8_t platform_num;
    uint8_t device_num;
    uint8_t context_num;
    uint8_t device_type_fuzz;
} TestState;

//////////////////////////////////////////////////////////////////////////////
template <typename T> int get_next_input_data(FuzzerInput *input, T *out_data) {
    size_t out_data_size = sizeof(out_data);
    if (input->data_size == 0 || input->data_size < out_data_size) {
        return -1;
    }
    *out_data = *input->data_ptr;
    input->data_ptr += out_data_size;
    input->data_size -= out_data_size;

    return 0;
}

int init_random_data(TestState *state) {
    if (state->input.data_size < 5) {
        return -1;
    }
    get_next_input_data(&state->input, &state->platform_num);
    get_next_input_data(&state->input, &state->device_num);
    get_next_input_data(&state->input, &state->context_num);
    get_next_input_data(&state->input, &state->device_type_fuzz);
    if (state->device_type_fuzz < 1 || state->device_type_fuzz > 7) {
        return -1;
    }
    state->device_type = static_cast<ur_device_type_t>(state->device_type_fuzz);

    return 0;
}

int get_next_api_call(FuzzerInput *input) {
    uint8_t next_api_call;
    if (get_next_input_data(input, &next_api_call) != 0) {
        return -1;
    }
    return next_api_call % UR_MAX_FUZZER_API_CALL;
}

bool check_device_exists(const TestState *state) {
    if (state->devices.empty() || state->device_num >= state->devices.size() ||
        state->devices[0] == nullptr) {
        return false;
    }

    return true;
}

bool check_context_exists(const TestState *state) {
    if (state->contexts.empty() ||
        state->context_num >= state->contexts.size() ||
        state->contexts[0] == nullptr) {
        return false;
    }

    return true;
}

bool check_pool_exists(
    const std::map<ur_usm_pool_handle_t, std::vector<void *>> *map,
    const uint8_t pool_num) {
    if (pool_num >= map->size()) {
        return false;
    }

    return true;
}

auto get_map_item(std::map<ur_usm_pool_handle_t, std::vector<void *>> *map,
                  const uint8_t item_index) {
    auto map_it = map->begin();
    std::advance(map_it, item_index);

    return map_it;
}

int load_kernel_source(std::vector<char> &binary_out) {
    std::string source_path = KERNEL_IL_PATH;

    std::ifstream source_file;
    source_file.open(source_path,
                     std::ios::binary | std::ios::in | std::ios::ate);
    if (!source_file.is_open()) {
        std::cerr << "Failed to open a kernel source file: " << source_path
                  << std::endl;
        return -1;
    }

    size_t source_size = static_cast<size_t>(source_file.tellg());
    source_file.seekg(0, std::ios::beg);

    std::vector<char> device_binary(source_size);
    source_file.read(device_binary.data(), source_size);
    if (!source_file) {
        source_file.close();
        std::cerr << "failed reading kernel source data from file: "
                  << source_path << std::endl;
        return -1;
    }
    source_file.close();

    binary_out = std::vector<char>(std::move(device_binary));

    return 0;
}

void cleanup(TestState *state) {
    urLoaderConfigRelease(state->config);

    for (auto &[pool, allocs] : state->pool_host_allocs) {
        for (auto &alloc : allocs) {
            urUSMFree(state->contexts[state->context_num], alloc);
        }
        urUSMPoolRelease(pool);
    }
    for (auto &[pool, allocs] : state->pool_device_allocs) {
        for (auto &alloc : allocs) {
            urUSMFree(state->contexts[state->context_num], alloc);
        }
        urUSMPoolRelease(pool);
    }
    for (auto &alloc : state->no_pool_host_allocs) {
        urUSMFree(state->contexts[state->context_num], alloc);
    }
    for (auto &alloc : state->no_pool_device_allocs) {
        urUSMFree(state->contexts[state->context_num], alloc);
    }
    for (auto &context : state->contexts) {
        urContextRelease(context);
    }
    for (auto &device : state->devices) {
        urDeviceRelease(device);
    }
}
} // namespace fuzz
