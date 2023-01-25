/*
 *
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */
#include <memory>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "ur_api.h"

#if defined(_WIN32)
    #define putenv_safe _putenv
#else
    #define putenv_safe putenv
#endif

inline bool argparse( int argc, char *argv[],
    const char *shortName, const char *longName )
{
    char **arg = &argv[ 1 ];
    char **argE = &argv[ argc ];

    for( ; arg != argE; ++arg )
        if( ( 0 == strcmp( *arg, shortName ) ) || ( 0 == strcmp( *arg, longName ) ) )
            return true;

    return false;
}

int main(int argc, char *argv[])
{
    if( argparse( argc, argv, "-val", "--enable_validation_layer" ) )
    {
        putenv_safe( const_cast<char *>( "UR_ENABLE_VALIDATION_LAYER=1" ) );
        putenv_safe( const_cast<char *>( "UR_ENABLE_PARAMETER_VALIDATION=1" ) );
    }

    ur_result_t status;

    ur_platform_handle_t platform = nullptr;
    ur_device_handle_t pDevice = nullptr;

    // Initialize the platform
    status = urInit(0, 0);
    if (status != UR_RESULT_SUCCESS)
    {
        std::cout << "urInit failed with return code: " << status << std::endl;
        return 1;
    }
    std::cout << "Platform initialized.\n";

    uint32_t platformCount = 0;
    std::vector<ur_platform_handle_t> platforms;

    status = urPlatformGet(1, nullptr, &platformCount);
    if (status != UR_RESULT_SUCCESS)
    {
        std::cout << "urPlatformGet failed with return code: " << status << std::endl;
        goto out;
    }

    platforms.resize(platformCount);
    status = urPlatformGet(platformCount, platforms.data(), nullptr);
    if (status != UR_RESULT_SUCCESS)
    {
        std::cout << "urPlatformGet failed with return code: " << status << std::endl;
        goto out;
    }

    for (auto p : platforms)
    {
        ur_api_version_t api_version = {};
        status = urPlatformGetApiVersion(p, &api_version);
        if (status != UR_RESULT_SUCCESS)
        {
            std::cout << "urPlatformGetApiVersion failed with return code: " << status << std::endl;
            goto out;
        }
        std::cout << "API version: " << UR_MAJOR_VERSION(api_version) << "." << UR_MINOR_VERSION(api_version) << std::endl;

        uint32_t deviceCount = 0;
        status = urDeviceGet(p, UR_DEVICE_TYPE_GPU, 0, nullptr, &deviceCount);
        if (status != UR_RESULT_SUCCESS)
        {
            std::cout << "urDeviceGet failed with return code: " << status << std::endl;
            goto out;
        }

        std::vector<ur_device_handle_t> devices(deviceCount);
        status = urDeviceGet(p, UR_DEVICE_TYPE_GPU, deviceCount, devices.data(), nullptr);
        if (status != UR_RESULT_SUCCESS)
        {
            std::cout << "urDeviceGet failed with return code: " << status << std::endl;
            goto out;
        }
        for (auto d : devices)
        {
            ur_device_type_t device_type;
            status = urDeviceGetInfo(d, UR_DEVICE_INFO_TYPE, sizeof(ur_device_type_t), static_cast<void *>(&device_type), nullptr);
            if (status != UR_RESULT_SUCCESS)
            {
                std::cout << "urDeviceGetInfo failed with return code: " << status << std::endl;
                goto out;
            }
            static const size_t DEVICE_NAME_MAX_LEN = 1024;
            char device_name[DEVICE_NAME_MAX_LEN] = {0};
            status = urDeviceGetInfo(d, UR_DEVICE_INFO_NAME, DEVICE_NAME_MAX_LEN - 1, static_cast<void *>(&device_name), nullptr);
            if (status != UR_RESULT_SUCCESS)
            {
                std::cout << "urDeviceGetInfo failed with return code: " << status << std::endl;
                goto out;
            }
            if (device_type == UR_DEVICE_TYPE_GPU)
            {
                std::cout << "Found a " << device_name << " gpu.\n";
            }
        }
    }

out:
    urTearDown(nullptr);
    return status == UR_RESULT_SUCCESS ? 0 : 1;
}
