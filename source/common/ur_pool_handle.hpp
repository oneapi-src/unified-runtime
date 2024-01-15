/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include "disjoint_pool.hpp"
#include "ur_pool_manager.hpp"
#include <umf_pools/disjoint_pool_config_parser.hpp>

namespace usm {

template <class ConfigType, class HostProvider, class DeviceProvider,
          class SharedProvider, class... Args>
std::pair<ur_result_t, umf::pool_unique_handle_t>
createUMFPoolForDesc(usm::pool_descriptor &Desc, Args &&...args) {
    umf_result_t UmfRet = UMF_RESULT_SUCCESS;
    umf::provider_unique_handle_t MemProvider = nullptr;

    switch (Desc.type) {
    case UR_USM_TYPE_HOST: {
        std::tie(UmfRet, MemProvider) =
            umf::memoryProviderMakeUnique<HostProvider>(Desc);
        break;
    }
    case UR_USM_TYPE_DEVICE: {
        std::tie(UmfRet, MemProvider) =
            umf::memoryProviderMakeUnique<DeviceProvider>(Desc);
        break;
    }
    case UR_USM_TYPE_SHARED: {
        std::tie(UmfRet, MemProvider) =
            umf::memoryProviderMakeUnique<SharedProvider>(Desc);
        break;
    }
    default:
        UmfRet = UMF_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (UmfRet) {
        return std::pair<ur_result_t, umf::pool_unique_handle_t>{
            umf::umf2urResult(UmfRet), nullptr};
    }

    umf::pool_unique_handle_t Pool = nullptr;
    std::tie(UmfRet, Pool) =
        umf::poolMakeUnique<ConfigType, 1>({std::move(MemProvider)}, args...);

    return std::pair<ur_result_t, umf::pool_unique_handle_t>{
        umf::umf2urResult(UmfRet), std::move(Pool)};
}

template <class HostProvider, class DeviceProvider, class SharedProvider>
struct pool_handle_base {
    pool_handle_base(ur_context_handle_t Context, ur_usm_pool_desc_t *PoolDesc)
        : Context(Context), PoolFlags(PoolDesc->flags) {
        const void *pNext = PoolDesc->pNext;
        while (pNext != nullptr) {
            const ur_base_desc_t *BaseDesc =
                static_cast<const ur_base_desc_t *>(pNext);
            switch (BaseDesc->stype) {
            case UR_STRUCTURE_TYPE_USM_POOL_LIMITS_DESC: {
                const ur_usm_pool_limits_desc_t *Limits =
                    reinterpret_cast<const ur_usm_pool_limits_desc_t *>(
                        BaseDesc);
                for (auto &config : DisjointPoolConfigs.Configs) {
                    config.MaxPoolableSize = Limits->maxPoolableSize;
                    config.SlabMinSize = Limits->minDriverAllocSize;
                }
                break;
            }
            default: {
                throw umf::UsmAllocationException(
                    UR_RESULT_ERROR_INVALID_ARGUMENT);
            }
            }
            pNext = BaseDesc->pNext;
        }

        ur_result_t Ret;
        std::tie(Ret, PoolManager) =
            usm::pool_manager<usm::pool_descriptor>::create();
        if (Ret) {
            throw umf::UsmAllocationException(Ret);
        }

        auto UrUSMPool = reinterpret_cast<ur_usm_pool_handle_t>(this);

        // We set up our pool handle with default pools for each of the three
        // allocation types
        std::vector<usm::pool_descriptor> Descs;
        std::tie(Ret, Descs) =
            usm::pool_descriptor::createDefaults(UrUSMPool, Context);

        for (auto &Desc : Descs) {
            umf::pool_unique_handle_t Pool = nullptr;
            auto PoolType = usm::urTypeToDisjointPoolType(Desc.type);
            std::tie(Ret, Pool) =
                createUMFPoolForDesc<usm::DisjointPool, HostProvider,
                                     DeviceProvider, SharedProvider>(
                    Desc, this->DisjointPoolConfigs.Configs[PoolType]);
            if (Ret) {
                throw umf::UsmAllocationException(Ret);
            }

            PoolManager.addPool(Desc, Pool);
        }
    }

    std::atomic_uint32_t RefCount = 1;
    ur_context_handle_t Context = nullptr;
    usm::DisjointPoolAllConfigs DisjointPoolConfigs =
        usm::DisjointPoolAllConfigs();
    usm::pool_manager<usm::pool_descriptor> PoolManager;
    ur_usm_pool_flags_t PoolFlags;

    uint32_t incrementReferenceCount() noexcept { return ++RefCount; }
    uint32_t decrementReferenceCount() noexcept { return --RefCount; }
    uint32_t getReferenceCount() const noexcept { return RefCount; }

    bool hasUMFPool(umf_memory_pool_t *umf_pool) {
        return PoolManager.hasPool(umf_pool);
    }

    std::optional<umf_memory_pool_handle_t>
    getOrCreatePool(usm::pool_descriptor &desc) {
        auto foundPool = PoolManager.getPool(desc);
        if (foundPool.has_value()) {
            return foundPool.value();
        }

        umf::pool_unique_handle_t newPool;
        ur_result_t Ret = UR_RESULT_SUCCESS;
        auto PoolType = usm::urTypeToDisjointPoolType(desc.type);
        std::tie(Ret, newPool) =
            createUMFPoolForDesc<usm::DisjointPool, HostProvider,
                                 DeviceProvider, SharedProvider>(
                desc, this->DisjointPoolConfigs.Configs[PoolType]);
        if (Ret) {
            throw umf::UsmAllocationException(Ret);
        }

        PoolManager.addPool(desc, newPool);
        // addPool std::moves newPool so we can't just return that
        return PoolManager.getPool(desc);
    }
};
} // namespace usm
