# Copyright (C) 2023 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

add_conformance_devices_test(queue
  urQueueCreate.cpp 
  urQueueCreateWithNativeHandle.cpp
  urQueueFinish.cpp
  urQueueFlush.cpp
  urQueueGetInfo.cpp
  urQueueGetNativeHandle.cpp
  urQueueRetain.cpp
  urQueueRelease.cpp)
