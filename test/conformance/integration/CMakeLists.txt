# Copyright (C) 2024 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_conformance_kernels_test(integration
        QueueEmptyStatus.cpp
        QueueUSM.cpp
        QueueBuffer.cpp
  )
