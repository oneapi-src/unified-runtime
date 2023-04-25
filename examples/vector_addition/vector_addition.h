// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: MIT

#ifndef UR_EXAMPLES_VECTOR_ADDITION_H_INCLUDED
#define UR_EXAMPLES_VECTOR_ADDITION_H_INCLUDED
#include <cstring>
#include <iostream>
#include <memory>
#include <string_view>
#include <ur_api.h>
#include <vector>

#define UR_CHECK_ERR(IT, MESSAGE, RETURN)                                      \
    if (ur_result_t theErr = IT) {                                             \
        std::cout << #MESSAGE << ": " << theErr << std::endl;                  \
        return RETURN;                                                         \
    }

/*
CUDA PTX obtained from the following kernel.

// CUDA kernel. Each thread takes care of one element of c
__global__ void vecAdd(double *a, double *b, double *c, int n) {
    // Get our global thread ID
    int id = blockIdx.x * blockDim.x + threadIdx.x;

    // Make sure we do not go out of bounds
    if (id < n) {
        c[id] = a[id] + b[id];
    }
}

 * CUDA code obtained using the following commands:
 * 
 * nvcc -c cudaVecAddKernel.cu
 * cuobjdump --dump-ptx cudaVecAddKernel.o
*/
constexpr std::string_view cudaPtx =
    ".version 6.4\n"
    ".target sm_30\n"
    ".address_size 64\n"
    ".visible .entry _Z6vecAddPdS_S_i(\n"
    ".param .u64 _Z6vecAddPdS_S_i_param_0,\n"
    ".param .u64 _Z6vecAddPdS_S_i_param_1,\n"
    ".param .u64 _Z6vecAddPdS_S_i_param_2,\n"
    ".param .u32 _Z6vecAddPdS_S_i_param_3\n"
    ")\n"
    "{\n"
    ".reg .pred %p<2>;\n"
    ".reg .b32 %r<6>;\n"
    ".reg .f64 %fd<4>;\n"
    ".reg .b64 %rd<11>;\n"
    "\n"
    "\n"
    "ld.param.u64 %rd1, [_Z6vecAddPdS_S_i_param_0];\n"
    "ld.param.u64 %rd2, [_Z6vecAddPdS_S_i_param_1];\n"
    "ld.param.u64 %rd3, [_Z6vecAddPdS_S_i_param_2];\n"
    "ld.param.u32 %r2, [_Z6vecAddPdS_S_i_param_3];\n"
    "mov.u32 %r3, %ctaid.x;\n"
    "mov.u32 %r4, %ntid.x;\n"
    "mov.u32 %r5, %tid.x;\n"
    "mad.lo.s32 %r1, %r4, %r3, %r5;\n"
    "setp.ge.s32     %p1, %r1, %r2;\n"
    "@%p1 bra BB0_2;\n"
    "\n"
    "cvta.to.global.u64 %rd4, %rd1;\n"
    "mul.wide.s32 %rd5, %r1, 8;\n"
    "add.s64 %rd6, %rd4, %rd5;\n"
    "cvta.to.global.u64 %rd7, %rd2;\n"
    "add.s64 %rd8, %rd7, %rd5;\n"
    "ld.global.f64 %fd1, [%rd8];\n"
    "ld.global.f64 %fd2, [%rd6];\n"
    "add.f64 %fd3, %fd2, %fd1;\n"
    "cvta.to.global.u64 %rd9, %rd3;\n"
    "add.s64 %rd10, %rd9, %rd5;\n"
    "st.global.f64 [%rd10], %fd3;\n"
    "\n"
    "BB0_2:\n"
    "ret;\n"
    "}\n";

using UrInitPtr = std::unique_ptr<bool, void (*)(bool *)>;
inline UrInitPtr initUr() {
    auto UrDeleter = [](bool *initialized) {
        if (initialized) {
            if (*initialized) {
                std::cout << "Tearing Down\n.";
                // TODO - Tear-down params are not optional yet.
                int x;
                urTearDown(&x);
            }
            delete initialized;
        }
    };

    ur_result_t err = urInit(0);
    bool init_success = !err;

    return UrInitPtr(new bool(init_success), std::move(UrDeleter));
}

