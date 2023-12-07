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

namespace cl_adapter {
ur_result_t getPlatformVersion(cl_platform_id Plat,
                               oclv::OpenCLVersion &Version);

// Older versions of GCC don't like "const" here
#if defined(__GNUC__) && (__GNUC__ < 7 || (__GNU__C == 7 && __GNUC_MINOR__ < 2))
#define CONSTFIX constexpr
#else
#define CONSTFIX const
#endif

// Names of USM functions that are queried from OpenCL
CONSTFIX char HostMemAllocName[] = "clHostMemAllocINTEL";
CONSTFIX char DeviceMemAllocName[] = "clDeviceMemAllocINTEL";
CONSTFIX char SharedMemAllocName[] = "clSharedMemAllocINTEL";
CONSTFIX char MemBlockingFreeName[] = "clMemBlockingFreeINTEL";
CONSTFIX char CreateBufferWithPropertiesName[] =
    "clCreateBufferWithPropertiesINTEL";
CONSTFIX char SetKernelArgMemPointerName[] = "clSetKernelArgMemPointerINTEL";
CONSTFIX char EnqueueMemFillName[] = "clEnqueueMemFillINTEL";
CONSTFIX char EnqueueMemcpyName[] = "clEnqueueMemcpyINTEL";
CONSTFIX char GetMemAllocInfoName[] = "clGetMemAllocInfoINTEL";
CONSTFIX char SetProgramSpecializationConstantName[] =
    "clSetProgramSpecializationConstant";
CONSTFIX char GetDeviceFunctionPointerName[] =
    "clGetDeviceFunctionPointerINTEL";
CONSTFIX char EnqueueWriteGlobalVariableName[] =
    "clEnqueueWriteGlobalVariableINTEL";
CONSTFIX char EnqueueReadGlobalVariableName[] =
    "clEnqueueReadGlobalVariableINTEL";
// Names of host pipe functions queried from OpenCL
CONSTFIX char EnqueueReadHostPipeName[] = "clEnqueueReadHostPipeINTEL";
CONSTFIX char EnqueueWriteHostPipeName[] = "clEnqueueWriteHostPipeINTEL";
// Names of command buffer functions queried from OpenCL
CONSTFIX char CreateCommandBufferName[] = "clCreateCommandBufferKHR";
CONSTFIX char RetainCommandBufferName[] = "clRetainCommandBufferKHR";
CONSTFIX char ReleaseCommandBufferName[] = "clReleaseCommandBufferKHR";
CONSTFIX char FinalizeCommandBufferName[] = "clFinalizeCommandBufferKHR";
CONSTFIX char CommandNRRangeKernelName[] = "clCommandNDRangeKernelKHR";
CONSTFIX char CommandCopyBufferName[] = "clCommandCopyBufferKHR";
CONSTFIX char CommandCopyBufferRectName[] = "clCommandCopyBufferRectKHR";
CONSTFIX char CommandFillBufferName[] = "clCommandFillBufferKHR";
CONSTFIX char EnqueueCommandBufferName[] = "clEnqueueCommandBufferKHR";

#undef CONSTFIX

using clGetDeviceFunctionPointer_fn = CL_API_ENTRY
cl_int(CL_API_CALL *)(cl_device_id device, cl_program program,
                      const char *FuncName, cl_ulong *ret_ptr);

using clEnqueueWriteGlobalVariable_fn = CL_API_ENTRY
cl_int(CL_API_CALL *)(cl_command_queue, cl_program, const char *, cl_bool,
                      size_t, size_t, const void *, cl_uint, const cl_event *,
                      cl_event *);

using clEnqueueReadGlobalVariable_fn = CL_API_ENTRY
cl_int(CL_API_CALL *)(cl_command_queue, cl_program, const char *, cl_bool,
                      size_t, size_t, void *, cl_uint, const cl_event *,
                      cl_event *);

using clSetProgramSpecializationConstant_fn = CL_API_ENTRY
cl_int(CL_API_CALL *)(cl_program program, cl_uint spec_id, size_t spec_size,
                      const void *spec_value);

using clEnqueueReadHostPipeINTEL_fn = CL_API_ENTRY
cl_int(CL_API_CALL *)(cl_command_queue queue, cl_program program,
                      const char *pipe_symbol, cl_bool blocking, void *ptr,
                      size_t size, cl_uint num_events_in_waitlist,
                      const cl_event *events_waitlist, cl_event *event);

using clEnqueueWriteHostPipeINTEL_fn = CL_API_ENTRY
cl_int(CL_API_CALL *)(cl_command_queue queue, cl_program program,
                      const char *pipe_symbol, cl_bool blocking,
                      const void *ptr, size_t size,
                      cl_uint num_events_in_waitlist,
                      const cl_event *events_waitlist, cl_event *event);

using clCreateCommandBufferKHR_fn = CL_API_ENTRY cl_command_buffer_khr(
    CL_API_CALL *)(cl_uint num_queues, const cl_command_queue *queues,
                   const cl_command_buffer_properties_khr *properties,
                   cl_int *errcode_ret);

using clRetainCommandBufferKHR_fn = CL_API_ENTRY
cl_int(CL_API_CALL *)(cl_command_buffer_khr command_buffer);

using clReleaseCommandBufferKHR_fn = CL_API_ENTRY
cl_int(CL_API_CALL *)(cl_command_buffer_khr command_buffer);

using clFinalizeCommandBufferKHR_fn = CL_API_ENTRY
cl_int(CL_API_CALL *)(cl_command_buffer_khr command_buffer);

using clCommandNDRangeKernelKHR_fn = CL_API_ENTRY cl_int(CL_API_CALL *)(
    cl_command_buffer_khr command_buffer, cl_command_queue command_queue,
    const cl_ndrange_kernel_command_properties_khr *properties,
    cl_kernel kernel, cl_uint work_dim, const size_t *global_work_offset,
    const size_t *global_work_size, const size_t *local_work_size,
    cl_uint num_sync_points_in_wait_list,
    const cl_sync_point_khr *sync_point_wait_list,
    cl_sync_point_khr *sync_point, cl_mutable_command_khr *mutable_handle);

using clCommandCopyBufferKHR_fn = CL_API_ENTRY cl_int(CL_API_CALL *)(
    cl_command_buffer_khr command_buffer, cl_command_queue command_queue,
    cl_mem src_buffer, cl_mem dst_buffer, size_t src_offset, size_t dst_offset,
    size_t size, cl_uint num_sync_points_in_wait_list,
    const cl_sync_point_khr *sync_point_wait_list,
    cl_sync_point_khr *sync_point, cl_mutable_command_khr *mutable_handle);

using clCommandCopyBufferRectKHR_fn = CL_API_ENTRY cl_int(CL_API_CALL *)(
    cl_command_buffer_khr command_buffer, cl_command_queue command_queue,
    cl_mem src_buffer, cl_mem dst_buffer, const size_t *src_origin,
    const size_t *dst_origin, const size_t *region, size_t src_row_pitch,
    size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch,
    cl_uint num_sync_points_in_wait_list,
    const cl_sync_point_khr *sync_point_wait_list,
    cl_sync_point_khr *sync_point, cl_mutable_command_khr *mutable_handle);

using clCommandFillBufferKHR_fn = CL_API_ENTRY cl_int(CL_API_CALL *)(
    cl_command_buffer_khr command_buffer, cl_command_queue command_queue,
    cl_mem buffer, const void *pattern, size_t pattern_size, size_t offset,
    size_t size, cl_uint num_sync_points_in_wait_list,
    const cl_sync_point_khr *sync_point_wait_list,
    cl_sync_point_khr *sync_point, cl_mutable_command_khr *mutable_handle);

using clEnqueueCommandBufferKHR_fn = CL_API_ENTRY
cl_int(CL_API_CALL *)(cl_uint num_queues, cl_command_queue *queues,
                      cl_command_buffer_khr command_buffer,
                      cl_uint num_events_in_wait_list,
                      const cl_event *event_wait_list, cl_event *event);

struct ExtFuncPtrT {
  clHostMemAllocINTEL_fn clHostMemAllocINTELCache;
  clDeviceMemAllocINTEL_fn clDeviceMemAllocINTELCache;
  clSharedMemAllocINTEL_fn clSharedMemAllocINTELCache;
  clGetDeviceFunctionPointer_fn clGetDeviceFunctionPointerCache;
  clCreateBufferWithPropertiesINTEL_fn clCreateBufferWithPropertiesINTELCache;
  clMemBlockingFreeINTEL_fn clMemBlockingFreeINTELCache;
  clSetKernelArgMemPointerINTEL_fn clSetKernelArgMemPointerINTELCache;
  clEnqueueMemFillINTEL_fn clEnqueueMemFillINTELCache;
  clEnqueueMemcpyINTEL_fn clEnqueueMemcpyINTELCache;
  clGetMemAllocInfoINTEL_fn clGetMemAllocInfoINTELCache;
  clEnqueueWriteGlobalVariable_fn clEnqueueWriteGlobalVariableCache;
  clEnqueueReadGlobalVariable_fn clEnqueueReadGlobalVariableCache;
  clEnqueueReadHostPipeINTEL_fn clEnqueueReadHostPipeINTELCache;
  clEnqueueWriteHostPipeINTEL_fn clEnqueueWriteHostPipeINTELCache;
  clSetProgramSpecializationConstant_fn clSetProgramSpecializationConstantCache;
  clCreateCommandBufferKHR_fn clCreateCommandBufferKHRCache;
  clRetainCommandBufferKHR_fn clRetainCommandBufferKHRCache;
  clReleaseCommandBufferKHR_fn clReleaseCommandBufferKHRCache;
  clFinalizeCommandBufferKHR_fn clFinalizeCommandBufferKHRCache;
  clCommandNDRangeKernelKHR_fn clCommandNDRangeKernelKHRCache;
  clCommandCopyBufferKHR_fn clCommandCopyBufferKHRCache;
  clCommandCopyBufferRectKHR_fn clCommandCopyBufferRectKHRCache;
  clCommandFillBufferKHR_fn clCommandFillBufferKHRCache;
  clEnqueueCommandBufferKHR_fn clEnqueueCommandBufferKHRCache;
};
} // namespace cl_adapter

