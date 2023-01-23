/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 * @file ur_validation_layer.cpp
 *
 */
#include "ur_validation_layer.h"

namespace validation_layer
{
    context_t context;

    ///////////////////////////////////////////////////////////////////////////////
    context_t::context_t()
    {
        enableParameterValidation = getenv_tobool( "UR_ENABLE_PARAMETER_VALIDATION" );
    }

    ///////////////////////////////////////////////////////////////////////////////
    context_t::~context_t()
    {
    }
} // namespace validation_layer
