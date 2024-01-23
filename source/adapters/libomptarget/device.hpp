//===----------- device.hpp - Libomptarget Adapter --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//
#pragma once

#include "common.hpp"

struct ur_device_handle_t_ {

private:
  int ID;
  std::atomic_uint32_t RefCount;
  ur_platform_handle_t Platform;

  int MaxChosenLocalMem{0};
  bool MaxLocalMemSizeChosen{false};

public:
  ur_device_handle_t_(int ID, ur_platform_handle_t Platform)
      : ID(ID), RefCount{1}, Platform(Platform) {}

  int getID() const { return ID; }
  uint32_t getRefCount() const { return RefCount; }
  ur_platform_handle_t getPlatform() const { return Platform; }

  int getMaxChosenLocalMem() const noexcept { return MaxChosenLocalMem; };
  bool maxLocalMemSizeChosen() { return MaxLocalMemSizeChosen; };
};
