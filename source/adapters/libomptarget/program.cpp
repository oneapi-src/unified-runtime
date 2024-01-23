//===--------- program.cpp - Libomptarget Adapter ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-------------------------------------------------------------------===//

#include "program.hpp"
#include "common.hpp"
#include "context.hpp"
#include <omptargetplugin.h>

// NOTE:
// * This implementation currently assumes NVPTX binaries for the ompt nvidia
//   plugin.

// MISSING FUNCTIONALITY:
// * Assumes IL can be used as device binary input, which is only true for
//   CUDA.
UR_APIEXPORT ur_result_t UR_APICALL
urProgramCreateWithIL(ur_context_handle_t hContext, const void *pIL,
                      size_t length, const ur_program_properties_t *pProperties,
                      ur_program_handle_t *phProgram) {
  ur_device_handle_t hDevice = hContext->getDevice();
  auto pBinary = reinterpret_cast<const uint8_t *>(pIL);

  return urProgramCreateWithBinary(hContext, hDevice, length, pBinary,
                                   pProperties, phProgram);
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramCreateWithBinary(
    ur_context_handle_t hContext, ur_device_handle_t hDevice, size_t size,
    const uint8_t *pBinary,
    [[maybe_unused]] const ur_program_properties_t *pProperties,
    ur_program_handle_t *phProgram) {

  UR_ASSERT(hContext->getDevice()->getID() == hDevice->getID(),
            UR_RESULT_ERROR_INVALID_CONTEXT);
  UR_ASSERT(size, UR_RESULT_ERROR_INVALID_SIZE);

  ur_result_t Result = UR_RESULT_SUCCESS;

  std::unique_ptr<ur_program_handle_t_> RetProgram{
      new ur_program_handle_t_{hContext}};

  auto pBinary_string = reinterpret_cast<const char *>(pBinary);

  Result = RetProgram->setBinary(pBinary_string, size);
  UR_ASSERT(Result == UR_RESULT_SUCCESS, Result);

  if (pProperties) {
    if (pProperties->count > 0 && pProperties->pMetadatas == nullptr) {
      return UR_RESULT_ERROR_INVALID_NULL_POINTER;
    } else if (pProperties->count == 0 && pProperties->pMetadatas != nullptr) {
      return UR_RESULT_ERROR_INVALID_SIZE;
    }
    Result =
        RetProgram->setMetadata(pProperties->pMetadatas, pProperties->count);
  }

  *phProgram = RetProgram.release();

  return Result;
}

UR_APIEXPORT ur_result_t UR_APICALL
urProgramCompile(ur_context_handle_t hContext, ur_program_handle_t hProgram,
                 const char *pOptions) {
  return urProgramBuild(hContext, hProgram, pOptions);
}

// MISSING FUNCTIONALITY:
// * Libomptarget requires all the function names in the program to be known
//   ahead of time to build/load the program. We don't have access to that yet,
//   so we have to defer the actual program build to later. When a kernel is
//   requested from the program, we add it to the list of known functions and
//   (re)build the program. This means we have to assume the build is successful
//   when this entry point is called.
// * No way to pass build options through
// * No way to enable build error logging or retrieve build errors
UR_APIEXPORT ur_result_t UR_APICALL urProgramBuild(
    [[maybe_unused]] ur_context_handle_t hContext, ur_program_handle_t hProgram,
    [[maybe_unused]] const char *pOptions) {
  hProgram->BuildStatus = UR_PROGRAM_BUILD_STATUS_SUCCESS;
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urProgramGetInfo(ur_program_handle_t hProgram, ur_program_info_t propName,
                 size_t propSize, void *pProgramInfo, size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propSize, pProgramInfo, pPropSizeRet);

  switch (propName) {
  case UR_PROGRAM_INFO_REFERENCE_COUNT:
    return ReturnValue(hProgram->getReferenceCount());
  case UR_PROGRAM_INFO_CONTEXT:
    return ReturnValue(hProgram->Context);
  case UR_PROGRAM_INFO_NUM_DEVICES:
    return ReturnValue(1u);
  case UR_PROGRAM_INFO_SOURCE:
    return ReturnValue(hProgram->Binary);
  case UR_PROGRAM_INFO_BINARY_SIZES:
    return ReturnValue(&hProgram->BinarySizeInBytes, 1);
  case UR_PROGRAM_INFO_BINARIES:
    return ReturnValue(&hProgram->Binary, 1);
  // TODO: Unsupported so far
  case UR_PROGRAM_INFO_DEVICES:
  case UR_PROGRAM_INFO_KERNEL_NAMES:
  default:
    break;
  }
  return UR_RESULT_ERROR_INVALID_ENUMERATION;
}

// MISSING FUNCTIONALITY:
// * No linking functionality at all, no way to implement it
UR_APIEXPORT ur_result_t UR_APICALL
urProgramLink([[maybe_unused]] ur_context_handle_t hContext,
              [[maybe_unused]] uint32_t count,
              [[maybe_unused]] const ur_program_handle_t *phPrograms,
              [[maybe_unused]] const char *pOptions,
              [[maybe_unused]] ur_program_handle_t *phProgram) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

// MISSING FUNCTIONALITY:
// * No way to pass or retrieve build options
// * No way to enable build error logging or retrieve build log
UR_APIEXPORT ur_result_t UR_APICALL urProgramGetBuildInfo(
    ur_program_handle_t hProgram, [[maybe_unused]] ur_device_handle_t hDevice,
    ur_program_build_info_t propName, size_t propSize, void *pPropValue,
    size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);

  switch (propName) {
  case UR_PROGRAM_BUILD_INFO_STATUS: {
    return ReturnValue(hProgram->BuildStatus);
  }
  case UR_PROGRAM_BUILD_INFO_OPTIONS:
  case UR_PROGRAM_BUILD_INFO_LOG:
  default:
    break;
  }
  return UR_RESULT_ERROR_INVALID_ENUMERATION;
}

