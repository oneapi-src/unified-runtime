/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include <uma/memory_provider.h>

#include <assert.h>
#include <stdlib.h>

struct uma_memory_provider_handle_t_ {
    struct uma_memory_provider_ops_t ops;
    uma_memory_provider_native_handle_t native_handle;
};

enum uma_result_t
umaMemoryProviderCreate(struct uma_memory_provider_ops_t *ops, void *params,
                        uma_memory_provider_handle_t *hProvider) {
    uma_memory_provider_handle_t provider =
        malloc(sizeof(struct uma_memory_provider_handle_t_));
    if (!provider) {
        return UMA_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    assert(ops->version == UMA_VERSION_CURRENT);

    provider->ops = *ops;

    uma_memory_provider_native_handle_t native_handle;
    enum uma_result_t ret = ops->initialize(params, &native_handle);
    if (ret != UMA_RESULT_SUCCESS) {
        free(provider);
        return ret;
    }

    provider->native_handle = native_handle;

    *hProvider = provider;

    return UMA_RESULT_SUCCESS;
}

void umaMemoryProviderDestroy(uma_memory_provider_handle_t hProvider) {
    hProvider->ops.finalize(hProvider->native_handle);
    free(hProvider);
}

enum uma_result_t umaMemoryProviderAlloc(uma_memory_provider_handle_t hProvider,
                                         size_t size, size_t alignment,
                                         void **ptr) {
    return hProvider->ops.alloc(hProvider->native_handle, size, alignment, ptr);
}

enum uma_result_t umaMemoryProviderFree(uma_memory_provider_handle_t hProvider,
                                        void *ptr, size_t size) {
    return hProvider->ops.free(hProvider->native_handle, ptr, size);
}

enum uma_result_t
umaMemoryProviderGetLastResult(uma_memory_provider_handle_t hProvider,
                               const char **ppMessage) {
    return hProvider->ops.get_last_result(hProvider->native_handle, ppMessage);
}

uma_memory_provider_native_handle_t
umaProviderGetNativeHandle(uma_memory_provider_handle_t hProvider) {
    return hProvider->native_handle;
}

enum uma_result_t
umaMemoryProviderGetRecommendedPageSize(uma_memory_provider_handle_t hProvider,
                                        size_t size, size_t *pageSize) {
    return hProvider->ops.get_recommended_page_size(hProvider->native_handle,
                                                    size, pageSize);
}

enum uma_result_t
umaMemoryProviderGetMinPageSize(uma_memory_provider_handle_t hProvider,
                                void *ptr, size_t *pageSize) {
    return hProvider->ops.get_min_page_size(hProvider->native_handle, ptr,
                                            pageSize);
}

enum uma_result_t
umaMemoryProviderPurgeLazy(uma_memory_provider_handle_t hProvider, void *ptr,
                           size_t size) {
    return hProvider->ops.purge_lazy(hProvider->native_handle, ptr, size);
}

enum uma_result_t
umaMemoryProviderPurgeForce(uma_memory_provider_handle_t hProvider, void *ptr,
                            size_t size) {
    return hProvider->ops.purge_force(hProvider->native_handle, ptr, size);
}
