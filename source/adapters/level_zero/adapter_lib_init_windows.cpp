//===--------- adapter_lib_init_windows.cpp - Level Zero Adapter --------------------===//
//
// Copyright (C) 2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions.
// See LICENSE.TXT SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "adapter.hpp"
#include "ur_level_zero.hpp"

extern "C" BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason,
                                 LPVOID lpvReserved) {
  if (fdwReason == DLL_PROCESS_DETACH) {
    delete Adapter;
  } else if (fdwReason == DLL_PROCESS_ATTACH) {
    Adapter = new ur_adapter_handle_t_();
  }
  return TRUE;
}