struct ur_platform_handle_t_ {
  using native_type = cl_platform_id;
  native_type Platform = nullptr;
  std::unique_ptr<cl_adapter::ExtFuncPtrT> ExtFuncPtr;
  std::vector<ur_device_handle_t> Devices;

  ur_platform_handle_t_(native_type Plat) : Platform(Plat) {
    ExtFuncPtr = std::make_unique<cl_adapter::ExtFuncPtrT>();
    InitDevices();
  }

  ~ur_platform_handle_t_() { ExtFuncPtr.reset(); }

  template <typename T>
  ur_result_t getExtFunc(T CachedExtFunc, const char *FuncName, T *Fptr) {
    if (!CachedExtFunc) {
      // TODO: check that the function is available
      CachedExtFunc = reinterpret_cast<T>(
          clGetExtensionFunctionAddressForPlatform(Platform, FuncName));
      if (!CachedExtFunc) {
        return UR_RESULT_ERROR_INVALID_VALUE;
      }
    }
    *Fptr = CachedExtFunc;
    return UR_RESULT_SUCCESS;
  }

  native_type get() { return Platform; }

  ur_result_t InitDevices() {
    cl_uint DeviceNum = 0;
    CL_RETURN_ON_FAILURE(clGetDeviceIDs(Platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &DeviceNum));

    std::vector<cl_device_id> CLDevices(DeviceNum);
    CL_RETURN_ON_FAILURE(clGetDeviceIDs(Platform, CL_DEVICE_TYPE_ALL, DeviceNum, CLDevices.data(), nullptr));

    Devices = std::vector<ur_device_handle_t>(DeviceNum);
    for (size_t i = 0; i < DeviceNum; i++) {
        Devices[i] = new ur_device_handle_t_(CLDevices[i], this, nullptr);
    }

    return UR_RESULT_SUCCESS;
  }
};
