//===--------- platform.cpp - OpenCL Adapter ---------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "program.hpp"
#include "common.hpp"
#include "context.hpp"
#include "device.hpp"
#include "platform.hpp"

#include <vector>

UR_APIEXPORT ur_result_t UR_APICALL urProgramCreateWithIL(
    ur_context_handle_t hContext, const void *pIL, size_t length,
    const ur_program_properties_t *, ur_program_handle_t *phProgram) {

  if (!hContext->DeviceCount || !hContext->Devices[0]->Platform) {
    return UR_RESULT_ERROR_INVALID_CONTEXT;
  }
  ur_platform_handle_t CurPlatform = hContext->Devices[0]->Platform;

  oclv::OpenCLVersion PlatVer;
  CL_RETURN_ON_FAILURE_AND_SET_NULL(CurPlatform->getPlatformVersion(PlatVer),
                                    phProgram);

  cl_int Err = CL_SUCCESS;
  if (PlatVer >= oclv::V2_1) {

    /* Make sure all devices support CL 2.1 or newer as well. */
    for (ur_device_handle_t URDev : hContext->getDevices()) {
      oclv::OpenCLVersion DevVer;

      CL_RETURN_ON_FAILURE_AND_SET_NULL(URDev->getDeviceVersion(DevVer),
                                        phProgram);

      /* If the device does not support CL 2.1 or greater, we need to make sure
       * it supports the cl_khr_il_program extension.
       */
      if (DevVer < oclv::V2_1) {
        bool Supported = false;
        CL_RETURN_ON_FAILURE_AND_SET_NULL(
            URDev->checkDeviceExtensions({"cl_khr_il_program"}, Supported),
            phProgram);

        if (!Supported) {
          return UR_RESULT_ERROR_COMPILER_NOT_AVAILABLE;
        }
      }
    }

    cl_program Program =
        clCreateProgramWithIL(hContext->get(), pIL, length, &Err);
    CL_RETURN_ON_FAILURE(Err);
    try {
      auto URProgram =
          std::make_unique<ur_program_handle_t_>(Program, hContext);
      *phProgram = URProgram.release();
    } catch (std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }
  } else {
    /* If none of the devices conform with CL 2.1 or newer make sure they all
     * support the cl_khr_il_program extension.
     */
    for (ur_device_handle_t URDev : hContext->getDevices()) {
      bool Supported = false;
      CL_RETURN_ON_FAILURE_AND_SET_NULL(
          URDev->checkDeviceExtensions({"cl_khr_il_program"}, Supported),
          phProgram);

      if (!Supported) {
        return UR_RESULT_ERROR_COMPILER_NOT_AVAILABLE;
      }
    }

    using ApiFuncT =
        cl_program(CL_API_CALL *)(cl_context, const void *, size_t, cl_int *);
    ApiFuncT FuncPtr =
        reinterpret_cast<ApiFuncT>(clGetExtensionFunctionAddressForPlatform(
            CurPlatform->get(), "clCreateProgramWithILKHR"));

    assert(FuncPtr != nullptr);
    try {
      cl_program Program = FuncPtr(hContext->get(), pIL, length, &Err);
      CL_RETURN_ON_FAILURE(Err);
      auto URProgram =
          std::make_unique<ur_program_handle_t_>(Program, hContext);
      *phProgram = URProgram.release();
    } catch (std::bad_alloc &) {
      return UR_RESULT_ERROR_OUT_OF_RESOURCES;
    } catch (...) {
      return UR_RESULT_ERROR_UNKNOWN;
    }
    CL_RETURN_ON_FAILURE(Err);
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramCreateWithBinary(
    ur_context_handle_t hContext, ur_device_handle_t hDevice, size_t size,
    const uint8_t *pBinary, const ur_program_properties_t *,
    ur_program_handle_t *phProgram) {

  const cl_device_id Devices[1] = {hDevice->get()};
  const size_t Lengths[1] = {size};
  cl_int BinaryStatus[1];
  cl_int CLResult;
  try {
    cl_program Program = clCreateProgramWithBinary(
        hContext->get(), cl_adapter::cast<cl_uint>(1u), Devices, Lengths,
        &pBinary, BinaryStatus, &CLResult);
    CL_RETURN_ON_FAILURE(CLResult);
    auto URProgram = std::make_unique<ur_program_handle_t_>(Program, hContext);
    *phProgram = URProgram.release();
  } catch (std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_RESOURCES;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }
  CL_RETURN_ON_FAILURE(BinaryStatus[0]);
  CL_RETURN_ON_FAILURE(CLResult);

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urProgramCompile([[maybe_unused]] ur_context_handle_t hContext,
                 ur_program_handle_t hProgram, const char *pOptions) {

  uint32_t DeviceCount = hProgram->Context->DeviceCount;
  std::vector<cl_device_id> CLDevicesInProgram(DeviceCount);
  for (uint32_t i = 0; i < DeviceCount; i++) {
    CLDevicesInProgram[i] = hProgram->Context->Devices[i]->get();
  }

  CL_RETURN_ON_FAILURE(clCompileProgram(hProgram->get(), DeviceCount,
                                        CLDevicesInProgram.data(), pOptions, 0,
                                        nullptr, nullptr, nullptr, nullptr));

  return UR_RESULT_SUCCESS;
}

static cl_int mapURProgramInfoToCL(ur_program_info_t URPropName) {

  switch (static_cast<uint32_t>(URPropName)) {
  case UR_PROGRAM_INFO_REFERENCE_COUNT:
    return CL_PROGRAM_REFERENCE_COUNT;
  case UR_PROGRAM_INFO_CONTEXT:
    return CL_PROGRAM_CONTEXT;
  case UR_PROGRAM_INFO_NUM_DEVICES:
    return CL_PROGRAM_NUM_DEVICES;
  case UR_PROGRAM_INFO_DEVICES:
    return CL_PROGRAM_DEVICES;
  case UR_PROGRAM_INFO_SOURCE:
    return CL_PROGRAM_SOURCE;
  case UR_PROGRAM_INFO_BINARY_SIZES:
    return CL_PROGRAM_BINARY_SIZES;
  case UR_PROGRAM_INFO_BINARIES:
    return CL_PROGRAM_BINARIES;
  case UR_PROGRAM_INFO_NUM_KERNELS:
    return CL_PROGRAM_NUM_KERNELS;
  case UR_PROGRAM_INFO_KERNEL_NAMES:
    return CL_PROGRAM_KERNEL_NAMES;
  default:
    return -1;
  }
}

UR_APIEXPORT ur_result_t UR_APICALL
urProgramGetInfo(ur_program_handle_t hProgram, ur_program_info_t propName,
                 size_t propSize, void *pPropValue, size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);

  const cl_program_info CLPropName = mapURProgramInfoToCL(propName);

  switch (static_cast<uint32_t>(propName)) {
  case UR_PROGRAM_INFO_CONTEXT: {
    return ReturnValue(hProgram->Context);
  }
  case UR_PROGRAM_INFO_NUM_DEVICES: {
    if (!hProgram->Context || !hProgram->Context->DeviceCount) {
      return UR_RESULT_ERROR_INVALID_PROGRAM;
    }
    cl_uint DeviceCount = hProgram->Context->DeviceCount;
    return ReturnValue(DeviceCount);
  }
  case UR_PROGRAM_INFO_DEVICES: {
    return ReturnValue(&hProgram->Context->Devices[0],
                       hProgram->Context->DeviceCount);
  }
  case UR_PROGRAM_INFO_REFERENCE_COUNT: {
    return ReturnValue(hProgram->getReferenceCount());
  }
  default: {
    size_t CheckPropSize = 0;
    auto ClResult = clGetProgramInfo(hProgram->get(), CLPropName, propSize,
                                     pPropValue, &CheckPropSize);
    if (pPropValue && CheckPropSize != propSize) {
      return UR_RESULT_ERROR_INVALID_SIZE;
    }
    CL_RETURN_ON_FAILURE(ClResult);
    if (pPropSizeRet) {
      *pPropSizeRet = CheckPropSize;
    }
  }
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urProgramBuild([[maybe_unused]] ur_context_handle_t hContext,
               ur_program_handle_t hProgram, const char *pOptions) {

  uint32_t DeviceCount = hProgram->Context->DeviceCount;
  std::vector<cl_device_id> CLDevicesInProgram(DeviceCount);
  for (uint32_t i = 0; i < DeviceCount; i++) {
    CLDevicesInProgram[i] = hProgram->Context->Devices[i]->get();
  }
  CL_RETURN_ON_FAILURE(
      clBuildProgram(hProgram->get(), cl_adapter::cast<cl_uint>(DeviceCount),
                     CLDevicesInProgram.data(), pOptions, nullptr, nullptr));
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urProgramLink(ur_context_handle_t hContext, uint32_t count,
              const ur_program_handle_t *phPrograms, const char *pOptions,
              ur_program_handle_t *phProgram) {

  cl_int CLResult;
  std::vector<cl_program> CLPrograms(count);
  for (uint32_t i = 0; i < count; i++) {
    CLPrograms[i] = phPrograms[i]->get();
  }
  cl_program Program = clLinkProgram(
      hContext->get(), 0, nullptr, pOptions, cl_adapter::cast<cl_uint>(count),
      CLPrograms.data(), nullptr, nullptr, &CLResult);
  CL_RETURN_ON_FAILURE(CLResult);
  try {
    auto URProgram = std::make_unique<ur_program_handle_t_>(Program, hContext);
    *phProgram = URProgram.release();
  } catch (std::bad_alloc &) {
    return UR_RESULT_ERROR_OUT_OF_RESOURCES;
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramCompileExp(ur_program_handle_t,
                                                        uint32_t,
                                                        ur_device_handle_t *,
                                                        const char *) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramBuildExp(ur_program_handle_t,
                                                      uint32_t,
                                                      ur_device_handle_t *,
                                                      const char *) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramLinkExp(
    ur_context_handle_t, uint32_t, ur_device_handle_t *, uint32_t,
    const ur_program_handle_t *, const char *, ur_program_handle_t *) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

static cl_int mapURProgramBuildInfoToCL(ur_program_build_info_t URPropName) {

  switch (static_cast<uint32_t>(URPropName)) {
  case UR_PROGRAM_BUILD_INFO_STATUS:
    return CL_PROGRAM_BUILD_STATUS;
  case UR_PROGRAM_BUILD_INFO_OPTIONS:
    return CL_PROGRAM_BUILD_OPTIONS;
  case UR_PROGRAM_BUILD_INFO_LOG:
    return CL_PROGRAM_BUILD_LOG;
  case UR_PROGRAM_BUILD_INFO_BINARY_TYPE:
    return CL_PROGRAM_BINARY_TYPE;
  default:
    return -1;
  }
}

static ur_program_binary_type_t
mapCLBinaryTypeToUR(cl_program_binary_type binaryType) {
  switch (binaryType) {
  default:
    // If we don't understand what OpenCL gave us, return NONE.
    // TODO: Emit a warning to the user.
    [[fallthrough]];
  case CL_PROGRAM_BINARY_TYPE_INTERMEDIATE:
    // The INTERMEDIATE binary type is defined by the cl_khr_spir extension
    // which we shouldn't encounter but do. Semantically this binary type is
    // equivelent to NONE as they both require compilation.
    [[fallthrough]];
  case CL_PROGRAM_BINARY_TYPE_NONE:
    return UR_PROGRAM_BINARY_TYPE_NONE;
  case CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT:
    return UR_PROGRAM_BINARY_TYPE_COMPILED_OBJECT;
  case CL_PROGRAM_BINARY_TYPE_LIBRARY:
    return UR_PROGRAM_BINARY_TYPE_LIBRARY;
  case CL_PROGRAM_BINARY_TYPE_EXECUTABLE:
    return UR_PROGRAM_BINARY_TYPE_EXECUTABLE;
  }
}

UR_APIEXPORT ur_result_t UR_APICALL
urProgramGetBuildInfo(ur_program_handle_t hProgram, ur_device_handle_t hDevice,
                      ur_program_build_info_t propName, size_t propSize,
                      void *pPropValue, size_t *pPropSizeRet) {
  if (propName == UR_PROGRAM_BUILD_INFO_BINARY_TYPE) {
    UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);
    cl_program_binary_type BinaryType;
    CL_RETURN_ON_FAILURE(clGetProgramBuildInfo(
        hProgram->get(), hDevice->get(), mapURProgramBuildInfoToCL(propName),
        sizeof(cl_program_binary_type), &BinaryType, nullptr));
    return ReturnValue(mapCLBinaryTypeToUR(BinaryType));
  }
  size_t CheckPropSize = 0;
  cl_int ClErr = clGetProgramBuildInfo(hProgram->get(), hDevice->get(),
                                       mapURProgramBuildInfoToCL(propName),
                                       propSize, pPropValue, &CheckPropSize);
  if (pPropValue && CheckPropSize != propSize) {
    return UR_RESULT_ERROR_INVALID_SIZE;
  }
  CL_RETURN_ON_FAILURE(ClErr);
  if (pPropSizeRet) {
    *pPropSizeRet = CheckPropSize;
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urProgramRetain(ur_program_handle_t hProgram) {
  hProgram->incrementReferenceCount();
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urProgramRelease(ur_program_handle_t hProgram) {
  if (hProgram->decrementReferenceCount() == 0) {
    delete hProgram;
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramGetNativeHandle(
    ur_program_handle_t hProgram, ur_native_handle_t *phNativeProgram) {

  *phNativeProgram = reinterpret_cast<ur_native_handle_t>(hProgram->get());
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramCreateWithNativeHandle(
    ur_native_handle_t hNativeProgram, ur_context_handle_t hContext,
    const ur_program_native_properties_t *pProperties,
    ur_program_handle_t *phProgram) {
  cl_program NativeHandle = reinterpret_cast<cl_program>(hNativeProgram);

  UR_RETURN_ON_FAILURE(
      ur_program_handle_t_::makeWithNative(NativeHandle, hContext, *phProgram));
  if (!pProperties || !pProperties->isNativeHandleOwned) {
    CL_RETURN_ON_FAILURE(clRetainProgram(NativeHandle));
  }
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramSetSpecializationConstants(
    ur_program_handle_t hProgram, uint32_t count,
    const ur_specialization_constant_info_t *pSpecConstants) {

  cl_program CLProg = hProgram->get();
  if (!hProgram->Context) {
    return UR_RESULT_ERROR_INVALID_PROGRAM;
  }
  ur_context_handle_t Ctx = hProgram->Context;
  if (!Ctx->DeviceCount || !Ctx->Devices[0]->Platform) {
    return UR_RESULT_ERROR_INVALID_CONTEXT;
  }

  ur_platform_handle_t CurPlatform = Ctx->Devices[0]->Platform;

  oclv::OpenCLVersion PlatVer;
  CurPlatform->getPlatformVersion(PlatVer);

  bool UseExtensionLookup = false;
  if (PlatVer < oclv::V2_2) {
    UseExtensionLookup = true;
  } else {
    for (ur_device_handle_t Dev : Ctx->getDevices()) {
      oclv::OpenCLVersion DevVer;

      UR_RETURN_ON_FAILURE(Dev->getDeviceVersion(DevVer));

      if (DevVer < oclv::V2_2) {
        UseExtensionLookup = true;
        break;
      }
    }
  }

  if (UseExtensionLookup == false) {
    for (uint32_t i = 0; i < count; ++i) {
      CL_RETURN_ON_FAILURE(clSetProgramSpecializationConstant(
          CLProg, pSpecConstants[i].id, pSpecConstants[i].size,
          pSpecConstants[i].pValue));
    }
  } else {
    cl_ext::clSetProgramSpecializationConstant_fn
        SetProgramSpecializationConstant = nullptr;
    const ur_result_t URResult = cl_ext::getExtFuncFromContext<
        decltype(SetProgramSpecializationConstant)>(
        Ctx->get(),
        cl_ext::ExtFuncPtrCache->clSetProgramSpecializationConstantCache,
        cl_ext::SetProgramSpecializationConstantName,
        &SetProgramSpecializationConstant);

    if (URResult != UR_RESULT_SUCCESS) {
      return URResult;
    }

    for (uint32_t i = 0; i < count; ++i) {
      CL_RETURN_ON_FAILURE(SetProgramSpecializationConstant(
          CLProg, pSpecConstants[i].id, pSpecConstants[i].size,
          pSpecConstants[i].pValue));
    }
  }
  return UR_RESULT_SUCCESS;
}

// Function gets characters between delimeter's in str
// then checks if they are equal to the sub_str.
// returns true if there is at least one instance
// returns false if there are no instances of the name
static bool isInSeparatedString(const std::string &Str, char Delimiter,
                                const std::string &SubStr) {
  size_t Beg = 0;
  size_t Length = 0;
  for (const auto &x : Str) {
    if (x == Delimiter) {
      if (Str.substr(Beg, Length) == SubStr)
        return true;

      Beg += Length + 1;
      Length = 0;
      continue;
    }
    Length++;
  }
  if (Length != 0)
    if (Str.substr(Beg, Length) == SubStr)
      return true;

  return false;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramGetFunctionPointer(
    ur_device_handle_t hDevice, ur_program_handle_t hProgram,
    const char *pFunctionName, void **ppFunctionPointer) {

  cl_context CLContext = hProgram->Context->get();

  cl_ext::clGetDeviceFunctionPointer_fn FuncT = nullptr;

  UR_RETURN_ON_FAILURE(
      cl_ext::getExtFuncFromContext<cl_ext::clGetDeviceFunctionPointer_fn>(
          CLContext, cl_ext::ExtFuncPtrCache->clGetDeviceFunctionPointerCache,
          cl_ext::GetDeviceFunctionPointerName, &FuncT));

  if (!FuncT) {
    return UR_RESULT_ERROR_INVALID_FUNCTION_NAME;
  }

  // Check if the kernel name exists to prevent the OpenCL runtime from throwing
  // an exception with the cpu runtime.
  // TODO: Use fallback search method if the clGetDeviceFunctionPointerINTEL
  // extension does not exist. Can only be done once the CPU runtime no longer
  // throws exceptions.
  *ppFunctionPointer = 0;
  size_t Size;
  CL_RETURN_ON_FAILURE(clGetProgramInfo(
      hProgram->get(), CL_PROGRAM_KERNEL_NAMES, 0, nullptr, &Size));

  std::string KernelNames(Size, ' ');

  CL_RETURN_ON_FAILURE(
      clGetProgramInfo(hProgram->get(), CL_PROGRAM_KERNEL_NAMES,
                       KernelNames.size(), &KernelNames[0], nullptr));

  // Get rid of the null terminator and search for the kernel name. If the
  // function cannot be found, return an error code to indicate it exists.
  KernelNames.pop_back();
  if (!isInSeparatedString(KernelNames, ';', pFunctionName)) {
    return UR_RESULT_ERROR_INVALID_KERNEL_NAME;
  }

  const cl_int CLResult =
      FuncT(hDevice->get(), hProgram->get(), pFunctionName,
            reinterpret_cast<cl_ulong *>(ppFunctionPointer));
  // GPU runtime sometimes returns CL_INVALID_ARG_VALUE if the function address
  // cannot be found but the kernel exists. As the kernel does exist, return
  // that the function name is invalid.
  if (CLResult == CL_INVALID_ARG_VALUE) {
    *ppFunctionPointer = 0;
    return UR_RESULT_ERROR_INVALID_FUNCTION_NAME;
  }

  CL_RETURN_ON_FAILURE(CLResult);

  return UR_RESULT_SUCCESS;
}
