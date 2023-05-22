/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef USM_POOL_MANAGER_HPP
#define USM_POOL_MANAGER_HPP 1

#include "logger/ur_logger.hpp"
#include "uma_helpers.hpp"
#include "ur_api.h"
#include "ur_util.hpp"

#include <uma/memory_pool.h>
#include <uma/memory_provider.h>

#include <functional>
#include <vector>

namespace usm {

/// @brief describes an internal USM pool instance.
struct pool_descriptor {
    ur_usm_pool_handle_t poolHandle;

    ur_context_handle_t hContext;
    ur_device_handle_t hDevice;
    ur_usm_type_t type;
    bool deviceReadOnly;

    bool operator==(const pool_descriptor &other) const;
    friend std::ostream &operator<<(std::ostream &os,
                                    const pool_descriptor &desc);
    static std::size_t hash(const pool_descriptor &desc);
    static std::pair<ur_result_t, std::vector<pool_descriptor>>
    create(ur_usm_pool_handle_t poolHandle, ur_context_handle_t hContext);
};

static inline std::pair<ur_result_t, std::vector<ur_device_handle_t>>
urGetSubDevices(ur_device_handle_t hDevice) {
    size_t nComputeUnits;
    auto ret = urDeviceGetInfo(hDevice, UR_DEVICE_INFO_MAX_COMPUTE_UNITS,
                               sizeof(nComputeUnits), &nComputeUnits, nullptr);
    if (ret != UR_RESULT_SUCCESS) {
        return {ret, {}};
    }

    ur_device_partition_property_t properties[] = {
        UR_DEVICE_PARTITION_EQUALLY,
        static_cast<ur_device_partition_property_t>(nComputeUnits), 0};

    // Get the number of devices that will be created
    uint32_t deviceCount;
    ret = urDevicePartition(hDevice, properties, 0, nullptr, &deviceCount);
    if (ret != UR_RESULT_SUCCESS) {
        return {ret, {}};
    }

    std::vector<ur_device_handle_t> sub_devices(deviceCount);
    ret = urDevicePartition(hDevice, properties,
                            static_cast<uint32_t>(sub_devices.size()),
                            sub_devices.data(), nullptr);
    if (ret != UR_RESULT_SUCCESS) {
        return {ret, {}};
    }

    return {UR_RESULT_SUCCESS, sub_devices};
}

inline std::pair<ur_result_t, std::vector<ur_device_handle_t>>
urGetAllDevicesAndSubDevices(ur_context_handle_t hContext) {
    size_t deviceCount;
    auto ret = urContextGetInfo(hContext, UR_CONTEXT_INFO_NUM_DEVICES,
                                sizeof(deviceCount), &deviceCount, nullptr);
    if (ret != UR_RESULT_SUCCESS) {
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
            return {ret, {}};
        }
    }

    return {UR_RESULT_SUCCESS, devicesAndSubDevices};
}

static inline bool
isSharedAllocationReadOnlyOnDevice(const pool_descriptor &desc) {
    return desc.type == UR_USM_TYPE_SHARED && desc.deviceReadOnly;
}

inline bool pool_descriptor::operator==(const pool_descriptor &other) const {
    const pool_descriptor &lhs = *this;
    const pool_descriptor &rhs = other;
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

    return lhsNative == rhsNative && lhs.type == rhs.type &&
           (isSharedAllocationReadOnlyOnDevice(lhs) ==
            isSharedAllocationReadOnlyOnDevice(rhs)) &&
           lhs.poolHandle == rhs.poolHandle;
}

inline std::size_t pool_descriptor::hash(const pool_descriptor &desc) {
    ur_native_handle_t native;
    auto ret = urDeviceGetNativeHandle(desc.hDevice, &native);
    if (ret != UR_RESULT_SUCCESS) {
        throw ret;
    }

    return combine_hashes(0, desc.type, native,
                          isSharedAllocationReadOnlyOnDevice(desc),
                          desc.poolHandle);
}

inline std::ostream &operator<<(std::ostream &os, const pool_descriptor &desc) {
    os << "pool handle: " << desc.poolHandle
       << " context handle: " << desc.hContext
       << " device handle: " << desc.hDevice << " memory type: " << desc.type
       << " is read only: " << desc.deviceReadOnly;
    return os;
}

