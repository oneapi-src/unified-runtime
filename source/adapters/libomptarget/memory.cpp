//===--------- memory.cpp - Libomptarget Adapter ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

#include "memory.hpp"

UR_APIEXPORT ur_result_t UR_APICALL urMemBufferCreate(
    [[maybe_unused]] ur_context_handle_t hContext,
    [[maybe_unused]] ur_mem_flags_t flags, [[maybe_unused]] size_t size,
    [[maybe_unused]] const ur_buffer_properties_t *pProperties,
    [[maybe_unused]] ur_mem_handle_t *phBuffer) {
  ur_result_t Result = UR_RESULT_SUCCESS;

  if (flags &
      (UR_MEM_FLAG_USE_HOST_POINTER | UR_MEM_FLAG_ALLOC_COPY_HOST_POINTER)) {
    UR_ASSERT(pProperties && pProperties->pHost,
              UR_RESULT_ERROR_INVALID_HOST_PTR);
  }
  UR_ASSERT(size != 0, UR_RESULT_ERROR_INVALID_BUFFER_SIZE);

  try {

    void *Ptr;
    void *HostPtr = pProperties ? pProperties->pHost : nullptr;
    ur_mem_handle_t_::AllocMode Mode;
    int Err = 0;

    if (flags & UR_MEM_FLAG_USE_HOST_POINTER) {
      // The current plugin for CUDA does do currently implement registering
      // host memory into the cuda memory space.
      return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (flags & UR_MEM_FLAG_ALLOC_HOST_POINTER) {
      Ptr = __tgt_rtl_data_alloc(hContext->getDevice()->getID(), size, HostPtr,
                                 TARGET_ALLOC_HOST);
      if (!Ptr) {
        return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
      }
      Mode = ur_mem_handle_t_::AllocMode::AllocHostPtr;
    } else if (flags & UR_MEM_FLAG_ALLOC_COPY_HOST_POINTER) {
      Ptr = __tgt_rtl_data_alloc(hContext->getDevice()->getID(), size, HostPtr,
                                 TARGET_ALLOC_HOST);
      if (!Ptr) {
        return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
      }
      // memcpy from the HostPtr into the new allocation
      Err = __tgt_rtl_data_submit(hContext->getDevice()->getID(), Ptr, HostPtr,
                                  size);
      if (Err) {
        __tgt_rtl_data_delete(hContext->getDevice()->getID(), Ptr,
                              TARGET_ALLOC_HOST);
        return UR_RESULT_ERROR_INVALID_MEM_OBJECT;
      }
      Mode = ur_mem_handle_t_::AllocMode::AllocCopyHostPtr;
    } else {
      Ptr = __tgt_rtl_data_alloc(hContext->getDevice()->getID(), size, HostPtr,
                                 TARGET_ALLOC_DEVICE);
      if (!Ptr) {
        return UR_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
      }
      Mode = ur_mem_handle_t_::AllocMode::Classic;
    }

    auto mem = std::make_unique<ur_mem_handle_t_>(hContext, nullptr, flags,
                                                  Mode, Ptr, HostPtr, size);

    if (!mem) {
      return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }
    *phBuffer = mem.release();

  } catch (std::bad_alloc &) {
    Result = UR_RESULT_ERROR_OUT_OF_RESOURCES;
  } catch (...) {
    Result = UR_RESULT_ERROR_UNKNOWN;
  }

  return Result;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemImageCreate(
    [[maybe_unused]] ur_context_handle_t hContext,
    [[maybe_unused]] ur_mem_flags_t flags,
    [[maybe_unused]] const ur_image_format_t *pImageFormat,
    [[maybe_unused]] const ur_image_desc_t *pImageDesc,
    [[maybe_unused]] void *pHost, [[maybe_unused]] ur_mem_handle_t *phMem) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL
urMemBufferPartition([[maybe_unused]] ur_mem_handle_t hBuffer,
                     [[maybe_unused]] ur_mem_flags_t flags,
                     [[maybe_unused]] ur_buffer_create_type_t bufferCreateType,
                     [[maybe_unused]] const ur_buffer_region_t *pRegion,
                     [[maybe_unused]] ur_mem_handle_t *phMem) {
  // Default value for flags means UR_MEM_FLAG_READ_WRITE.
  if (flags == 0) {
    flags = UR_MEM_FLAG_READ_WRITE;
  }

  UR_ASSERT(!(flags &
              (UR_MEM_FLAG_ALLOC_COPY_HOST_POINTER |
               UR_MEM_FLAG_ALLOC_HOST_POINTER | UR_MEM_FLAG_USE_HOST_POINTER)),
            UR_RESULT_ERROR_INVALID_VALUE);
  if (hBuffer->MemFlags & UR_MEM_FLAG_WRITE_ONLY) {
    UR_ASSERT(!(flags & (UR_MEM_FLAG_READ_WRITE | UR_MEM_FLAG_READ_ONLY)),
              UR_RESULT_ERROR_INVALID_VALUE);
  }
  if (hBuffer->MemFlags & UR_MEM_FLAG_READ_ONLY) {
    UR_ASSERT(!(flags & (UR_MEM_FLAG_READ_WRITE | UR_MEM_FLAG_WRITE_ONLY)),
              UR_RESULT_ERROR_INVALID_VALUE);
  }

  UR_ASSERT(pRegion->size != 0u, UR_RESULT_ERROR_INVALID_BUFFER_SIZE);

  assert((pRegion->origin <= (pRegion->origin + pRegion->size)) && "Overflow");
  UR_ASSERT(pRegion->origin + pRegion->size <= hBuffer->GetSize(),
            UR_RESULT_ERROR_INVALID_BUFFER_SIZE);

  const auto AllocMode = ur_mem_handle_t_::AllocMode::Classic;

  void *Ptr = static_cast<uint8_t *>(hBuffer->Ptr) + pRegion->origin;
  void *HostPtr = nullptr;
  if (hBuffer->HostPtr) {
    HostPtr = static_cast<uint8_t *>(hBuffer->HostPtr) + pRegion->origin;
  }

  std::unique_ptr<ur_mem_handle_t_> MemObj{nullptr};
  try {
    MemObj = std::make_unique<ur_mem_handle_t_>(hBuffer->GetContext(), hBuffer,
                                                flags, AllocMode, Ptr, HostPtr,
                                                pRegion->size);
  } catch (ur_result_t Err) {
    *phMem = nullptr;
    return Err;
  } catch (...) {
    *phMem = nullptr;
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  }

  *phMem = MemObj.release();
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urMemGetNativeHandle([[maybe_unused]] ur_mem_handle_t hMem,
                     [[maybe_unused]] ur_native_handle_t *phNativeMem) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemBufferCreateWithNativeHandle(
    [[maybe_unused]] ur_native_handle_t hNativeMem,
    [[maybe_unused]] ur_context_handle_t hContext,
    [[maybe_unused]] const ur_mem_native_properties_t *pProperties,
    [[maybe_unused]] ur_mem_handle_t *phMem) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemImageCreateWithNativeHandle(
    [[maybe_unused]] ur_native_handle_t hNativeMem,
    [[maybe_unused]] ur_context_handle_t hContext,
    [[maybe_unused]] const ur_image_format_t *pImageFormat,
    [[maybe_unused]] const ur_image_desc_t *pImageDesc,
    [[maybe_unused]] const ur_mem_native_properties_t *pProperties,
    [[maybe_unused]] ur_mem_handle_t *phMem) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemGetInfo(
    [[maybe_unused]] ur_mem_handle_t hMemory,
    [[maybe_unused]] ur_mem_info_t propName, [[maybe_unused]] size_t propSize,
    [[maybe_unused]] void *pPropValue, [[maybe_unused]] size_t *pPropSizeRet) {
  UrReturnHelper ReturnValue(propSize, pPropValue, pPropSizeRet);
  switch (propName) {
  case UR_MEM_INFO_SIZE:
    return ReturnValue(hMemory->GetSize());
  case UR_MEM_INFO_CONTEXT:
    return ReturnValue(hMemory->GetContext());

  default:
    return UR_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
  }

  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL urMemImageGetInfo(
    [[maybe_unused]] ur_mem_handle_t hMemory,
    [[maybe_unused]] ur_image_info_t propName, [[maybe_unused]] size_t propSize,
    [[maybe_unused]] void *pPropValue, [[maybe_unused]] size_t *pPropSizeRet) {
  OMPT_DIE("Feature is not implemented");
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

UR_APIEXPORT ur_result_t UR_APICALL
urMemRetain([[maybe_unused]] ur_mem_handle_t hMem) {
  UR_ASSERT(hMem->GetReferenceCount() > 0, UR_RESULT_ERROR_INVALID_MEM_OBJECT);
  hMem->IncrementReferenceCount();
  return UR_RESULT_SUCCESS;
}

UR_APIEXPORT ur_result_t UR_APICALL
urMemRelease([[maybe_unused]] ur_mem_handle_t hMem) {
  ur_result_t Result = UR_RESULT_SUCCESS;
  try {
    if (hMem->DecrementReferenceCount() > 0) {
      return UR_RESULT_SUCCESS;
    }
    std::unique_ptr<ur_mem_handle_t_> MemObjPtr(hMem);
  } catch (...) {
    return UR_RESULT_ERROR_UNKNOWN;
  }
  return Result;
}
