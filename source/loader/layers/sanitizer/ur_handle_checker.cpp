#include "ur_handle_checker.hpp"

#define WRAP(old_func_ptr, new_func_ptr)                                       \
    do {                                                                       \
        if (old_func_ptr != nullptr) {                                         \
            old_func_ptr = new_func_ptr;                                       \
        }                                                                      \
    } while (0)

namespace {
struct UrApiWrapper {
    static ur_dditable_t urDdiTable;
    static ur_sanitizer_layer::UrHandleChecker *urHandleChecker;
    static void wrapDditable(ur_sanitizer_layer::UrHandleChecker *checker,
                             ur_dditable_t *DdiTable) {
        // Wrap the UR DDI table
        UrApiWrapper::urHandleChecker = checker;
        UrApiWrapper::urDdiTable = *DdiTable;

        WRAP(DdiTable->Context.pfnCreate, UrApiWrapper::wrapUrContextCreate);
        WRAP(DdiTable->Context.pfnCreateWithNativeHandle,
             UrApiWrapper::wrapUrContextCreateWithNativeHandle);
        WRAP(DdiTable->Context.pfnRetain, UrApiWrapper::wrapUrContextRetain);
        WRAP(DdiTable->Context.pfnRelease, UrApiWrapper::wrapUrContextRelease);

        WRAP(DdiTable->Kernel.pfnCreate, UrApiWrapper::wrapUrKernelCreate);
        WRAP(DdiTable->Kernel.pfnCreateWithNativeHandle,
             UrApiWrapper::wrapUrKernelCreateWithNativeHandle);
        WRAP(DdiTable->Kernel.pfnRetain, UrApiWrapper::wrapUrKernelRetain);
        WRAP(DdiTable->Kernel.pfnRelease, UrApiWrapper::wrapUrKernelRelease);

        WRAP(DdiTable->Queue.pfnCreate, UrApiWrapper::wrapUrQueueCreate);
        WRAP(DdiTable->Queue.pfnRetain, UrApiWrapper::wrapUrQueueRetain);
        WRAP(DdiTable->Queue.pfnRelease, UrApiWrapper::wrapUrQueueRelease);

        WRAP(DdiTable->Program.pfnCreateWithIL,
             UrApiWrapper::wrapUrProgramCreateWithIL);
        WRAP(DdiTable->Program.pfnCreateWithBinary,
             UrApiWrapper::wrapUrProgramCreateWithBinary);
        WRAP(DdiTable->Program.pfnCreateWithNativeHandle,
             UrApiWrapper::wrapUrProgramCreateWithNativeHandle);
        WRAP(DdiTable->Program.pfnLink, UrApiWrapper::wrapUrProgramLink);
        WRAP(DdiTable->ProgramExp.pfnLinkExp,
             UrApiWrapper::wrapUrProgramLinkExp);
        WRAP(DdiTable->Program.pfnRetain, UrApiWrapper::wrapUrProgramRetain);
        WRAP(DdiTable->Program.pfnRelease, UrApiWrapper::wrapUrProgramRelease);
    }

