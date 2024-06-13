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
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file ${name}.cpp
 *
 */
#include "${x}_mock_layer.hpp"

namespace ur_mock_layer
{
    %for obj in th.get_adapter_functions(specs):
    <%
        func_name=th.make_func_name(n, tags, obj)
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
    {
        auto ${th.make_pfn_name(n, tags, obj)} = context.${n}DdiTable.${th.get_table_name(n, tags, obj)}.${th.make_pfn_name(n, tags, obj)};

        if( nullptr == ${th.make_pfn_name(n, tags, obj)} ) {
            return ${X}_RESULT_ERROR_UNINITIALIZED;
        }

        ${th.make_pfncb_param_type(n, tags, obj)} params = { &${",&".join(th.make_param_lines(n, tags, obj, format=["name"]))} };

        ${x}_result_t result = UR_RESULT_SUCCESS;

        auto beforeCallback = reinterpret_cast<ur_mock_callback_t>(
                context.apiCallbacks.get_before_callback("${func_name}"));
        if(beforeCallback) {
            result = beforeCallback( &params );
            if(result != UR_RESULT_SUCCESS) {
                return result;
            }
        }

        auto replaceCallback = reinterpret_cast<ur_mock_callback_t>(
                context.apiCallbacks.get_replace_callback("${func_name}"));
        if(replaceCallback) {
            result = replaceCallback( &params );
        } else {
            <%
                # We can use the loader epilogue to know when we should be creating mock handles
                epilogue = th.get_loader_epilogue(specs, n, tags, obj, meta)
            %>
            %if 'NativeHandle' in func_name:
                <% func_class = th.subt(namespace, tags, obj['class'], False, True) %>
                %if 'CreateWith' in func_name:
                    *ph${func_class} = reinterpret_cast<ur_${func_class.lower()}_handle_t>(hNative${func_class});
                    retainDummyHandle(*ph${func_class});
                %else:
                    *phNative${func_class} = reinterpret_cast<ur_native_handle_t>(h${func_class});
                %endif
            %else:
                %if func_name == 'urAdapterGet' or func_name == 'urDeviceGet' or func_name == 'urPlatformGet':
                    <%
                        num_param = th.find_param_name(".*pNum.*", n, tags, obj)
                    %>
                    if(${num_param}) {
                        *${num_param} = 1;
                    }
                %endif
                %for item in epilogue:
                    %if item['release']:
                        releaseDummyHandle(${item['name']});
                    %elif item['retain']:
                        retainDummyHandle(${item['name']});
                    %elif 'type' in item:
                        %if 'range' in item or ('optional' in item and item['optional']):
                        // optional output handle
                        if(${item['name']}) {
                            *${item['name']} = createDummyHandle<${item['type']}>();
                        }
                        %else:
                        *${item['name']} = createDummyHandle<${item['type']}>();
                        %endif
                    %endif
                %endfor
            %endif
            result = UR_RESULT_SUCCESS;
        }
        if(result != UR_RESULT_SUCCESS) {
            return result;
        }

        auto afterCallback = reinterpret_cast<ur_mock_callback_t>(
                context.apiCallbacks.get_after_callback("${func_name}"));
        if(afterCallback) {
            return afterCallback( &params );
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
        auto& dditable = ur_mock_layer::context.${n}DdiTable.${tbl['name']};

        if( nullptr == pDdiTable )
            return ${X}_RESULT_ERROR_INVALID_NULL_POINTER;

        if (UR_MAJOR_VERSION(ur_mock_layer::context.version) != UR_MAJOR_VERSION(version) ||
            UR_MINOR_VERSION(ur_mock_layer::context.version) > UR_MINOR_VERSION(version))
            return ${X}_RESULT_ERROR_UNSUPPORTED_VERSION;

        ${x}_result_t result = ${X}_RESULT_SUCCESS;

        %for obj in tbl['functions']:
        %if 'condition' in obj:
    #if ${th.subt(n, tags, obj['condition'])}
        %endif
        dditable.${th.append_ws(th.make_pfn_name(n, tags, obj), 43)} = pDdiTable->${th.make_pfn_name(n, tags, obj)};
        pDdiTable->${th.append_ws(th.make_pfn_name(n, tags, obj), 41)} = ur_mock_layer::${th.make_func_name(n, tags, obj)};
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
                    const std::set<std::string> &enabledLayerNames,
                    codeloc_data, api_callbacks apiCallbacks) {
        ${x}_result_t result = ${X}_RESULT_SUCCESS;

        if(!enabledLayerNames.count(name)) {
            return result;
        }

        ur_mock_layer::context.apiCallbacks = apiCallbacks;

        %for tbl in th.get_pfntables(specs, meta, n, tags):
        if ( ${X}_RESULT_SUCCESS == result )
        {
            result = ur_mock_layer::${tbl['export']['name']}( ${X}_API_VERSION_CURRENT, &dditable->${tbl['name']} );
        }

        %endfor
        return result;
    }

} // namespace ur_mock_layer
