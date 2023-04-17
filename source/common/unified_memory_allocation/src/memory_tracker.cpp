/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include "memory_tracker.h"
#include <uma/memory_provider.h>
#include <uma/memory_provider_ops.h>

#include <cassert>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <stdlib.h>

// TODO: reimplement in C and optimize...
struct uma_memory_tracker_t {
    enum uma_result_t add(void *pool, const void *ptr, size_t size) {
        std::unique_lock<std::shared_mutex> lock(mtx);

        if (size == 0) {
            return UMA_RESULT_SUCCESS;
        }

        auto ret =
            map.try_emplace(reinterpret_cast<uintptr_t>(ptr), size, pool);
        return ret.second ? UMA_RESULT_SUCCESS : UMA_RESULT_ERROR_UNKNOWN;
    }

    enum uma_result_t remove(const void *ptr, size_t size) {
        std::unique_lock<std::shared_mutex> lock(mtx);

        map.erase(reinterpret_cast<uintptr_t>(ptr));

        // TODO: handle removing part of the range
        (void)size;

        return UMA_RESULT_SUCCESS;
    }

    void *find(const void *ptr) {
        std::shared_lock<std::shared_mutex> lock(mtx);

        auto intptr = reinterpret_cast<uintptr_t>(ptr);
        auto it = map.upper_bound(intptr);
        if (it == map.begin()) {
            return nullptr;
        }

        --it;

        auto address = it->first;
        auto size = it->second.first;
        auto pool = it->second.second;

        if (intptr >= address && intptr < address + size) {
            return pool;
        }

        return nullptr;
    }

  private:
    std::shared_mutex mtx;
    std::map<uintptr_t, std::pair<size_t, void *>> map;
};

static enum uma_result_t
umaMemoryTrackerAdd(uma_memory_tracker_handle_t hTracker, void *pool,
                    const void *ptr, size_t size) {
    return hTracker->add(pool, ptr, size);
}

static enum uma_result_t
umaMemoryTrackerRemove(uma_memory_tracker_handle_t hTracker, const void *ptr,
                       size_t size) {
    return hTracker->remove(ptr, size);
}

