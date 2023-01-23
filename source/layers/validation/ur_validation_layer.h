/*
 *
 * Copyright (C) 2023 Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 * @file ur_layer.h
 *
 */
#pragma once
#include "ur_ddi.h"
#include "ur_util.h"

#define VALIDATION_COMP_NAME "validation layer"

namespace validation_layer
{
    ///////////////////////////////////////////////////////////////////////////////
    class __urdlllocal context_t
    {
    public:
        ur_api_version_t version = UR_API_VERSION_0_9;

        bool enableParameterValidation = false;

        ur_dditable_t   urDdiTable = {};

        context_t();
        ~context_t();
    };

    extern context_t context;
} // namespace validation_layer
