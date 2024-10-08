# Copyright (C) 2022 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/UrLoaderVersion.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/UrLoaderVersion.rc
    @ONLY
)

set(LOADER_LIB_TYPE SHARED)
if(UR_STATIC_LOADER)
  set(LOADER_LIB_TYPE STATIC)
endif()

add_ur_library(ur_loader
    ${LOADER_LIB_TYPE}
    ""
    ${CMAKE_CURRENT_BINARY_DIR}/UrLoaderVersion.rc
)
install_ur_library(ur_loader)

if (MSVC)
    set(TARGET_LIBNAME ur_loader)
    string(TOUPPER ${TARGET_LIBNAME} TARGET_LIBNAME)

    set(LOADER_VERSION_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/ur_loader.def)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/loader.def.in ${LOADER_VERSION_SCRIPT} @ONLY)
    set_target_properties(ur_loader PROPERTIES
        LINK_FLAGS "/DEF:${LOADER_VERSION_SCRIPT}"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(TARGET_LIBNAME libur_loader_${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
    string(TOUPPER ${TARGET_LIBNAME} TARGET_LIBNAME)

    set(LOADER_VERSION_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/ur_loader.map)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/loader.map.in ${LOADER_VERSION_SCRIPT} @ONLY)
    target_link_options(ur_loader PRIVATE "-Wl,--version-script=${LOADER_VERSION_SCRIPT}")
endif()

set_target_properties(ur_loader PROPERTIES
    LIBRARY_OUTPUT_NAME ur_loader
    RUNTIME_OUTPUT_NAME ur_loader
)

add_library(${PROJECT_NAME}::loader ALIAS ur_loader)

target_include_directories(ur_loader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/layers
)

set_target_properties(ur_loader PROPERTIES
    VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
)

target_link_libraries(ur_loader PRIVATE
    ${PROJECT_NAME}::common
    ${PROJECT_NAME}::headers
)

if(UR_STATIC_ADAPTER_L0)
    target_link_libraries(ur_loader PRIVATE
        ur_adapter_level_zero
    )
    target_compile_definitions(ur_loader PRIVATE UR_STATIC_ADAPTER_LEVEL_ZERO)
endif()

if(UR_ENABLE_TRACING)
    target_link_libraries(ur_loader PRIVATE ${TARGET_XPTI})
    target_include_directories(ur_loader PRIVATE ${xpti_SOURCE_DIR}/include)
    target_compile_definitions(ur_loader PRIVATE XPTI_STATIC_LIBRARY)
endif()

if (UNIX)
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)
    target_link_libraries(ur_loader PRIVATE Threads::Threads)
    # Older gcc versions need -pthread, not just -lpthread
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.1)
            if(CMAKE_USE_PTHREADS_INIT)
                target_link_libraries(ur_loader PRIVATE "-pthread")
            endif()
        endif()
    endif()
endif()

if(WIN32)
    target_link_libraries(ur_loader PRIVATE cfgmgr32.lib)
endif()

if(UNIX)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/libur_loader.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/libur_loader.pc
        @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libur_loader.pc" DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig" COMPONENT unified-runtime)
endif()

target_sources(ur_loader
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_object.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_loader.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_loader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_ldrddi.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_ldrddi.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_libapi.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_libddi.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_lib.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_lib.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_codeloc.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_print.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/validation/ur_valddi.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/validation/ur_validation_layer.cpp
)

if(UR_ENABLE_TRACING)
    target_sources(ur_loader
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/tracing/ur_tracing_layer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/tracing/ur_trcddi.cpp
    )
endif()

if(UR_ENABLE_SANITIZER)
    target_sources(ur_loader
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../ur/ur.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_allocator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_allocator.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_buffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_buffer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_interceptor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_interceptor.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_libdevice.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_options.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_options.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_quarantine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_quarantine.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_report.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_report.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_shadow_setup.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_shadow_setup.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_statistics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_statistics.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_validator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/asan_validator.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/common.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/stacktrace.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/stacktrace.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/ur_sanddi.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/ur_sanitizer_layer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/ur_sanitizer_layer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/ur_sanitizer_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/ur_sanitizer_utils.hpp
    )

    target_sources(ur_loader
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/linux/backtrace.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/linux/sanitizer_utils.cpp
    )

    if(UR_ENABLE_SYMBOLIZER)
        target_sources(ur_loader
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/linux/symbolizer.cpp
        )
        target_include_directories(ur_loader PRIVATE ${LLVM_INCLUDE_DIRS})
        target_link_libraries(ur_loader PRIVATE LLVMSupport LLVMSymbolize)
        # In in-tree build, if LLVM is built with libc++, we also need to build
        # symbolizer.cpp with libc++ abi and link libc++ in.
        if(NOT UR_STANDALONE_BUILD AND LLVM_LIBCXX_USED)
            execute_process(
                COMMAND ${CMAKE_CXX_COMPILER} --print-file-name=libc++.a
                OUTPUT_VARIABLE LIBCXX_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            execute_process(
                COMMAND ${CMAKE_CXX_COMPILER} --print-file-name=libc++abi.a
                OUTPUT_VARIABLE LIBCXX_ABI_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            set_property(SOURCE
                ${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer/linux/symbolizer.cpp
                APPEND_STRING PROPERTY COMPILE_FLAGS
                " -stdlib=libc++ ")
            if(NOT EXISTS ${LIBCXX_PATH} OR NOT EXISTS ${LIBCXX_ABI_PATH})
                message(FATAL_ERROR "libc++ is required but can't find the libraries")
            endif()
            target_link_libraries(ur_loader PRIVATE ${LIBCXX_PATH} ${LIBCXX_ABI_PATH})
        endif()
    endif()

    target_include_directories(ur_loader PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/layers/sanitizer"
        "${CMAKE_CURRENT_SOURCE_DIR}/../"
    )
endif()


# link validation backtrace dependencies
if(UNIX)
    find_package(Libbacktrace)
endif()
if (VAL_USE_LIBBACKTRACE_BACKTRACE AND LIBBACKTRACE_FOUND)
    message(STATUS "Using libbacktrace backtrace for validation")

    target_sources(ur_loader PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/layers/validation/backtrace_libbacktrace.cpp)
    target_link_libraries(ur_loader PRIVATE Libbacktrace)
else()
    message(STATUS "Using default backtrace for validation")

    if(WIN32)
        target_sources(ur_loader PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/layers/validation/backtrace_win.cpp)
        target_link_libraries(ur_loader PRIVATE dbghelp)
    else()
        target_sources(ur_loader PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/layers/validation/backtrace_lin.cpp)
    endif()
endif()

if(WIN32)
    target_sources(ur_loader
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/windows/adapter_search.cpp
    )
else()
    target_sources(ur_loader
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/linux/adapter_search.cpp
    )
endif()
