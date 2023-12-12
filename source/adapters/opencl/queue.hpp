//===--------- queue.hpp - OpenCL Adapter ---------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

#include "common.hpp"

#include <vector>

struct ur_queue_handle_t_ {
  using native_type = cl_command_queue;
  native_type Queue;
  ur_context_handle_t Context;
  ur_device_handle_t Device;

  ur_queue_handle_t_(native_type Queue, ur_context_handle_t Ctx,
                     ur_device_handle_t Dev)
      : Queue(Queue), Context(Ctx), Device(Dev) {}

  ~ur_queue_handle_t_() {}

  native_type get() { return Queue; }
};