inline std::pair<ur_result_t, std::vector<pool_descriptor>>
pool_descriptor::create(ur_usm_pool_handle_t poolHandle,
                        ur_context_handle_t hContext) {
    auto [ret, devices] = urGetAllDevicesAndSubDevices(hContext);
    if (ret != UR_RESULT_SUCCESS) {
        return {ret, {}};
    }

    std::vector<pool_descriptor> descriptors;
    pool_descriptor &desc = descriptors.emplace_back();
    desc.poolHandle = poolHandle;
    desc.hContext = hContext;
    desc.type = UR_USM_TYPE_HOST;

    for (auto &device : devices) {
        {
            pool_descriptor &desc = descriptors.emplace_back();
            desc.poolHandle = poolHandle;
            desc.hContext = hContext;
            desc.type = UR_USM_TYPE_DEVICE;
        }
        {
            pool_descriptor &desc = descriptors.emplace_back();
            desc.poolHandle = poolHandle;
            desc.hContext = hContext;
            desc.type = UR_USM_TYPE_SHARED;
            desc.hDevice = device;
            desc.deviceReadOnly = false;
        }
        {
            pool_descriptor &desc = descriptors.emplace_back();
            desc.poolHandle = poolHandle;
            desc.hContext = hContext;
            desc.type = UR_USM_TYPE_SHARED;
            desc.hDevice = device;
            desc.deviceReadOnly = true;
        }
    }

    return {ret, descriptors};
}

struct pool_config {
    // TODO: Config influenced by env variables
    // dimensions[memType] = {allocMax(MB), capacity, poolSize(MB)}
    std::map<int32_t, std::vector<int32_t>> dimensions = {
        {UR_USM_TYPE_DEVICE, {1, 4, 256}},
        {UR_USM_TYPE_HOST, {1, 4, 256}},
        {UR_USM_TYPE_SHARED, {8, 4, 256}}};
} PoolConfig;

struct proxy_pool {
    struct block {
        // Base address of this block
        uintptr_t base = 0;
        // Size of the block
        size_t size = 0;
        // Supported allocation size by this block
        size_t chunkSize = 0;
        // Total number of slots
        uint32_t numSlots = 0;
        // Number of slots in use
        uint32_t numUsedSlots = 0;
        // Cached available slot returned by the last dealloc() call
        uint32_t freeSlot = UINT32_MAX;
        // Marker for the currently used slots
        std::vector<bool> usedSlots;
        // Used for default initialization
        block(){};

        block(void *base, size_t size, size_t chunkSize)
            : base(reinterpret_cast<uintptr_t>(base)), size(size),
              chunkSize(chunkSize) {
            numSlots = size / chunkSize;
            numUsedSlots = 0;
            usedSlots.resize(numSlots, false);
        }

        // Check if the current block is fully used
        bool isFull() { return numUsedSlots == numSlots; }

        // Check if the given address belongs to the current block
        bool contains(void *ptr) {
            auto mem = reinterpret_cast<uintptr_t>(ptr);
            return mem >= base && mem < base + size;
        }

        // Allocate a single chunk from the block
        void *alloc() {
            if (isFull()) {
                return nullptr;
            }

            if (freeSlot != UINT32_MAX) {
                uint32_t slot = freeSlot;
                freeSlot = UINT32_MAX;
                usedSlots[slot] = true;
                numUsedSlots++;
                return reinterpret_cast<void *>(base + slot * chunkSize);
            }

            for (size_t i = 0; i < numSlots; i++) {
                if (usedSlots[i]) {
                    continue;
                }
                usedSlots[i] = true;
                numUsedSlots++;
                return reinterpret_cast<void *>(base + i * chunkSize);
            }

            // Should not reach here
            assert(0 && "Inconsistent memory pool state");
            return nullptr;
        }

        // Deallocate the memory at given address
        void dealloc(void *ptr) {
            if (!contains(ptr)) {
                assert(0 && "Inconsistent memory pool state");
            }
            uint32_t slot =
                (reinterpret_cast<uintptr_t>(ptr) - base) / chunkSize;
            usedSlots[slot] = false;
            numUsedSlots--;
            freeSlot = slot;
        }
    }; // block

