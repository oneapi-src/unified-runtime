//===--------- usm.hpp - Level Zero Adapter -------------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

#include <umf_helpers.hpp>
#include <ur_pool_handle.hpp>
#include <ur_pool_manager.hpp>

usm::DisjointPoolAllConfigs InitializeDisjointPoolConfig();

ur_result_t USMFreeImpl(ur_context_handle_t Context, void *Ptr);

// Helper that queries the min page size from the given pointer. If no pointer
// is provided (i.e. Ptr == nullptr), Provider is used to create a small
// allocation for the purpose of running the query.
ur_result_t GetL0MinPageSize(ur_context_handle_t Context, void *Ptr,
                             size_t *PageSize,
                             umf::USMMemoryProvider *Provider);

// Allocation routines for shared memory type
class L0SharedMemoryProvider final : public umf::USMSharedMemoryProvider {
public:
  umf_result_t initialize(usm::pool_descriptor Desc) override;

  umf_result_t get_min_page_size(void *Ptr, size_t *PageSize) override {
    if (!Ptr) {
      *PageSize = MinPageSize;
      return UMF_RESULT_SUCCESS;
    }
    UMF_PROVIDER_CHECK_UR_RESULT(
        GetL0MinPageSize(Context, Ptr, PageSize, this));
    return UMF_RESULT_SUCCESS;
  }

protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           size_t Alignment) override;

  ur_result_t freeImpl(void *Ptr, size_t) override {
    return USMFreeImpl(Context, Ptr);
  }
};

// Allocation routines for device memory type
class L0DeviceMemoryProvider final : public umf::USMDeviceMemoryProvider {
public:
  umf_result_t initialize(usm::pool_descriptor Desc) override;

  umf_result_t get_min_page_size(void *Ptr, size_t *PageSize) override {
    if (!Ptr) {
      *PageSize = MinPageSize;
      return UMF_RESULT_SUCCESS;
    }
    UMF_PROVIDER_CHECK_UR_RESULT(
        GetL0MinPageSize(Context, Ptr, PageSize, this));
    return UMF_RESULT_SUCCESS;
  }

protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           size_t Alignment) override;

  ur_result_t freeImpl(void *Ptr, size_t) override {
    return USMFreeImpl(Context, Ptr);
  }
};

// Allocation routines for host memory type
class L0HostMemoryProvider final : public umf::USMHostMemoryProvider {
public:
  umf_result_t initialize(usm::pool_descriptor Desc) override;

  umf_result_t get_min_page_size(void *Ptr, size_t *PageSize) override {
    if (!Ptr) {
      *PageSize = MinPageSize;
      return UMF_RESULT_SUCCESS;
    }
    UMF_PROVIDER_CHECK_UR_RESULT(
        GetL0MinPageSize(Context, Ptr, PageSize, this));
    return UMF_RESULT_SUCCESS;
  }

protected:
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           size_t Alignment) override;

  ur_result_t freeImpl(void *Ptr, size_t) override {
    return USMFreeImpl(Context, Ptr);
  }
};

#undef PROVIDER_CHECK_UR_RESULT

struct ur_usm_pool_handle_t_
    : public usm::pool_handle_base<L0HostMemoryProvider, L0DeviceMemoryProvider,
                                   L0SharedMemoryProvider> {
  ur_usm_pool_handle_t_(ur_context_handle_t Context,
                        ur_usm_pool_desc_t *PoolDesc)
      : usm::pool_handle_base<L0HostMemoryProvider, L0DeviceMemoryProvider,
                              L0SharedMemoryProvider>(Context, PoolDesc){};
};

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

// If indirect access tracking is not enabled then this functions just performs
// zeMemFree. If indirect access tracking is enabled then reference counting is
// performed.
ur_result_t ZeMemFreeHelper(ur_context_handle_t Context, void *Ptr);

ur_result_t USMFreeHelper(ur_context_handle_t Context, void *Ptr,
                          bool OwnZeMemHandle = true);

extern const bool UseUSMAllocator;
