/*
 *
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */
#include "ur_loader.h"
#include "platform_discovery.h"

#include <iostream>

namespace loader
{
    ///////////////////////////////////////////////////////////////////////////////
    context_t *context;

    ///////////////////////////////////////////////////////////////////////////////
    ur_result_t context_t::init()
    {
        auto discoveredPlatforms = discoverEnabledPlatforms();

        for( auto name : discoveredPlatforms )
        {
            auto handle = LOAD_DRIVER_LIBRARY( name.c_str() );
            if( NULL != handle )
            {
                platforms.emplace_back();
                platforms.rbegin()->handle = handle;
            }
        }

        if(platforms.size()==0)
            return UR_RESULT_ERROR_UNINITIALIZED;

        if( getenv_tobool( "UR_ENABLE_VALIDATION_LAYER" ) )
        {
            auto valLibName = MAKE_LIBRARY_NAME("ur_validation_layer", UR_VERSION);
            validationLayer = LOAD_DRIVER_LIBRARY( valLibName );
            if ( validationLayer == NULL )
                std::cout << "VALIDATION LAYER NULL\n";
        }

        forceIntercept = getenv_tobool( "UR_ENABLE_LOADER_INTERCEPT" );

        if(forceIntercept || platforms.size() > 1)
             intercept_enabled = true;

        return UR_RESULT_SUCCESS;
    };

    ///////////////////////////////////////////////////////////////////////////////
    context_t::~context_t()
    {
        FREE_DRIVER_LIBRARY( validationLayer );

        for( auto& drv : platforms )
        {
            FREE_DRIVER_LIBRARY( drv.handle );
        }
    };

}

ur_result_t
urLoaderInit()
{
    return loader::context->init();
}
