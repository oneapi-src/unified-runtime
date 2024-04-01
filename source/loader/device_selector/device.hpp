// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "descriptor.hpp"
#include "ur_api.h"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace ur::device_selector {

namespace detail {

inline std::vector<std::string_view> split(std::string_view str, char delim) {
    std::vector<std::string_view> parts;
    size_t start = 0, end = 0;
    while ((end = str.find(delim, start)) != std::string_view::npos) {
        parts.push_back(str.substr(start, end - start));
        start = end + 1;
    }
    parts.push_back(str.substr(start));
    return parts;
}

} // namespace detail

struct DeviceMatcher {
    std::optional<Diagnostic> init(std::string_view str) {
        if (str.empty()) {
            return Diagnostic("empty device");
        }

        // Make all characters lower case for case insensitive matching
        std::string lower(str);
        std::transform(lower.cbegin(), lower.cend(), lower.begin(),
                       [](char c) { return std::tolower(c); });

        if (lower.front() == '*') {
            if (lower.size() == 1) {
                descriptor.index = ANY_INDEX;
                kind = DeviceMatchKind::INDEX;
            } else if (lower == "*.*") {
                descriptor.index = ANY_INDEX;
                descriptor.subIndex = ANY_INDEX;
                kind = DeviceMatchKind::SUB_INDEX;
            } else if (lower == "*.*.*") {
                descriptor.index = ANY_INDEX;
                descriptor.subIndex = ANY_INDEX;
                descriptor.subSubIndex = ANY_INDEX;
                kind = DeviceMatchKind::SUB_SUB_INDEX;
            } else {
                std::string message = "invalid device: '";
                message.append(str);
                return Diagnostic(message + "'");
            }

        } else if (lower.front() >= '0' && lower.front() <= '9') {
            auto parts = detail::split(lower, '.');

            // We can assume parts is at least 1 element long due to empty
            // string check above
            uint32_t index = 0;
            auto result = toUInt32(std::string(parts[0]), &index);
            if (result.has_value()) {
                return result.value();
            }
            descriptor.index = index;
            kind = DeviceMatchKind::INDEX;

            if (parts.size() >= 2) {
                if (parts[1] == "*") {
                    descriptor.subIndex = ANY_INDEX;
                } else {
                    uint32_t subIndex = 0;
                    auto result = toUInt32(std::string(parts[1]), &subIndex);
                    if (result.has_value()) {
                        auto message = result->message + " in '";
                        message.append(str);
                        return message + "'";
                    }
                    descriptor.subIndex = subIndex;
                }
                kind = DeviceMatchKind::SUB_INDEX;
            }

            if (parts.size() == 3) {
                if (parts[2] == "*") {
                    descriptor.subSubIndex = ANY_INDEX;
                } else {
                    uint32_t subSubIndex = 0;
                    auto result = toUInt32(std::string(parts[2]), &subSubIndex);
                    if (result.has_value()) {
                        auto message = result->message + " in '";
                        message.append(str);
                        return message + "'";
                    }
                    descriptor.subSubIndex = subSubIndex;
                }
                kind = DeviceMatchKind::SUB_SUB_INDEX;

            } else if (parts.size() > 3) {
                std::string message = "invalid device: '";
                message.append(str);
                return Diagnostic(message + "'");
            }

        } else if (lower == "gpu") {
            descriptor.type = UR_DEVICE_TYPE_GPU;
            kind = DeviceMatchKind::TYPE;
        } else if (lower == "cpu") {
            descriptor.type = UR_DEVICE_TYPE_CPU;
            kind = DeviceMatchKind::TYPE;
        } else if (lower == "fpga") {
            descriptor.type = UR_DEVICE_TYPE_FPGA;
            kind = DeviceMatchKind::TYPE;
        } else if (lower == "mca") {
            descriptor.type = UR_DEVICE_TYPE_MCA;
            kind = DeviceMatchKind::TYPE;
        } else if (lower == "npu") {
            descriptor.type = UR_DEVICE_TYPE_VPU;
            kind = DeviceMatchKind::TYPE;
        } else {
            std::string message = "invalid device: '";
            message.append(str);
            return Diagnostic(message + "'");
        }

        return std::nullopt;
    }

    bool isSubDeviceMatcher() const {
        return kind == DeviceMatchKind::SUB_INDEX || isSubSubDeviceMatcher();
    }

    bool isSubSubDeviceMatcher() const {
        return kind == DeviceMatchKind::SUB_SUB_INDEX;
    }

