#include "ur_handle_checker.hpp"

namespace {
struct UrApiWrapper {
    static ur_dditable_t urDdiTable;
    static ur_sanitizer_layer::UrHandleChecker *urHandleChecker;
    static void wrapDditable(ur_sanitizer_layer::UrHandleChecker *checker,
                             ur_dditable_t *DdiTable) {
        // Wrap the UR DDI table
        UrApiWrapper::urHandleChecker = checker;
        UrApiWrapper::urDdiTable = *DdiTable;

        DdiTable->Context.pfnCreate = UrApiWrapper::wrapUrContextCreate;
        DdiTable->Context.pfnCreateWithNativeHandle =
            UrApiWrapper::wrapUrContextCreateWithNativeHandle;
        DdiTable->Context.pfnRetain = UrApiWrapper::wrapUrContextRetain;
        DdiTable->Context.pfnRelease = UrApiWrapper::wrapUrContextRelease;
    }

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
