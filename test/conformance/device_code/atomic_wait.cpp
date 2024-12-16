// Copyright (C) 2024 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <sycl/sycl.hpp>

template <typename T>
using global_atomic_ref =
    sycl::atomic_ref<T, sycl::memory_order::relaxed, sycl::memory_scope::system,
                     sycl::access::address_space::global_space>;

int main() {
    sycl::queue deviceQueue;

    auto atomic_cnt = sycl::malloc_shared<uint64_t>(1, deviceQueue);

    auto e1 = deviceQueue.submit([&](sycl::handler &cgh) {
        cgh.single_task<class atomic_wait>([=]() {
            global_atomic_ref<uint64_t> atomic(*atomic_cnt);
            while (atomic.load() == 0) {
            }
        });
    });
    e1.wait();

    return 0;
}