    // Context
    static ur_result_t
    wrapUrContextCreate(uint32_t numDevices,
                        const ur_device_handle_t *phDevices,
                        const ur_context_properties_t *pProperties,
                        ur_context_handle_t *phContext) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Context.pfnCreate != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Context.pfnCreate(
            numDevices, phDevices, pProperties, phContext);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->add(*phContext);
        }
        return Res;
    }

    static ur_result_t wrapUrContextCreateWithNativeHandle(
        ur_native_handle_t hNativeContext, ur_adapter_handle_t hAdapter,
        uint32_t numDevices, const ur_device_handle_t *phDevices,
        const ur_context_native_properties_t *pProperties,
        ur_context_handle_t *phContext) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Context.pfnCreateWithNativeHandle !=
               nullptr);

        auto Res = UrApiWrapper::urDdiTable.Context.pfnCreateWithNativeHandle(
            hNativeContext, hAdapter, numDevices, phDevices, pProperties,
            phContext);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->add(*phContext);
        }
        return Res;
    }

    static ur_result_t wrapUrContextRetain(ur_context_handle_t hContext) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Context.pfnRetain != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Context.pfnRetain(hContext);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->retain(hContext);
        }
        return Res;
    }

    static ur_result_t wrapUrContextRelease(ur_context_handle_t hContext) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Context.pfnRelease != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Context.pfnRelease(hContext);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->release(hContext);
        }
        return Res;
    }

    // Kernel
    static ur_result_t wrapUrKernelCreate(ur_program_handle_t hProgram,
                                          const char *pKernelName,
                                          ur_kernel_handle_t *phKernel) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Kernel.pfnCreate != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Kernel.pfnCreate(
            hProgram, pKernelName, phKernel);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->add(*phKernel);
        }
        return Res;
    }

    static ur_result_t wrapUrKernelCreateWithNativeHandle(
        ur_native_handle_t hNativeKernel, ur_context_handle_t hContext,
        ur_program_handle_t hProgram,
        const ur_kernel_native_properties_t *pProperties,
        ur_kernel_handle_t *phKernel) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Kernel.pfnCreateWithNativeHandle !=
               nullptr);

        auto Res = UrApiWrapper::urDdiTable.Kernel.pfnCreateWithNativeHandle(
            hNativeKernel, hContext, hProgram, pProperties, phKernel);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->add(*phKernel);
        }
        return Res;
    }

    static ur_result_t wrapUrKernelRetain(ur_kernel_handle_t hKernel) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Kernel.pfnRetain != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Kernel.pfnRetain(hKernel);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->retain(hKernel);
        }
        return Res;
    }

    static ur_result_t wrapUrKernelRelease(ur_kernel_handle_t hKernel) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Kernel.pfnRelease != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Kernel.pfnRelease(hKernel);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->release(hKernel);
        }
        return Res;
    }

    // Queue
    static ur_result_t
    wrapUrQueueCreate(ur_context_handle_t hContext, ur_device_handle_t hDevice,
                      const ur_queue_properties_t *pProperties,
                      ur_queue_handle_t *phQueue) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Queue.pfnCreate != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Queue.pfnCreate(
            hContext, hDevice, pProperties, phQueue);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->add(*phQueue);
        }
        return Res;
    }

    static ur_result_t wrapUrQueueRetain(ur_queue_handle_t hQueue) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Queue.pfnRetain != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Queue.pfnRetain(hQueue);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->retain(hQueue);
        }
        return Res;
    }

    static ur_result_t wrapUrQueueRelease(ur_queue_handle_t hQueue) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Queue.pfnRelease != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Queue.pfnRelease(hQueue);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->release(hQueue);
        }
        return Res;
    }

    // Program
    static ur_result_t
    wrapUrProgramCreateWithIL(ur_context_handle_t hContext, const void *pIL,
                              size_t length,
                              const ur_program_properties_t *pProperties,
                              ur_program_handle_t *phProgram) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Program.pfnCreateWithIL != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Program.pfnCreateWithIL(
            hContext, pIL, length, pProperties, phProgram);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->add(*phProgram);
        }
        return Res;
    }

    static ur_result_t wrapUrProgramCreateWithBinary(
        ur_context_handle_t hContext, uint32_t numDevices,
        ur_device_handle_t *phDevices, size_t *pLengths,
        const uint8_t **ppBinaries, const ur_program_properties_t *pProperties,
        ur_program_handle_t *phProgram) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Program.pfnCreateWithBinary != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Program.pfnCreateWithBinary(
            hContext, numDevices, phDevices, pLengths, ppBinaries, pProperties,
            phProgram);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->add(*phProgram);
        }
        return Res;
    }

    static ur_result_t wrapUrProgramCreateWithNativeHandle(

        ur_native_handle_t hNativeProgram, ur_context_handle_t hContext,
        const ur_program_native_properties_t *pProperties,
        ur_program_handle_t *phProgram) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Program.pfnCreateWithNativeHandle !=
               nullptr);

        auto Res = UrApiWrapper::urDdiTable.Program.pfnCreateWithNativeHandle(
            hNativeProgram, hContext, pProperties, phProgram);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->add(*phProgram);
        }
        return Res;
    }

    static ur_result_t wrapUrProgramLink(ur_context_handle_t hContext,
                                         uint32_t count,
                                         const ur_program_handle_t *phPrograms,
                                         const char *pOptions,
                                         ur_program_handle_t *phProgram) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Program.pfnLink != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Program.pfnLink(
            hContext, count, phPrograms, pOptions, phProgram);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->add(*phProgram);
        }
        return Res;
    }

    static ur_result_t
    wrapUrProgramLinkExp(ur_context_handle_t hContext, uint32_t numDevices,
                         ur_device_handle_t *phDevices, uint32_t count,
                         const ur_program_handle_t *phPrograms,
                         const char *pOptions, ur_program_handle_t *phProgram

    ) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.ProgramExp.pfnLinkExp != nullptr);

        auto Res = UrApiWrapper::urDdiTable.ProgramExp.pfnLinkExp(
            hContext, numDevices, phDevices, count, phPrograms, pOptions,
            phProgram);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->add(*phProgram);
        }
        return Res;
    }

    static ur_result_t wrapUrProgramRetain(ur_program_handle_t hProgram) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Program.pfnRetain != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Program.pfnRetain(hProgram);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->retain(hProgram);
        }
        return Res;
    }

    static ur_result_t wrapUrProgramRelease(ur_program_handle_t hProgram) {
        assert(UrApiWrapper::urHandleChecker != nullptr);
        assert(UrApiWrapper::urDdiTable.Program.pfnRelease != nullptr);

        auto Res = UrApiWrapper::urDdiTable.Program.pfnRelease(hProgram);
        if (Res == UR_RESULT_SUCCESS) {
            UrApiWrapper::urHandleChecker->release(hProgram);
        }
        return Res;
    }
};

