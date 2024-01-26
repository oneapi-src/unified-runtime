/*
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 * @file fixtures.hpp
 *
 * Implementation of simple C++ wrappers for UR types. Reduces boilerplate when
 * writing benchmarks.
 */

#pragma once

#include "kernel_entry_points.h"
#include "ur_api.h"
#include <assert.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#define UR_ASSERT(f)                                                           \
    do {                                                                       \
        auto result = (f);                                                     \
        assert(result == UR_RESULT_SUCCESS);                                   \
    } while (0)

namespace ur {

template <typename HandleType, ur_result_t (*release)(HandleType)>
struct HandleDeleter {
    void operator()(HandleType handle) {
        if (release && handle) {
            release(handle);
        }
    }
};

/* Handle wrapper that ensures release is called exactly once */
template <typename HandleType, ur_result_t (*release)(HandleType) = nullptr>
class Handle {
  public:
    Handle(HandleType handle, bool owned = true) : handle(handle) {}
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    Handle(Handle &&) = default;
    Handle &operator=(Handle &&) = default;

    using deleter = HandleDeleter<HandleType, release>;
    const HandleType raw() { return handle.get(); }

  private:
    std::unique_ptr<typename std::remove_pointer<HandleType>::type, deleter>
        handle;
};

class Platform;

using DeviceHandle = Handle<ur_device_handle_t>;
class Device : public DeviceHandle {
  public:
    Device(ur_device_handle_t handle) : DeviceHandle(handle) {}
    std::string name() {
        size_t len = 0;
        UR_ASSERT(
            urDeviceGetInfo(raw(), UR_DEVICE_INFO_NAME, 0, nullptr, &len));
        std::string n;
        n.resize(len);

        UR_ASSERT(urDeviceGetInfo(raw(), UR_DEVICE_INFO_NAME, len, n.data(),
                                  nullptr));
        n.pop_back();
        return n;
    }
    Platform platform();
};

using PlatformHandle = Handle<ur_platform_handle_t>;
class Platform : public PlatformHandle {
  public:
    Platform(ur_platform_handle_t _handle) : PlatformHandle(_handle) {
        uint32_t ndevices = 0;
        UR_ASSERT(
            urDeviceGet(raw(), UR_DEVICE_TYPE_GPU, 0, nullptr, &ndevices));
        if (ndevices == 0) {
            return;
        }
        std::vector<ur_device_handle_t> handles(ndevices);
        UR_ASSERT(urDeviceGet(raw(), UR_DEVICE_TYPE_GPU, ndevices,
                              handles.data(), nullptr));
        for (auto device : handles) {
            devices.emplace_back(device);
        }
    }

    ur_platform_backend_t backend() {
        ur_platform_backend_t backend;
        UR_ASSERT(urPlatformGetInfo(raw(), UR_PLATFORM_INFO_BACKEND,
                                    sizeof(backend), &backend, nullptr));

        return backend;
    }

    std::string name() {
        size_t len = 0;
        UR_ASSERT(
            urPlatformGetInfo(raw(), UR_PLATFORM_INFO_NAME, 0, nullptr, &len));
        std::string n;
        n.resize(len);

        UR_ASSERT(urPlatformGetInfo(raw(), UR_PLATFORM_INFO_NAME, len, n.data(),
                                    nullptr));

        n.pop_back(); /* gets rid of trailing character */

        return n;
    }

    std::vector<Device> devices;
};

inline Platform Device::platform() {
    ur_platform_handle_t platform;
    UR_ASSERT(urDeviceGetInfo(raw(), UR_DEVICE_INFO_PLATFORM,
                              sizeof(ur_platform_handle_t), &platform,
                              nullptr));

    return platform;
}

using AdapterHandle = Handle<ur_adapter_handle_t, urAdapterRelease>;
class Adapter : public AdapterHandle {
  public:
    Adapter(ur_adapter_handle_t _handle) : AdapterHandle(_handle) {
        uint32_t nplatforms = 0;
        ur_adapter_handle_t handle = raw();
        UR_ASSERT(urPlatformGet(&handle, 1, 0, nullptr, &nplatforms));
        assert(nplatforms > 0);
        std::vector<ur_platform_handle_t> handles(nplatforms);
        UR_ASSERT(
            urPlatformGet(&handle, 1, nplatforms, handles.data(), nullptr));
        for (auto platform : handles) {
            platforms.emplace_back(platform);
        }
    }

    std::vector<Platform> platforms;
};

using ContextHandle = Handle<ur_context_handle_t, urContextRelease>;
class Context : public ContextHandle {
  public:
    Context(Device &device) : ContextHandle(createContext(device)) {}

