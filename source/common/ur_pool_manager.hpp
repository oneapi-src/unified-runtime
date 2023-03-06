/*
    *
    * Copyright (C) 2023 Intel Corporation
    *
    * SPDX-License-Identifier: MIT
    *
    */

#ifndef USM_POOL_MANAGER_HPP
#define USM_POOL_MANAGER_HPP 1

#include "uma_helpers.hpp"
#include "ur_api.h"
#include "ur_util.hpp"

#include <uma/memory_pool.h>
#include <uma/memory_provider.h>

#include <cassert>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace usm {

//////////////// TODO: remove those once implemented //////////
template <typename Config>
using PoolConfigurations = std::unordered_map<ur_usm_mem_flags_t, Config>;
//////////////////////////////////////////////////////////////

std::pair<ur_result_t, std::vector<ur_device_handle_t>> urGetSubDevices(ur_device_handle_t hDevice) {
    size_t n_compute_units;
    auto ret = urDeviceGetInfo(hDevice, UR_DEVICE_INFO_MAX_COMPUTE_UNITS, sizeof(n_compute_units), &n_compute_units, nullptr);
    if (ret != UR_RESULT_SUCCESS) {
        return {ret, {}};
    }

    ur_device_partition_property_t properties[] = {UR_DEVICE_PARTITION_EQUALLY, static_cast<intptr_t>(n_compute_units), 0};

    // Get the number of devices that will be created
    uint32_t n_devices;
    ret = urDevicePartition(hDevice, properties, 0, nullptr, &n_devices);
    if (ret != UR_RESULT_SUCCESS) {
        return {ret, {}};
    }

    std::vector<ur_device_handle_t> sub_devices(n_devices);
    ret = urDevicePartition(hDevice, properties, static_cast<uint32_t>(sub_devices.size()), sub_devices.data(), nullptr);
    if (ret != UR_RESULT_SUCCESS) {
        return {ret, {}};
    }

    return {UR_RESULT_SUCCESS, sub_devices};
}

struct allocation_descriptor_t {
    ur_context_handle_t hContext;
    ur_device_handle_t hDevice;

    ur_usm_type_t type;
    ur_usm_mem_flags_t flags;
    ur_mem_advice_t advice;

    ur_usm_pool_handle_t usmPool;
};

///
/// \brief Manages USM memory pools. Creates 'sub-pools' for Host, Device, Shared and SharedReadOnly
///        allocations for each device. Sub-devices and sub-sub-devices for a specific device share
///        a memory pool.
struct uma_pool_manager_t {
    static constexpr size_t POOLS_PER_DEVICE = 4; /* Host, Device, Shared, SharedReadOnly */

    template <typename ProviderType, typename PoolType>
    void initialize(ur_context_handle_t hContext, uint32_t DeviceCount, const ur_device_handle_t *phDevices,
                    const PoolConfigurations<typename PoolType::Config> &configs, ur_usm_pool_flags_t poolFlags) {
        this->hContext = hContext;
        this->DeviceCount = DeviceCount;
        this->phDevices = phDevices;
        urManagedPools = per_device_type_map_t(DeviceCount * POOLS_PER_DEVICE, &Hash, &Equal);

        addPools<ProviderType, PoolType>(urManagedPools, configs, poolFlags);
    }

    template <typename ProviderType, typename PoolType>
    ur_usm_pool_handle_t poolCreate(const typename PoolType::Config &config, ur_usm_pool_flags_t poolFlags) {
        auto usmPool = new per_device_type_map_t;
        addPools<ProviderType, PoolType>(usmPool, config, poolFlags);
        return reinterpret_cast<ur_usm_pool_handle_t>(usmPool);
    }

    void poolDestroy(ur_usm_pool_handle_t usmPool) {
        delete reinterpret_cast<per_device_type_map_t *>(usmPool);
    }

    uma_memory_pool_handle_t poolForDescriptor(const allocation_descriptor_t &allocDesc) {
        auto &pools = *(allocDesc.usmPool ? reinterpret_cast<per_device_type_map_t *>(allocDesc.usmPool) : &urManagedPools);

        auto poolIt = pools.find(allocDesc);
        if (poolIt == pools.end()) {
            // TODO: LOG
            return nullptr;
        }

        return poolIt->second.get();
    }

    // TOOD: implement poolByPtr once supported by UMA

  private:
    static constexpr bool isSharedAllocationReadOnlyOnDevice(const allocation_descriptor_t &desc) {
        return (desc.type == UR_USM_TYPE_SHARED) && (desc.flags & UR_USM_MEM_FLAG_DEVICE_READ_ONLY);
    }

