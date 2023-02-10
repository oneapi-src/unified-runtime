/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef UR_UMA_HELPERS_H
#define UR_UMA_HELPERS_H 1

#include <uma/memory_pool.h>
#include <uma/memory_pool_ops.h>
#include <uma/memory_provider.h>
#include <uma/memory_provider_ops.h>

#include <stdexcept>
#include <tuple>
#include <utility>

#include <iostream>

namespace uma {
using ur_memory_provider_handle_unique =
    std::unique_ptr<uma_memory_provider_t, decltype(&umaMemoryProviderDestroy)>;
using ur_memory_pool_handle_unique =
    std::unique_ptr<uma_memory_pool_t, decltype(&umaPoolDestroy)>;

// C++14 version of std::make_from_tuple (but uses operator new)
template <typename T, typename Tuple, size_t... I>
auto createFromTuple(Tuple &&t, std::index_sequence<I...>) {
    return new T(std::get<I>(std::forward<Tuple>(t))...);
}
template <typename T, typename Tuple>
auto createFromTuple(Tuple &&t) {
    return createFromTuple<T>(
        std::forward<Tuple>(t),
        std::make_index_sequence<std::tuple_size<Tuple>::value>{});
}

/// @brief creates UMA memory provider based on given MemProvider type.
/// MemProvider should implement all functions defined by
/// uma_memory_provider_ops_t, except for intialize and finalize (those are
/// replaced by ctor and dtor). All arguments passed to this function are
/// forwarded to MemProvider ctor.
template <typename MemProvider, typename... Args>
auto memoryProviderMakeUnique(Args &&...args) {
    struct uma_memory_provider_ops_t ops;
    ops.version = UMA_VERSION_CURRENT;
    ops.initialize = [](void *params, void **provider) {
        try {
            auto *tuple = reinterpret_cast<std::tuple<Args...> *>(params);
            *provider = createFromTuple<MemProvider>(std::move(*tuple));
            return UMA_RESULT_SUCCESS;
        } catch (...) {
            return UMA_RESULT_ERROR_UNKNOWN;
        }
    };
    ops.finalize = [](void *provider) {
        delete reinterpret_cast<MemProvider *>(provider);
    };
    ops.alloc = [](void *provider, size_t size, size_t alignment, void **ptr) {
        try {
            return reinterpret_cast<MemProvider *>(provider)->alloc(
                size, alignment, ptr);
        } catch (...) {
            return UMA_RESULT_ERROR_UNKNOWN;
        }
    };
    ops.free = [](void *provider, void *ptr, size_t size) {
        try {
            return reinterpret_cast<MemProvider *>(provider)->free(ptr, size);
        } catch (...) {
            return UMA_RESULT_ERROR_UNKNOWN;
        }
    };
    ops.get_last_result = [](void *provider, const char **ppMessage) {
        try {
            return reinterpret_cast<MemProvider *>(provider)->get_last_result(
                ppMessage);
        } catch (...) {
            return UMA_RESULT_ERROR_UNKNOWN;
        }
    };

    auto tuple = std::make_tuple(std::forward<Args>(args)...);
    uma_memory_provider_handle_t hProvider;
    auto ret = umaMemoryProviderCreate(&ops, reinterpret_cast<void *>(&tuple),
                                       &hProvider);

    if (ret != UMA_RESULT_SUCCESS) {
        throw std::runtime_error("umaMemoryProviderCreate failed");
    }

    return ur_memory_provider_handle_unique(hProvider,
                                            &umaMemoryProviderDestroy);
}

/// @brief creates UMA memory pool based on given MemPool type.
/// MemPool should implement all functions defined by
/// uma_memory_pool_ops_t, except for intialize and finalize (those are
/// replaced by ctor and dtor). All arguments passed to this function are
/// forwarded to MemProvider ctor.
template <typename MemPool, typename... Args>
auto poolMakeUnique(Args &&...args) {
    auto tuple = std::make_tuple(std::forward<Args>(args)...);

    struct uma_memory_pool_ops_t ops;
    ops.version = UMA_VERSION_CURRENT;
    ops.initialize = [](void *params, void **pool) {
        try {
            auto tuplePtr = reinterpret_cast<decltype(tuple) *>(params);
            *pool = createFromTuple<MemPool>(std::move(*tuplePtr));
            return UMA_RESULT_SUCCESS;
        } catch (...) {
            return UMA_RESULT_ERROR_UNKNOWN;
        }
    };
    ops.finalize = [](void *pool) { delete reinterpret_cast<MemPool *>(pool); };
    ops.malloc = [](void *pool, size_t size) -> void * {
        try {
            return reinterpret_cast<MemPool *>(pool)->malloc(size);
        } catch (...) {
            return nullptr;
        }
    };
    ops.calloc = [](void *pool, size_t num, size_t size) -> void * {
        try {
            return reinterpret_cast<MemPool *>(pool)->calloc(num, size);
        } catch (...) {
            return nullptr;
        }
    };
    ops.realloc = [](void *pool, void *ptr, size_t size) -> void * {
        try {
            return reinterpret_cast<MemPool *>(pool)->realloc(ptr, size);
        } catch (...) {
            return nullptr;
        }
    };
    ops.aligned_malloc = [](void *pool, size_t size,
                            size_t alignment) -> void * {
        try {
            return reinterpret_cast<MemPool *>(pool)->aligned_malloc(size,
                                                                     alignment);
        } catch (...) {
            return nullptr;
        }
    };
    ops.malloc_usable_size = [](void *pool, void *ptr) -> size_t {
        try {
            return reinterpret_cast<MemPool *>(pool)->malloc_usable_size(ptr);
        } catch (...) {
            // TODO: log error
            return 0;
        }
    };
    ops.free = [](void *pool, void *ptr) {
        try {
            return reinterpret_cast<MemPool *>(pool)->free(ptr);
        } catch (...) {
            // TODO: log error
        }
    };
    ops.get_last_result = [](void *pool, const char **ppMessage) {
        try {
            return reinterpret_cast<MemPool *>(pool)->get_last_result(
                ppMessage);
        } catch (...) {
            return UMA_RESULT_ERROR_UNKNOWN;
        }
    };

    uma_memory_pool_handle_t hPool;
    auto ret = umaPoolCreate(&ops, reinterpret_cast<void *>(&tuple), &hPool);

    if (ret != UMA_RESULT_SUCCESS) {
        throw std::runtime_error("umaPoolCreate failed");
    }

    return ur_memory_pool_handle_unique(hPool, &umaPoolDestroy);
}
} // namespace uma

#endif /* UR_UMA_HELPERS_H */
