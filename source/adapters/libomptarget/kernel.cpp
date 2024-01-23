//===----------- kernel.cpp - Libomptarget Adapter ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-------------------------------------------------------------------===//
#include "kernel.hpp"
#include "common.hpp"
#include "memory.hpp"
#include "program.hpp"

// MISSING FUNCTIONALITY:
// * No way to query underlying CUDA kernel about per-thread register usage
UR_APIEXPORT ur_result_t UR_APICALL
urKernelCreate(ur_program_handle_t hProgram, const char *pKernelName,
               ur_kernel_handle_t *phKernel) {
  ur_result_t Result = UR_RESULT_SUCCESS;

  // Find regular kernel
  auto *Func = hProgram->getFunctionPointer(pKernelName);
  if (!Func) {
    return UR_RESULT_ERROR_INVALID_KERNEL_NAME;
  }

  // Find offset kernel variant
  // It might be missing but `nullptr` is a valid value
  std::string KernelNameWithOffset = std::string(pKernelName) + "_with_offset";
  auto *FuncWithOffset =
      hProgram->getFunctionPointer(KernelNameWithOffset.c_str());

  *phKernel = new ur_kernel_handle_t_{Func, FuncWithOffset, pKernelName,
                                      hProgram, hProgram->getContext()};

  return Result;
}

// MISSING FUNCTIONALITY:
// * libomptarget seems to expect that all values still have the size of a
//   device pointer, so values larger than 8 bytes (e.g. structs) cannot be
//   set correctly. See: test-e2e/USM/fill.cpp
UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgValue(
    ur_kernel_handle_t hKernel, uint32_t argIndex, size_t argSize,
    [[maybe_unused]] const ur_kernel_arg_value_properties_t *pProperties,
    const void *pArgValue) {
  UR_ASSERT(argSize, UR_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_SIZE);

  ur_result_t Result = UR_RESULT_SUCCESS;
  if (pArgValue) {
    hKernel->setKernelArg(argIndex, argSize, pArgValue);
  } else {
    hKernel->setKernelLocalArg(argIndex, argSize);
  }

  return Result;
}

// MISSING FUNCTIONALITY:
// * No equivalent to cuFuncGetAttribute, so cannot get UR_KERNEL_INFO_NUM_REGS
UR_APIEXPORT ur_result_t UR_APICALL urKernelGetInfo(ur_kernel_handle_t hKernel,
                                                    ur_kernel_info_t propName,
                                                    size_t propSize,
                                                    void *pKernelInfo,
                                                    size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propSize, pKernelInfo, pPropSizeRet);

  switch (propName) {
  case UR_KERNEL_INFO_FUNCTION_NAME:
    return ReturnValue(hKernel->getName());
  case UR_KERNEL_INFO_NUM_ARGS:
    return ReturnValue(hKernel->getNumArgs());
  case UR_KERNEL_INFO_REFERENCE_COUNT:
    return ReturnValue(hKernel->getReferenceCount());
  case UR_KERNEL_INFO_CONTEXT:
    return ReturnValue(hKernel->getContext());
  case UR_KERNEL_INFO_PROGRAM:
    return ReturnValue(hKernel->getProgram());
  case UR_KERNEL_INFO_ATTRIBUTES:
    return ReturnValue("");
  // UNSUPPORTED:
  case UR_KERNEL_INFO_NUM_REGS:
  default:
    break;
  }

  return UR_RESULT_ERROR_INVALID_ENUMERATION;
}

// MISSING FUNCTIONALITY:
// * No way to query these things generically for any plugin
// * No way to query device attributes (cuDeviceGetAttribute) to get info needed
//   for global work size (CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X,
//   CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X, etc)
// * No way to query function attributes (cuFuncGetAttribute) to get info needed
//   for various queries (CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK,
//   CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES, etc)
UR_APIEXPORT ur_result_t UR_APICALL urKernelGetGroupInfo(
    ur_kernel_handle_t hKernel, [[maybe_unused]] ur_device_handle_t hDevice,
    ur_kernel_group_info_t propName, size_t propSize, void *pPropValue,
    size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);

  switch (propName) {
  case UR_KERNEL_GROUP_INFO_COMPILE_WORK_GROUP_SIZE: {
    size_t GroupSize[3] = {0, 0, 0};
    const auto &ReqdWGSizeMDMap =
        hKernel->getProgram()->KernelReqdWorkGroupSizeMD;
    const auto ReqdWGSizeMD = ReqdWGSizeMDMap.find(hKernel->getName());
    if (ReqdWGSizeMD != ReqdWGSizeMDMap.end()) {
      const auto ReqdWGSize = ReqdWGSizeMD->second;
      GroupSize[0] = std::get<0>(ReqdWGSize);
      GroupSize[1] = std::get<1>(ReqdWGSize);
      GroupSize[2] = std::get<2>(ReqdWGSize);
    }
    return ReturnValue(GroupSize, 3);
  }
  case UR_KERNEL_GROUP_INFO_GLOBAL_WORK_SIZE:
  case UR_KERNEL_GROUP_INFO_WORK_GROUP_SIZE:
  case UR_KERNEL_GROUP_INFO_LOCAL_MEM_SIZE:
  case UR_KERNEL_GROUP_INFO_PREFERRED_WORK_GROUP_SIZE_MULTIPLE:
  case UR_KERNEL_GROUP_INFO_PRIVATE_MEM_SIZE:
  default:
    break;
  }

  return UR_RESULT_ERROR_INVALID_ENUMERATION;
}

