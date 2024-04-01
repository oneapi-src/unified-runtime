// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "device_selector/backend.hpp"
#include "device_selector/device.hpp"

namespace ur::device_selector {

struct FilterMatcher {
    std::optional<Diagnostic> init(std::string_view filter) {
        if (filter.empty()) {
            return Diagnostic("empty filter");
        }

        auto pos = filter.find(':');
        if (pos == std::string_view::npos) {
            std::string message = "invalid filter: '";
            message.append(filter);
            message += "'";
            return Diagnostic(message);
        }

        if (auto result = backendMatcher.init(filter.substr(0, pos))) {
            return *result;
        }

        if (auto result = devicesMatcher.init(filter.substr(pos + 1))) {
            return *result;
        }

        return std::nullopt;
    }

    bool isSubDeviceMatcher() const {
        return devicesMatcher.isSubDeviceMatcher();
    }

    bool isSubSubDeviceMatcher() const {
        return devicesMatcher.isSubSubDeviceMatcher();
    }

    friend bool operator==(const FilterMatcher &matcher,
                           const Descriptor &descriptor) {
        return matcher.backendMatcher == descriptor &&
               matcher.devicesMatcher == descriptor;
    }

    friend bool operator==(const Descriptor &descriptor,
                           const FilterMatcher &matcher) {
        return matcher == descriptor;
    }

    friend bool operator!=(const FilterMatcher &matcher,
                           const Descriptor &descriptor) {
        return matcher.backendMatcher != descriptor ||
               matcher.devicesMatcher != descriptor;
    }

    friend bool operator!=(const Descriptor &descriptor,
                           const FilterMatcher &matcher) {
        return matcher != descriptor;
    }

  private:
    BackendMatcher backendMatcher;
    DevicesMatcher devicesMatcher;
};

} // namespace ur::device_selector
