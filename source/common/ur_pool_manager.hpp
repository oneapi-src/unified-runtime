/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include "disjoint_pool_config_parser.hpp"
#ifndef USM_POOL_MANAGER_HPP
#define USM_POOL_MANAGER_HPP 1

#include "logger/ur_logger.hpp"
#include "umf_helpers.hpp"
#include "ur_api.h"
#include "ur_util.hpp"

#include <umf/memory_pool.h>
#include <umf/memory_provider.h>

#include <functional>
#include <unordered_map>
#include <vector>

namespace usm {

/// @brief describes an internal USM pool instance.
struct pool_descriptor {
    pool_descriptor(ur_usm_pool_handle_t poolHandle,
                    ur_context_handle_t hContext, ur_device_handle_t hDevice,
                    ur_usm_type_t type, const ur_usm_desc_t *pDesc)
        : poolHandle(poolHandle), hContext(hContext), hDevice(hDevice),
          type(type) {
        const ur_base_desc_t *Next =
            pDesc ? static_cast<const ur_base_desc_t *>(pDesc->pNext) : nullptr;
        while (Next) {
            switch (Next->stype) {
            case UR_STRUCTURE_TYPE_USM_HOST_DESC: {
                const auto *hostDesc =
                    reinterpret_cast<const ur_usm_host_desc_t *>(Next);
                hostFlags |= hostDesc->flags;
                break;
            }
            case UR_STRUCTURE_TYPE_USM_DEVICE_DESC: {
                const auto *deviceDesc =
                    reinterpret_cast<const ur_usm_device_desc_t *>(Next);
                deviceFlags |= deviceDesc->flags;
                break;
            }
            case UR_STRUCTURE_TYPE_USM_ALLOC_LOCATION_DESC: {
                allocLocation =
                    *reinterpret_cast<const ur_usm_alloc_location_desc_t *>(
                        Next);
                break;
            }
            default:
                break;
            }
            Next = Next->pNext
                       ? static_cast<const ur_base_desc_t *>(Next->pNext)
                       : nullptr;
        }
    }

    ur_usm_pool_handle_t poolHandle;
    ur_context_handle_t hContext;
    ur_device_handle_t hDevice;
    ur_usm_type_t type;
    ur_usm_host_mem_flags_t hostFlags = 0;
    ur_usm_device_mem_flags_t deviceFlags = 0;
    std::optional<ur_usm_alloc_location_desc_t> allocLocation;

    bool operator==(const pool_descriptor &other) const;
    friend std::ostream &operator<<(std::ostream &os,
                                    const pool_descriptor &desc);

