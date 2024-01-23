//===--------- kernel.hpp - Libomptarget Adapter -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-------------------------------------------------------------------===//
#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <string>
#include <ur_api.h>
#include <vector>

struct ur_kernel_handle_t_ {
  void *Function;
  void *FunctionWithOffsetParam;
  std::string Name;
  ur_context_handle_t Context;
  ur_program_handle_t Program;
  std::atomic_uint32_t RefCount;

  struct arguments {
    static constexpr size_t MaxParamBytes = 4000u;
    using args_t = std::array<char, MaxParamBytes>;
    using args_size_t = std::vector<size_t>;
    using args_index_t = std::vector<void *>;
    args_t Storage;
    args_size_t ParamSizes;
    args_index_t Indices;
    args_size_t OffsetPerIndex;

    std::uint32_t ImplicitOffsetArgs[3] = {0, 0, 0};

    // NOTE:
    // This implementation is an exact copy of the CUDA adapter's argument
    // implementation. Even though it was designed for CUDA, the design of
    // libomptarget means it should work for other plugins as they will expect
    // the same argument layout.
    arguments() {
      // Place the implicit offset index at the end of the indicies collection
      Indices.emplace_back(&ImplicitOffsetArgs);
    }

    /// Add an argument to the kernel.
    /// If the argument existed before, it is replaced.
    /// Otherwise, it is added.
    /// Gaps are filled with empty arguments.
    /// Implicit offset argument is kept at the back of the indices collection.
    void addArg(size_t Index, size_t Size, const void *Arg,
                size_t LocalSize = 0) {
      if (Index + 2 > Indices.size()) {
        // Move implicit offset argument index with the end
        Indices.resize(Index + 2, Indices.back());
        // Ensure enough space for the new argument
        ParamSizes.resize(Index + 1);
        OffsetPerIndex.resize(Index + 1);
      }
      ParamSizes[Index] = Size;
      // calculate the insertion point on the array
      size_t InsertPos = std::accumulate(std::begin(ParamSizes),
                                         std::begin(ParamSizes) + Index, 0);
      // Update the stored value for the argument
      std::memcpy(&Storage[InsertPos], Arg, Size);
      Indices[Index] = &Storage[InsertPos];
      OffsetPerIndex[Index] = LocalSize;
    }

    void addLocalArg(size_t Index, size_t Size) {
      size_t LocalOffset = this->getLocalSize();

      // maximum required alignment is the size of the largest vector type
      const size_t MaxAlignment = sizeof(double) * 16;

      // for arguments smaller than the maximum alignment simply align to the
      // size of the argument
      const size_t Alignment = std::min(MaxAlignment, Size);

      // align the argument
      size_t AlignedLocalOffset = LocalOffset;
      size_t Pad = LocalOffset % Alignment;
      if (Pad != 0) {
        AlignedLocalOffset += Alignment - Pad;
      }

      addArg(Index, sizeof(size_t), (const void *)&(AlignedLocalOffset),
             Size + (AlignedLocalOffset - LocalOffset));
    }

    void setImplicitOffset(size_t Size, std::uint32_t *ImplicitOffset) {
      assert(Size == sizeof(std::uint32_t) * 3);
      std::memcpy(ImplicitOffsetArgs, ImplicitOffset, Size);
    }

    void clearLocalSize() {
      std::fill(std::begin(OffsetPerIndex), std::end(OffsetPerIndex), 0);
    }

    const args_index_t &getIndices() const noexcept { return Indices; }

    uint32_t getLocalSize() const {
      return std::accumulate(std::begin(OffsetPerIndex),
                             std::end(OffsetPerIndex), 0);
    }

    const char *getStorage() const noexcept { return Storage.data(); }

  } Args;

  ur_kernel_handle_t_(void *Func, void *FuncWithOffsetParam, const char *Name,
                      ur_program_handle_t Program, ur_context_handle_t Context)
      : Function{Func}, FunctionWithOffsetParam{FuncWithOffsetParam},
        Name{Name}, Context{Context}, Program{Program}, RefCount{1} {
    urProgramRetain(Program);
    urContextRetain(Context);
  }

  void setKernelArg(int Index, size_t Size, const void *Arg) {
    Args.addArg(Index, Size, Arg);
  }

  void setKernelLocalArg(int Index, size_t Size) {
    Args.addLocalArg(Index, Size);
  }

  void setImplicitOffsetArg(size_t Size, std::uint32_t *ImplicitOffset) {
    return Args.setImplicitOffset(Size, ImplicitOffset);
  }

  uint32_t getLocalSize() const noexcept { return Args.getLocalSize(); }

  ur_context_handle_t getContext() const noexcept { return Context; };

  ur_program_handle_t getProgram() const noexcept { return Program; };

  const char *getName() const noexcept { return Name.c_str(); }

  /// Get the number of kernel arguments, excluding the implicit global offset.
  /// Note this only returns the current known number of arguments, not the
  /// real one required by the kernel, since this cannot be queried from
  /// the libomptarget API
  size_t getNumArgs() const noexcept { return Args.Indices.size() - 1; }

  ~ur_kernel_handle_t_() {
    urProgramRelease(Program);
    urContextRelease(Context);
  }

  uint32_t incrementReferenceCount() noexcept { return ++RefCount; }

  uint32_t decrementReferenceCount() noexcept { return --RefCount; }

  uint32_t getReferenceCount() const noexcept { return RefCount; }
};
