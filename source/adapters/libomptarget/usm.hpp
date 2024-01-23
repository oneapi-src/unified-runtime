//===--------- usm.hpp - Libomptarget Adapter -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once
#include "common.hpp"
#include <map>
#include <umf_helpers.hpp>
#include <umf_pools/disjoint_pool_config_parser.hpp>

struct ur_usm_pool_handle_t_ {
  bool zeroInit;
  usm::DisjointPoolAllConfigs DisjointPoolConfigs;
  std::atomic_uint32_t ReferenceCount;
  ur_context_handle_t Context;
  umf::pool_unique_handle_t DeviceMemPool;
  umf::pool_unique_handle_t SharedMemPool;
  umf::pool_unique_handle_t HostMemPool;

  ur_usm_pool_handle_t_(ur_context_handle_t Context,
                        ur_usm_pool_desc_t *PoolDesc);
  ~ur_usm_pool_handle_t_();

  uint32_t IncrementReferenceCount() noexcept { return ++ReferenceCount; }
  uint32_t DecrementReferenceCount() noexcept { return --ReferenceCount; }
  uint32_t GetReferenceCount() noexcept { return ReferenceCount; }
  bool hasUMFPool(umf_memory_pool_t *UMFPool);
};

// Exception type to pass allocation errors
class USMAllocationException {
  const ur_result_t Error;

public:
  USMAllocationException(ur_result_t Err) : Error(Err) {}
  ur_result_t getError() const { return Error; }
};

class USMMemoryProvider {
  ur_result_t &getLastStatusRef() {
    static thread_local ur_result_t LastStatus = UR_RESULT_SUCCESS;
    return LastStatus;
  }

protected:
  ur_context_handle_t Context;
  ur_device_handle_t Device;
  size_t MinPageSize;

  // Internal allocation routine which must be implemented for each allocation
  // type
  virtual ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                                   uint32_t Alignment) = 0;

public:
  umf_result_t initialize(ur_context_handle_t Context,
                          ur_device_handle_t Device);
  umf_result_t alloc(size_t Size, size_t Align, void **Ptr);
  umf_result_t free(void *Ptr, size_t Size);
  void get_last_native_error(const char **ErrMsg, int32_t *ErrCode);
  umf_result_t get_min_page_size(void *, size_t *);
  umf_result_t get_recommended_page_size(size_t, size_t *) {
    return UMF_RESULT_ERROR_NOT_SUPPORTED;
  }
  umf_result_t purge_lazy(void *, size_t) {
    return UMF_RESULT_ERROR_NOT_SUPPORTED;
  }
  umf_result_t purge_force(void *, size_t) {
    return UMF_RESULT_ERROR_NOT_SUPPORTED;
  }
  const char *get_name() { return "USM"; }
  virtual ~USMMemoryProvider() = default;
};

class USMSharedMemoryProvider final : public USMMemoryProvider {
protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           uint32_t Alignment) override;
};

class USMSharedReadOnlyMemoryProvider final : public USMMemoryProvider {
protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           uint32_t Alignment) override;
};

class USMDeviceMemoryProvider final : public USMMemoryProvider {
protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           uint32_t Alignment) override;
};

class USMHostMemoryProvider final : public USMMemoryProvider {
protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           uint32_t Alignment) override;
};

struct USMAllocation {
  ur_usm_type_t alloc_type;
  void *usm_ptr;
  size_t alloc_size;
  size_t alignment;
  ur_device_handle_t Device;
};
