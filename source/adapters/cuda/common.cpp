//===--------- common.cpp - CUDA Adapter ----------------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "common.hpp"
#include "logger/ur_logger.hpp"
#include "ur_api.h"

#include <cuda.h>

#include <sstream>
#include <string.h>

ur_result_t mapErrorUR(CUresult Result) {
  switch (Result) {
  case CUDA_SUCCESS:
    return UR_RESULT_SUCCESS;
  case CUDA_ERROR_NOT_PERMITTED:
    return UR_RESULT_ERROR_INVALID_OPERATION;
  case CUDA_ERROR_INVALID_CONTEXT:
    return UR_RESULT_ERROR_INVALID_CONTEXT;
  case CUDA_ERROR_INVALID_DEVICE:
    return UR_RESULT_ERROR_INVALID_DEVICE;
  case CUDA_ERROR_INVALID_VALUE:
    return UR_RESULT_ERROR_INVALID_VALUE;
  case CUDA_ERROR_OUT_OF_MEMORY:
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  case CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES:
    return UR_RESULT_ERROR_OUT_OF_RESOURCES;
  default:
    return UR_RESULT_ERROR_UNKNOWN;
  }
}

void checkErrorUR(CUresult Result, const char *Function, int Line,
                  const char *File) {
  if (Result == CUDA_SUCCESS || Result == CUDA_ERROR_DEINITIALIZED) {
    return;
  }

  const char *ErrorString = nullptr;
  const char *ErrorName = nullptr;
  cuGetErrorName(Result, &ErrorName);
  cuGetErrorString(Result, &ErrorString);
  std::stringstream SS;
  SS << "\nUR CUDA ERROR:"
     << "\n\tValue:           " << Result
     << "\n\tName:            " << ErrorName
     << "\n\tDescription:     " << ErrorString
     << "\n\tFunction:        " << Function << "\n\tSource Location: " << File
     << ":" << Line << "\n";
  logger::error("{}", SS.str());

  if (std::getenv("PI_CUDA_ABORT") != nullptr ||
      std::getenv("UR_CUDA_ABORT") != nullptr) {
    std::abort();
  }

  throw mapErrorUR(Result);
}

void checkErrorUR(ur_result_t Result, const char *Function, int Line,
                  const char *File) {
  if (Result == UR_RESULT_SUCCESS) {
    return;
  }

  std::stringstream SS;
  SS << "\nUR ERROR:"
     << "\n\tValue:           " << Result << "\n\tFunction:        " << Function
     << "\n\tSource Location: " << File << ":" << Line << "\n";
  logger::error("{}", SS.str());

  if (std::getenv("PI_CUDA_ABORT") != nullptr) {
    std::abort();
  }

  throw Result;
}

std::string getCudaVersionString() {
  int driver_version = 0;
  cuDriverGetVersion(&driver_version);
  // The version is returned as (1000 major + 10 minor).
  std::stringstream stream;
  stream << "CUDA " << driver_version / 1000 << "."
         << driver_version % 1000 / 10;
  return stream.str();
}

void detail::ur::die(const char *Message) {
  logger::always("ur_die:{}", Message);
  std::terminate();
}

void detail::ur::assertion(bool Condition, const char *Message) {
  if (!Condition)
    die(Message);
}

// Global variables for ZER_EXT_RESULT_ADAPTER_SPECIFIC_ERROR
thread_local ur_result_t ErrorMessageCode = UR_RESULT_SUCCESS;
thread_local char ErrorMessage[MaxMessageSize];
thread_local int32_t ErrorAdapterNativeCode = 0;

// Utility function for setting a message and warning
[[maybe_unused]] void setErrorMessage(const char *pMessage,
                                      ur_result_t ErrorCode) {
  assert(strlen(pMessage) <= MaxMessageSize);
  strcpy(ErrorMessage, pMessage);
  ErrorMessageCode = ErrorCode;
}

[[maybe_unused]] void setAdapterSpecificMessage(CUresult cu_res) {
  ErrorAdapterNativeCode = static_cast<int32_t>(cu_res);
  // according to the documentation of the cuGetErrorName and cuGetErrorString
  // CUDA driver APIs, both error_name and error_string are null-terminated.
  const char *error_string;
  const char *error_name;
  cuGetErrorName(cu_res, &error_name);
  cuGetErrorString(cu_res, &error_string);
  static constexpr char new_line[] = "\n";
  // non-null-terminated sizes
  const size_t error_string_size = std::strlen(error_string);
  const size_t error_name_size = std::strlen(error_name);
  char *message = reinterpret_cast<char *>(
      std::malloc(error_string_size + error_name_size + sizeof(new_line)));
  std::strcpy(message, error_name);
  std::strcat(message, new_line);
  std::strncat(message, error_string, error_string_size);

  setErrorMessage(message, UR_RESULT_ERROR_ADAPTER_SPECIFIC);
  std::free(message);
}
