/*
 *
 * Copyright (C) 2023 Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file ur_exception_sanitizer_layer.h
 *
 */

#include "ur_exception_sanitizer_layer.hpp"

namespace ur_exception_sanitizer_layer {

context_t *getContext() { return context_t::get_direct(); }

} // namespace ur_exception_sanitizer_layer
