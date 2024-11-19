# Copyright (C) 2024 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

if(NOT WIN32 AND NOT UR_STATIC_ADAPTER_L0)
    # Make L0 use CallMap from a seprate shared lib so that we can access the map
    # from the tests. This only seems to work on linux
    add_library(zeCallMap SHARED zeCallMap.cpp)
    install_ur_library(zeCallMap)
endif()

function(add_adapter_tests adapter)
    if(NOT UR_DPCXX)
        # Tests that require kernels can't be used if we aren't generating
        # device binaries
        message(WARNING
            "UR_DPCXX is not defined, skipping some adapter tests for ${adapter}")
    else()
        add_adapter_test(${adapter}
            FIXTURE KERNELS
            SOURCES
                urProgramLink.cpp
                urKernelCreateWithNativeHandle.cpp
                urEventCreateWithNativeHandle.cpp
            ENVIRONMENT
                "UR_ADAPTERS_FORCE_LOAD=\"$<TARGET_FILE:ur_adapter_${adapter}>\""
        )
        # TODO: valgrind tests require very new environment.
        # Enable once all L0 runners are updated.
        # add_adapter_memcheck_test(${adapter}
        #    ENVIRONMENT
        #         "UR_ADAPTERS_FORCE_LOAD=\"$<TARGET_FILE:ur_adapter_${adapter}>\""
        # )

        target_link_libraries(test-adapter-${adapter} PRIVATE
            LevelZeroLoader
            LevelZeroLoader-Headers
        )

        target_include_directories(test-adapter-${adapter} PRIVATE
            ${PROJECT_SOURCE_DIR}/source
            ${PROJECT_SOURCE_DIR}/source/adapters/${adapter}
            LevelZeroLoader-Headers
        )

        add_dependencies(test-adapter-${adapter}
            generate_device_binaries kernel_names_header)
    endif()

    if(NOT WIN32 AND NOT UR_STATIC_ADAPTER_L0)
        # Make L0 use CallMap from a seprate shared lib so that we can access the map
        # from the tests. This only seems to work on linux
        target_compile_definitions(ur_adapter_${adapter} PRIVATE UR_L0_CALL_COUNT_IN_TESTS)
        # TODO: stop exporting internals like this for tests...
        target_link_libraries(ur_adapter_${adapter} PRIVATE zeCallMap)

        # TODO: adjust to work for v2
        if("${adapter}" STREQUAL "level_zero")
            add_adapter_test(${adapter}_ze_calls
                FIXTURE DEVICES
                SOURCES
                    event_cache_tests.cpp
                ENVIRONMENT
                    "UR_ADAPTERS_FORCE_LOAD=\"$<TARGET_FILE:ur_adapter_${adapter}>\""
                    "UR_L0_LEAKS_DEBUG=1"
            )

            target_link_libraries(test-adapter-${adapter}_ze_calls PRIVATE zeCallMap)
        endif()

        # TODO: enable once driver bug is fixed
        if("${adapter}" STREQUAL "level_zero")
            add_adapter_test(${adapter}_multi_queue
                FIXTURE DEVICES
                SOURCES
                    multi_device_event_cache_tests.cpp
                ENVIRONMENT
                    "UR_ADAPTERS_FORCE_LOAD=\"$<TARGET_FILE:ur_adapter_${adapter}>\""
                    "UR_L0_LEAKS_DEBUG=1"
            )

            target_link_libraries(test-adapter-${adapter}_multi_queue PRIVATE zeCallMap)
        endif()
    endif()

    # TODO: debug for v2
    if("${adapter}" STREQUAL "level_zero")
        add_adapter_test(${adapter}_ipc
            FIXTURE DEVICES
            SOURCES
                ipc.cpp
            ENVIRONMENT
                "UR_ADAPTERS_FORCE_LOAD=\"$<TARGET_FILE:ur_adapter_${adapter}>\""
        )

        target_link_libraries(test-adapter-${adapter}_ipc PRIVATE
            ur_umf
        )
    endif()
endfunction()

if(UR_BUILD_ADAPTER_L0)
    add_adapter_tests(level_zero)
endif()

if(UR_BUILD_ADAPTER_L0_V2)
    add_adapter_tests(level_zero_v2)
    add_subdirectory(v2)
endif()
