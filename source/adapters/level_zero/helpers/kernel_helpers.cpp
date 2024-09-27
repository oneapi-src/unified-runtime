//===--------- kernel_helpers.cpp - Level Zero Adapter -------------------===//
//
// Copyright (C) 2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "kernel_helpers.hpp"
#include "logger/ur_logger.hpp"

#include "../common.hpp"
#include "../device.hpp"

#ifdef UR_ADAPTER_LEVEL_ZERO_V2
#include "../v2/context.hpp"
#else
#include "../context.hpp"
#endif

ur_result_t getSuggestedLocalWorkSize(ur_device_handle_t hDevice,
                                      ze_kernel_handle_t hZeKernel,
                                      size_t GlobalWorkSize3D[3],
                                      uint32_t SuggestedLocalWorkSize3D[3]) {
  uint32_t *WG = SuggestedLocalWorkSize3D;

  // We can't call to zeKernelSuggestGroupSize if 64-bit GlobalWorkSize
  // values do not fit to 32-bit that the API only supports currently.
  bool SuggestGroupSize = true;
  for (int I : {0, 1, 2}) {
    if (GlobalWorkSize3D[I] > UINT32_MAX) {
      SuggestGroupSize = false;
    }
  }
  if (SuggestGroupSize) {
    ZE2UR_CALL(zeKernelSuggestGroupSize,
               (hZeKernel, GlobalWorkSize3D[0], GlobalWorkSize3D[1],
                GlobalWorkSize3D[2], &WG[0], &WG[1], &WG[2]));
  } else {
    for (int I : {0, 1, 2}) {
      // Try to find a I-dimension WG size that the GlobalWorkSize[I] is
      // fully divisable with. Start with the max possible size in
      // each dimension.
      uint32_t GroupSize[] = {
          hDevice->ZeDeviceComputeProperties->maxGroupSizeX,
          hDevice->ZeDeviceComputeProperties->maxGroupSizeY,
          hDevice->ZeDeviceComputeProperties->maxGroupSizeZ};
      GroupSize[I] = (std::min)(size_t(GroupSize[I]), GlobalWorkSize3D[I]);
      while (GlobalWorkSize3D[I] % GroupSize[I]) {
        --GroupSize[I];
      }
      if (GlobalWorkSize3D[I] / GroupSize[I] > UINT32_MAX) {
        logger::error("getSuggestedLocalWorkSize: can't find a WG size "
                      "suitable for global work size > UINT32_MAX");
        return UR_RESULT_ERROR_INVALID_WORK_GROUP_SIZE;
      }
      WG[I] = GroupSize[I];
    }
    logger::debug(
        "getSuggestedLocalWorkSize: using computed WG size = {{{}, {}, {}}}",
        WG[0], WG[1], WG[2]);
  }

  return UR_RESULT_SUCCESS;
}

ur_result_t setKernelGlobalOffset(ur_context_handle_t Context,
                                  ze_kernel_handle_t Kernel,
                                  const size_t *GlobalWorkOffset) {
  if (!Context->getPlatform()->ZeDriverGlobalOffsetExtensionFound) {
    logger::debug("No global offset extension found on this driver");
    return UR_RESULT_ERROR_INVALID_VALUE;
  }

  ZE2UR_CALL(
      zeKernelSetGlobalOffsetExp,
      (Kernel, GlobalWorkOffset[0], GlobalWorkOffset[1], GlobalWorkOffset[2]));

  return UR_RESULT_SUCCESS;
}

