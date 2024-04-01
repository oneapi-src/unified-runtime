// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "descriptor.hpp"
#include "ur_api.h"
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace ur::device_selector {

struct BackendMatcher {
    /// @brief Initilalize the backend matcher from a string in a case
    /// insensitive way.
    /// @param[in] backend Backend filter string.
    /// @return Returns a diagnostic if an invalid backend is found.
    std::optional<Diagnostic> init(std::string_view str) {
        if (str.empty()) {
            return Diagnostic("empty backend");
        }
        std::string lower(str);
        std::transform(lower.cbegin(), lower.cend(), lower.begin(),
                       [](char c) { return std::tolower(c); });
        if (lower == "*") {
            matches.insert(matches.begin(), {
                                                UR_PLATFORM_BACKEND_UNKNOWN,
                                                UR_PLATFORM_BACKEND_LEVEL_ZERO,
                                                UR_PLATFORM_BACKEND_OPENCL,
                                                UR_PLATFORM_BACKEND_CUDA,
                                                UR_PLATFORM_BACKEND_HIP,
                                                UR_PLATFORM_BACKEND_OPENCL,
                                                UR_PLATFORM_BACKEND_NATIVE_CPU,
                                            });
        } else if (lower == "opencl") {
            matches.push_back(UR_PLATFORM_BACKEND_OPENCL);
        } else if (lower == "level_zero" || lower == "ext_oneapi_level_zero") {
            matches.push_back(UR_PLATFORM_BACKEND_LEVEL_ZERO);
        } else if (lower == "cuda" || lower == "ext_oneapi_cuda") {
            matches.push_back(UR_PLATFORM_BACKEND_CUDA);
        } else if (lower == "hip" || lower == "ext_oneapi_hip") {
            matches.push_back(UR_PLATFORM_BACKEND_HIP);
        } else if (lower == "native_cpu") {
            matches.push_back(UR_PLATFORM_BACKEND_NATIVE_CPU);
        } else {
            std::string diagnostic = "invalid backend: '";
            diagnostic.append(str);
            return Diagnostic(diagnostic + "'");
        }
        return std::nullopt;
    }

    friend bool operator==(const BackendMatcher &matcher,
                           const Descriptor &descriptor) {
        return std::any_of(
            matcher.matches.begin(), matcher.matches.end(),
            [&descriptor](const ur_platform_backend_t &backendMatch) {
                return backendMatch == descriptor.backend;
            });
    }

    friend bool operator==(const Descriptor &backend,
                           const BackendMatcher &matcher) {
        return matcher == backend;
    }

    friend bool operator!=(const BackendMatcher &matcher,
                           const Descriptor &descriptor) {
        return std::none_of(
            matcher.matches.begin(), matcher.matches.end(),
            [&descriptor](const ur_platform_backend_t &backendMatch) {
                return backendMatch == descriptor.backend;
            });
    }

    friend bool operator!=(const Descriptor &backend,
                           const BackendMatcher &matcher) {
        return matcher != backend;
    }

  private:
    std::vector<ur_platform_backend_t> matches;
};

} // namespace ur::device_selector
