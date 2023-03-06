# Copyright (C) 2022-2023 Intel Corporation
# SPDX-License-Identifier: MIT

set(UR_USM_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

function(add_usm_test name)
    set(TEST_TARGET_NAME usm_test-${name})
    add_executable(${TEST_TARGET_NAME}
        ${UR_USM_TEST_DIR}/../conformance/source/environment.cpp
        ${UR_USM_TEST_DIR}/../conformance/source/main.cpp
        ${ARGN})
    target_link_libraries(${TEST_TARGET_NAME}
        PRIVATE
        ${PROJECT_NAME}::common
        ${PROJECT_NAME}::loader
        uma_test_helpers
        ur_testing
        GTest::gtest_main)
    add_test(NAME  uma-${name}
        COMMAND ${TEST_TARGET_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(uma-${name} PROPERTIES LABELS "uma")
    target_compile_definitions("usm_test-${name}" PRIVATE DEVICES_ENVIRONMENT)
endfunction()

add_usm_test(usmPoolManager usmPoolManager.cpp)