ur_dditable_t UrApiWrapper::urDdiTable;
ur_sanitizer_layer::UrHandleChecker *UrApiWrapper::urHandleChecker;
} // namespace

namespace ur_sanitizer_layer {

UrHandleChecker::~UrHandleChecker() {
    // TODO: add stack traces for unreleased handles(or any error report)
    for (auto &[handle, info] : urHandleStatusMap) {
        if (info.refCount > 0) {
            if (std::holds_alternative<ur_context_handle_t>(handle)) {
                logger.info("UrContext {} is not released",
                            std::get<ur_context_handle_t>(handle));
            } else {
                // TODO: add support for other handle types
            }
        }
    }
}

void UrHandleChecker::add(UrHandleT UrHandle) {
    assert(ddiTableWrapped && "DdiTable is not wrapped");
    assert(urHandleStatusMap.find(UrHandle) == urHandleStatusMap.end() &&
           "Add of a existed handle");
    std::ignore = urHandleStatusMap[UrHandle];
    logEvent("is added", UrHandle);
}

void UrHandleChecker::retain(UrHandleT UrHandle) {
    assert(ddiTableWrapped && "DdiTable is not wrapped");
    assert(urHandleStatusMap.find(UrHandle) != urHandleStatusMap.end() &&
           "Retain of a nonexistent handle");
    urHandleStatusMap[UrHandle].retain();
    logEvent("is retained", UrHandle);
}

void UrHandleChecker::release(UrHandleT UrHandle) {
    assert(ddiTableWrapped && "DdiTable is not wrapped");
    assert(urHandleStatusMap.find(UrHandle) != urHandleStatusMap.end() &&
           "Release of a nonexistent handle");
    urHandleStatusMap[UrHandle].release();
    if (urHandleStatusMap[UrHandle].refCount == 0) {
        std::scoped_lock<ur_shared_mutex> Guard(urHandleStatusMapMutex);
        urHandleStatusMap.erase(UrHandle);
        logEvent("is released(and removed)", UrHandle, false);
    } else {
        logEvent("is released", UrHandle);
    }
}

bool UrHandleChecker::use(UrHandleT UrHandle) {
    assert(ddiTableWrapped && "DdiTable is not wrapped");
    assert(urHandleStatusMap.find(UrHandle) != urHandleStatusMap.end() &&
           "Use of a nonexistent handle");
    return urHandleStatusMap[UrHandle].use();
}

ur_result_t UrHandleChecker::wrapDditable(ur_dditable_t *DdiTable) {
    ddiTableWrapped = true;
    UrApiWrapper::wrapDditable(this, DdiTable);
    return UR_RESULT_SUCCESS;
}

} // namespace ur_sanitizer_layer
