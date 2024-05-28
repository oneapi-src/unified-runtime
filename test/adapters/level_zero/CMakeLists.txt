# Copyright (C) 2024 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

if(NOT UR_DPCXX)
    # Tests that require kernels can't be used if we aren't generating
    # device binaries
    message(WARNING
        "UR_DPCXX is not defined, skipping adapter tests for level_zero")
else()

    set(EXTRA_ENV "")
    if(CMAKE_SYSTEM_NAME STREQUAL Linux)
        set(EXTRA_SOURCES event_cache_tests.cpp)
        set(EXTRA_ENV "UR_L0_LEAKS_DEBUG=1")
    endif()

    add_adapter_test(level_zero
        FIXTURE KERNELS
        SOURCES
            urProgramLink.cpp
            ${EXTRA_SOURCES}
        ENVIRONMENT
            "UR_ADAPTERS_FORCE_LOAD=\"$<TARGET_FILE:ur_adapter_level_zero>\""
            ${EXTRA_ENV}
    )

    target_include_directories(test-adapter-level_zero PRIVATE
        ${PROJECT_SOURCE_DIR}/source
        ${PROJECT_SOURCE_DIR}/source/adapters/level_zero
    )

    add_dependencies(test-adapter-level_zero
        generate_device_binaries kernel_names_header)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    # Make L0 use CallMap from a seprate shared lib so that we can access the map
    # from the tests. This only seems to work on linux
    add_library(zeCallMap SHARED zeCallMap.cpp)
    target_compile_definitions(ur_adapter_level_zero PRIVATE UR_L0_CALL_COUNT_IN_TESTS)
    target_link_libraries(ur_adapter_level_zero PRIVATE zeCallMap)
    # If UR_DPCXX is defined then the test-adapter-level-zero was added already taking into 
    # consideration the needed extra sources and envs.
    if(NOT UR_DPCXX)
        add_adapter_test(level_zero
            FIXTURE DEVICES
            SOURCES
                event_cache_tests.cpp
            ENVIRONMENT
                "UR_ADAPTERS_FORCE_LOAD=\"$<TARGET_FILE:ur_adapter_level_zero>\""
                "UR_L0_LEAKS_DEBUG=1"
        )
    endif()

    target_link_libraries(test-adapter-level_zero PRIVATE zeCallMap)
endif()
