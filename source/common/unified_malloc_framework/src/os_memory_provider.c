/*
    Copyright (c) 2022 Intel Corporation
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
        http://www.apache.org/licenses/LICENSE-2.0
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <umf.h>
#include <umf/memory_provider_ops.h>

#include "os_memory_provider.h"

struct umf_os_memory_provider_config_t {
    int protection;
    enum umf_mem_visibility visibility;

    // NUMA config
    unsigned long *nodemask;
    unsigned long maxnode;
    int numa_mode;
    int numa_flags;
};

umf_os_memory_provider_config_handle_t umfOsMemoryProviderConfigCreate() {
    struct umf_os_memory_provider_config_t *config =
        malloc(sizeof(struct umf_os_memory_provider_config_t));

    config->protection = ProtectionRead | ProtectionWrite;
    config->visibility = VisibilityPrivate;

    config->nodemask = NULL;
    config->maxnode = 0;
    config->numa_mode = NumaModeDefault;
    config->numa_flags = 0;

    return (umf_os_memory_provider_config_handle_t)config;
}

void umfOsMemoryProviderConfigDestroy(
    umf_os_memory_provider_config_handle_t hConfig) {
    free(((struct umf_os_memory_provider_config_t *)hConfig)->nodemask);
    free(hConfig);
}

void umfOsMemoryProviderConfigSetMemoryProtection(
    umf_os_memory_provider_config_handle_t hConfig, int memoryProtection) {
    hConfig->protection = memoryProtection;
}

void umfOsMemoryProviderConfigSetMemoryVisibility(
    umf_os_memory_provider_config_handle_t hConfig,
    enum umf_mem_visibility memoryVisibility) {
    hConfig->visibility = memoryVisibility;
}

static enum umf_result_t copyNodeMask(const unsigned long *nodemask,
                                      unsigned long maxnode,
                                      unsigned long **outNodemask) {
    if (maxnode == 0 || nodemask == NULL) {
        *outNodemask = NULL;
        return UMF_RESULT_SUCCESS;
    }

    size_t nodemask_bytes = maxnode / 8;

    // round to the next multiple of sizeof(unsigned long)
    if (nodemask_bytes % sizeof(unsigned long) != 0 || nodemask_bytes == 0) {
        nodemask_bytes +=
            sizeof(unsigned long) - nodemask_bytes % sizeof(unsigned long);
    }

    *outNodemask = (unsigned long *)calloc(1, nodemask_bytes);
    if (!*outNodemask) {
        return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
    }

    memcpy(*outNodemask, nodemask, nodemask_bytes);

    return UMF_RESULT_SUCCESS;
}

enum umf_result_t umfOsMemoryProviderConfigSetNumaMemBind(
    umf_os_memory_provider_config_handle_t handle,
    const unsigned long *nodemask, unsigned long maxnode, int mode, int flags) {

    enum umf_result_t ret = copyNodeMask(nodemask, maxnode, &handle->nodemask);
    if (ret != UMF_RESULT_SUCCESS) {
        return ret;
    }

    handle->maxnode = maxnode;
    handle->numa_mode = mode;
    handle->numa_flags = flags;

    return UMF_RESULT_SUCCESS;
}

static enum umf_result_t
memoryProviderConfigCreateCopy(umf_os_memory_provider_config_handle_t in,
                               umf_os_memory_provider_config_handle_t *out) {
    *out = umfOsMemoryProviderConfigCreate();
    if (!*out) {
        return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
    }

    **out = *in;
    return copyNodeMask(in->nodemask, in->maxnode, &(*out)->nodemask);
}

enum umf_result_t os_initialize(void *params, void **pool) {
    umf_os_memory_provider_config_handle_t config = NULL;
    umf_os_memory_provider_config_handle_t input =
        (umf_os_memory_provider_config_handle_t)params;
    enum umf_result_t ret = UMF_RESULT_SUCCESS;

    if (input) {
        ret = memoryProviderConfigCreateCopy(input, &config);
    } else {
        config = umfOsMemoryProviderConfigCreate();
    }

    if (ret == UMF_RESULT_SUCCESS) {
        *pool = (void *)config;
    }
    return ret;
}

void os_finalize(void *pool) {
    umfOsMemoryProviderConfigDestroy(
        (umf_os_memory_provider_config_handle_t)pool);
}

static enum umf_result_t os_alloc(void *provider, size_t size, size_t alignment,
                                  void **resultPtr) {
    umf_os_memory_provider_config_handle_t config =
        (umf_os_memory_provider_config_handle_t)provider;
    if (alignment > sysconf(_SC_PAGE_SIZE)) {
        return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
    }

    int flags = MAP_ANONYMOUS | config->visibility;
    int protection = config->protection;

    if (config->visibility == VisibilityShared &&
        config->numa_mode != NumaModeDefault) {
        // TODO: add support for that
        fprintf(stderr, "numa binding not supported for VisibilityShared");
        return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
    }

    void *result = mmap(NULL, size, protection, flags, -1, 0);
    if (result == MAP_FAILED) {
        fprintf(stderr, "syscall mmap() returned: %p", result);
        return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
    }

    int ret = mbind(result, size, config->numa_mode, config->nodemask,
                    config->maxnode, config->numa_flags);
    if (ret) {
        return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
    }

    *resultPtr = result;

    return UMF_RESULT_SUCCESS;
}

static enum umf_result_t os_free(void *provider, void *ptr, size_t bytes) {
    // TODO: round size to page?

    int ret = munmap(ptr, bytes);

    if (ret) {
        // TODO: map error
        return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
    }

    return UMF_RESULT_SUCCESS;
}

void os_get_last_native_error(void *provider, const char **ppMessage,
                              int32_t *pError) {}

struct umf_memory_provider_ops_t OS_MEMORY_PROVIDER_OPS = {
    .version = UMF_VERSION_CURRENT,
    .type = UMF_MEMORY_PROVIDER_TYPE_NUMA,
    .initialize = os_initialize,
    .finalize = os_finalize,
    .alloc = os_alloc,
    .free = os_free,
    .get_last_native_error = os_get_last_native_error};
