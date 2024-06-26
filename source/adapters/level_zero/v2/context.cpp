//===--------- context.cpp - Level Zero Adapter --------------------------===//
//
// Copyright (C) 2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "context.hpp"

namespace v2 {

ur_context_handle_t_::ur_context_handle_t_(ze_context_handle_t ZeContext,
                                           uint32_t NumDevices,
                                           const ur_device_handle_t *Devs,
                                           bool OwnZeContext)
    : ::ur_context_handle_t_(ZeContext, NumDevices, Devs, OwnZeContext),
      CommandListCache(ZeContext) {}

} // namespace v2
