/*
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file enqueue_kernel.cpp
 *
 * Enqueue microbenchmark for measuring the effects of various queue parameters.
 */

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <nanobench.h>
#include <sstream>
#include <string>

#include "fixtures.hpp"
#include "ur_api.h"
#include "ur_print.hpp"

using namespace ankerl::nanobench;
using namespace ur;

struct {
    uint32_t nDim;
    size_t size[3];
    const char *name;
} kernelVariants[] = {{1, {1, 0, 0}, "small"}, {3, {128, 128, 128}, "large"}};

void runKernelVariants(Bench &bench, Device &device, ur_queue_flags_t flags,
                       const std::string &name) {
    auto context = Context(device);
    auto queue = Queue(context, device, flags);

    /* kernels are implemented in conformance/device_code */
    auto program = Program(context, "foo");
    auto kernel = program.createKernel(program.entry_points[0]);

    const size_t offset[] = {0, 0, 0};
    for (const auto &variant : kernelVariants) {
        bench.run(name + " - " + variant.name, [&] {
            urEnqueueKernelLaunch(queue.raw(), kernel.raw(), variant.nDim,
                                  offset, variant.size, nullptr, 0, nullptr,
                                  nullptr);
        });
        /* make sure everything finishes before starting another benchmark */
        urEnqueueEventsWaitWithBarrier(queue.raw(), 0, nullptr, nullptr);
    }
}

struct {
    ur_queue_flags_t flag;
    const char *name;
} queueVariants[] = {
    {UR_QUEUE_FLAG_SUBMISSION_BATCHED, "Enqueue Batched in-order"},
    {UR_QUEUE_FLAG_SUBMISSION_BATCHED |
         UR_QUEUE_FLAG_OUT_OF_ORDER_EXEC_MODE_ENABLE,
     "Enqueue Batched out-of-order"},
    {UR_QUEUE_FLAG_SUBMISSION_IMMEDIATE |
         UR_QUEUE_FLAG_OUT_OF_ORDER_EXEC_MODE_ENABLE,
     "Enqueue Immcmd out-of-order"},
    {UR_QUEUE_FLAG_SUBMISSION_IMMEDIATE, "Enqueue Immcmd in-order"},
};

void runQueueVariants(Bench &bench, Device &device) {
    for (const auto &variant : queueVariants) {
        runKernelVariants(bench, device, variant.flag,
                          device.platform().name() + " - " + device.name() +
                              " - " + variant.name);
    }
}

int main(int argc, const char *argv[]) {
    uint64_t epochs = 1000;
    uint64_t epochIters = 1000;
    if (argc == 3) {
        try {
            epochIters = std::stoull(argv[1]);
            epochIters = std::stoull(argv[2]);
        } catch (const std::exception &e) {
            std::cerr << "Invalid arguments. Usage: " << argv[0]
                      << " [epochs] [epochIters]" << std::endl;
            return -1;
        }
    }

    UR ur;

    Bench bench;
    bench.epochs(epochs);
    bench.epochIterations(epochIters);

    for (auto &adapter : ur.adapters) {
        for (auto &platform : adapter.platforms) {
            if (!Program::file_il_ext(platform.backend())) {
                continue; /* unsupported platform */
            }

            for (auto &device : platform.devices) {
                runQueueVariants(bench, device);
            }
        }
    }

    std::ofstream renderOut("enqueue_kernel.html");
    render(ankerl::nanobench::templates::htmlBoxplot(), bench, renderOut);

    return 0;
}
