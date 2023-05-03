# Copyright (C) 2022 Intel Corporation
# SPDX-License-Identifier: MIT

set(TARGET_NAME ur_adapter_level_zero)


if (NOT DEFINED LEVEL_ZERO_LIBRARY OR NOT DEFINED LEVEL_ZERO_INCLUDE_DIR)
  message(STATUS "Download Level Zero loader and headers from github.com")

  set(LEVEL_ZERO_LOADER_REPO "https://github.com/oneapi-src/level-zero.git")
  set(LEVEL_ZERO_LOADER_TAG v1.8.8)

  # Disable due to a bug https://github.com/oneapi-src/level-zero/issues/104
  set(CMAKE_INCLUDE_CURRENT_DIR OFF)
  # Prevent L0 loader from exporting extra symbols
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

  message(STATUS "Will fetch Level Zero Loader from ${LEVEL_ZERO_LOADER_REPO}")
  include(FetchContent)
  FetchContent_Declare(level-zero-loader
    GIT_REPOSITORY    ${LEVEL_ZERO_LOADER_REPO}
    GIT_TAG           ${LEVEL_ZERO_LOADER_TAG}
  )

  # Workaround warnings/errors for Level Zero build
  set(CMAKE_CXX_FLAGS_BAK "${CMAKE_CXX_FLAGS}")
  if (WIN32)
    # FIXME: Level Zero build fails with /DUNICODE
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /UUNICODE")
    # USE_Z7 forces use of /Z7 instead of /Zi which is broken with sccache
    set(USE_Z7 ON)
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-truncation")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat-extra-semi")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option")
  endif()

  FetchContent_MakeAvailable(level-zero-loader)
  FetchContent_GetProperties(level-zero-loader)

  # Restore original flags
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_BAK}")

  set(LEVEL_ZERO_LIBRARY ze_loader)
  set(LEVEL_ZERO_INCLUDE_DIR
    ${level-zero-loader_SOURCE_DIR}/include CACHE PATH "Path to Level Zero Headers")
endif()

add_library (LevelZeroLoader INTERFACE)
# The MSVC linker does not like / at the start of a path, so to work around this
# we split it into a link library and a library path, where the path is allowed
# to have leading /.
get_filename_component(LEVEL_ZERO_LIBRARY_SRC "${LEVEL_ZERO_LIBRARY}" DIRECTORY)
get_filename_component(LEVEL_ZERO_LIB_NAME "${LEVEL_ZERO_LIBRARY}" NAME)
target_link_directories(LevelZeroLoader
  INTERFACE "${LEVEL_ZERO_LIBRARY_SRC}"
)
target_link_libraries(LevelZeroLoader
  INTERFACE "${LEVEL_ZERO_LIB_NAME}"
)

add_library (LevelZeroLoader-Headers INTERFACE)
target_include_directories(LevelZeroLoader-Headers
  INTERFACE "${LEVEL_ZERO_INCLUDE_DIR}"
)


add_library(${TARGET_NAME}
    SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_loader_interface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_common.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_context.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_device.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_event.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_mem.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_kernel.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_platform.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_program.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_queue.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_sampler.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_common.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_device.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_event.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_mem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_kernel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_platform.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_program.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_queue.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ur_level_zero_sampler.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/../ur.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../usm_allocator_config.hpp
)

set_target_properties(${TARGET_NAME} PROPERTIES
    VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
)

target_link_libraries(${TARGET_NAME} PRIVATE
    ${PROJECT_NAME}::headers
    ${PROJECT_NAME}::common
    LevelZeroLoader-Headers
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    LevelZeroLoader-Headers
)


if(UNIX)
    set(GCC_COVERAGE_COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden -fno-strict-aliasing")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")
endif()
