// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <uur/fixtures.h>
#include <uur/raii.h>

struct urEnqueueKernelLaunchMultiDeviceMultiplyTest
    : public uur::urMultiDeviceQueueTest {
    static constexpr char ProgramName[] = "multiply";
    static constexpr size_t ArraySize = 100;
    static constexpr size_t InitialValue = 10;
    static constexpr size_t duplicateDevices = 10;

    std::vector<ur_device_handle_t> devices;
    std::string KernelName;
    std::vector<uur::raii::Program> Programs;
    std::vector<uur::raii::Kernel> Kernels;
    std::vector<void *> SharedMem;

    void SetUp() override {
        UUR_RETURN_ON_FATAL_FAILURE(urMultiDeviceQueueTest::SetUp());

        devices = uur::KernelsEnvironment::instance->devices;
        auto originalQueues = queues;

        for (size_t i = 0; i < duplicateDevices; i++) {
            devices.insert(devices.end(),
                           uur::KernelsEnvironment::instance->devices.begin(),
                           uur::KernelsEnvironment::instance->devices.end());

            queues.insert(queues.end(), originalQueues.begin(),
                          originalQueues.end());
        }

        Programs.resize(devices.size());
        Kernels.resize(devices.size());
        SharedMem.resize(devices.size());

        KernelName = uur::KernelsEnvironment::instance->GetEntryPointNames(
            ProgramName)[0];

        std::shared_ptr<std::vector<char>> il_binary;
        std::vector<ur_program_metadata_t> metadatas{};

        uur::KernelsEnvironment::instance->LoadSource(ProgramName, il_binary);

        for (size_t i = 0; i < devices.size(); i++) {
            const ur_program_properties_t properties = {
                UR_STRUCTURE_TYPE_PROGRAM_PROPERTIES, nullptr,
                static_cast<uint32_t>(metadatas.size()),
                metadatas.empty() ? nullptr : metadatas.data()};

            uur::raii::Program Program;
            ASSERT_SUCCESS(uur::KernelsEnvironment::instance->CreateProgram(
                platform, context, devices[i], *il_binary, &properties,
                Programs[i].ptr()));

            ASSERT_SUCCESS(urProgramBuildExp(Programs[i].get(), devices.size(),
                                             devices.data(), nullptr));
            ASSERT_SUCCESS(urKernelCreate(Programs[i].get(), KernelName.data(),
                                          Kernels[i].ptr()));

            ASSERT_SUCCESS(
                urUSMSharedAlloc(context, devices[i], nullptr, nullptr,
                                 ArraySize * sizeof(uint32_t), &SharedMem[i]));

            ASSERT_SUCCESS(urEnqueueUSMFill(queues[i], SharedMem[i],
                                            sizeof(uint32_t), &InitialValue,
                                            ArraySize * sizeof(uint32_t), 0,
                                            nullptr, nullptr /* &Event */));
            ASSERT_SUCCESS(urKernelSetArgPointer(Kernels[i].get(), 0, nullptr,
                                                 SharedMem[i]));
        }

        size_t returned_size;
        ASSERT_SUCCESS(urDeviceGetInfo(devices[0], UR_DEVICE_INFO_EXTENSIONS, 0,
                                       nullptr, &returned_size));

        std::unique_ptr<char[]> returned_extensions(new char[returned_size]);

        ASSERT_SUCCESS(urDeviceGetInfo(devices[0], UR_DEVICE_INFO_EXTENSIONS,
                                       returned_size, returned_extensions.get(),
                                       nullptr));

        std::string_view extensions_string(returned_extensions.get());
        const bool usm_p2p_support =
            extensions_string.find(UR_USM_P2P_EXTENSION_STRING_EXP) !=
            std::string::npos;

        if (!usm_p2p_support) {
            GTEST_SKIP() << "EXP usm p2p feature is not supported.";
        }
    }

    void TearDown() override {
        for (auto &Ptr : SharedMem) {
            urUSMFree(context, Ptr);
        }
        UUR_RETURN_ON_FATAL_FAILURE(urMultiDeviceQueueTest::TearDown());
    }
};

TEST_F(urEnqueueKernelLaunchMultiDeviceMultiplyTest, Success) {
    constexpr size_t global_offset = 0;
    constexpr size_t n_dimensions = 1;

    std::vector<uur::raii::Event> Events(devices.size() * 2);
    for (size_t i = 0; i < devices.size(); i++) {
        size_t waitNum = i > 0 ? 1 : 0;
        auto lastEvent = i > 0 ? Events[i * 2 - 1].ptr() : nullptr;
        auto kernelEvent = Events[i * 2].ptr();
        auto memcpyEvent = Events[i * 2 + 1].ptr();

        ASSERT_SUCCESS(urEnqueueKernelLaunch(
            queues[i], Kernels[i].get(), n_dimensions, &global_offset,
            &ArraySize, nullptr, waitNum, lastEvent, kernelEvent));

        if (i < devices.size() - 1) {
            ASSERT_SUCCESS(urEnqueueUSMMemcpy(
                queues[i], false, SharedMem[i + 1], SharedMem[i],
                ArraySize * sizeof(uint32_t), 1, kernelEvent, memcpyEvent));
        }
    }

    for (auto Queue : queues) {
        ASSERT_SUCCESS(urQueueFinish(Queue));
    }

    size_t ExpectedValue = InitialValue;
    for (size_t i = 0; i < devices.size(); i++) {
        ExpectedValue *= 2;
        for (uint32_t j = 0; j < ArraySize; ++j) {
            ASSERT_EQ(reinterpret_cast<uint32_t *>(SharedMem[i])[j],
                      ExpectedValue);
        }
    }
}
