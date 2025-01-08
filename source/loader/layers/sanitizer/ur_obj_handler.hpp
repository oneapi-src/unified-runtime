/*
The UrObjectHandler is intend to provide a global maps for all UrObjects and corresponding XXXInfo objects that used with sanitizer layers.
Also, it provides a checker that checks for UrObjects' status to avoid any use-after-released cases.
*/

/**
 * 20250107: first we impl this as a checker to check for use-after-released cases.
 */

#include "ur/ur.hpp"
#include "ur_api.h"

#include <atomic>
#include <cassert>
#include <unordered_map>
#include <variant>

#pragma once

namespace ur_sanitizer_layer {

typedef std::variant<ur_context_handle_t, ur_device_handle_t> UrObjectT;

class UrObjectHandler {
  public:
    void add(UrObjectT urObject) {
        std::scoped_lock<ur_shared_mutex> Guard(urObjectStatusMapMutex);
        // if (urObjectStatusMap.find(urObject) != urObjectStatusMap.end()) {
        //     if (urObjectStatusMap[urObject].refCount > 0) {
        //         assert(false && "Add of a exist object");
        //     } else {
        //         // remove an old object
        //         ; // Nothing to do for now as we only do ref-counting
        //     }
        // }
        assert(urObjectStatusMap.find(urObject) == urObjectStatusMap.end() &&
               "Add of a exist object");
        std::ignore = urObjectStatusMap[urObject];
    }

    ur_result_t retain(UrObjectT urObject) {
        assert(ddiTableInstalled && "DdiTable is not installed");
        assert(urObjectStatusMap.find(urObject) != urObjectStatusMap.end() &&
               "Retain of a nonexistent object");
        urObjectStatusMap[urObject].retain();

        if (std::holds_alternative<ur_context_handle_t>(urObject)) {
            return urDdiTable.Context.pfnRetain(
                std::get<ur_context_handle_t>(urObject));
        } else if (std::holds_alternative<ur_device_handle_t>(urObject)) {
            return urDdiTable.Device.pfnRetain(
                std::get<ur_device_handle_t>(urObject));
        }
        assert(false && "Abonomal object type");
        return UR_RESULT_SUCCESS;
    }

    ur_result_t release(UrObjectT urObject) {
        assert(ddiTableInstalled && "DdiTable is not installed");
        assert(urObjectStatusMap.find(urObject) != urObjectStatusMap.end() &&
               "Release of a nonexistent object");
        urObjectStatusMap[urObject].release();
        if (urObjectStatusMap[urObject].refCount == 0) {
            std::scoped_lock<ur_shared_mutex> Guard(urObjectStatusMapMutex);
            urObjectStatusMap.erase(urObject);
        }

        if (std::holds_alternative<ur_context_handle_t>(urObject)) {
            return urDdiTable.Context.pfnRelease(
                std::get<ur_context_handle_t>(urObject));
        } else if (std::holds_alternative<ur_device_handle_t>(urObject)) {
            return urDdiTable.Device.pfnRelease(
                std::get<ur_device_handle_t>(urObject));
        }
        assert(false && "Abonomal object type");
        return UR_RESULT_SUCCESS;
    }

    bool use(UrObjectT urObject) {
        assert(urObjectStatusMap.find(urObject) != urObjectStatusMap.end() &&
               "Use of a nonexistent object");
        return urObjectStatusMap[urObject].use();
    }

    void installDdiTable(ur_dditable_t *dditable) {
        urDdiTable = *dditable;
        ddiTableInstalled = true;
    }

    ~UrObjectHandler() {
        // Check for not released objects
    }

  private:
    struct UrObjectInfo {
        UrObjectInfo() : refCount(1) {}
        std::atomic<int> refCount;

        void retain() { refCount += 1; }
        void release() {
            assert(refCount > 0 && "Release of a invalid object");
            refCount -= 1;
        }
        bool use() {
            assert(refCount > 0 && "Use of a invalid object");
            return refCount > 0;
        }
    };

    ur_dditable_t urDdiTable;
    bool ddiTableInstalled = false;
    ur_shared_mutex urObjectStatusMapMutex;
    std::unordered_map<UrObjectT, UrObjectInfo> urObjectStatusMap;
};

} // namespace ur_sanitizer_layer
