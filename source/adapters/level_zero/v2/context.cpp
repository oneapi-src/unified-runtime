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

#include "event_provider_normal.hpp"

namespace v2 {

ur_context_handle_t_::ur_context_handle_t_(ze_context_handle_t hContext,
                                           uint32_t numDevices,
                                           const ur_device_handle_t *phDevices,
                                           bool ownZeContext)
    : ::ur_context_handle_t_(hContext, numDevices, phDevices, ownZeContext),
      commandListCache(hContext),
      eventPoolCache(phDevices[0]->Platform->getNumDevices(),
                     [context = this,
                      platform = phDevices[0]->Platform](DeviceId deviceId) {
                       auto device = platform->getDeviceById(deviceId);
                       // TODO: just use per-context id?
                       return std::make_unique<provider_normal>(
                           context, device, EVENT_COUNTER, QUEUE_IMMEDIATE);
                     }) {}

} // namespace v2
