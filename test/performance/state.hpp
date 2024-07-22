// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <ur_api.h>
#include <uur/raii.h>

struct urState {
    urState();

    uur::raii::Adapter adapter;
    uur::raii::Context context;
    uur::raii::Device device;
};