    uma_result_t initialize(uma_memory_provider_handle_t providers[],
                            size_t numProviders,
                            const pool_descriptor &desc) noexcept {
        provider = providers[0];

        auto memType = desc.type;
        auto configAllocMax = PoolConfig.dimensions[memType][0];
        auto configCapacity = PoolConfig.dimensions[memType][1];
        auto configPoolSize = PoolConfig.dimensions[memType][2];

        blockCapacity = configCapacity;
        poolSizeMax = configPoolSize << 20; // MB to B
        poolSize = 0;

        // MB to B and round up to power of 2
        allocMax = allocMin << getBucketId(configAllocMax * (1 << 20));

        // Make bucket for each allocation size between allocMin and allocMax
        auto minId = getBucketId(allocMin);
        auto maxId = getBucketId(allocMax);
        buckets.resize(maxId - minId + 1);

        // Set bucket parameters
        for (size_t i = 0; i < buckets.size(); i++) {
            size_t chunkSize = allocMin << i;
            size_t blockSize = chunkSize * blockCapacity;
            if (blockSize <= allocUnit) {
                blockSize =
                    allocUnit; // Allocation unit is already large enough
            }

            bucketParams.emplace_back(chunkSize, blockSize);
        }

        // Populate each bucket with one memory block
        for (size_t i = 0; i < buckets.size(); i++) {
            auto [ret, _] = populateBucket(i);
            if (ret) {
                return ret;
            }
        }

        return UMA_RESULT_SUCCESS;
    }

    void finalize() {
        for (auto &bucket : buckets) {
            auto ret = clearBucket(bucket);
            assert(ret == UMA_RESULT_SUCCESS &&
                   "Inconsistent memory pool state");
        }
    }

    std::pair<uma_result_t, std::optional<block *>>
    populateBucket(uint32_t bucketId) {
        auto &bucket = buckets[bucketId];
        auto chunkSize = bucketParams[bucketId].first;
        auto blockSize = bucketParams[bucketId].second;

        void *base;
        // TODO: Initialize specific memory type (host, device, shared)
        auto ret = umaMemoryProviderAlloc(provider, blockSize, 0, &base);
        if (ret) {
            return {ret, std::nullopt};
        }

        auto &block = bucket.emplace_back(base, blockSize, chunkSize);
        return {UMA_RESULT_SUCCESS, &block};
    }

    uma_result_t clearBucket(std::vector<block> &bucket) {
        for (auto &block : bucket) {
            auto blockBase = reinterpret_cast<void *>(block.base);
            auto blockSize = block.size;
            auto ret = umaMemoryProviderFree(provider, blockBase, blockSize);
            if (ret) {
                return ret;
            }
        }

        return UMA_RESULT_SUCCESS;
    }

    // Get bucket ID from the specified allocation size
    uint32_t getBucketId(size_t size) {
        uint32_t count = 0;
        for (size_t allocSz = allocMin; allocSz < size; count++) {
            allocSz <<= 1;
        }

        return count;
    }

    void *malloc(size_t size) noexcept { return aligned_malloc(size, 0); }

    void *calloc(size_t num, size_t size) noexcept {
        auto mem = malloc(num * size);
        // TODO: Zero initialize memory in memory provider specific way
        memset(mem, 0, num * size);
        return mem;
    }

    void *realloc(void *ptr, size_t size) noexcept {
        if (ptr && ptrToBlock.count(ptr) == 0) {
            return nullptr;
        }

        auto mem = malloc(size);
        if (!ptr) {
            return mem;
        }

        // TODO: Use UMA data movement API
        memmove(mem, ptr, ptrToBlock[ptr]->chunkSize);
        free(ptr);

        return mem;
    }

    void *aligned_malloc(size_t size, size_t alignment) noexcept {
        // TODO: Use alignment
        uint32_t bucketId = getBucketId(size);
        auto &bucket = buckets[bucketId];

        void *mem = nullptr;
        for (auto &block : bucket) {
            if (block.isFull()) {
                continue;
            }

            mem = block.alloc();
            ptrToBlock.emplace(mem, &block);
        }

        auto isFull = (poolSize >= poolSizeMax);
        if (mem == nullptr && !isFull) {
            auto [ret, blockOpt] = populateBucket(bucketId);
            if (ret || !blockOpt.has_value()) {
                return nullptr;
            }

            mem = blockOpt.value()->alloc();
        }

        return mem;
    }

