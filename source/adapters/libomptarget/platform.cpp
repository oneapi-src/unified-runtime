//===--------- platform.cpp - Libomptarget Adapter --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

#include "platform.hpp"
#include "device.hpp"

#include <mutex>

UR_DLLEXPORT ur_result_t UR_APICALL urPlatformGetInfo(
    [[maybe_unused]] ur_platform_handle_t hPlatform,
    [[maybe_unused]] ur_platform_info_t propName,
    [[maybe_unused]] size_t propSize, [[maybe_unused]] void *pPropValue,
    [[maybe_unused]] size_t *pSizeRet) {

  UR_ASSERT(hPlatform, UR_RESULT_ERROR_INVALID_NULL_HANDLE);
  UrReturnHelper ReturnValue(propSize, pPropValue, pSizeRet);

  switch (propName) {
  case UR_PLATFORM_INFO_NAME: {
    return ReturnValue("Intel(R) oneAPI Unified Runtime over Libomptarget");
  }
  case UR_PLATFORM_INFO_VENDOR_NAME: {
    return ReturnValue("LLVM Project");
  }
  case UR_PLATFORM_INFO_PROFILE: {
    return ReturnValue("FULL PROFILE");
  }
  case UR_PLATFORM_INFO_VERSION: {
    return ReturnValue("1.0");
  }
  case UR_PLATFORM_INFO_EXTENSIONS: {
    return ReturnValue("");
  }
  case UR_PLATFORM_INFO_BACKEND: {
    /* Using UNKNOWN since there is no libomptarget enum at the moment */
    return ReturnValue(UR_PLATFORM_BACKEND_UNKNOWN);
  }
  default:
    return UR_RESULT_ERROR_INVALID_ENUMERATION;
  }

  return UR_RESULT_SUCCESS;
}

UR_DLLEXPORT ur_result_t UR_APICALL
urPlatformGetApiVersion([[maybe_unused]] ur_platform_handle_t hPlatform,
                        ur_api_version_t *pVersion) {
  *pVersion = UR_API_VERSION_CURRENT;
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urPlatformGet([[maybe_unused]] ur_adapter_handle_t *, [[maybe_unused]] uint32_t,
              [[maybe_unused]] uint32_t NumEntries,
              [[maybe_unused]] ur_platform_handle_t *phPlatforms,
              [[maybe_unused]] uint32_t *pNumPlatforms) {

  static std::once_flag InitFlag;
  static uint32_t NumPlatforms = 1;
  static std::vector<ur_platform_handle_t_> Platforms;

  ur_result_t Result = UR_RESULT_SUCCESS;

  std::call_once(
      InitFlag,
      [](ur_result_t &Result) {
        const int NumDevices = __tgt_rtl_number_of_devices();

        if (NumDevices == 0) {
          NumPlatforms = 0;
          return;
        }

        NumPlatforms = NumDevices;
        Platforms.resize(NumDevices);

        for (int DeviceID = 0; DeviceID < NumDevices; ++DeviceID) {
          Platforms[DeviceID].Devices.emplace_back(
              std::make_unique<ur_device_handle_t_>(DeviceID,
                                                    &Platforms[DeviceID]));
          OMPT_RETURN_ON_FAILURE_VOID(__tgt_rtl_init_device(DeviceID), Result);
        }
      },
      Result);

  if (Result != UR_RESULT_SUCCESS) {
    return Result;
  }

  if (pNumPlatforms != nullptr) {
    *pNumPlatforms = NumPlatforms;
  }

  if (phPlatforms != nullptr) {
    for (unsigned i = 0; i < std::min(NumEntries, NumPlatforms); ++i) {
      phPlatforms[i] = &Platforms[i];
    }
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urPlatformGetNativeHandle(
    [[maybe_unused]] ur_platform_handle_t hPlatform,
    [[maybe_unused]] ur_native_handle_t *phNativePlatform) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urPlatformCreateWithNativeHandle(
    [[maybe_unused]] ur_native_handle_t hNativePlatform,
    [[maybe_unused]] const ur_platform_native_properties_t *,
    [[maybe_unused]] ur_platform_handle_t *phPlatform) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urPlatformGetBackendOption(
    ur_platform_handle_t, [[maybe_unused]] const char *pFrontendOption,
    [[maybe_unused]] const char **ppPlatformOption) {

  using namespace std::literals;
  if (pFrontendOption == nullptr) {
    return UR_RESULT_ERROR_INVALID_NULL_POINTER;
  }
  if (pFrontendOption == "-O0"sv || pFrontendOption == "-O1"sv ||
      pFrontendOption == "-O2"sv || pFrontendOption == "-O3"sv ||
      pFrontendOption == ""sv) {
    *ppPlatformOption = "";
    return UR_RESULT_SUCCESS;
  }
  return UR_RESULT_ERROR_INVALID_VALUE;
}