UR_APIEXPORT ur_result_t UR_APICALL
urProgramRetain(ur_program_handle_t hProgram) {
  UR_ASSERT(hProgram->getReferenceCount() > 0, UR_RESULT_ERROR_INVALID_PROGRAM);
  hProgram->incrementReferenceCount();
  return UR_RESULT_SUCCESS;
}

// MISSING FUNCTIONALITY:
// * When the hProgram is deleted, the underlying omptarget binary data is NOT
//   freed. It will only be unloaded when the device is destroyed. There is no
//   way to manually release it.
UR_APIEXPORT ur_result_t UR_APICALL
urProgramRelease(ur_program_handle_t hProgram) {
  UR_ASSERT(hProgram->getReferenceCount() != 0,
            UR_RESULT_ERROR_INVALID_PROGRAM);

  if (hProgram->decrementReferenceCount() == 0) {
    delete hProgram;
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urProgramGetNativeHandle([[maybe_unused]] ur_program_handle_t hProgram,
                         [[maybe_unused]] ur_native_handle_t *phNativeProgram) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramCreateWithNativeHandle(
    [[maybe_unused]] ur_native_handle_t hNativeProgram,
    [[maybe_unused]] ur_context_handle_t,
    [[maybe_unused]] const ur_program_native_properties_t *,
    [[maybe_unused]] ur_program_handle_t *phProgram) {
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramSetSpecializationConstants(
    [[maybe_unused]] ur_program_handle_t hProgram,
    [[maybe_unused]] uint32_t count,
    [[maybe_unused]] const ur_specialization_constant_info_t *pSpecConstants) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urProgramGetFunctionPointer(
    [[maybe_unused]] ur_device_handle_t hDevice, ur_program_handle_t hProgram,
    const char *pFunctionName, void **ppFunctionPointer) {
  if (!hProgram->TargetTable) {
    return UR_RESULT_ERROR_INVALID_PROGRAM_EXECUTABLE;
  }

  void *Function = hProgram->getFunctionPointer(pFunctionName);
  if (Function) {
    *ppFunctionPointer = Function;
    return UR_RESULT_SUCCESS;
  } else {
    return UR_RESULT_ERROR_INVALID_FUNCTION_NAME;
  }
}

ur_program_handle_t_::ur_program_handle_t_(ur_context_handle_t Context)
    : Binary{}, BinarySizeInBytes{0}, RefCount{1}, Context{Context} {
  urContextRetain(Context);
}

ur_result_t ur_program_handle_t_::setBinary(const char *Source, size_t Length) {
  // Do not re-set program binary data which has already been set as that will
  // delete the old binary data.
  UR_ASSERT(Binary == nullptr && BinarySizeInBytes == 0,
            UR_RESULT_ERROR_INVALID_OPERATION);
  Binary = Source;
  BinarySizeInBytes = Length;
  return UR_RESULT_SUCCESS;
}

std::pair<std::string, std::string>
splitMetadataName(const std::string &metadataName) {
  size_t splitPos = metadataName.rfind('@');
  if (splitPos == std::string::npos)
    return std::make_pair(metadataName, std::string{});
  return std::make_pair(metadataName.substr(0, splitPos),
                        metadataName.substr(splitPos, metadataName.length()));
}

// NOTE: This is a copy of the metadata implementation in the CUDA adapter
ur_result_t
ur_program_handle_t_::setMetadata(const ur_program_metadata_t *Metadata,
                                  size_t Length) {
  for (size_t i = 0; i < Length; ++i) {
    const ur_program_metadata_t MetadataElement = Metadata[i];
    std::string MetadataElementName{MetadataElement.pName};

    auto [Prefix, Tag] = splitMetadataName(MetadataElementName);

    if (Tag == __SYCL_UR_PROGRAM_METADATA_TAG_REQD_WORK_GROUP_SIZE) {
      // If metadata is reqd_work_group_size, record it for the corresponding
      // kernel name.
      size_t MDElemsSize = MetadataElement.size - sizeof(std::uint64_t);

      // Expect between 1 and 3 32-bit integer values.
      UR_ASSERT(MDElemsSize >= sizeof(std::uint32_t) &&
                    MDElemsSize <= sizeof(std::uint32_t) * 3,
                UR_RESULT_ERROR_INVALID_WORK_GROUP_SIZE);

      // Get pointer to data, skipping 64-bit size at the start of the data.
      const char *ValuePtr =
          reinterpret_cast<const char *>(MetadataElement.value.pData) +
          sizeof(std::uint64_t);
      // Read values and pad with 1's for values not present.
      std::uint32_t ReqdWorkGroupElements[] = {1, 1, 1};
      std::memcpy(ReqdWorkGroupElements, ValuePtr, MDElemsSize);
      KernelReqdWorkGroupSizeMD[Prefix] =
          std::make_tuple(ReqdWorkGroupElements[0], ReqdWorkGroupElements[1],
                          ReqdWorkGroupElements[2]);
    } else if (Tag == __SYCL_UR_PROGRAM_METADATA_GLOBAL_ID_MAPPING) {
      const char *MetadataValPtr =
          reinterpret_cast<const char *>(MetadataElement.value.pData) +
          sizeof(std::uint64_t);
      const char *MetadataValPtrEnd =
          MetadataValPtr + MetadataElement.size - sizeof(std::uint64_t);
      GlobalIDMD[Prefix] = std::string{MetadataValPtr, MetadataValPtrEnd};
    }
  }
  return UR_RESULT_SUCCESS;
}

void *ur_program_handle_t_::getFunctionPointer(const char *Name) {

  // If the function name is not known yet, try rebuilding the target table with
  // this function included.
  // NOTE: For some reason libomptarget needs a pointer (which probably
  // represents the host pointer of the offloaded function) to be able to
  // query for the device function pointer even though it is not used, so just
  // pass a dummy value.
  auto *FakeHostPtr = (void *)0x12345678lu;
  if (KnownFuncs.count(Name) == 0) {
    // Lock to prevent concurrent kernel lookups modifying the known function
    // names simultaneously - KnownFuncs may contain an invalid function name
    // until the end of this block
    std::lock_guard<std::mutex>{ProgramBuildMutex};
    auto [FuncNameIter, _] = KnownFuncs.insert(Name);
    SearchEntries.push_back(__tgt_offload_entry{
        FakeHostPtr, const_cast<char *>(FuncNameIter->c_str()), 0, 0, 0});

    __tgt_device_image DeviceImage{};
    DeviceImage.ImageStart =
        const_cast<void *>(static_cast<const void *>(Binary));
    DeviceImage.ImageEnd =
        static_cast<char *>(DeviceImage.ImageStart) + BinarySizeInBytes - 1;

    DeviceImage.EntriesBegin = SearchEntries.data();
    DeviceImage.EntriesEnd = SearchEntries.data() + SearchEntries.size();

    auto *NewTargetTable =
        __tgt_rtl_load_binary(Context->getDevice()->getID(), &DeviceImage);

    // Function not found, return null and also drop this function name from
    // future builds
    if (!NewTargetTable) {
      SearchEntries.pop_back();
      KnownFuncs.erase(Name);
      return nullptr;
    } else {
      TargetTable = NewTargetTable;
    }
  }

  if (!TargetTable) {
    return nullptr;
  }

  auto *Entry = TargetTable->EntriesBegin;
  while (Entry != TargetTable->EntriesEnd) {
    if (std::strcmp(Entry->name, Name) == 0) {
      return Entry->addr;
    }
  }
  return nullptr;
}
