# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: MIT
set(TARGET_NAME ur_validation_layer)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/UrValidationLayerVersion.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/UrValidationLayerVersion.rc
    @ONLY)

add_library(${TARGET_NAME}
    SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_validation_layer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_validation_layer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_valddi.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/UrValidationLayerVersion.rc
)

target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME} PRIVATE
    ${PROJECT_NAME}::headers
    ${PROJECT_NAME}::common
)

if(UNIX)
    set(GCC_COVERAGE_COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")
endif()

set_target_properties(${TARGET_NAME} PROPERTIES
    VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
    SOVERSION  "${PROJECT_VERSION_MAJOR}"
)

install(TARGETS ur_validation_layer
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT unified-runtime
        NAMELINK_SKIP
)
