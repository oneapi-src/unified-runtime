//===--------- usm.hpp - CUDA Adapter -------------------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "common.hpp"

#include <umf_helpers.hpp>
#include <umf_pools/disjoint_pool_config_parser.hpp>
#include <ur_pool_manager.hpp>

usm::DisjointPoolAllConfigs InitializeDisjointPoolConfig();

struct ur_usm_pool_handle_t_ {
  std::atomic_uint32_t RefCount = 1;

  ur_context_handle_t Context = nullptr;

  usm::DisjointPoolAllConfigs DisjointPoolConfigs =
      usm::DisjointPoolAllConfigs();

  usm::pool_manager<usm::pool_descriptor> PoolManager;

  ur_usm_pool_handle_t_(ur_context_handle_t Context,
                        ur_usm_pool_desc_t *PoolDesc);

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }

  bool hasUMFPool(umf_memory_pool_t *umf_pool);
};

// Exception type to pass allocation errors
class UsmAllocationException {
  const ur_result_t Error;

public:
  UsmAllocationException(ur_result_t Err) : Error{Err} {}
  ur_result_t getError() const { return Error; }
};

// Implements memory allocation via driver API for USM allocator interface.
class USMMemoryProvider {
private:
  ur_result_t &getLastStatusRef() {
    static thread_local ur_result_t LastStatus = UR_RESULT_SUCCESS;
    return LastStatus;
  }

protected:
  ur_context_handle_t Context;
  ur_device_handle_t Device;
  size_t MinPageSize;

  // Internal allocation and deallocation routines which must be implemented for
  // each allocation type
  virtual ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                                   uint32_t Alignment) = 0;
  virtual ur_result_t freeImpl(ur_context_handle_t Context, void *Ptr) = 0;

public:
  umf_result_t initialize(ur_context_handle_t Ctx, ur_device_handle_t Dev);
  umf_result_t alloc(size_t Size, size_t Align, void **Ptr);
  umf_result_t free(void *Ptr, size_t Size);
  void get_last_native_error(const char **ErrMsg, int32_t *ErrCode);
  umf_result_t get_min_page_size(void *, size_t *);
  umf_result_t get_recommended_page_size(size_t, size_t *) {
    return UMF_RESULT_ERROR_NOT_SUPPORTED;
  };
  umf_result_t purge_lazy(void *, size_t) {
    return UMF_RESULT_ERROR_NOT_SUPPORTED;
  };
  umf_result_t purge_force(void *, size_t) {
    return UMF_RESULT_ERROR_NOT_SUPPORTED;
  };
  virtual const char *get_name() = 0;

  virtual ~USMMemoryProvider() = default;
};

// Allocation routines for shared memory type
class USMSharedMemoryProvider final : public USMMemoryProvider {
public:
  const char *get_name() override { return "USMSharedMemoryProvider"; }

protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           uint32_t Alignment) override;
  ur_result_t freeImpl(ur_context_handle_t Context, void *Ptr) override;
};

// Allocation routines for device memory type
class USMDeviceMemoryProvider final : public USMMemoryProvider {
public:
  const char *get_name() override { return "USMSharedMemoryProvider"; }

protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           uint32_t Alignment) override;
  ur_result_t freeImpl(ur_context_handle_t Context, void *Ptr) override;
};

// Allocation routines for host memory type
class USMHostMemoryProvider final : public USMMemoryProvider {
public:
  const char *get_name() override { return "USMSharedMemoryProvider"; }

protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           uint32_t Alignment) override;
  ur_result_t freeImpl(ur_context_handle_t Context, void *Ptr) override;
};

ur_result_t USMDeviceAllocImpl(void **ResultPtr, ur_context_handle_t Context,
                               ur_device_handle_t Device,
                               ur_usm_device_mem_flags_t *Flags, size_t Size,
                               uint32_t Alignment);

ur_result_t USMSharedAllocImpl(void **ResultPtr, ur_context_handle_t Context,
                               ur_device_handle_t Device,
                               ur_usm_host_mem_flags_t *,
                               ur_usm_device_mem_flags_t *, size_t Size,
                               uint32_t Alignment);

ur_result_t USMHostAllocImpl(void **ResultPtr, ur_context_handle_t Context,
                             ur_usm_host_mem_flags_t *Flags, size_t Size,
                             uint32_t Alignment);

// Simple proxy for memory allocations. It is used for the UMF tracking
// capabilities.
class USMProxyPool {
public:
  umf_result_t initialize(umf_memory_provider_handle_t *Providers,
                          size_t NumProviders) noexcept {
    std::ignore = NumProviders;

    this->hProvider = Providers[0];
    return UMF_RESULT_SUCCESS;
  }
  void *malloc(size_t Size) noexcept { return aligned_malloc(Size, 0); }
  void *calloc(size_t Num, size_t Size) noexcept {
    std::ignore = Num;
    std::ignore = Size;

    // Currently not needed
    umf::getPoolLastStatusRef<USMProxyPool>() = UMF_RESULT_ERROR_NOT_SUPPORTED;
    return nullptr;
  }
  void *realloc(void *Ptr, size_t Size) noexcept {
    std::ignore = Ptr;
    std::ignore = Size;

    // Currently not needed
    umf::getPoolLastStatusRef<USMProxyPool>() = UMF_RESULT_ERROR_NOT_SUPPORTED;
    return nullptr;
  }
  void *aligned_malloc(size_t Size, size_t Alignment) noexcept {
    void *Ptr = nullptr;
    auto Ret = umfMemoryProviderAlloc(hProvider, Size, Alignment, &Ptr);
    if (Ret != UMF_RESULT_SUCCESS) {
      umf::getPoolLastStatusRef<USMProxyPool>() = Ret;
    }
    return Ptr;
  }
  size_t malloc_usable_size(void *Ptr) noexcept {
    std::ignore = Ptr;

    // Currently not needed
    return 0;
  }
  enum umf_result_t free(void *Ptr) noexcept {
    return umfMemoryProviderFree(hProvider, Ptr, 0);
  }
  enum umf_result_t get_last_allocation_error() {
    return umf::getPoolLastStatusRef<USMProxyPool>();
  }
  umf_memory_provider_handle_t hProvider;
};

// Template helper function for creating USM pools for given pool descriptor.
template <typename P, typename... Args>
std::pair<ur_result_t, umf::pool_unique_handle_t>
createUMFPoolForDesc(usm::pool_descriptor &Desc, Args &&...args);
