//===--------- common.cpp - Libomptarget Adapter ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

#include "common.hpp"
#include <sstream>

/* Global variables for urPlatformGetLastError() */
thread_local int32_t omptarget_adapter::ErrorMessageCode = 0;
thread_local char omptarget_adapter::ErrorMessage[MaxMessageSize];

[[maybe_unused]] void omptarget_adapter::setErrorMessage(const char *Message,
                                                         int32_t ErrorCode) {
  assert(strlen(Message) <= omptarget_adapter::MaxMessageSize);
  strcpy(omptarget_adapter::ErrorMessage, Message);
  ErrorMessageCode = ErrorCode;
}

std::string omptarget_adapter::generateErrorMessage(const char *Message,
                                                    const char *Function,
                                                    int Line,
                                                    const char *File) {
  std::stringstream SS;
  SS << Message << "\n\tFunction:        " << Function
     << "\n\tSource Location: " << File << ":" << Line << std::endl;
  return SS.str();
}

ur_result_t omptarget_adapter::handleNativeError(int32_t Error,
                                                 const char *Function, int Line,
                                                 const char *File) {
  const std::string message = omptarget_adapter::generateErrorMessage(
      "UR Native Error:", Function, Line, File);
  omptarget_adapter::setErrorMessage(message.c_str(), Error);
  omptarget_adapter::ErrorMessageCode = Error;
  omptarget_adapter::die(message.c_str());
}

void omptarget_adapter::assertion(bool Condition, const char *Message) {
  if (!Condition) {
    die(Message);
  }
}

void omptarget_adapter::die(const char *Message) {
  std::cerr << "ur_die: " << Message << "\n";
  std::terminate();
}