    size_t malloc_usable_size(void *ptr) noexcept {
        if (ptrToBlock.count(ptr) == 0) {
            return 0;
        }

        return ptrToBlock[ptr]->chunkSize;
    }

    void free(void *ptr) noexcept {
        if (ptrToBlock.count(ptr) == 0) {
            return;
        }

        ptrToBlock[ptr]->dealloc(ptr);
        ptrToBlock.erase(ptr);
        // TODO: Return free'd size?
    }

    enum uma_result_t get_last_result(const char **ppMessage) noexcept {
        // TODO: Not supported
        return UMA_RESULT_ERROR_NOT_SUPPORTED;
    }

    // Memory provider tied to the pool
    uma_memory_provider_handle_t provider = 0;
    // Minimum supported memory allocation size from pool
    size_t allocMin = 1 << 6; // 64B
    // Maximum supported memory allocation size from pool
    size_t allocMax = 0;
    // Allocation size when the pool needs to allocate a block
    size_t allocUnit = 1 << 16; // 64KB
    // Capacity of each block in the buckets which decides number of
    // allocatable chunks from the block. Each block in the bucket can serve
    // at least BlockCapacity chunks.
    // If chunkSize * blockCapacity <= allocUnit
    //   blockSize = allocUnit
    // Otherwise,
    //   blockSize = chunkSize * blockCapacity
    // This simply means how much memory is over-allocated
    uint32_t blockCapacity = 0;
    // Total memory allocated for this pool
    size_t poolSize = 0;
    // Maximum allowed pool size
    size_t poolSizeMax = 0;
    // Buckets with memory blocks
    std::vector<std::vector<block>> buckets;
    // List of bucket parameters
    std::vector<std::pair<size_t, size_t>> bucketParams;
    // Map from allocated pointer to corresponding block.
    std::unordered_map<void *, block *> ptrToBlock;
}; // proxy_pool

template <typename D> struct pool_manager {
  private:
    std::unordered_map<D, uma::pool_unique_handle_t, decltype(&D::hash)>
        descToPoolMap;

    std::optional<uma_memory_pool_handle_t> getPool(const D &desc) noexcept {
        auto it = descToPoolMap.find(desc);
        if (it == descToPoolMap.end()) {
            logger::error("Pool descriptor doesn't match any existing pool: {}",
                          desc);
            return std::nullopt;
        }

        return it->second.get();
    }

  public:
    std::pair<uma_result_t, std::optional<pool_manager>>
    create(std::vector<std::pair<D, uma_memory_provider_handle_t>>
               descHandlePairs) {
        pool_manager<D> poolManager;
        for (auto &[desc, memProvider] : descHandlePairs) {
            auto [ret, hPool] =
                uma::poolMakeUnique<proxy_pool>(&memProvider, 1, desc);
            if (ret != UMA_RESULT_SUCCESS) {
                logger::error("Failed to create a pool from memory provider: "
                              "{}, for pool descriptor: {}",
                              memProvider, desc);
                return {ret, std::nullopt};
            }

            poolManager.descToPoolMap.emplace(desc, std::move(hPool));
        }

        return {UMA_RESULT_SUCCESS, std::move(poolManager)};
    }

    uma_result_t addPool(const D &desc,
                         uma_memory_pool_handle_t hPool) noexcept {
        if (descToPoolMap.count(desc) != 0) {
            logger::error("Pool for pool descriptor: {}, already exists", desc);
            return UMA_RESULT_ERROR_INVALID_ARGUMENT;
        }

        descToPoolMap.emplace(desc, std::move(hPool));

        return UMA_RESULT_SUCCESS;
    }

    void *alloc(D desc, size_t size) noexcept {
        auto poolHandleOpt = getPool(desc);
        if (poolHandleOpt.has_value()) {
            return umaPoolMalloc(poolHandleOpt.value(), size);
        }

        return nullptr;
    }

    void dealloc(D desc, void *ptr) noexcept {
        auto poolHandleOpt = getPool(desc);
        if (poolHandleOpt.has_value()) {
            umaPoolFree(poolHandleOpt.value(), ptr);
        }
    }
};

} // namespace usm

#endif /* USM_POOL_MANAGER_HPP */
