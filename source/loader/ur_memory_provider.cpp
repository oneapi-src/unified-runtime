/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

/*
    Copyright (c) 2023 Intel Corporation
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

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ur_api.h>

#include <umf.h>
#include <umf/memory_provider.h>

#include <ur_memory_provider.hpp>

enum umf_result_t ur_initialize(void *params, void **pool) {

    if (pool == NULL) {
        return UMF_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (params == NULL) {
        return UMF_RESULT_ERROR_INVALID_ARGUMENT;
    }

    struct ur_provider_config_t *config = (struct ur_provider_config_t *)malloc(
        sizeof(struct ur_provider_config_t));

    if (config) {
        config->device = ((ur_provider_config_t *)params)->device;
        config->context = ((ur_provider_config_t *)params)->context;
        *pool = config;

        return UMF_RESULT_SUCCESS;
    }

    // else
    return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
}

void ur_finalize(void *provider) { free(provider); }

static enum umf_result_t ur_alloc(void *provider, size_t size, size_t alignment,
                                  void **resultPtr) {
    struct ur_provider_config_t *config =
        (struct ur_provider_config_t *)provider;
    umf_result_t result = UMF_RESULT_SUCCESS;

    // TODO - policy
    // TODO check errors
    urUSMSharedAlloc(config->context, config->device, NULL, NULL, size,
                     resultPtr);

    return result;
}

static enum umf_result_t ur_free(void *provider, void *ptr, size_t size) {
    struct ur_provider_config_t *config =
        (struct ur_provider_config_t *)provider;

    // TODO check errors
    urUSMFree(config->context, ptr);

    // TODO - size?

    return UMF_RESULT_SUCCESS;
}

void ur_get_last_native_error(void *provider, const char **ppMessage,
                              int32_t *pError) {
    // TODO
}

enum umf_result_t ur_get_min_page_size(void *provider, void *ptr,
                                       size_t *pageSize) {
    *pageSize = 1024; // TODO call urVirtualMemGranularityGetInfo here
    return UMF_RESULT_SUCCESS;
}

struct umf_memory_provider_ops_t UR_MEMORY_PROVIDER_OPS = {
    .version = UMF_VERSION_CURRENT,
    .type = UMF_MEMORY_PROVIDER_TYPE_USM,
    .initialize = ur_initialize,
    .finalize = ur_finalize,
    .alloc = ur_alloc,
    .free = ur_free,
    .get_last_native_error = ur_get_last_native_error,
    .get_min_page_size = ur_get_min_page_size,
};