extern "C" {

uma_memory_tracker_handle_t umaMemoryTrackerGet(void) {
    static uma_memory_tracker_t tracker;
    return &tracker;
}

void *umaMemoryTrackerGetPool(uma_memory_tracker_handle_t hTracker,
                              const void *ptr) {
    return hTracker->find(ptr);
}

struct uma_memory_provider_t {
    uma_memory_provider_handle_t hUpstream;
    uma_memory_tracker_handle_t hTracker;
    uma_memory_pool_handle_t pool;
};

static enum uma_result_t
trackingAlloc(uma_memory_provider_native_handle_t hProvider, size_t size,
              size_t alignment, void **ptr) {
    enum uma_result_t ret = UMA_RESULT_SUCCESS;

    ret = umaMemoryProviderAlloc(hProvider->hUpstream, size, alignment, ptr);
    if (ret != UMA_RESULT_SUCCESS) {
        return ret;
    }

    ret = umaMemoryTrackerAdd(hProvider->hTracker, hProvider->pool, *ptr, size);
    if (ret != UMA_RESULT_SUCCESS && hProvider->hUpstream) {
        if (umaMemoryProviderFree(hProvider->hUpstream, *ptr, size)) {
            // TODO: LOG
        }
    }

    return ret;
}

static enum uma_result_t
trackingFree(uma_memory_provider_native_handle_t hProvider, void *ptr,
             size_t size) {
    enum uma_result_t ret;

    // umaMemoryTrackerRemove should be called before umaMemoryProviderFree
    // to avoid a race condition. If the order would be different, other thread
    // could allocate the memory at address `ptr` before a call to umaMemoryTrackerRemove
    // resulting in inconsistent state.
    ret = umaMemoryTrackerRemove(hProvider->hTracker, ptr, size);
    if (ret != UMA_RESULT_SUCCESS) {
        return ret;
    }

    ret = umaMemoryProviderFree(hProvider->hUpstream, ptr, size);
    if (ret != UMA_RESULT_SUCCESS) {
        if (umaMemoryTrackerAdd(hProvider->hTracker, hProvider->pool, ptr,
                                size) != UMA_RESULT_SUCCESS) {
            // TODO: LOG
        }
        return ret;
    }

    return ret;
}

static enum uma_result_t
trackingInitialize(void *params, uma_memory_provider_native_handle_t *ret) {
    uma_memory_provider_native_handle_t provider =
        (uma_memory_provider_t *)malloc(sizeof(uma_memory_provider_t));
    if (!provider) {
        return UMA_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    *provider = *((uma_memory_provider_t *)params);
    *ret = provider;
    return UMA_RESULT_SUCCESS;
}

static void trackingFinalize(uma_memory_provider_native_handle_t provider) {
    free(provider);
}

static enum uma_result_t
trackingGetLastResult(uma_memory_provider_native_handle_t hProvider,
                      const char **msg) {
    return umaMemoryProviderGetLastResult(hProvider->hUpstream, msg);
}

static enum uma_result_t
trackingGetRecommendedPageSize(uma_memory_provider_native_handle_t hProvider,
                               size_t size, size_t *pageSize) {
    return umaMemoryProviderGetRecommendedPageSize(hProvider->hUpstream, size,
                                                   pageSize);
}

static enum uma_result_t
trackingGetMinPageSize(uma_memory_provider_native_handle_t hProvider, void *ptr,
                       size_t *pageSize) {
    return umaMemoryProviderGetMinPageSize(hProvider->hUpstream, ptr, pageSize);
}

static enum uma_result_t
trackingPurgeLazy(uma_memory_provider_native_handle_t hProvider, void *ptr,
                  size_t size) {
    return umaMemoryProviderPurgeLazy(hProvider->hUpstream, ptr, size);
}

static enum uma_result_t
trackingPurgeForce(uma_memory_provider_native_handle_t hProvider, void *ptr,
                   size_t size) {
    return umaMemoryProviderPurgeForce(hProvider->hUpstream, ptr, size);
}

enum uma_result_t umaTrackingMemoryProviderCreate(
    uma_memory_provider_handle_t hUpstream, uma_memory_pool_handle_t hPool,
    uma_memory_provider_handle_t *hTrackingProvider) {
    uma_memory_provider_t params;
    params.hUpstream = hUpstream;
    params.hTracker = umaMemoryTrackerGet();
    params.pool = hPool;

    struct uma_memory_provider_ops_t trackingMemoryProviderOps;
    trackingMemoryProviderOps.version = UMA_VERSION_CURRENT;
    trackingMemoryProviderOps.initialize = trackingInitialize;
    trackingMemoryProviderOps.finalize = trackingFinalize;
    trackingMemoryProviderOps.alloc = trackingAlloc;
    trackingMemoryProviderOps.free = trackingFree;
    trackingMemoryProviderOps.get_last_result = trackingGetLastResult;
    trackingMemoryProviderOps.get_min_page_size = trackingGetMinPageSize;
    trackingMemoryProviderOps.get_recommended_page_size =
        trackingGetRecommendedPageSize;
    trackingMemoryProviderOps.purge_force = trackingPurgeForce;
    trackingMemoryProviderOps.purge_lazy = trackingPurgeLazy;

    return umaMemoryProviderCreate(&trackingMemoryProviderOps, &params,
                                   hTrackingProvider);
}

void umaTrackingMemoryProviderGetUpstreamProvider(
    uma_memory_provider_native_handle_t hTrackingProvider,
    uma_memory_provider_handle_t *hUpstream) {
    assert(hUpstream);
    *hUpstream = hTrackingProvider->hUpstream;
}
}
