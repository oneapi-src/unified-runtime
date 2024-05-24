/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file ur_lib.cpp
 *
 */

// avoids windows.h from defining macros for min and max
// which avoids playing havoc with std::min and std::max
// (not quite sure why windows.h is being included here)
#ifndef NOMINMAX
#define NOMINMAX
#include "ur_util.hpp"
#endif // !NOMINMAX

#include "logger/ur_logger.hpp"
#include "ur_lib.hpp"
#include "ur_loader.hpp"

#include <cstring>

namespace ur_lib {
///////////////////////////////////////////////////////////////////////////////
context_t *context;

///////////////////////////////////////////////////////////////////////////////
context_t::context_t() {
    for (auto l : layers) {
        if (l->isAvailable()) {
            for (auto &layerName : l->getNames()) {
                availableLayers += layerName + ";";
            }
        }
    }
    // Remove the trailing ";"
    availableLayers.pop_back();
    parseEnvEnabledLayers();
}

///////////////////////////////////////////////////////////////////////////////
context_t::~context_t() {}

bool context_t::layerExists(const std::string &layerName) const {
    return availableLayers.find(layerName) != std::string::npos;
}

void context_t::parseEnvEnabledLayers() {
    auto maybeEnableEnvVarMap = getenv_to_map("UR_ENABLE_LAYERS", false);
    if (!maybeEnableEnvVarMap.has_value()) {
        return;
    }
    auto enableEnvVarMap = maybeEnableEnvVarMap.value();

    for (auto &key : enableEnvVarMap) {
        enabledLayerNames.insert(key.first);
    }
}

void context_t::initLayers() const {
    for (auto &l : layers) {
        if (l->isAvailable()) {
            l->init(&context->urDdiTable, enabledLayerNames, codelocData);
        }
    }
}

void context_t::tearDownLayers() const {
    for (auto &l : layers) {
        if (l->isAvailable()) {
            l->tearDown();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
__urdlllocal ur_result_t context_t::Init(
    ur_device_init_flags_t, ur_loader_config_handle_t hLoaderConfig) {
    ur_result_t result;
    const char *logger_name = "loader";
    logger::init(logger_name);
    logger::debug("Logger {} initialized successfully!", logger_name);

    result = ur_loader::context->init();

    if (UR_RESULT_SUCCESS == result) {
        result = urLoaderInit();
    }

    if (hLoaderConfig) {
        codelocData = hLoaderConfig->codelocData;
        enabledLayerNames.merge(hLoaderConfig->getEnabledLayerNames());
    }

    if (!enabledLayerNames.empty()) {
        initLayers();
    }

    return result;
}

#if UR_ENABLE_DEVICE_SELECTOR
ur_result_t context_t::enumerateSelectedDevices() {
    uint32_t numAdapters;
    if (auto error = urAdapterGet(0, nullptr, &numAdapters)) {
        return error;
    }
    std::vector<ur_adapter_handle_t> adapters(numAdapters);
    if (auto error = urAdapterGet(numAdapters, adapters.data(), nullptr)) {
        return error;
    }
    logger::debug("{}: found {} adapters", __FUNCTION__, numAdapters);

    uint32_t numPlatforms;
    if (auto error = urPlatformGet(adapters.data(), numAdapters, 0, nullptr,
                                   &numPlatforms)) {
        return error;
    }
    if (numPlatforms == 0) {
        return UR_RESULT_SUCCESS;
    }
    std::vector<ur_platform_handle_t> platforms(numPlatforms);
    if (auto error = urPlatformGet(adapters.data(), numAdapters, numPlatforms,
                                   platforms.data(), nullptr)) {
        return error;
    }
    logger::debug("{}: found {} platforms", __FUNCTION__, numPlatforms);

    // Keep track of device indicies for each platform backend type to ensure
    // global ordering of devices can be maintained
    std::unordered_map<ur_platform_backend_t, uint32_t> deviceIndexCounters;

    for (auto hPlatform : platforms) {
        ur_platform_backend_t platformBackend;
        if (auto error = urPlatformGetInfo(hPlatform, UR_PLATFORM_INFO_BACKEND,
                                           sizeof(ur_platform_backend_t),
                                           &platformBackend, nullptr)) {
            return error;
        }
        logger::debug("{}: platform {} backend {}", __FUNCTION__, hPlatform,
                      platformBackend);

        if (deviceIndexCounters.find(platformBackend) ==
            deviceIndexCounters.end()) {
            deviceIndexCounters[platformBackend] = 0;
        }

        uint32_t numDevices;
        if (auto error = urDeviceGet(hPlatform, UR_DEVICE_TYPE_ALL, 0, nullptr,
                                     &numDevices)) {
            return error;
        }
        std::vector<ur_device_handle_t> devices(numDevices);
        if (auto error = urDeviceGet(hPlatform, UR_DEVICE_TYPE_ALL, numDevices,
                                     devices.data(), nullptr)) {
            return error;
        }
        logger::debug("{}: found {} devices in platform {}", __FUNCTION__,
                      numDevices, hPlatform);

        for (auto hDevice : devices) {
            ur_device_type_t deviceType;
            if (auto error = urDeviceGetInfo(hDevice, UR_DEVICE_INFO_TYPE,
                                             sizeof(ur_device_type_t),
                                             &deviceType, nullptr)) {
                return error;
            }
            logger::debug("{}: device {} type {}", __FUNCTION__, hDevice,
                          deviceType);

            ur::device_selector::Descriptor desc;
            desc.backend = platformBackend;
            desc.type = deviceType;
            desc.index = deviceIndexCounters[platformBackend];

            if (desc == matcher) {
                auto &device = context->selectedDevices.emplace_back();
                device.hPlatform = hPlatform;
                device.hDevice = hDevice;
                device.desc = desc;
                logger::debug(
                    "{}: matched device {} type {} in platform {} backend {}",
                    __FUNCTION__, hDevice, deviceType, hPlatform,
                    platformBackend);
            } else {
                logger::debug(
                    "{}: unmatched device {} type {} in platform {} backend {}",
                    __FUNCTION__, hDevice, deviceType, hPlatform,
                    platformBackend);
            }

            if (matcher.isSubDeviceMatcher()) {
                // TODO: Create sub-devices
                return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }

            if (matcher.isSubSubDeviceMatcher()) {
                // TODO: Create sub-sub-devices
                return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }

            deviceIndexCounters[platformBackend]++;
        }
    }

    // TODO: Sort devices based on preferred type first (gpu before others)
    // TODO: Filter unwanted platfomrs, e.g. NVIDIA/AMD OpenCL drivers

    logger::debug("{}: matched {} devices", __FUNCTION__,
                  selectedDevices.size());

    return UR_RESULT_SUCCESS;
}
#endif

ur_result_t urLoaderConfigCreate(ur_loader_config_handle_t *phLoaderConfig) {
    if (!phLoaderConfig) {
        return UR_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    *phLoaderConfig = new ur_loader_config_handle_t_;
    return UR_RESULT_SUCCESS;
}

ur_result_t urLoaderConfigRetain(ur_loader_config_handle_t hLoaderConfig) {
    if (!hLoaderConfig) {
        return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    hLoaderConfig->incrementReferenceCount();
    return UR_RESULT_SUCCESS;
}

ur_result_t urLoaderConfigRelease(ur_loader_config_handle_t hLoaderConfig) {
    if (!hLoaderConfig) {
        return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    if (hLoaderConfig->decrementReferenceCount() == 0) {
        delete hLoaderConfig;
    }
    return UR_RESULT_SUCCESS;
}

ur_result_t urLoaderConfigGetInfo(ur_loader_config_handle_t hLoaderConfig,
                                  ur_loader_config_info_t propName,
                                  size_t propSize, void *pPropValue,
                                  size_t *pPropSizeRet) {
    if (!hLoaderConfig) {
        return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    if (!pPropValue && !pPropSizeRet) {
        return UR_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    switch (propName) {
    case UR_LOADER_CONFIG_INFO_AVAILABLE_LAYERS: {
        if (pPropSizeRet) {
            *pPropSizeRet = context->availableLayers.size() + 1;
        }
        if (pPropValue) {
            char *outString = static_cast<char *>(pPropValue);
            if (propSize != context->availableLayers.size() + 1) {
                return UR_RESULT_ERROR_INVALID_SIZE;
            }
            std::memcpy(outString, context->availableLayers.data(),
                        propSize - 1);
            outString[propSize - 1] = '\0';
        }
        break;
    }
    case UR_LOADER_CONFIG_INFO_REFERENCE_COUNT: {
        auto refCount = hLoaderConfig->getReferenceCount();
        auto truePropSize = sizeof(refCount);
        if (pPropSizeRet) {
            *pPropSizeRet = truePropSize;
        }
        if (pPropValue) {
            if (propSize != truePropSize) {
                return UR_RESULT_ERROR_INVALID_SIZE;
            }
            std::memcpy(pPropValue, &refCount, truePropSize);
        }
        break;
    }
    default:
        return UR_RESULT_ERROR_INVALID_ENUMERATION;
    }
    return UR_RESULT_SUCCESS;
}

ur_result_t urLoaderConfigEnableLayer(ur_loader_config_handle_t hLoaderConfig,
                                      const char *pLayerName) {
    if (!hLoaderConfig) {
        return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    if (!pLayerName) {
        return UR_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    if (!context->layerExists(std::string(pLayerName))) {
        return UR_RESULT_ERROR_LAYER_NOT_PRESENT;
    }
    hLoaderConfig->enabledLayers.insert(pLayerName);
    return UR_RESULT_SUCCESS;
}

ur_result_t urLoaderTearDown() {
    context->tearDownLayers();

    return UR_RESULT_SUCCESS;
}

ur_result_t
urLoaderConfigSetCodeLocationCallback(ur_loader_config_handle_t hLoaderConfig,
                                      ur_code_location_callback_t pfnCodeloc,
                                      void *pUserData) {
    if (!hLoaderConfig) {
        return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    if (!pfnCodeloc) {
        return UR_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    hLoaderConfig->codelocData.codelocCb = pfnCodeloc;
    hLoaderConfig->codelocData.codelocUserdata = pUserData;

    return UR_RESULT_SUCCESS;
}

ur_result_t urDeviceGetSelected(ur_platform_handle_t hPlatform,
                                ur_device_type_t deviceType,
                                uint32_t numEntries,
                                ur_device_handle_t *phDevices,
                                uint32_t *pNumDevices) {
#if UR_ENABLE_DEVICE_SELECTOR
    if (!context->deviceSelectorEnabled) {
        return context->urDdiTable.Device.pfnGet(
            hPlatform, deviceType, numEntries, phDevices, pNumDevices);
    }

    if (hPlatform == nullptr) {
        return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    if (numEntries > 0 && phDevices == nullptr) {
        return UR_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    if (numEntries == 0 && phDevices != nullptr) {
        return UR_RESULT_ERROR_INVALID_SIZE;
    }

    logger::debug("{}: platform {} device type {}", __FUNCTION__, hPlatform,
                  deviceType);

    logger::debug("{}: total num devices {}", __FUNCTION__,
                  context->selectedDevices.size());

    std::vector<ur_device_handle_t> devices;
    for (auto selectedDevice : context->selectedDevices) {
        logger::debug("{}: device {} type {} in platform {} backend {}",
                      __FUNCTION__, selectedDevice.hDevice,
                      selectedDevice.desc.type, selectedDevice.hPlatform,
                      selectedDevice.desc.backend);

        if (selectedDevice.hPlatform == hPlatform) {
            if (deviceType == UR_DEVICE_TYPE_ALL ||
                selectedDevice.desc.type == deviceType) {
                devices.push_back(selectedDevice.hDevice);
                logger::debug(
                    "{}: return device {} type {} in platform {} backend {}",
                    __FUNCTION__, selectedDevice.hDevice,
                    selectedDevice.desc.type, selectedDevice.hPlatform,
                    selectedDevice.desc.backend);
            }
        }
    }

    if (phDevices) {
        auto end =
            devices.begin() + std::min(size_t(numEntries), devices.size());
        std::copy(devices.begin(), end, phDevices);
    }
    if (pNumDevices) {
        *pNumDevices = uint32_t(devices.size());
    }

    return UR_RESULT_SUCCESS;
#else
    return context->urDdiTable.Device.pfnGet(hPlatform, deviceType, numEntries,
                                             phDevices, pNumDevices);
#endif
}

} // namespace ur_lib
