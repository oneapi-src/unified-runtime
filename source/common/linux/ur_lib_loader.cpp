/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
 * Exceptions. See LICENSE.TXT
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */
#include <dlfcn.h>
#if __has_include(<link.h>)
#include <link.h>
#define ADD_FULL_PATH_LOG
#endif

#include "logger/ur_logger.hpp"
#include "ur_lib_loader.hpp"

#define DEEP_BIND_ENV "UR_ADAPTERS_DEEP_BIND"

namespace ur_loader {

void LibLoader::freeAdapterLibrary(HMODULE handle) {
  if (handle) {
    int res = dlclose(handle);
    if (res) {
      UR_LOG(ERR, "Failed to unload the library with the handle at address {}",
             handle);
    } else {
      UR_LOG(INFO, "unloaded adapter 0x{}", handle);
    }
  }
}

std::unique_ptr<HMODULE, LibLoader::lib_dtor>
LibLoader::loadAdapterLibrary(const char *name) {
  int mode = RTLD_LAZY | RTLD_LOCAL;
#if !defined(__APPLE__)
  bool deepbind = getenv_tobool(DEEP_BIND_ENV);
  if (deepbind) {
#if defined(SANITIZER_ANY)
    UR_LOG(WARN,
           "Enabling RTLD_DEEPBIND while running under a sanitizer is likely "
           "to cause issues. Consider disabling {} environment variable.",
           DEEP_BIND_ENV);
#endif
    mode |= RTLD_DEEPBIND;
  }
#endif
  HMODULE handle = dlopen(name, mode);
  if (!handle) {
    const char *err = dlerror();

    // Check if the error string does not contain the adapter name or if it
    // contains a "required by" (missing symbol) message.
    if (err &&
        (strstr(err, name) == NULL || strstr(err, "required by") != NULL)) {
      // If the adapter cannot be loaded due to missing dependencies or any
      // other related error, it is considered as an error.
      UR_LOG(INFO, "failed to load adapter '{}' with error: {}", name, err);
    } else {
      // Simply having the adapter library missing isn't an error.
      UR_LOG(INFO, "failed to load adapter '{}' with error: {}", name,
             err ? err : "unknown error");
    }
  } else {
#if defined(ADD_FULL_PATH_LOG)
    struct link_map *dlinfo_map;
    if (dlinfo(handle, RTLD_DI_LINKMAP, &dlinfo_map) == 0) {
      UR_LOG(INFO, "loaded adapter 0x{} ({}) from {}", handle, name,
             dlinfo_map->l_name);
    } else
#endif
      UR_LOG(INFO, "loaded adapter 0x{} ({})", handle, name);
  }
  return std::unique_ptr<HMODULE, LibLoader::lib_dtor>(handle);
}

void *LibLoader::getFunctionPtr(HMODULE handle, const char *func_name) {
  return dlsym(handle, func_name);
}

} // namespace ur_loader
