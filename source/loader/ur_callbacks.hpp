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
    api_callbacks(bool enableMock) : enableMock(enableMock){};

    api_callbacks() : enableMock(false){};

    void set_before_callback(std::string name, void *callback) {
        beforeCallbacks[name] = callback;
    }

    void *get_before_callback(std::string name) {
        auto callback = beforeCallbacks.find(name);

        if (callback != beforeCallbacks.end()) {
            return callback->second;
        }
        return nullptr;
    }

    void set_replace_callback(std::string name, void *callback) {
        replaceCallbacks[name] = callback;
    }

    void *get_replace_callback(std::string name) {
        auto callback = replaceCallbacks.find(name);

        if (callback != replaceCallbacks.end()) {
            return callback->second;
        }
        return nullptr;
    }

    void set_after_callback(std::string name, void *callback) {
        afterCallbacks[name] = callback;
    }

    void *get_after_callback(std::string name) {
        auto callback = afterCallbacks.find(name);

        if (callback != afterCallbacks.end()) {
            return callback->second;
        }
        return nullptr;
    }

    ur_result_t add_callback(ur_callback_properties_t *callback_properties) {
        switch (callback_properties->mode) {
        case UR_CALLBACK_OVERRIDE_MODE_BEFORE:
            set_before_callback(callback_properties->name,
                                callback_properties->pCallbackFuncPointer);
            break;
        case UR_CALLBACK_OVERRIDE_MODE_REPLACE:
            set_replace_callback(callback_properties->name,
                                 callback_properties->pCallbackFuncPointer);
            break;
        case UR_CALLBACK_OVERRIDE_MODE_AFTER:
            set_after_callback(callback_properties->name,
                               callback_properties->pCallbackFuncPointer);
            break;
        default:
            return UR_RESULT_ERROR_INVALID_ENUMERATION;
        }
        return UR_RESULT_SUCCESS;
    }

    const ur_bool_t mockEnabled() const { return enableMock; }

  private:
    std::unordered_map<std::string, void *> beforeCallbacks;
    std::unordered_map<std::string, void *> replaceCallbacks;
    std::unordered_map<std::string, void *> afterCallbacks;

    ur_bool_t enableMock = false;
};

#endif /* UR_CODELOC_HPP */
