/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef USM_POOL
#define USM_POOL

#include "usm_allocator.hpp"
#include "usm_allocator_config.hpp"

// TODO: generalize to 'DisjointPool'
class UsmPool {
  public:
    UsmPool(uma::ur_memory_provider_handle_unique memProvider,
            USMAllocatorParameters parameters)
        : ctx(std::move(memProvider), parameters) {}

    void *malloc(size_t size) { return ctx.allocate(size); }

    void *calloc(size_t, size_t) { return NULL; }

    void *realloc(void *, size_t) { return NULL; }

    void *aligned_malloc(size_t size, size_t alignment) {
        return ctx.allocate(size, alignment);
    }

    size_t malloc_usable_size(void *) { return 0; }

    void free(void *ptr) { ctx.deallocate(ptr); }

  private:
    USMAllocContext ctx;
};

#endif
