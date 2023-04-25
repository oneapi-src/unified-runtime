# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: MIT

add_executable(vector_addition
    ${CMAKE_CURRENT_SOURCE_DIR}/vector_addition.cpp
)
add_executable(vector_addition_buffer
    ${CMAKE_CURRENT_SOURCE_DIR}/vector_addition_buffer.cpp
)

if(MSVC)
    set_target_properties(vector_addition
        PROPERTIES
            VS_DEBUGGER_COMMAND_ARGUMENTS ""
            VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)"
    )
    set_target_properties(vector_addition_buffer
        PROPERTIES
            VS_DEBUGGER_COMMAND_ARGUMENTS ""
            VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)"
    )
endif()

target_link_libraries(vector_addition PUBLIC
    ${PROJECT_NAME}::headers
    ${PROJECT_NAME}::loader
    ${PROJECT_NAME}::common
    ${CMAKE_DL_LIBS}
)
target_link_libraries(vector_addition_buffer PUBLIC
    ${PROJECT_NAME}::headers
    ${PROJECT_NAME}::loader
    ${PROJECT_NAME}::common
    ${CMAKE_DL_LIBS}
)
