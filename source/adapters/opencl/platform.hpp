//===--------- platform.hpp - OpenCL Adapter ---------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

#include "common.hpp"
#include "device.hpp"

#include <vector>

using namespace cl_ext;

struct ur_platform_handle_t_ {
  using native_type = cl_platform_id;
  native_type Platform = nullptr;
  std::vector<std::unique_ptr<ur_device_handle_t_>> Devices;

  ur_platform_handle_t_(native_type Plat) : Platform(Plat) {
    ExtFuncPtr = std::make_unique<ExtFuncPtrT>();
  }

  ~ur_platform_handle_t_() {
    for (auto &Dev : Devices) {
      Dev.reset();
    }
    Devices.clear();
    ExtFuncPtr.reset();
  }

  native_type get() { return Platform; }

  ur_result_t InitDevices() {
    if (Devices.empty()) {
      cl_uint DeviceNum = 0;
      CL_RETURN_ON_FAILURE(
          clGetDeviceIDs(Platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &DeviceNum));

      std::vector<cl_device_id> CLDevices(DeviceNum);
      CL_RETURN_ON_FAILURE(clGetDeviceIDs(
          Platform, CL_DEVICE_TYPE_ALL, DeviceNum, CLDevices.data(), nullptr));

      try {
        Devices.resize(DeviceNum);
        for (size_t i = 0; i < DeviceNum; i++) {
          Devices[i] = std::make_unique<ur_device_handle_t_>(CLDevices[i], this,
                                                             nullptr);
        }
      } catch (std::bad_alloc &) {
        return UR_RESULT_ERROR_OUT_OF_RESOURCES;
      } catch (...) {
        return UR_RESULT_ERROR_UNKNOWN;
      }
    }

    return UR_RESULT_SUCCESS;
  }

  ur_result_t getPlatformVersion(oclv::OpenCLVersion &Version) {
    size_t PlatVerSize = 0;
    CL_RETURN_ON_FAILURE(clGetPlatformInfo(Platform, CL_PLATFORM_VERSION, 0,
                                           nullptr, &PlatVerSize));

    std::string PlatVer(PlatVerSize, '\0');
    CL_RETURN_ON_FAILURE(clGetPlatformInfo(
        Platform, CL_PLATFORM_VERSION, PlatVerSize, PlatVer.data(), nullptr));

    Version = oclv::OpenCLVersion(PlatVer);
    if (!Version.isValid()) {
      return UR_RESULT_ERROR_INVALID_PLATFORM;
    }

    return UR_RESULT_SUCCESS;
  }

  ur_result_t checkPlatformExtensions(const std::vector<std::string> &Exts,
                                      bool &Supported) {
    size_t ExtSize = 0;
    CL_RETURN_ON_FAILURE(clGetPlatformInfo(Platform, CL_PLATFORM_EXTENSIONS, 0,
                                           nullptr, &ExtSize));

    std::string ExtStr(ExtSize, '\0');

    CL_RETURN_ON_FAILURE(clGetPlatformInfo(Platform, CL_PLATFORM_EXTENSIONS,
                                           ExtSize, ExtStr.data(), nullptr));

    Supported = true;
    for (const std::string &Ext : Exts) {
      if (!(Supported = (ExtStr.find(Ext) != std::string::npos))) {
        break;
      }
    }

    return UR_RESULT_SUCCESS;
  }

  struct ExtFuncPtrT {
    clHostMemAllocINTEL_fn clHostMemAllocINTELCache = nullptr;
    clDeviceMemAllocINTEL_fn clDeviceMemAllocINTELCache = nullptr;
    clSharedMemAllocINTEL_fn clSharedMemAllocINTELCache = nullptr;
    clGetDeviceFunctionPointer_fn clGetDeviceFunctionPointerCache = nullptr;
    clCreateBufferWithPropertiesINTEL_fn
        clCreateBufferWithPropertiesINTELCache = nullptr;
    clMemBlockingFreeINTEL_fn clMemBlockingFreeINTELCache = nullptr;
    clSetKernelArgMemPointerINTEL_fn clSetKernelArgMemPointerINTELCache =
        nullptr;
    clEnqueueMemFillINTEL_fn clEnqueueMemFillINTELCache = nullptr;
    clEnqueueMemcpyINTEL_fn clEnqueueMemcpyINTELCache = nullptr;
    clGetMemAllocInfoINTEL_fn clGetMemAllocInfoINTELCache = nullptr;
    clEnqueueWriteGlobalVariable_fn clEnqueueWriteGlobalVariableCache = nullptr;
    clEnqueueReadGlobalVariable_fn clEnqueueReadGlobalVariableCache = nullptr;
    clEnqueueReadHostPipeINTEL_fn clEnqueueReadHostPipeINTELCache = nullptr;
    clEnqueueWriteHostPipeINTEL_fn clEnqueueWriteHostPipeINTELCache = nullptr;
    clSetProgramSpecializationConstant_fn
        clSetProgramSpecializationConstantCache = nullptr;
    clCreateCommandBufferKHR_fn clCreateCommandBufferKHRCache = nullptr;
    clRetainCommandBufferKHR_fn clRetainCommandBufferKHRCache = nullptr;
    clReleaseCommandBufferKHR_fn clReleaseCommandBufferKHRCache = nullptr;
    clFinalizeCommandBufferKHR_fn clFinalizeCommandBufferKHRCache = nullptr;
    clCommandNDRangeKernelKHR_fn clCommandNDRangeKernelKHRCache = nullptr;
    clCommandCopyBufferKHR_fn clCommandCopyBufferKHRCache = nullptr;
    clCommandCopyBufferRectKHR_fn clCommandCopyBufferRectKHRCache = nullptr;
    clCommandFillBufferKHR_fn clCommandFillBufferKHRCache = nullptr;
    clEnqueueCommandBufferKHR_fn clEnqueueCommandBufferKHRCache = nullptr;
    clGetCommandBufferInfoKHR_fn clGetCommandBufferInfoKHRCache = nullptr;
  };

  std::unique_ptr<ExtFuncPtrT> ExtFuncPtr;
  template <typename T>
  ur_result_t getExtFunc(T *CachedExtFunc, const char *FuncName,
                         const char *Extension) {
    if (!(*CachedExtFunc)) {
      // Check that the function ext is supported by the platform.
      bool Supported = false;
      UR_RETURN_ON_FAILURE(checkPlatformExtensions({Extension}, Supported));
      if (!Supported) {
        return UR_RESULT_ERROR_INVALID_OPERATION;
      }

      *CachedExtFunc = reinterpret_cast<T>(
          clGetExtensionFunctionAddressForPlatform(Platform, FuncName));
      if (!(*CachedExtFunc)) {
        return UR_RESULT_ERROR_INVALID_OPERATION;
      }
    }
    return UR_RESULT_SUCCESS;
  }
};