inline ur_platform_handle_t GetCudaPlatform() {

    uint32_t n_platforms = 0;
    ur_result_t err = urPlatformGet(0, nullptr, &n_platforms);
    if (n_platforms == 0 || err) {
        return nullptr;
    }
    std::vector<ur_platform_handle_t> platforms(n_platforms);
    err = urPlatformGet(n_platforms, platforms.data(), nullptr);
    if (err) {
        return nullptr;
    }

    // find a cuda compatible platform
    for (auto hPlatform : platforms) {
        size_t platform_name_len = 0;
        err = urPlatformGetInfo(hPlatform, UR_PLATFORM_INFO_NAME, 0, nullptr,
                                &platform_name_len);
        if (err || platform_name_len == 0) {
            return nullptr;
        }
        char *platform_name = (char *)alloca(platform_name_len);
        err = urPlatformGetInfo(hPlatform, UR_PLATFORM_INFO_NAME,
                                platform_name_len, platform_name, nullptr);
        if (err) {
            return nullptr;
        }
        const char *cudaBackendName = "NVIDIA CUDA BACKEND";
        if (std::strcmp(platform_name, cudaBackendName) == 0) {
            return hPlatform;
        }
    }
    return nullptr;
}

using DeviceUniquePtr =
    std::unique_ptr<ur_device_handle_t_, void (*)(ur_device_handle_t)>;
inline DeviceUniquePtr GetDevice(ur_platform_handle_t platform) {

    auto DeviceDeleter = [](ur_device_handle_t dev) {
        if (dev) {
            std::cout << "Releasing Device\n";
            urDeviceRelease(dev);
        }
    };

    uint32_t n_devices = 0;
    ur_result_t err =
        urDeviceGet(platform, UR_DEVICE_TYPE_ALL, 0, nullptr, &n_devices);
    if (err || n_devices == 0) {
        return DeviceUniquePtr(nullptr, std::move(DeviceDeleter));
    }

    // fetch only 1 device
    ur_device_handle_t device = nullptr;
    err = urDeviceGet(platform, UR_DEVICE_TYPE_ALL, 1, &device, nullptr);
    if (err || !device) {
        return DeviceUniquePtr(nullptr, std::move(DeviceDeleter));
    }

    return DeviceUniquePtr(device, std::move(DeviceDeleter));
}

using CtxUniquePtr =
    std::unique_ptr<ur_context_handle_t_, void (*)(ur_context_handle_t)>;
inline CtxUniquePtr GetContext(ur_device_handle_t device) {

    auto CtxDeleter = [](ur_context_handle_t ctx) {
        if (ctx) {
            std::cout << "Releasing Ctx.\n";
            urContextRelease(ctx);
        }
    };

    ur_context_handle_t ctx = nullptr;
    ur_result_t err = urContextCreate(1, &device, nullptr, &ctx);
    if (err || !ctx) {
        std::cout << "Failed to create context.\n";
        return CtxUniquePtr(nullptr, std::move(CtxDeleter));
    }
    return CtxUniquePtr(ctx, std::move(CtxDeleter));
}

using QueueUniquePtr =
    std::unique_ptr<ur_queue_handle_t_, void (*)(ur_queue_handle_t)>;
inline QueueUniquePtr GetQueue(ur_context_handle_t ctx,
                               ur_device_handle_t device) {

    auto QueueDeleter = [](ur_queue_handle_t queue) {
        if (queue) {
            std::cout << "Releasing Queue.\n";
            urQueueRelease(queue);
        }
    };

    ur_queue_handle_t queue = nullptr;
    ur_result_t err = urQueueCreate(ctx, device, nullptr, &queue);
    if (err || !queue) {
        std::cout << "Failed to create Queue.\n";
        return QueueUniquePtr(nullptr, std::move(QueueDeleter));
    }
    return QueueUniquePtr(queue, std::move(QueueDeleter));
}

using ProgramUniquePtr =
    std::unique_ptr<ur_program_handle_t_, void (*)(ur_program_handle_t)>;
