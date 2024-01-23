# Copyright (C) 2024 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set(TARGET_NAME ur_adapter_libomptarget)

set(UR_LIBOMPTARGET_LLVM_DIR "" CACHE PATH "Directory containing LLVM install")
set(UR_LIBOMPTARGET_INCLUDES_DIR "" CACHE PATH "Directory containing LLVM headers for libomptarget (e.g. <llvm>/openmp/libomptarget/include)")

if (UR_LIBOMPTARGET_LLVM_DIR STREQUAL "" OR UR_LIBOMPTARGET_INCLUDES_DIR STREQUAL "")
    message(FATAL_ERROR "UR_LIBOMPTARGET_LLVM_DIR and UR_LIBOMPTARGET_INCLUDES_DIR must be defined for the libomptarget adapter")
endif()

find_package(CUDAToolkit QUIET)

add_ur_adapter(${TARGET_NAME} SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/ur_interface_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/context.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/event.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/program.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/program.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/queue.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/usm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/usm.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/usm_p2p.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ur/ur.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ur/ur.hpp
)

set_target_properties(${TARGET_NAME} PROPERTIES
    VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
)

target_link_libraries(${TARGET_NAME} PRIVATE
    ${PROJECT_NAME}::headers
    ${PROJECT_NAME}::common
    CUDA::cuda_driver
    ${UR_LIBOMPTARGET_LLVM_DIR}/lib/libomptarget.so
    ${UR_LIBOMPTARGET_LLVM_DIR}/lib/libomptarget.rtl.cuda.nextgen.so
)

target_include_directories(${TARGET_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../"
    ${UR_LIBOMPTARGET_LLVM_DIR}/include
    ${UR_LIBOMPTARGET_INCLUDES_DIR}
    ${CUDAToolkit_INCLUDE_DIRS}
)
