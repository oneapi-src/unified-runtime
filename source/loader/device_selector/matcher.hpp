// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "device_selector/descriptor.hpp"
#include "device_selector/filter.hpp"
#include <optional>
#include <string_view>
#include <vector>

namespace ur::device_selector {

struct Matcher {
    Matcher() = default;

    std::optional<Diagnostic> init(std::string_view selector) {
        if (selector.empty()) {
            return Diagnostic("empty selector");
        }

        auto parts = detail::split(selector, ';');
        for (const auto &part : parts) {
            if (parts.empty()) {
                std::string message = "empty filter: '";
                message.append(part);
                return Diagnostic(message + "'");
            }

            if (part[0] == '!') {
                FilterMatcher discardFilter;
                if (auto error = discardFilter.init(part.substr(1, -1))) {
                    return error->message;
                }
                discardFilters.push_back(discardFilter);
            } else {
                if (!discardFilters.empty()) {
                    std::string message =
                        "accept filters must appear before discard filters: '";
                    message.append(part);
                    return Diagnostic(message + "'");
                }
                FilterMatcher acceptFilter;
                if (auto error = acceptFilter.init(part)) {
                    return error->message;
                }
                acceptFilters.push_back(acceptFilter);
            }
        }

        if (acceptFilters.empty() && !discardFilters.empty()) {
            // Add an implicit accept all filter if only discard filters were
            // specified by the user
            FilterMatcher matcher;
            matcher.init("*:*");
            acceptFilters.push_back(matcher);
        }

        return std::nullopt;
    }

    bool isSubDeviceMatcher() const {
        auto isSubDeviceMatcher = [](const auto &matcher) {
            return matcher.isSubDeviceMatcher();
        };
        return std::any_of(acceptFilters.begin(), acceptFilters.end(),
                           isSubDeviceMatcher) ||
               std::any_of(discardFilters.begin(), discardFilters.end(),
                           isSubDeviceMatcher);
    }

    bool isSubSubDeviceMatcher() const {
        auto isSubSubDeviceMatcher = [](const auto &filter) {
            return filter.isSubSubDeviceMatcher();
        };
        return std::any_of(acceptFilters.begin(), acceptFilters.end(),
                           isSubSubDeviceMatcher) ||
               std::any_of(discardFilters.begin(), discardFilters.end(),
                           isSubSubDeviceMatcher);
    }

    friend bool operator==(const Matcher &matcher,
                           const Descriptor &descriptor) {
        auto isEqual = [&descriptor](const auto &filter) {
            return filter == descriptor;
        };
        return std::any_of(matcher.acceptFilters.begin(),
                           matcher.acceptFilters.end(), isEqual) &&
               std::none_of(matcher.discardFilters.begin(),
                            matcher.discardFilters.end(), isEqual);
    }

    friend bool operator==(const Descriptor &descriptor,
                           const Matcher &matcher) {
        return matcher == descriptor;
    }

    friend bool operator!=(const Matcher &matcher,
                           const Descriptor &descriptor) {
        auto isEqual = [&descriptor](const auto &filter) {
            return filter == descriptor;
        };
        return std::any_of(matcher.discardFilters.begin(),
                           matcher.discardFilters.end(), isEqual) ||
               std::none_of(matcher.acceptFilters.begin(),
                            matcher.acceptFilters.end(), isEqual);
    }

    friend bool operator!=(const Descriptor &descriptor,
                           const Matcher &matcher) {
        return matcher != descriptor;
    }

 private:
    std::vector<ur::device_selector::FilterMatcher> acceptFilters;
    std::vector<ur::device_selector::FilterMatcher> discardFilters;
};

} // namespace ur::device_selector
