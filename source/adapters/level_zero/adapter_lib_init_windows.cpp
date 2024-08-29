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

BOOL WINAPI DllMain(HINSTANCE hinstDLL, // handle to DLL module
                    DWORD fdwReason,    // reason for calling function
                    LPVOID lpReserved)  // reserved
{
  switch (fdwReason) {
  case DLL_PROCESS_ATTACH:
    break;
  case DLL_PROCESS_DETACH: {
    std::lock_guard<std::mutex> Lock{GlobalAdapter->Mutex};
    const auto *platforms = GlobalAdapter->PlatformCache->get_value();
    for (const auto &p : *platforms) {
      std::scoped_lock<ur_shared_mutex> ContextsLock(p->ContextsMutex);
      while (!p->Contexts.empty()) {
        ur_context_handle_t &ctx = p->Contexts.front();
        ctx->deleteCachedObjectsOnDestruction();
        UR_CALL(urContextRelease(ctx));
        deleteFromCachedList<ur_context_handle_t>(ctx, p->Contexts);
      }
    }
    break;
  }
  case DLL_THREAD_ATTACH:
    break;
  case DLL_THREAD_DETACH:
    break;
  }
  return TRUE;
}
