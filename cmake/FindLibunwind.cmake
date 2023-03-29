# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: MIT

#
# FindLibunwind.cmake -- module searching for libunwind library.
#                        LIBUNWIND_FOUND is set to true if libunwind is found.
#

find_library(LIBUNWIND_LIBRARIES NAMES unwind)
find_path(LIBUNWIND_INCLUDE_DIR NAMES libunwind.h)

if (LIBUNWIND_LIBRARIES AND LIBUNWIND_INCLUDE_DIR)
    set(LIBUNWIND_FOUND TRUE)
endif()

if (LIBUNWIND_FOUND)
    add_library(Libunwind INTERFACE IMPORTED)
    set_target_properties(Libunwind PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LIBUNWIND_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${LIBUNWIND_LIBRARIES}"
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libunwind DEFAULT_MSG LIBUNWIND_LIBRARIES LIBUNWIND_INCLUDE_DIR)
