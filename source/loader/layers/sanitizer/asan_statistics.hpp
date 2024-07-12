/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file asan_report.hpp
 *
 */

#pragma once

#include "common.hpp"

namespace ur_sanitizer_layer {

void add_memory(size_t malloced, size_t redzone);
void del_memory(size_t malloced, size_t redzone);

void add_shadow(size_t shadow);

} // namespace ur_sanitizer_layer
