//===--------- event.cpp - Level Zero Adapter -----------------------------===//
//
// Copyright (C) 2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "event.hpp"
#include "adapters/level_zero/v2/event_provider.hpp"
#include "event_pool.hpp"
#include "ze_api.h"

namespace v2 {

ur_event_handle_t_::ur_event_handle_t_(event_allocation eventAllocation,
                                       event_pool *pool)
    : type(eventAllocation.type), zeEvent(std::move(eventAllocation.borrow)),
      pool(pool) {}

void ur_event_handle_t_::reset() {
  // consider make an abstraction for regular/counter based
  // events if there's more of this type of conditions
  if (type == event_type::EVENT_REGULAR) {
    zeEventHostReset(zeEvent.get());
  }
}

ze_event_handle_t ur_event_handle_t_::getZeEvent() const {
  return zeEvent.get();
}

ur_result_t ur_event_handle_t_::retain() {
  RefCount.increment();
  return UR_RESULT_SUCCESS;
}

ur_result_t ur_event_handle_t_::release() {
  if (!RefCount.decrementAndTest())
    return UR_RESULT_SUCCESS;

  pool->free(this);
  RefCount.increment();

  return UR_RESULT_SUCCESS;
}

} // namespace v2
