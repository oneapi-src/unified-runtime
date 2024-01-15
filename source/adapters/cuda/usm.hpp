//===--------- usm.hpp - CUDA Adapter -------------------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <umf_helpers.hpp>
#include <umf_pools/disjoint_pool_config_parser.hpp>
#include <ur_pool_handle.hpp>
#include <ur_pool_manager.hpp>

class CudaUSMSharedMemoryProvider : public umf::USMSharedMemoryProvider {
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           size_t Alignment) override;
  ur_result_t freeImpl(void *Ptr, size_t) override;
};

class CudaUSMHostMemoryProvider : public umf::USMHostMemoryProvider {
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           size_t Alignment) override;
  ur_result_t freeImpl(void *Ptr, size_t) override;
};

class CudaUSMDeviceMemoryProvider : public umf::USMDeviceMemoryProvider {
  ur_result_t allocateImpl(void **ResultPtr, size_t Size,
                           size_t Alignment) override;
  ur_result_t freeImpl(void *Ptr, size_t) override;
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

struct ur_usm_pool_handle_t_
    : public usm::pool_handle_base<CudaUSMHostMemoryProvider,
                                   CudaUSMDeviceMemoryProvider,
                                   CudaUSMSharedMemoryProvider> {
  ur_usm_pool_handle_t_(ur_context_handle_t Context,
                        ur_usm_pool_desc_t *PoolDesc)
      : usm::pool_handle_base<CudaUSMHostMemoryProvider,
                              CudaUSMDeviceMemoryProvider,
                              CudaUSMSharedMemoryProvider>(Context, PoolDesc){};
};
