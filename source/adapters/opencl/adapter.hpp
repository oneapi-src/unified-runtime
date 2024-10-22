//===-------------- adapter.hpp - OpenCL Adapter ---------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "logger/ur_logger.hpp"
#include "platform.hpp"

struct ur_adapter_handle_t_ {
  std::atomic<uint32_t> RefCount = 0;
  std::mutex Mutex;
  logger::Logger &log = logger::get_logger("opencl");

  std::vector<std::unique_ptr<ur_platform_handle_t_>> URPlatforms;
  uint32_t NumPlatforms = 0;
};
