/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file ur_codeloc.hpp
 *
 */

#ifndef UR_CALLBACKS_HPP
#define UR_CALLBACKS_HPP 1

#include <ur_api.h>

#include <string>
#include <unordered_map>

struct api_callbacks {
    void set_before_callback(std::string name, ur_mock_callback_t callback) {
        beforeCallbacks[name] = callback;
    }

    ur_mock_callback_t get_before_callback(std::string name) {
        auto callback = beforeCallbacks.find(name);

        if (callback != beforeCallbacks.end()) {
            return callback->second;
        }
        return nullptr;
    }

    void set_replace_callback(std::string name, ur_mock_callback_t callback) {
        replaceCallbacks[name] = callback;
    }

    ur_mock_callback_t get_replace_callback(std::string name) {
        auto callback = replaceCallbacks.find(name);

        if (callback != replaceCallbacks.end()) {
            return callback->second;
        }
        return nullptr;
    }

    void set_after_callback(std::string name, ur_mock_callback_t callback) {
        afterCallbacks[name] = callback;
    }

    ur_mock_callback_t get_after_callback(std::string name) {
        auto callback = afterCallbacks.find(name);

        if (callback != afterCallbacks.end()) {
            return callback->second;
        }
        return nullptr;
    }

    ur_result_t
    add_callback(ur_mock_callback_properties_t *callback_properties) {
        if (!callback_properties->pCallback) {
            return UR_RESULT_ERROR_INVALID_NULL_POINTER;
        }
        switch (callback_properties->mode) {
        case UR_CALLBACK_OVERRIDE_MODE_BEFORE:
            set_before_callback(callback_properties->name,
                                callback_properties->pCallback);
            break;
        case UR_CALLBACK_OVERRIDE_MODE_REPLACE:
            set_replace_callback(callback_properties->name,
                                 callback_properties->pCallback);
            break;
        case UR_CALLBACK_OVERRIDE_MODE_AFTER:
            set_after_callback(callback_properties->name,
                               callback_properties->pCallback);
            break;
        default:
            return UR_RESULT_ERROR_INVALID_ENUMERATION;
        }
        return UR_RESULT_SUCCESS;
    }

  private:
    std::unordered_map<std::string, ur_mock_callback_t> beforeCallbacks;
    std::unordered_map<std::string, ur_mock_callback_t> replaceCallbacks;
    std::unordered_map<std::string, ur_mock_callback_t> afterCallbacks;
};

#endif /* UR_CODELOC_HPP */