// MISSING FUNCTIONALITY:
// * No way to query these things generically for any plugin
// * No way to query device attributes (cuDeviceGetAttribute) to get info needed
//   for max subgroup size (CU_DEVICE_ATTRIBUTE_WARP_SIZE)
// * No way to query function attributes (cuFuncGetAttribute) to get info needed
//   for max subgroup count (CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK)
UR_APIEXPORT ur_result_t UR_APICALL
urKernelGetSubGroupInfo([[maybe_unused]] ur_kernel_handle_t hKernel,
                        [[maybe_unused]] ur_device_handle_t hDevice,
                        ur_kernel_sub_group_info_t propName, size_t propSize,
                        void *pPropValue, size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);
  switch (propName) {
  case UR_KERNEL_SUB_GROUP_INFO_MAX_SUB_GROUP_SIZE:
  case UR_KERNEL_SUB_GROUP_INFO_MAX_NUM_SUB_GROUPS:
    break;

  // NOTE: The below comments are copied verbatim from the CUDA adapter so are
  // specific to PTX binaries
  case UR_KERNEL_SUB_GROUP_INFO_COMPILE_NUM_SUB_GROUPS: {
    // Return value of 0 => not specified
    // TODO: Revisit if PTX is generated for compile-time work-group sizes
    return ReturnValue(0);
  }
  case UR_KERNEL_SUB_GROUP_INFO_SUB_GROUP_SIZE_INTEL: {
    // Return value of 0 => unspecified or "auto" sub-group size
    // Correct for now, since warp size may be read from special register
    // TODO: Return warp size once default is primary sub-group size
    // TODO: Revisit if we can recover [[sub_group_size]] attribute from PTX
    return ReturnValue(0);
  }
  default:
    break;
  }

  return UR_RESULT_ERROR_INVALID_ENUMERATION;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelRetain(ur_kernel_handle_t hKernel) {
  hKernel->incrementReferenceCount();
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urKernelRelease(ur_kernel_handle_t hKernel) {
  if (hKernel->decrementReferenceCount() == 0) {
    delete hKernel;
  }
  return UR_RESULT_SUCCESS;
}

// NOTE: This is apparently a nop for the CUDA backend; not sure what would be
// needed to support non-CUDA targets.
UR_APIEXPORT ur_result_t UR_APICALL
urKernelSetExecInfo([[maybe_unused]] ur_kernel_handle_t hKernel,
                    [[maybe_unused]] ur_kernel_exec_info_t propName,
                    [[maybe_unused]] size_t propSize,
                    [[maybe_unused]] const ur_kernel_exec_info_properties_t *,
                    [[maybe_unused]] const void *pPropValue) {
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgPointer(
    ur_kernel_handle_t hKernel, uint32_t argIndex,
    [[maybe_unused]] const ur_kernel_arg_pointer_properties_t *,
    const void *pArgValue) {
  hKernel->setKernelArg(argIndex, sizeof(pArgValue), pArgValue);
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urKernelGetNativeHandle([[maybe_unused]] ur_kernel_handle_t hKernel,
                        [[maybe_unused]] ur_native_handle_t *phNativeKernel) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelCreateWithNativeHandle(
    [[maybe_unused]] ur_native_handle_t hNativeKernel,
    [[maybe_unused]] ur_context_handle_t, [[maybe_unused]] ur_program_handle_t,
    [[maybe_unused]] const ur_kernel_native_properties_t *,
    [[maybe_unused]] ur_kernel_handle_t *phKernel) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgMemObj(
    ur_kernel_handle_t hKernel, uint32_t argIndex,
    [[maybe_unused]] const ur_kernel_arg_mem_obj_properties_t *,
    ur_mem_handle_t hArgValue) {
  hKernel->setKernelArg(argIndex, sizeof(hArgValue->Ptr), &(hArgValue->Ptr));
  return UR_RESULT_SUCCESS;
}

// MISSING FUNCTIONALITY:
// * No support for samplers, images
UR_APIEXPORT ur_result_t UR_APICALL urKernelSetArgSampler(
    [[maybe_unused]] ur_kernel_handle_t hKernel,
    [[maybe_unused]] uint32_t argIndex,
    [[maybe_unused]] const ur_kernel_arg_sampler_properties_t *,
    [[maybe_unused]] ur_sampler_handle_t hArgValue) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