    // Returns default initialized pool descriptors for host, device and shared
    // allocations.
    static std::pair<ur_result_t, std::vector<pool_descriptor>>
    createDefaults(ur_usm_pool_handle_t poolHandle,
                   ur_context_handle_t hContext);
};

static inline std::pair<ur_result_t, std::vector<ur_device_handle_t>>
urGetSubDevices(ur_device_handle_t hDevice) {
    uint32_t nComputeUnits;
    auto ret = urDeviceGetInfo(hDevice, UR_DEVICE_INFO_MAX_COMPUTE_UNITS,
                               sizeof(nComputeUnits), &nComputeUnits, nullptr);
    if (ret != UR_RESULT_SUCCESS) {
        return {ret, {}};
    }

    ur_device_partition_property_t prop;
    prop.type = UR_DEVICE_PARTITION_BY_CSLICE;
    prop.value.affinity_domain = 0;

    ur_device_partition_properties_t properties{
        UR_STRUCTURE_TYPE_DEVICE_PARTITION_PROPERTIES,
        nullptr,
        &prop,
        1,
    };

    // Get the number of devices that will be created
    uint32_t deviceCount;
    ret = urDevicePartition(hDevice, &properties, 0, nullptr, &deviceCount);
    if (ret != UR_RESULT_SUCCESS) {
        return {ret, {}};
    }

    std::vector<ur_device_handle_t> sub_devices(deviceCount);
    ret = urDevicePartition(hDevice, &properties,
                            static_cast<uint32_t>(sub_devices.size()),
                            sub_devices.data(), nullptr);
    if (ret != UR_RESULT_SUCCESS) {
        return {ret, {}};
    }

    return {UR_RESULT_SUCCESS, sub_devices};
}

inline std::pair<ur_result_t, std::vector<ur_device_handle_t>>
urGetAllDevicesAndSubDevices(ur_context_handle_t hContext) {
    size_t deviceCount = 0;
    auto ret = urContextGetInfo(hContext, UR_CONTEXT_INFO_NUM_DEVICES,
                                sizeof(deviceCount), &deviceCount, nullptr);
    if (ret != UR_RESULT_SUCCESS || deviceCount == 0) {
        return {ret, {}};
    }

    std::vector<ur_device_handle_t> devices(deviceCount);
    ret = urContextGetInfo(hContext, UR_CONTEXT_INFO_DEVICES,
                           sizeof(ur_device_handle_t) * deviceCount,
                           devices.data(), nullptr);
    if (ret != UR_RESULT_SUCCESS) {
        return {ret, {}};
    }

    std::vector<ur_device_handle_t> devicesAndSubDevices;
    std::function<ur_result_t(ur_device_handle_t)> addPoolsForDevicesRec =
        [&](ur_device_handle_t hDevice) {
            devicesAndSubDevices.push_back(hDevice);
            auto [ret, subDevices] = urGetSubDevices(hDevice);
            if (ret != UR_RESULT_SUCCESS) {
                return ret;
            }
            for (auto &subDevice : subDevices) {
                ret = addPoolsForDevicesRec(subDevice);
                if (ret != UR_RESULT_SUCCESS) {
                    return ret;
                }
            }
            return UR_RESULT_SUCCESS;
        };

    for (size_t i = 0; i < deviceCount; i++) {
        ret = addPoolsForDevicesRec(devices[i]);
        if (ret != UR_RESULT_SUCCESS) {
            if (ret == UR_RESULT_ERROR_UNSUPPORTED_FEATURE) {
                // Return main devices when sub-devices are unsupported.
                return {ret, std::move(devices)};
            }

            return {ret, {}};
        }
    }

    return {UR_RESULT_SUCCESS, devicesAndSubDevices};
}

inline bool pool_descriptor::operator==(const pool_descriptor &other) const {
    const pool_descriptor &lhs = *this;
    const pool_descriptor &rhs = other;
    ur_native_handle_t lhsNative = nullptr, rhsNative = nullptr;

    // We want to share a memory pool for sub-devices and sub-sub devices.
    // Sub-devices and sub-sub-devices might be represented by different ur_device_handle_t but
    // they share the same native_handle_t (which is used by UMF provider).
    // Ref: https://github.com/intel/llvm/commit/86511c5dc84b5781dcfd828caadcb5cac157eae1
    // TODO: is this L0 specific?
    if (lhs.hDevice) {
        auto ret = urDeviceGetNativeHandle(lhs.hDevice, &lhsNative);
        if (ret != UR_RESULT_SUCCESS) {
            throw ret;
        }
    }

    if (rhs.hDevice) {
        auto ret = urDeviceGetNativeHandle(rhs.hDevice, &rhsNative);
        if (ret != UR_RESULT_SUCCESS) {
            throw ret;
        }
    }

    if (lhs.allocLocation.has_value()) {
        if (!rhs.allocLocation.has_value()) {
            return false;
        }

        if (lhs.allocLocation.value().location !=
            rhs.allocLocation.value().location) {
            return false;
        }
    }

    return lhsNative == rhsNative && lhs.type == rhs.type &&
           lhs.hostFlags == rhs.hostFlags &&
           lhs.deviceFlags == rhs.deviceFlags &&
           lhs.poolHandle == rhs.poolHandle;
}

inline std::ostream &operator<<(std::ostream &os, const pool_descriptor &desc) {
    os << "pool handle: " << desc.poolHandle
       << " context handle: " << desc.hContext
       << " device handle: " << desc.hDevice << " memory type: " << desc.type
       << " is read only: "
       << (desc.deviceFlags & UR_USM_DEVICE_MEM_FLAG_DEVICE_READ_ONLY);
    return os;
}

inline std::pair<ur_result_t, std::vector<pool_descriptor>>
pool_descriptor::createDefaults(ur_usm_pool_handle_t poolHandle,
                                ur_context_handle_t hContext) {
    auto [ret, devices] = urGetAllDevicesAndSubDevices(hContext);
    if (ret != UR_RESULT_SUCCESS) {
        return {ret, {}};
    }

    std::vector<pool_descriptor> descriptors;
    descriptors.emplace_back(poolHandle, hContext, nullptr, UR_USM_TYPE_HOST,
                             nullptr);

    for (auto &device : devices) {
        {
            descriptors.emplace_back(poolHandle, hContext, device,
                                     UR_USM_TYPE_DEVICE, nullptr);
        }
        {
            descriptors.emplace_back(poolHandle, hContext, device,
                                     UR_USM_TYPE_SHARED, nullptr);
        }
        {
            ur_usm_device_desc_t deviceDesc = {
                UR_STRUCTURE_TYPE_USM_DEVICE_DESC, nullptr,
                UR_USM_DEVICE_MEM_FLAG_DEVICE_READ_ONLY};
            ur_usm_desc_t desc = {UR_STRUCTURE_TYPE_USM_DESC, &deviceDesc, 0,
                                  0};

            descriptors.emplace_back(poolHandle, hContext, device,
                                     UR_USM_TYPE_SHARED, &desc);
        }
    }

    return {ret, descriptors};
}

inline usm::DisjointPoolMemType
urTypeToDisjointPoolType(const ur_usm_type_t &type) {
    switch (type) {
    case UR_USM_TYPE_HOST:
        return usm::DisjointPoolMemType::Host;
    case UR_USM_TYPE_DEVICE:
        return usm::DisjointPoolMemType::Device;
    case UR_USM_TYPE_SHARED:
        return usm::DisjointPoolMemType::Shared;
    default:
        // Added to suppress 'not all control paths return a value' warning.
        return usm::DisjointPoolMemType::All;
    }
}

template <typename D> struct pool_manager {
  private:
    using desc_to_pool_map_t = std::unordered_map<D, umf::pool_unique_handle_t>;

    desc_to_pool_map_t descToPoolMap;

  public:
    static std::pair<ur_result_t, pool_manager>
    create(desc_to_pool_map_t descToHandleMap = {}) {
        auto manager = pool_manager();

        for (auto &[desc, hPool] : descToHandleMap) {
            auto ret = manager.addPool(desc, hPool);
            if (ret != UR_RESULT_SUCCESS) {
                return {ret, pool_manager()};
            }
        }

        return {UR_RESULT_SUCCESS, std::move(manager)};
    }

    ur_result_t addPool(const D &desc,
                        umf::pool_unique_handle_t &hPool) noexcept {
        if (!descToPoolMap.try_emplace(desc, std::move(hPool)).second) {
            logger::error("Pool for pool descriptor: {}, already exists", desc);
            return UR_RESULT_ERROR_INVALID_ARGUMENT;
        }

        return UR_RESULT_SUCCESS;
    }

    std::optional<umf_memory_pool_handle_t> getPool(const D &desc) noexcept {
        auto it = descToPoolMap.find(desc);
        if (it == descToPoolMap.end()) {
            logger::error(
                "Pool descriptor: {}, doesn't match any existing pool", desc);
            return std::nullopt;
        }
        return it->second.get();
    }

    bool hasPool(umf_memory_pool_handle_t hPool) noexcept {
        return std::any_of(
            descToPoolMap.begin(), descToPoolMap.end(),
            [&hPool](const auto &pair) { return hPool == pair.second.get(); });
    }
};

} // namespace usm

namespace std {
/// @brief hash specialization for usm::pool_descriptor
template <> struct hash<usm::pool_descriptor> {
    inline size_t operator()(const usm::pool_descriptor &desc) const {
        ur_native_handle_t native = nullptr;
        if (desc.hDevice) {
            auto ret = urDeviceGetNativeHandle(desc.hDevice, &native);
            if (ret != UR_RESULT_SUCCESS) {
                throw ret;
            }
        }

        return combine_hashes(
            0, desc.type, native, desc.allocLocation.has_value(),
            desc.allocLocation.has_value() ? desc.allocLocation.value().location
                                           : 0,
            desc.deviceFlags, desc.hostFlags, desc.poolHandle);
    }
};

} // namespace std

#endif /* USM_POOL_MANAGER_HPP */
