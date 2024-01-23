//===--------- common.hpp - Libomptarget Adapter ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//
#pragma once

#include <omptargetplugin.h>
#include <ur/ur.hpp>

#include <sstream>

// NOTE: This declaration is missing from omptargetplugin.h - it's not a problem
// for OpenMP since the symbol is loaded via dlsym anyway. We explicitly define
// it here as a workaround
extern "C" {

int32_t __tgt_rtl_launch_kernel(int32_t DeviceId, void *TgtEntryPtr,
                                void **TgtArgs, ptrdiff_t *TgtOffsets,
                                KernelArgsTy *KernelArgs,
                                __tgt_async_info *AsyncInfoPtr);
}

namespace omptarget_adapter {

constexpr size_t MaxMessageSize = 256;
extern thread_local int32_t ErrorMessageCode;
extern thread_local char ErrorMessage[MaxMessageSize];

// Utility function for setting a message and warning
[[maybe_unused]] void setErrorMessage(const char *Message, int32_t ErrorCode);

std::string generateErrorMessage(const char *Message, const char *Function,
                                 int Line, const char *File);

ur_result_t handleNativeError(int32_t Error, const char *Function, int Line,
                              const char *File);

void assertion(bool Condition, const char *Message = nullptr);

[[noreturn]] void die(const char *Message);
} // namespace omptarget_adapter

#define OMPT_DIE(Message)                                                      \
  omptarget_adapter::die(omptarget_adapter::generateErrorMessage(              \
                             Message, __func__, __LINE__, __FILE__)            \
                             .c_str());

#define OMPT_RETURN_ON_FAILURE(Ompt_call)                                      \
  if (const int32_t Ompt_result = Ompt_call; Ompt_result != OFFLOAD_SUCCESS) { \
    omptarget_adapter::handleNativeError(Ompt_result, __func__, __LINE__,      \
                                         __FILE__);                            \
    return UR_RESULT_ERROR_ADAPTER_SPECIFIC;                                   \
  }

#define OMPT_RETURN_ON_FAILURE_VOID(Ompt_call, Result)                         \
  if (const int32_t Ompt_result = Ompt_call; Ompt_result != OFFLOAD_SUCCESS) { \
    omptarget_adapter::handleNativeError(Ompt_result, __func__, __LINE__,      \
                                         __FILE__);                            \
    Result = UR_RESULT_ERROR_ADAPTER_SPECIFIC;                                 \
    return;                                                                    \
  }
