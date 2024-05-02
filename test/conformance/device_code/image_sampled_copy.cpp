// Copyright (C) 2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <sycl/sycl.hpp>

int main() {
    sycl::queue sycl_queue;

    const int width = 3;
    auto image_range = sycl::range<1>(width);
    const int channels = 4;
    std::vector<float> in_data(width * channels, 0);
    std::vector<float> out_data(width * channels, 0);

    sycl::image<1> image_in(in_data.data(), sycl::image_channel_order::rgba,
                            sycl::image_channel_type::fp32, image_range);
    sycl::image<1> image_out(out_data.data(), sycl::image_channel_order::rgba,
                             sycl::image_channel_type::fp32, image_range);

    sycl_queue.submit([&](sycl::handler &cgh) {
        sycl::accessor<sycl::float4, 1, sycl::access::mode::read,
                       sycl::access::target::image>
            in_acc(image_in, cgh);
        sycl::accessor<sycl::float4, 1, sycl::access::mode::write,
                       sycl::access::target::image>
            out_acc(image_out, cgh);

        sycl::sampler smpl(sycl::coordinate_normalization_mode::unnormalized,
                           sycl::addressing_mode::clamp,
                           sycl::filtering_mode::nearest);

        cgh.single_task<class image_sampled_copy>(
            [=]() { out_acc.write(0, in_acc.read(4, smpl)); });
    });
    return 0;
}
