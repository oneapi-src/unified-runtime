/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UMF_UTIL_H
#define UMF_UTIL_H 1

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
inline bool umf_getenv_tobool(const char *name) {
#if defined(_WIN32)
    const int buffer_size = 1024;
    char value[buffer_size] = "0";
    auto rc = GetEnvironmentVariableA(name, buffer, buffer_size);
    if (rc == 0) {
        return false;
    }
#else
    const char *value = getenv(name);
    if (value == NULL) {
        return false;
    }
#endif
    const char zeroStr[] = "0";
    const char falseStr[] = "false";
    return (strncmp(value, zeroStr, strlen(zeroStr)) == 0 ||
            strncmp(value, falseStr, strlen(falseStr)) == 0)
               ? false
               : true;
}

#endif /* UMF_UTIL_H */
