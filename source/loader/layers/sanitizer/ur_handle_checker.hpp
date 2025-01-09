/**
 * UR handle checker, used to check if the handle is valid. To detect any possible 
 * use-after-release issues of UR handles.
 */

#pragma once

#include "logger/ur_logger.hpp"
#include "ur/ur.hpp"
#include "ur_api.h"
#include "ur_ddi.h"

#include <atomic>
#include <cassert>
#include <unordered_map>
#include <variant>

// Some helper functions to make it easier to use the UR handle checker
#define UR_USE(handle)                                                         \
    (getContext()->urHandleChecker.use(handle) ? (handle)                      \
                                               : decltype(handle){nullptr})

namespace ur_sanitizer_layer {

class UrHandleChecker {

  public:
    typedef std::variant<ur_context_handle_t, ur_device_handle_t> UrHandleT;

    UrHandleChecker()
        : logger(logger::create_logger("handle_checker", false, false,
                                       logger::Level::INFO)) {};
    ~UrHandleChecker();

    void add(UrHandleT UrHandle);
    void retain(UrHandleT UrHandle);
    void release(UrHandleT UrHandle);
    bool use(UrHandleT UrHandle);
    ur_result_t wrapDditable(ur_dditable_t *DdiTable);

  private:
    struct UrHandleInfo {
        UrHandleInfo() : refCount(1) {}
        ~UrHandleInfo() = default;
        void retain() { refCount += 1; }
        void release() {
            assert(refCount > 0 && "Release of a invalid handle");
            refCount -= 1;
        }
        bool use() {
            assert(refCount > 0 && "Use of a invalid handle");
            return refCount > 0;
        }
        int getRefCount() { return refCount; }
        std::atomic<int> refCount;
    };

    logger::Logger logger;
    bool ddiTableWrapped = false;
    ur_shared_mutex urHandleStatusMapMutex;
    std::unordered_map<UrHandleT, UrHandleInfo> urHandleStatusMap;

    void logEvent(const char *eventMsg, UrHandleT UrHandle,
                  bool printRefCount = true) {
        int refCount = 0;
        if (printRefCount) {
            assert(urHandleStatusMap.find(UrHandle) != urHandleStatusMap.end());
            refCount = urHandleStatusMap[UrHandle].getRefCount();
        }
        if (std::holds_alternative<ur_context_handle_t>(UrHandle)) {
            auto handle = std::get<ur_context_handle_t>(UrHandle);
            logger.info("UrContext {}(RefCount:{}) {}", handle, refCount,
                        eventMsg);
        } else {
            // TODO: add support for other handle types
        }
    }
};

} // namespace ur_sanitizer_layer