inline ProgramUniquePtr GetAndBuildProgram(ur_context_handle_t ctx) {

    auto ProgramDeleter = [](ur_program_handle_t prog) {
        if (prog) {
            std::cout << "Releasing Program.\n";
            urProgramRelease(prog);
        }
    };

    ur_program_handle_t program = nullptr;
    // TODO - properties argument should be optional - but SEGFAULTs without it.
    ur_program_properties_t prog_props{UR_STRUCTURE_TYPE_PROGRAM_PROPERTIES,
                                       nullptr, 0, nullptr};
    ur_result_t err = urProgramCreateWithIL(ctx, cudaPtx.data(), cudaPtx.size(),
                                            &prog_props, &program);

    if (err || !program) {
        std::cout << "Failed to compile program.\n";
        return ProgramUniquePtr(nullptr, std::move(ProgramDeleter));
    }

    err = urProgramBuild(ctx, program, nullptr);
    if (err) {
        std::cout << "Failed to build program.\n";
        urProgramRelease(program);
        return ProgramUniquePtr(nullptr, std::move(ProgramDeleter));
    }
    return ProgramUniquePtr(program, std::move(ProgramDeleter));
}

using KernelUniquePtr =
    std::unique_ptr<ur_kernel_handle_t_, void (*)(ur_kernel_handle_t)>;
inline KernelUniquePtr GetKernel(ur_program_handle_t program,
                                 const std::string &kernel_name) {

    auto KernelDeleter = [](ur_kernel_handle_t kernel) {
        if (kernel) {
            std::cout << "Releasing Kernel.\n";
            urKernelRelease(kernel);
        }
    };

    ur_kernel_handle_t kernel = nullptr;
    ur_result_t err = urKernelCreate(program, kernel_name.c_str(), &kernel);
    if (err || !kernel) {
        std::cout << "Failed to find kernel: " << kernel_name << std::endl;
        return KernelUniquePtr(nullptr, std::move(KernelDeleter));
    }
    return KernelUniquePtr(kernel, std::move(KernelDeleter));
}

struct USMAllocation {
    void *devicePtr = nullptr;
    ur_context_handle_t ctx;
};

using USMUniquePtr = std::unique_ptr<USMAllocation, void (*)(USMAllocation *)>;
inline USMUniquePtr GetDeviceAlloc(ur_context_handle_t ctx,
                                   ur_device_handle_t device, size_t size) {

    auto AllocDeleter = [](USMAllocation *alloc) {
        if (alloc) {
            if (alloc->devicePtr) {
                std::cout << "Freeing USM Device Allocation.\n";
                urUSMFree(alloc->ctx, alloc->devicePtr);
            }
            delete alloc;
        }
    };
    auto allocPtr =
        USMUniquePtr(new USMAllocation{nullptr, ctx}, std::move(AllocDeleter));

    ur_usm_desc_t usmDesc{UR_STRUCTURE_TYPE_USM_DESC, nullptr, 0u,
                          UR_USM_ADVICE_DEFAULT, 0u};
    ur_result_t err = urUSMDeviceAlloc(ctx, device, &usmDesc, nullptr, size,
                                       &allocPtr->devicePtr);
    if (err) {
        std::cout << "Failed to allocate USM Device Ptr.\n";
        allocPtr->devicePtr = nullptr;
    }
    return allocPtr;
}

using BufferUniquePtr =
    std::unique_ptr<ur_mem_handle_t_, void (*)(ur_mem_handle_t)>;
inline BufferUniquePtr GetBuffer(ur_context_handle_t ctx, size_t size) {

    auto BufferDeleter = [](ur_mem_handle_t mem) {
        if (mem) {
            std::cout << "Releasing Buffer.\n";
            urMemRelease(mem);
        }
    };

    ur_mem_handle_t mem_buf = nullptr;
    ur_buffer_properties_t bufProps{UR_STRUCTURE_TYPE_BUFFER_PROPERTIES,
                                    nullptr, nullptr};
    ur_result_t res = urMemBufferCreate(ctx, UR_MEM_FLAG_READ_WRITE, size,
                                        &bufProps, &mem_buf);

    if (res || !mem_buf) {
        std::cout << "";
        return BufferUniquePtr(nullptr, std::move(BufferDeleter));
    }
    return BufferUniquePtr(mem_buf, std::move(BufferDeleter));
}

inline void printVector(const double *vec, size_t size) {
    std::cout << "[ ";
    for (size_t i = 0; i < size; ++i) {
        std::cout << vec[i] << ",\t";
    }
    std::cout << "]\n";
}

#endif // UR_EXAMPLES_VECTOR_ADDITION_H_INCLUDED