    friend bool operator==(const DeviceMatcher &matcher,
                           Descriptor descriptor) {
        switch (matcher.kind) {
        case DeviceMatchKind::TYPE:
            return matcher.descriptor.type == descriptor.type;

        case DeviceMatchKind::INDEX:
            return matcher.descriptor.index == ANY_INDEX
                       ? true
                       : matcher.descriptor.index == descriptor.index;

        case DeviceMatchKind::SUB_INDEX:
            if (!descriptor.subIndex.has_value()) {
                return false;
            }
            return (matcher.descriptor.index == ANY_INDEX
                        ? true
                        : matcher.descriptor.index == descriptor.index) &&
                   (matcher.descriptor.subIndex.value() == ANY_INDEX
                        ? true
                        : matcher.descriptor.subIndex.value() ==
                              descriptor.subIndex.value());

        case DeviceMatchKind::SUB_SUB_INDEX:
            if (!descriptor.subIndex.has_value() ||
                !descriptor.subSubIndex.has_value()) {
                return false;
            }
            return (matcher.descriptor.index == ANY_INDEX
                        ? true
                        : matcher.descriptor.index == descriptor.index) &&
                   (matcher.descriptor.subIndex.value() == ANY_INDEX
                        ? true
                        : matcher.descriptor.subIndex.value() ==
                              descriptor.subIndex.value()) &&
                   (matcher.descriptor.subSubIndex.value() == ANY_INDEX
                        ? true
                        : matcher.descriptor.subSubIndex.value() ==
                              descriptor.subSubIndex.value());

        default:
            return false;
        }
    }

    friend bool operator==(Descriptor backend, const DeviceMatcher &matcher) {
        return matcher == backend;
    }

    friend bool operator!=(const DeviceMatcher &matcher,
                           Descriptor descriptor) {
        return !(matcher == descriptor);
    }

    friend bool operator!=(Descriptor backend, const DeviceMatcher &matcher) {
        return matcher != backend;
    }

  private:
    static constexpr uint32_t ANY_INDEX = 0xFFFFFFFF;

    enum class DeviceMatchKind : uint8_t {
        NONE = 0,
        TYPE,
        INDEX,
        SUB_INDEX,
        SUB_SUB_INDEX,
    };

    [[nodiscard]] std::optional<Diagnostic> toUInt32(const std::string &str,
                                                     uint32_t *value) {
        try {
            size_t pos;
            uint32_t result = std::stoul(str, &pos, 10);
            if (str.size() == pos) {
                *value = result;
                return std::nullopt;
            }
        } catch (const std::invalid_argument &) {
        } catch (const std::out_of_range &) {
        }
        std::string message = "invalid number: '";
        message.append(str);
        message += "'";
        return Diagnostic(message);
    }

    Descriptor descriptor;
    DeviceMatchKind kind;
};

struct DevicesMatcher {
    std::optional<Diagnostic> init(std::string_view str) {
        auto parts = detail::split(str, ',');
        for (const auto &part : parts) {
            DeviceMatcher matcher;
            auto result = matcher.init(part);
            if (result.has_value()) {
                if (parts.size() > 1) {
                    auto message = result.value().message + " in: '";
                    message.append(str);
                    return message + "'";
                } else {
                    return result.value();
                }
            }
            matches.push_back(matcher);
        }
        return std::nullopt;
    }

    bool isSubDeviceMatcher() const {
        return std::any_of(
            matches.begin(), matches.end(),
            [](const auto &matcher) { return matcher.isSubDeviceMatcher(); });
    }

    bool isSubSubDeviceMatcher() const {
        return std::any_of(matches.begin(), matches.end(),
                           [](const auto &matcher) {
                               return matcher.isSubSubDeviceMatcher();
                           });
    }

    friend bool operator==(const DevicesMatcher &matcher,
                           const Descriptor &descriptor) {
        return std::any_of(
            matcher.matches.begin(), matcher.matches.end(),
            [&](const auto &matcher) { return matcher == descriptor; });
    }

    friend bool operator==(const Descriptor &descriptor,
                           const DevicesMatcher &matcher) {
        return matcher == descriptor;
    }

    friend bool operator!=(const DevicesMatcher &matcher,
                           const Descriptor &descriptor) {
        return std::any_of(
            matcher.matches.begin(), matcher.matches.end(),
            [&](const auto &matcher) { return matcher != descriptor; });
    }

    friend bool operator!=(const Descriptor &descriptor,
                           const DevicesMatcher &matcher) {
        return matcher != descriptor;
    }

  private:
    std::vector<DeviceMatcher> matches;
};

} // namespace ur::device_selector
