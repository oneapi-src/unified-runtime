// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "device_selector/descriptor.hpp"
#include <gtest/gtest.h>

#define DESCRIPTOR(BACKEND, TYPE, ...)                                         \
    ur::device_selector::Descriptor(UR_PLATFORM_BACKEND_##BACKEND,             \
                                    UR_DEVICE_TYPE_##TYPE, __VA_ARGS__)