    std::vector<Device> devices() {
        size_t nbytes;
        UR_ASSERT(urContextGetInfo(raw(), UR_CONTEXT_INFO_DEVICES, 0, nullptr,
                                   &nbytes));
        std::vector<ur_device_handle_t> devices(nbytes /
                                                sizeof(ur_context_handle_t));
        UR_ASSERT(urContextGetInfo(raw(), UR_CONTEXT_INFO_DEVICES, nbytes,
                                   devices.data(), nullptr));

        std::vector<Device> wrapped;
        for (auto d : devices) {
            /*
            * Returning Device is safe here because that type does not automatically
            * release the underlying handle. Otherwise we'd need to `ur...Retain` here.
            */
            wrapped.emplace_back(d);
        }

        return wrapped;
    }

    Platform platform() { return devices().front().platform(); }

  private:
    static ur_context_handle_t createContext(Device &device) {
        ur_context_handle_t h;
        ur_device_handle_t dev = device.raw();
        UR_ASSERT(urContextCreate(1, &dev, nullptr, &h));
        return h;
    }
};

using QueueHandle = Handle<ur_queue_handle_t, urQueueRelease>;
class Queue : public QueueHandle {
  public:
    Queue(Context &context, Device &device, ur_queue_flags_t flags)
        : QueueHandle(createQueue(context, device, flags)) {}

  private:
    static ur_queue_handle_t createQueue(Context &context, Device &device,
                                         ur_queue_flags_t flags) {
        ur_queue_properties_t props;
        props.flags = flags;
        props.pNext = nullptr;
        props.stype = UR_STRUCTURE_TYPE_QUEUE_PROPERTIES;
        ur_queue_handle_t queue;
        UR_ASSERT(urQueueCreate(context.raw(), device.raw(), &props, &queue));

        return queue;
    }
};

using KernelHandle = Handle<ur_kernel_handle_t, urKernelRelease>;
class Kernel : public KernelHandle {
  public:
    Kernel(ur_kernel_handle_t handle) : KernelHandle(handle) {}
};

using ProgramHandle = Handle<ur_program_handle_t, urProgramRelease>;
class Program : public ProgramHandle {
  public:
    Program(ur_program_handle_t handle) : ProgramHandle(handle) {}
    Program(Context &context, const std::string &name)
        : entry_points(entryPoints(name)),
          ProgramHandle(createProgram(context, name)) {}

    Kernel createKernel(const std::string &entry_point) {
        ur_kernel_handle_t kernel;
        UR_ASSERT(urKernelCreate(raw(), entry_point.c_str(), &kernel));
        return kernel;
    }

    static const char *file_il_ext(ur_platform_backend_t backend) {
        switch (backend) {
        case UR_PLATFORM_BACKEND_LEVEL_ZERO:
        case UR_PLATFORM_BACKEND_OPENCL:
            return "sycl_spir64.spv";
        default:
            return nullptr;
        }
        return nullptr;
    }
    std::vector<std::string> entry_points;

  private:
    static std::string loadFile(const std::string &path) {
        std::ifstream input_file(path);
        if (!input_file.is_open()) {
            return "";
        }
        std::stringstream buffer;
        buffer << input_file.rdbuf();
        return buffer.str();
    }

    static std::vector<std::string> entryPoints(const std::string &name) {
        auto iter = uur::device_binaries::program_kernel_map.find(name);
        assert(iter != uur::device_binaries::program_kernel_map.end());
        return iter->second;
    }

    static ur_program_handle_t createProgram(Context &context,
                                             const std::string &name) {
        auto il_ext = file_il_ext(context.platform().backend());
        assert(il_ext);

        std::filesystem::path path(KERNEL_IL_PATH);
        path /= name;
        path /= il_ext;

        std::string il = loadFile(path);
        assert(!il.empty());

        ur_program_handle_t program;
        UR_ASSERT(urProgramCreateWithIL(context.raw(), il.data(), il.size(),
                                        nullptr, &program));
        UR_ASSERT(urProgramBuild(context.raw(), program, nullptr));

        return program;
    }
};

class Loader {
  public:
    Loader() { UR_ASSERT(urLoaderInit(0, nullptr)); }
    ~Loader() { UR_ASSERT(urLoaderTearDown()); }
};

class UR {
  public:
    UR() {
        uint32_t nadapters = 0;
        UR_ASSERT(urAdapterGet(0, nullptr, &nadapters));
        assert(nadapters > 0);
        std::vector<ur_adapter_handle_t> adapterHandles(nadapters);
        UR_ASSERT(urAdapterGet(nadapters, adapterHandles.data(), nullptr));
        for (auto handle : adapterHandles) {
            adapters.emplace_back(handle);
        }
    }

    UR(const UR &) = delete;
    UR &operator=(const UR &) = delete;
    UR(UR &&) = default;
    UR &operator=(UR &&) = default;

    Loader loader; /* must be destroyed last */
    std::vector<Adapter> adapters;
};

} // namespace ur
