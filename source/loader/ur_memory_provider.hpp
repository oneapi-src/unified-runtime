/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "umf/memory_provider.h"

struct ur_provider_config_t {
    ur_device_handle_t device;
    ur_context_handle_t context;
};

extern struct umf_memory_provider_ops_t UR_MEMORY_PROVIDER_OPS;

#ifdef __cplusplus
}
#endif
