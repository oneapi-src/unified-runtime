//===--------- program.hpp - Libomptarget Adapter ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-------------------------------------------------------------------===//
#pragma once

#include <atomic>
#include <mutex>
#include <omptargetplugin.h>
#include <set>
#include <ur_api.h>

struct ur_program_handle_t_ {
  const char *Binary;
  size_t BinarySizeInBytes;
  std::atomic_uint32_t RefCount;
  ur_context_handle_t Context;
  __tgt_target_table *TargetTable;
  ur_program_build_status_t BuildStatus = UR_PROGRAM_BUILD_STATUS_NONE;
  std::mutex ProgramBuildMutex;

  // Metadata
  std::unordered_map<std::string, std::tuple<uint32_t, uint32_t, uint32_t>>
      KernelReqdWorkGroupSizeMD;
  std::unordered_map<std::string, std::string> GlobalIDMD;

  std::vector<__tgt_offload_entry> SearchEntries{};
  std::set<std::string> KnownFuncs{};

  ur_program_handle_t_(ur_context_handle_t Context);
  ~ur_program_handle_t_() { urContextRelease(Context); }

  ur_context_handle_t getContext() { return Context; }

  ur_result_t setBinary(const char *Binary, size_t BinarySizeInBytes);

  ur_result_t setMetadata(const ur_program_metadata_t *Metadata, size_t Length);

  void *getFunctionPointer(const char *Name);

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }
};