ur_result_t calculateKernelWorkDimensions(
    ze_kernel_handle_t Kernel, ur_device_handle_t Device,
    ze_group_count_t &ZeThreadGroupDimensions, uint32_t (&WG)[3],
    uint32_t WorkDim, const size_t *GlobalWorkSize,
    const size_t *LocalWorkSize) {

  UR_ASSERT(GlobalWorkSize, UR_RESULT_ERROR_INVALID_VALUE);
  // If LocalWorkSize is not provided then Kernel must be provided to query
  // suggested group size.
  UR_ASSERT(LocalWorkSize || Kernel, UR_RESULT_ERROR_INVALID_VALUE);

  // New variable needed because GlobalWorkSize parameter might not be of size
  // 3
  size_t GlobalWorkSize3D[3]{1, 1, 1};
  std::copy(GlobalWorkSize, GlobalWorkSize + WorkDim, GlobalWorkSize3D);

  if (LocalWorkSize) {
    WG[0] = ur_cast<uint32_t>(LocalWorkSize[0]);
    WG[1] = WorkDim >= 2 ? ur_cast<uint32_t>(LocalWorkSize[1]) : 1;
    WG[2] = WorkDim == 3 ? ur_cast<uint32_t>(LocalWorkSize[2]) : 1;
  } else {
    UR_CALL(getSuggestedLocalWorkSize(Device, Kernel, GlobalWorkSize3D, WG));
  }

  // TODO: assert if sizes do not fit into 32-bit?
  switch (WorkDim) {
  case 3:
    ZeThreadGroupDimensions.groupCountX =
        ur_cast<uint32_t>(GlobalWorkSize3D[0] / WG[0]);
    ZeThreadGroupDimensions.groupCountY =
        ur_cast<uint32_t>(GlobalWorkSize3D[1] / WG[1]);
    ZeThreadGroupDimensions.groupCountZ =
        ur_cast<uint32_t>(GlobalWorkSize3D[2] / WG[2]);
    break;
  case 2:
    ZeThreadGroupDimensions.groupCountX =
        ur_cast<uint32_t>(GlobalWorkSize3D[0] / WG[0]);
    ZeThreadGroupDimensions.groupCountY =
        ur_cast<uint32_t>(GlobalWorkSize3D[1] / WG[1]);
    WG[2] = 1;
    break;
  case 1:
    ZeThreadGroupDimensions.groupCountX =
        ur_cast<uint32_t>(GlobalWorkSize3D[0] / WG[0]);
    WG[1] = WG[2] = 1;
    break;

  default:
    logger::error("calculateKernelWorkDimensions: unsupported work_dim");
    return UR_RESULT_ERROR_INVALID_VALUE;
  }

  // Error handling for non-uniform group size case
  if (GlobalWorkSize3D[0] !=
      size_t(ZeThreadGroupDimensions.groupCountX) * WG[0]) {
    logger::error("calculateKernelWorkDimensions: invalid work_dim. The range "
                  "is not a multiple of the group size in the 1st dimension");
    return UR_RESULT_ERROR_INVALID_WORK_GROUP_SIZE;
  }
  if (GlobalWorkSize3D[1] !=
      size_t(ZeThreadGroupDimensions.groupCountY) * WG[1]) {
    logger::error("calculateKernelWorkDimensions: invalid work_dim. The range "
                  "is not a multiple of the group size in the 2nd dimension");
    return UR_RESULT_ERROR_INVALID_WORK_GROUP_SIZE;
  }
  if (GlobalWorkSize3D[2] !=
      size_t(ZeThreadGroupDimensions.groupCountZ) * WG[2]) {
    logger::error("calculateKernelWorkDimensions: invalid work_dim. The range "
                  "is not a multiple of the group size in the 3rd dimension");
    return UR_RESULT_ERROR_INVALID_WORK_GROUP_SIZE;
  }

  return UR_RESULT_SUCCESS;
}

ur_result_t kernelSetExecInfo(ze_kernel_handle_t zeKernel,
                              ur_kernel_exec_info_t propName,
                              const void *propValue) {
  if (propName == UR_KERNEL_EXEC_INFO_USM_INDIRECT_ACCESS &&
      *(static_cast<const ur_bool_t *>(propValue)) == true) {
    // The whole point for users really was to not need to know anything
    // about the types of allocations kernel uses. So in DPC++ we always
    // just set all 3 modes for each kernel.
    ze_kernel_indirect_access_flags_t IndirectFlags =
        ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST |
        ZE_KERNEL_INDIRECT_ACCESS_FLAG_DEVICE |
        ZE_KERNEL_INDIRECT_ACCESS_FLAG_SHARED;
    ZE2UR_CALL(zeKernelSetIndirectAccess, (zeKernel, IndirectFlags));
  } else if (propName == UR_KERNEL_EXEC_INFO_CACHE_CONFIG) {
    ze_cache_config_flag_t ZeCacheConfig{};
    auto CacheConfig =
        *(static_cast<const ur_kernel_cache_config_t *>(propValue));
    if (CacheConfig == UR_KERNEL_CACHE_CONFIG_LARGE_SLM)
      ZeCacheConfig = ZE_CACHE_CONFIG_FLAG_LARGE_SLM;
    else if (CacheConfig == UR_KERNEL_CACHE_CONFIG_LARGE_DATA)
      ZeCacheConfig = ZE_CACHE_CONFIG_FLAG_LARGE_DATA;
    else if (CacheConfig == UR_KERNEL_CACHE_CONFIG_DEFAULT)
      ZeCacheConfig = static_cast<ze_cache_config_flag_t>(0);
    else
      // Unexpected cache configuration value.
      return UR_RESULT_ERROR_INVALID_VALUE;
    ZE2UR_CALL(zeKernelSetCacheConfig, (zeKernel, ZeCacheConfig););
  } else {
    logger::error("urKernelSetExecInfo: unsupported paramName");
    return UR_RESULT_ERROR_INVALID_VALUE;
  }

  return UR_RESULT_SUCCESS;
}

