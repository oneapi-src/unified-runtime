<%!
import re
from templates import helper as th
%><%
    n=namespace
    N=n.upper()

    x=tags['$x']
    X=x.upper()

    handle_create_get_retain_release_funcs=th.get_handle_create_get_retain_release_functions(specs, n, tags)
%>/*
 *
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file ${name}.cpp
 *
 */
#include "${x}_exception_sanitizer_layer.hpp"

namespace ur_exception_sanitizer_layer
{
    %for obj in th.get_adapter_functions(specs):
    <%
        func_name=th.make_func_name(n, tags, obj)

        param_checks=th.make_param_checks(n, tags, obj, meta=meta).items()
        first_errors = [X + "_RESULT_ERROR_INVALID_NULL_POINTER", X + "_RESULT_ERROR_INVALID_NULL_HANDLE"]
        sorted_param_checks = sorted(param_checks, key=lambda pair: False if pair[0] in first_errors else True)

        tracked_params = list(filter(lambda p: any(th.subt(n, tags, p['type']) in [hf['handle'], hf['handle'] + "*"] for hf in handle_create_get_retain_release_funcs), obj['params']))
    %>
    ///////////////////////////////////////////////////////////////////////////////
    /// @brief Intercept function for ${th.make_func_name(n, tags, obj)}
    %if 'condition' in obj:
    #if ${th.subt(n, tags, obj['condition'])}
    %endif
    __${x}dlllocal ${x}_result_t ${X}_APICALL
    ${func_name}(
        %for line in th.make_param_lines(n, tags, obj):
        ${line}
        %endfor
        )
    {${th.get_initial_null_set(obj)}
        auto ${th.make_pfn_name(n, tags, obj)} = getContext()->${n}DdiTable.${th.get_table_name(n, tags, obj)}.${th.make_pfn_name(n, tags, obj)};

        if( nullptr == ${th.make_pfn_name(n, tags, obj)} ) {
            return ${X}_RESULT_ERROR_UNINITIALIZED;
        }

        ${x}_result_t result = UR_RESULT_SUCCESS;
        try {
            result = ${th.make_pfn_name(n, tags, obj)}( ${", ".join(th.make_param_lines(n, tags, obj, format=["name"]))} );
        } catch (...) {
            std::cerr << "Exception caught from adapter layer in " << __func__ << " aborting\n";
        }

        return result;
    }
    %if 'condition' in obj:
    #endif // ${th.subt(n, tags, obj['condition'])}
    %endif

    %endfor
    %for tbl in th.get_pfntables(specs, meta, n, tags):
    ///////////////////////////////////////////////////////////////////////////////
    /// @brief Exported function for filling application's ${tbl['name']} table
    ///        with current process' addresses
    ///
    /// @returns
    ///     - ::${X}_RESULT_SUCCESS
    ///     - ::${X}_RESULT_ERROR_INVALID_NULL_POINTER
    ///     - ::${X}_RESULT_ERROR_UNSUPPORTED_VERSION
    ${X}_DLLEXPORT ${x}_result_t ${X}_APICALL
    ${tbl['export']['name']}(
        %for line in th.make_param_lines(n, tags, tbl['export']):
        ${line}
        %endfor
        )
    {
        auto& dditable = ur_exception_sanitizer_layer::getContext()->${n}DdiTable.${tbl['name']};

        if( nullptr == pDdiTable )
            return ${X}_RESULT_ERROR_INVALID_NULL_POINTER;

        if (UR_MAJOR_VERSION(ur_exception_sanitizer_layer::getContext()->version) != UR_MAJOR_VERSION(version) ||
            UR_MINOR_VERSION(ur_exception_sanitizer_layer::getContext()->version) > UR_MINOR_VERSION(version))
            return ${X}_RESULT_ERROR_UNSUPPORTED_VERSION;

        ${x}_result_t result = ${X}_RESULT_SUCCESS;

        %for obj in tbl['functions']:
        %if 'condition' in obj:
    #if ${th.subt(n, tags, obj['condition'])}
        %endif
        dditable.${th.append_ws(th.make_pfn_name(n, tags, obj), 43)} = pDdiTable->${th.make_pfn_name(n, tags, obj)};
        pDdiTable->${th.append_ws(th.make_pfn_name(n, tags, obj), 41)} = ur_exception_sanitizer_layer::${th.make_func_name(n, tags, obj)};
        %if 'condition' in obj:
    #else
        dditable.${th.append_ws(th.make_pfn_name(n, tags, obj), 43)} = nullptr;
        pDdiTable->${th.append_ws(th.make_pfn_name(n, tags, obj), 41)} = nullptr;
    #endif
        %endif

        %endfor
        return result;
    }

    %endfor
    ${x}_result_t
    context_t::init(ur_dditable_t *dditable,
                    const std::set<std::string> &,
                    codeloc_data) {
        ${x}_result_t result = ${X}_RESULT_SUCCESS;

    %for tbl in th.get_pfntables(specs, meta, n, tags):
        if( ${X}_RESULT_SUCCESS == result )
        {
            result = ur_exception_sanitizer_layer::${tbl['export']['name']}( ${X}_API_VERSION_CURRENT, &dditable->${tbl['name']} );
        }

    %endfor
        return result;
    }

} // namespace ur_exception_sanitizer_layer
