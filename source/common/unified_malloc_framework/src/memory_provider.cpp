/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include <assert.h>
#include <stdlib.h>

#include <cstring>
#include <map>
#include <string>

#include <umf/memory_provider.h>

#include "memory_provider_internal.h"
#include "os_memory_provider.h"

struct umf_memory_provider_t {
    struct umf_memory_provider_ops_t ops;
    void *provider_priv;
};

std::map<std::string, umf_memory_provider_ops_t> globalProviders;

enum umf_result_t
umfMemoryProviderCreate(struct umf_memory_provider_ops_t *ops, void *params,
                        umf_memory_provider_handle_t *hProvider) {
    umf_memory_provider_handle_t provider =
        (umf_memory_provider_t *)malloc(sizeof(struct umf_memory_provider_t));
    if (!provider) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    assert(ops->version == UMF_VERSION_CURRENT);

    provider->ops = *ops;

    void *provider_priv;
    enum umf_result_t ret = ops->initialize(params, &provider_priv);
    if (ret != UMF_RESULT_SUCCESS) {
        free(provider);
        return ret;
    }

    provider->provider_priv = provider_priv;

    *hProvider = provider;

    return UMF_RESULT_SUCCESS;
}

enum umf_result_t umfMemoryProviderRegister(umf_memory_provider_ops_t *ops,
                                            char *name) {

    // TODO improve - use the ops->get_name()
    globalProviders[name] = *ops;

    return UMF_RESULT_SUCCESS;
}

enum umf_result_t umfMemoryProvidersRegisterGetNames(char *providers,
                                                     size_t *numProviders) {
    // TODO improve
    if (globalProviders.count("OS") == 0) {

        umfMemoryProviderRegister(&OS_MEMORY_PROVIDER_OPS,
                                  std::string("OS").data());

        // TODO IMPORTANT
        // as the NUMA (OS) memory provider is the default provider here in the UMF,
        // it should be available (predefined) somehow so a user could use it
        // without any umalloc or UR libs etc
    }

    if (providers == NULL) {
        *numProviders = globalProviders.size();
    } else {
        // get min
        //size_t num = (*numProviders > UMF_MEMORY_PROVIDER_TYPE_NUM)
        //                 ? UMF_MEMORY_PROVIDER_TYPE_NUM
        //                 : *numProviders;

        for (auto p : globalProviders) {
            std::strcat(providers, p.first.c_str());
            std::strcat(providers, ";");
        }
    }

    return UMF_RESULT_SUCCESS;
}

// TODO rename ;)
umf_memory_provider_ops_t umfMemoryProvidersRegisterGetOps(char *name) {
    return globalProviders[name];
}

umf_memory_provider_type_t umfMemoryProvidersRegisterGetType(char *name) {
    return globalProviders[name].type;
}

void umfMemoryProviderDestroy(umf_memory_provider_handle_t hProvider) {
    hProvider->ops.finalize(hProvider->provider_priv);
    free(hProvider);
}

static void
checkErrorAndSetLastProvider(enum umf_result_t result,
                             umf_memory_provider_handle_t hProvider) {
    if (result != UMF_RESULT_SUCCESS) {
        *umfGetLastFailedMemoryProviderPtr() = hProvider;
    }
}

enum umf_result_t umfMemoryProviderAlloc(umf_memory_provider_handle_t hProvider,
                                         size_t size, size_t alignment,
                                         void **ptr) {
    enum umf_result_t res =
        hProvider->ops.alloc(hProvider->provider_priv, size, alignment, ptr);
    checkErrorAndSetLastProvider(res, hProvider);
    return res;
}

enum umf_result_t umfMemoryProviderFree(umf_memory_provider_handle_t hProvider,
                                        void *ptr, size_t size) {
    enum umf_result_t res =
        hProvider->ops.free(hProvider->provider_priv, ptr, size);
    checkErrorAndSetLastProvider(res, hProvider);
    return res;
}

void umfMemoryProviderGetLastNativeError(umf_memory_provider_handle_t hProvider,
                                         const char **ppMessage,
                                         int32_t *pError) {
    hProvider->ops.get_last_native_error(hProvider->provider_priv, ppMessage,
                                         pError);
}

void *umfMemoryProviderGetPriv(umf_memory_provider_handle_t hProvider) {
    return hProvider->provider_priv;
}

enum umf_result_t
umfMemoryProviderGetRecommendedPageSize(umf_memory_provider_handle_t hProvider,
                                        size_t size, size_t *pageSize) {
    enum umf_result_t res = hProvider->ops.get_recommended_page_size(
        hProvider->provider_priv, size, pageSize);
    checkErrorAndSetLastProvider(res, hProvider);
    return res;
}

enum umf_result_t
umfMemoryProviderGetMinPageSize(umf_memory_provider_handle_t hProvider,
                                void *ptr, size_t *pageSize) {
    enum umf_result_t res = hProvider->ops.get_min_page_size(
        hProvider->provider_priv, ptr, pageSize);
    checkErrorAndSetLastProvider(res, hProvider);
    return res;
}

enum umf_result_t
umfMemoryProviderPurgeLazy(umf_memory_provider_handle_t hProvider, void *ptr,
                           size_t size) {
    enum umf_result_t res =
        hProvider->ops.purge_lazy(hProvider->provider_priv, ptr, size);
    checkErrorAndSetLastProvider(res, hProvider);
    return res;
}

enum umf_result_t
umfMemoryProviderPurgeForce(umf_memory_provider_handle_t hProvider, void *ptr,
                            size_t size) {
    enum umf_result_t res =
        hProvider->ops.purge_force(hProvider->provider_priv, ptr, size);
    checkErrorAndSetLastProvider(res, hProvider);
    return res;
}

const char *umfMemoryProviderGetName(umf_memory_provider_handle_t hProvider) {
    return hProvider->ops.get_name(hProvider->provider_priv);
}

umf_memory_provider_handle_t umfGetLastFailedMemoryProvider(void) {
    return *umfGetLastFailedMemoryProviderPtr();
}