ur_result_t kernelGetSubGroupInfo(ze_kernel_properties_t *pProperties,
                                  ur_kernel_sub_group_info_t propName,
                                  size_t propSize, void *pPropValue,
                                  size_t *pPropSizeRet) {
  UrReturnHelper returnValue(propSize, pPropValue, pPropSizeRet);

  if (propName == UR_KERNEL_SUB_GROUP_INFO_MAX_SUB_GROUP_SIZE) {
    returnValue(uint32_t{pProperties->maxSubgroupSize});
  } else if (propName == UR_KERNEL_SUB_GROUP_INFO_MAX_NUM_SUB_GROUPS) {
    returnValue(uint32_t{pProperties->maxNumSubgroups});
  } else if (propName == UR_KERNEL_SUB_GROUP_INFO_COMPILE_NUM_SUB_GROUPS) {
    returnValue(uint32_t{pProperties->requiredNumSubGroups});
  } else if (propName == UR_KERNEL_SUB_GROUP_INFO_SUB_GROUP_SIZE_INTEL) {
    returnValue(uint32_t{pProperties->requiredSubgroupSize});
  } else {
    die("urKernelGetSubGroupInfo: parameter not implemented");
    return {};
  }
  return UR_RESULT_SUCCESS;
}

std::vector<char> getKernelSourceAttributes(ze_kernel_handle_t kernel) {
  uint32_t size;
  ZE2UR_CALL_THROWS(zeKernelGetSourceAttributes, (kernel, &size, nullptr));
  std::vector<char> attributes(size);
  char *pAttributes = attributes.data();
  ZE2UR_CALL_THROWS(zeKernelGetSourceAttributes, (kernel, &size, &pAttributes));
  return attributes;
}

ur_result_t kernelGetGroupInfo(ze_kernel_handle_t hKernel,
                               ur_device_handle_t hUrDevice,
                               ze_kernel_properties_t *pProperties,
                               ur_kernel_group_info_t paramName,
                               size_t paramValueSize, void *pParamValue,
                               size_t *pParamValueSizeRet) {
  UrReturnHelper ReturnValue(paramValueSize, pParamValue, pParamValueSizeRet);

  switch (paramName) {
  case UR_KERNEL_GROUP_INFO_GLOBAL_WORK_SIZE: {
    // TODO: To revisit after level_zero/issues/262 is resolved
    struct {
      size_t Arr[3];
    } GlobalWorkSize = {
        {(hUrDevice->ZeDeviceComputeProperties->maxGroupSizeX *
          hUrDevice->ZeDeviceComputeProperties->maxGroupCountX),
         (hUrDevice->ZeDeviceComputeProperties->maxGroupSizeY *
          hUrDevice->ZeDeviceComputeProperties->maxGroupCountY),
         (hUrDevice->ZeDeviceComputeProperties->maxGroupSizeZ *
          hUrDevice->ZeDeviceComputeProperties->maxGroupCountZ)}};
    return ReturnValue(GlobalWorkSize);
  }
  case UR_KERNEL_GROUP_INFO_WORK_GROUP_SIZE: {
    ZeStruct<ze_kernel_max_group_size_properties_ext_t> workGroupProperties;
    workGroupProperties.maxGroupSize = 0;

    ZeStruct<ze_kernel_properties_t> kernelProperties;
    kernelProperties.pNext = &workGroupProperties;

    if (hKernel) {
      auto ZeResult =
          ZE_CALL_NOCHECK(zeKernelGetProperties, (hKernel, &kernelProperties));
      if (ZeResult || workGroupProperties.maxGroupSize == 0) {
        return ReturnValue(
            uint64_t{hUrDevice->ZeDeviceComputeProperties->maxTotalGroupSize});
      }
      return ReturnValue(workGroupProperties.maxGroupSize);
    } else {
      return ReturnValue(
          uint64_t{hUrDevice->ZeDeviceComputeProperties->maxTotalGroupSize});
    }
  }
  case UR_KERNEL_GROUP_INFO_COMPILE_WORK_GROUP_SIZE: {
    struct {
      size_t Arr[3];
    } WgSize = {{pProperties->requiredGroupSizeX,
                 pProperties->requiredGroupSizeY,
                 pProperties->requiredGroupSizeZ}};
    return ReturnValue(WgSize);
  }
  case UR_KERNEL_GROUP_INFO_LOCAL_MEM_SIZE:
    return ReturnValue(uint32_t{pProperties->localMemSize});
  case UR_KERNEL_GROUP_INFO_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: {
    return ReturnValue(
        size_t{hUrDevice->ZeDeviceProperties->physicalEUSimdWidth});
  }
  case UR_KERNEL_GROUP_INFO_PRIVATE_MEM_SIZE: {
    return ReturnValue(uint32_t{pProperties->privateMemSize});
  }
  case UR_KERNEL_GROUP_INFO_COMPILE_MAX_WORK_GROUP_SIZE:
  case UR_KERNEL_GROUP_INFO_COMPILE_MAX_LINEAR_WORK_GROUP_SIZE:
    // No corresponding enumeration in Level Zero
    return UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
  default: {
    logger::error(
        "Unknown paramName in urKernelGetGroupInfo: paramName={}(0x{})",
        paramName, logger::toHex(paramName));
    return UR_RESULT_ERROR_INVALID_VALUE;
  }
  }
  return UR_RESULT_SUCCESS;
}