    static bool Equal(const allocation_descriptor_t &lhs, const allocation_descriptor_t &rhs) {
        ur_native_handle_t lhsNative, rhsNative;

        // We want to share a memory pool for sub-devices and sub-sub devices.
        // Sub-devices and sub-sub-devices might be represented by different ur_device_handle_t but
        // they share the same native_handle_t (which is used by UMA provider).
        // Ref: https://github.com/intel/llvm/commit/86511c5dc84b5781dcfd828caadcb5cac157eae1
        // TODO: is this L0 specific?
        auto ret = urDeviceGetNativeHandle(lhs.hDevice, &lhsNative);
        if (ret != UR_RESULT_SUCCESS) {
            throw ret;
        }
        ret = urDeviceGetNativeHandle(rhs.hDevice, &rhsNative);
        if (ret != UR_RESULT_SUCCESS) {
            throw ret;
        }

        return lhsNative == rhsNative && lhs.type == rhs.type && (isSharedAllocationReadOnlyOnDevice(lhs) == isSharedAllocationReadOnlyOnDevice(rhs));
    };

    static std::size_t Hash(const allocation_descriptor_t &desc) {
        ur_native_handle_t native;
        auto ret = urDeviceGetNativeHandle(desc.hDevice, &native);
        if (ret != UR_RESULT_SUCCESS) {
            throw ret;
        }
        return combine_hashes(0, desc.type, native, isSharedAllocationReadOnlyOnDevice(desc));
    };

    using per_device_type_map_t = std::unordered_map<allocation_descriptor_t, uma::pool_unique_handle_t, decltype(&Hash), decltype(&Equal)>;

    template <typename ProviderType, typename PoolType>
    void addPools(per_device_type_map_t &pools, const PoolConfigurations<typename PoolType::Config> &configs, ur_usm_pool_flags_t poolFlags) {
        addPoolForHost<ProviderType, PoolType>(pools, configs, poolFlags);

        std::function<void(ur_device_handle_t)> addPoolsForDevicesRec = [&](ur_device_handle_t hDevice) {
            addPoolsForDevice<ProviderType, PoolType>(pools, hDevice, configs, poolFlags);
            auto [ret, subDevices] = urGetSubDevices(hDevice);
            if (ret != UR_RESULT_SUCCESS) {
                throw ret;
            }
            for (auto &subDevice : subDevices) {
                addPoolsForDevicesRec(subDevice);
            }
        };

        for (size_t i = 0; i < DeviceCount; i++) {
            addPoolsForDevicesRec(phDevices[i]);
        }
    }

    template <typename PoolType>
    const typename PoolType::Config &getConfig(const PoolConfigurations<typename PoolType::Config> &configs, const allocation_descriptor_t &desc) {
        auto configIt = configs.find(desc.type); // TODO: also consider readOnly
        if (configIt == configs.end()) {
            // TODO: log
            throw UR_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return configIt->second;
    }

    template <typename ProviderType, typename PoolType>
    void addPoolForHost(per_device_type_map_t &pools, const PoolConfigurations<typename PoolType::Config> &configs, ur_usm_pool_flags_t poolFlags) {
        allocation_descriptor_t desc = {};
        desc.type = UR_USM_TYPE_HOST;
        desc.hDevice = nullptr;

        // TODO: release provider in the pool dtor...
        auto provider = uma::memoryProviderMakeUnique<ProviderType>(poolFlags & UR_USM_POOL_FLAG_ZERO_INITIALIZE_BLOCK);
        auto pool = uma::poolMakeUnique<PoolType>(provider.second.get(), 1, getConfig<PoolType>(configs, desc));

        auto ret = pools.emplace(desc, std::move(pool.second)); // TODO: error handling
        assert(ret.second);
    }

    template <typename ProviderType, typename PoolType>
    void addPoolsForDevice(per_device_type_map_t &pools, ur_device_handle_t hDevice, const PoolConfigurations<typename PoolType::Config> &configs, ur_usm_pool_flags_t poolFlags) {
        allocation_descriptor_t descriptors[3] = {};
        descriptors[0].type = UR_USM_TYPE_DEVICE;
        descriptors[0].hDevice = nullptr;

        descriptors[1].type = UR_USM_TYPE_SHARED;
        descriptors[1].hDevice = hDevice;

        descriptors[2].type = UR_USM_TYPE_SHARED;
        descriptors[2].hDevice = hDevice;
        descriptors[2].flags = UR_USM_MEM_FLAG_DEVICE_READ_ONLY;

        for (auto &desc : descriptors) {
            // TODO: release provider in the pool dtor...
            auto provider = uma::memoryProviderMakeUnique<ProviderType>(poolFlags & UR_USM_POOL_FLAG_ZERO_INITIALIZE_BLOCK);
            auto pool = uma::poolMakeUnique<PoolType>(provider.second.get(), 1, getConfig<PoolType>(configs, desc));

            auto ret = pools.emplace(desc, std::move(pool.second)); // TODO: error handling
            assert(ret.second);
        }
    }

    ur_context_handle_t hContext;
    uint32_t DeviceCount;
    const ur_device_handle_t *phDevices;
    per_device_type_map_t urManagedPools;
};
} // namespace usm

#endif /* USM_POOL_MANAGER_HPP */