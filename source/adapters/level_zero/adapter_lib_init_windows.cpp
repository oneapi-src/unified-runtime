//===--------- adapter_lib_init_linux.cpp - Level Zero Adapter ------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "adapter.hpp"
#include "ur_level_zero.hpp"

#include <windows.h>

// On windows, UR is destructed before sycl-rt have destructed the objects, so
// we need to clear the leftover memory before UR destruction.
ur_result_t deleteCacheOnDestruction() {
  std::lock_guard<std::mutex> Lock{GlobalAdapter->Mutex};
  const auto *platforms = GlobalAdapter->PlatformCache->get_value();
  for (const auto &p : *platforms) {
    std::scoped_lock<ur_shared_mutex> ContextsLock(p->ContextsMutex);
    while (!p->Contexts.empty()) {
      ur_context_handle_t &ctx = p->Contexts.front();
      ctx->deleteCachedObjectsOnDestruction();
      uint32_t RefCount = ctx->RefCount.load();
      while (RefCount--) {
        UR_CALL(urContextRelease(ctx));
      }
      // context object should be deleted at this point, but this is a guard
      // call to protect from infinite loop on case of error.
      deleteFromCachedList<ur_context_handle_t>(ctx, p->Contexts);
    }
  }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, // handle to DLL module
                    DWORD fdwReason,    // reason for calling function
                    LPVOID lpReserved)  // reserved
{
  switch (fdwReason) {
  case DLL_PROCESS_ATTACH:
    break;
  case DLL_PROCESS_DETACH: {
    return deleteCacheOnDestruction() == UR_RESULT_SUCCESS;
  }
  case DLL_THREAD_ATTACH:
    break;
  case DLL_THREAD_DETACH:
    break;
  }
  return TRUE;
}
