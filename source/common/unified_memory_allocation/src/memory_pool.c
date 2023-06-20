/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include "memory_provider_internal.h"
#include "memory_tracker.h"

#include <uma/memory_pool.h>
#include <uma/memory_pool_ops.h>

#include <assert.h>
#include <stdlib.h>

struct uma_memory_pool_t {
    void *pool_priv;
    struct uma_memory_pool_ops_t ops;

    // All providers are wrapped by memory tracking providers (owned and released by UMA).
    uma_memory_provider_handle_t data_provider;
    uma_memory_provider_handle_t metadata_provider;

    size_t numProviders;
};

enum uma_result_t umaPoolCreate(struct uma_memory_pool_ops_t *ops,
                                uma_memory_provider_handle_t data_provider,
                                uma_memory_provider_handle_t metadata_provider,
                                void *params, uma_memory_pool_handle_t *hPool) {
    if (!data_provider) {
        return UMA_RESULT_ERROR_INVALID_ARGUMENT;
    }

    enum uma_result_t ret = UMA_RESULT_SUCCESS;
    uma_memory_pool_handle_t pool = malloc(sizeof(struct uma_memory_pool_t));
    if (!pool) {
        return UMA_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    assert(ops->version == UMA_VERSION_CURRENT);

    // Wrap each provider with memory tracking provider.
    ret = umaTrackingMemoryProviderCreate(data_provider, pool,
                                          &pool->data_provider);
    if (ret != UMA_RESULT_SUCCESS) {
        goto err_providers_wrap;
    }

    if (metadata_provider) {
        ret = umaTrackingMemoryProviderCreate(metadata_provider, pool,
                                              &pool->metadata_provider);
        if (ret != UMA_RESULT_SUCCESS) {
            goto err_providers_metadata_wrap;
        }
    } else {
        pool->metadata_provider = NULL;
    }

    pool->ops = *ops;
    ret = ops->initialize(pool->data_provider, pool->metadata_provider, params,
                          &pool->pool_priv);
    if (ret != UMA_RESULT_SUCCESS) {
        goto err_pool_init;
    }

    *hPool = pool;
    return UMA_RESULT_SUCCESS;

err_pool_init:
    if (pool->metadata_provider) {
        umaMemoryProviderDestroy(pool->metadata_provider);
    }
err_providers_metadata_wrap:
    umaMemoryProviderDestroy(pool->data_provider);
err_providers_wrap:
    free(pool);

    return ret;
}

void umaPoolDestroy(uma_memory_pool_handle_t hPool) {
    hPool->ops.finalize(hPool->pool_priv);
    if (hPool->metadata_provider) {
        umaMemoryProviderDestroy(hPool->metadata_provider);
    }
    umaMemoryProviderDestroy(hPool->data_provider);
    free(hPool);
}

void *umaPoolMalloc(uma_memory_pool_handle_t hPool, size_t size) {
    return hPool->ops.malloc(hPool->pool_priv, size);
}

void *umaPoolAlignedMalloc(uma_memory_pool_handle_t hPool, size_t size,
                           size_t alignment) {
    return hPool->ops.aligned_malloc(hPool->pool_priv, size, alignment);
}

void *umaPoolCalloc(uma_memory_pool_handle_t hPool, size_t num, size_t size) {
    return hPool->ops.calloc(hPool->pool_priv, num, size);
}

void *umaPoolRealloc(uma_memory_pool_handle_t hPool, void *ptr, size_t size) {
    return hPool->ops.realloc(hPool->pool_priv, ptr, size);
}

size_t umaPoolMallocUsableSize(uma_memory_pool_handle_t hPool, void *ptr) {
    return hPool->ops.malloc_usable_size(hPool->pool_priv, ptr);
}

void umaPoolFree(uma_memory_pool_handle_t hPool, void *ptr) {
    hPool->ops.free(hPool->pool_priv, ptr);
}

void umaFree(void *ptr) {
    uma_memory_pool_handle_t hPool = umaPoolByPtr(ptr);
    if (hPool) {
        umaPoolFree(hPool, ptr);
    }
}

enum uma_result_t umaPoolGetLastResult(uma_memory_pool_handle_t hPool,
                                       const char **ppMessage) {
    return hPool->ops.get_last_result(hPool->pool_priv, ppMessage);
}

uma_memory_pool_handle_t umaPoolByPtr(const void *ptr) {
    return umaMemoryTrackerGetPool(umaMemoryTrackerGet(), ptr);
}

uma_memory_provider_handle_t
umaPoolGetMetadataMemoryProvider(uma_memory_pool_handle_t hPool) {
    return hPool->ops.get_metadata_memory_provider(hPool->pool_priv);
}

uma_memory_provider_handle_t
umaPoolGetDataMemoryProvider(uma_memory_pool_handle_t hPool) {
    return hPool->ops.get_data_memory_provider(hPool->pool_priv);
}
