/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */
#include "backtrace.hpp"

#include <cxxabi.h>
#include <execinfo.h>
#include <vector>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

namespace validation_layer {

std::vector<BacktraceLine> getCurrentBacktrace() {
    unw_cursor_t cursor;
    unw_context_t context;
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    if (unw_step(&cursor) <= 0) {
        return std::vector<BacktraceLine>(1, "Failed to acquire a backtrace");
    }

    std::vector<BacktraceLine> backtrace;
    try {
        char proc_name[256];
        unw_word_t ip, offset;
        std::stringstream backtraceLine;

        do {
            if (unw_get_proc_name(&cursor, proc_name, sizeof(proc_name),
                                  &offset) != 0) {
                backtrace.push_back("????????");
            } else if (unw_get_reg(&cursor, UNW_REG_IP, &ip) != 0) {
                backtraceLine.str("");
                backtraceLine << "(" << proc_name << "+"
                              << std::to_string(offset) << ") [????????]";
                backtrace.push_back(backtraceLine.str());
            } else {
                backtraceLine.str("");
                backtraceLine << "(" << proc_name << "+"
                              << std::to_string(offset) << ") [0x" << std::hex
                              << ip << "]";
                backtrace.push_back(backtraceLine.str());
            }
        } while (unw_step(&cursor) > 0);
    } catch (std::bad_alloc &) {
        return std::vector<BacktraceLine>(1, "Failed to acquire a backtrace");
    }

    return backtrace;
}

} // namespace validation_layer
