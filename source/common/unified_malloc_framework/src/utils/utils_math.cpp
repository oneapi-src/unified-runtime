/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include "utils_math.h"

// Retrieves the position of the leftmost set bit.
// The position of the bit is counted from 0
// e.g. for 01000011110 the position equals 9.
size_t getLeftmostSetBitPos(size_t num) {
    // From C++20 countl_zero could be used for that.
    size_t position = 0;
    while (num > 0) {
        num >>= 1;
        position++;
    }
    position--;
    return position;
}
