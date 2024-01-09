
//===--------- ur_adapter_util.cpp - Unified Runtime  ---------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ur_util.hpp"
#include <cassert>

// Controls tracing UR calls from within the UR itself.
bool PrintTrace = [] {
    const auto PiRet = ur_getenv("SYCL_PI_TRACE");
    const char *Trace = PiRet ? PiRet->c_str() : nullptr;
    const int TraceValue = Trace ? std::stoi(Trace) : 0;
    if (TraceValue == -1 || TraceValue == 2) { // Means print all traces
        return true;
    }
    return false;
}();

// Apparatus for maintaining immutable cache of platforms.
std::vector<ur_platform_handle_t> *URPlatformsCache =
    new std::vector<ur_platform_handle_t>;
ur::SpinLock *URPlatformsCacheMutex = new ur::SpinLock;
bool URPlatformCachePopulated = false;

const bool SingleThreadMode = [] {
    const auto UrRet = ur_getenv("UR_L0_SINGLE_THREAD_MODE");
    const auto PiRet = ur_getenv("SYCL_PI_LEVEL_ZERO_SINGLE_THREAD_MODE");
    const bool RetVal = UrRet ? std::stoi(UrRet->c_str())
                              : (PiRet ? std::stoi(PiRet->c_str()) : 0);
    return RetVal;
}();

std::optional<std::string> ur_getenv(const char *name) {
#if defined(_WIN32)
    constexpr int buffer_size = 1024;
    char buffer[buffer_size];
    auto rc = GetEnvironmentVariableA(name, buffer, buffer_size);
    if (0 != rc && rc < buffer_size) {
        return std::string(buffer);
    } else if (rc >= buffer_size) {
        std::stringstream ex_ss;
        ex_ss << "Environment variable " << name << " value too long!"
              << " Maximum length is " << buffer_size - 1 << " characters.";
        throw std::invalid_argument(ex_ss.str());
    }
    return std::nullopt;
#else
    const char *tmp_env = getenv(name);
    if (tmp_env != nullptr) {
        return std::string(tmp_env);
    } else {
        return std::nullopt;
    }
#endif
}
