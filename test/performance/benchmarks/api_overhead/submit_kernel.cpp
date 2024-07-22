// Copyright (C) 2022-2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <ur_api.h>

#include "file.hpp"
#include "state.hpp"
#include "statistics.hpp"
#include "timer.hpp"

static inline void checkError(ur_result_t error) {
    if (error != UR_RESULT_SUCCESS) {
        std::cerr << "Benchmark failed" << std::endl;
        exit(1);
    }
}

static constexpr size_t n_dimensions = 3;
static constexpr size_t global_size[] = {1, 1, 1};
static constexpr size_t local_size[] = {1, 1, 1};
static constexpr bool measureCompletion = true;

int main(int argc, char *argv[]) {
    if (argc < 7) {
        std::cerr << "Usage: " << argv[0]
                  << " kernelSource kernelName numKernels numIterations "
                     "kernelOperationsCount discardEvents"
                  << std::endl;
        return 1;
    }

    std::string kernelSource = argv[1];
    std::string kernelName = argv[2];
    int numKernels = std::stoi(argv[3]);
    int numIterations = std::stoi(argv[4]);
    int kernelOperationsCount = std::stoi(argv[5]);
    bool discardedEvents = std::stoi(argv[6]);

    urState state;

    auto spirv = FileHelper::loadBinaryFile(kernelSource);
    if (spirv.empty()) {
        std::cerr << "Kernel not found" << std::endl;
        return 1;
    }

    uur::raii::Program program;
    checkError(urProgramCreateWithIL(state.context.get(), spirv.data(),
                                     spirv.size(), nullptr, program.ptr()));

    checkError(urProgramBuild(state.context.get(), program.get(), nullptr));

    uur::raii::Kernel kernel;
    checkError(urKernelCreate(program.get(), kernelName.c_str(), kernel.ptr()));

    uur::raii::Queue queue;
    ur_queue_properties_t queueProperties = {};
    queueProperties.flags = UR_QUEUE_FLAG_SUBMISSION_IMMEDIATE;

    checkError(urQueueCreate(state.context.get(), state.device.get(),
                             &queueProperties, queue.ptr()));

    std::vector<uur::raii::Event> events(numKernels);

    // warmup
    for (auto iteration = 0u; iteration < numKernels; iteration++) {
        checkError(urKernelSetArgValue(
            kernel.get(), 0, sizeof(int), nullptr,
            reinterpret_cast<void *>(&kernelOperationsCount)));

        ur_event_handle_t *signalEvent = nullptr;
        if (!discardedEvents) {
            signalEvent = events[iteration].ptr();
        }

        checkError(urEnqueueKernelLaunch(
            queue.get(), kernel.get(), n_dimensions, nullptr, &local_size[0],
            &global_size[0], 0, nullptr, signalEvent));
    }

    events.clear();
    events.resize(numKernels);

    Timer timer;
    std::vector<double> samples;

    // Benchmark
    for (auto i = 0u; i < numIterations; i++) {
        timer.measureStart();
        for (auto iteration = 0u; iteration < numKernels; iteration++) {
            checkError(urKernelSetArgValue(
                kernel.get(), 0, sizeof(int), nullptr,
                reinterpret_cast<void *>(&kernelOperationsCount)));

            ur_event_handle_t *signalEvent = nullptr;
            if (!discardedEvents) {
                signalEvent = events[iteration].ptr();
            }

            checkError(urEnqueueKernelLaunch(
                queue.get(), kernel.get(), n_dimensions, nullptr,
                &local_size[0], &global_size[0], 0, nullptr, signalEvent));
        }

        if (!measureCompletion) {
            timer.measureEnd();
            auto us =
                std::chrono::duration<double, std::micro>(timer.get()).count();
            samples.push_back(us);
        }

        checkError(urQueueFinish(queue.get()));

        if (measureCompletion) {
            timer.measureEnd();
            auto us =
                std::chrono::duration<double, std::micro>(timer.get()).count();
            samples.push_back(us);
        }
    }

    std::string testCase =
        "api_overhead_benchmark(api=UR DiscardedEvents=" +
        std::to_string(discardedEvents) +
        " NumKernels=" + std::to_string(numKernels) +
        " kernelExecTime=" + std::to_string(kernelOperationsCount) + ")";
    printStatistics(testCase, "us", samples);
}
