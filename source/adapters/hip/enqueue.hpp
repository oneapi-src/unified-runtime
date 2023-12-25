//===--------- enqueue.hpp - HIP Adapter ---------------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

#include <cassert>
#include <hip/hip_runtime.h>
#include <ur_api.h>

ur_result_t
setKernelParams(const ur_device_handle_t Device, const uint32_t WorkDim,
                const size_t *GlobalWorkOffset, const size_t *GlobalWorkSize,
                const size_t *LocalWorkSize, ur_kernel_handle_t &Kernel,
                hipFunction_t &HIPFunc, size_t (&ThreadsPerBlock)[3],
                size_t (&BlocksPerGrid)[3]);
