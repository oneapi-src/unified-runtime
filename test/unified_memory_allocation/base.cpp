// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: MIT

#include "helpers.hpp"

#include <uma/base.h>

using uma::test;

TEST_F(test, versionEncodeDecode) {
    auto encoded = UMA_MAKE_VERSION(0, 9);
    ASSERT_EQ(UMA_MAJOR_VERSION(encoded), 0);
    ASSERT_EQ(UMA_MINOR_VERSION(encoded), 9);
}
