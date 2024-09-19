//===--------- program.hpp - Level Zero Adapter ---------------------------===//
//
// Copyright (C) 2023 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

#include "common.hpp"
#include "context.hpp"
#include "device.hpp"

struct ur_program_handle_t_ : _ur_object {
  // ur_program_handle_t_() {}

  typedef enum {
    // The program has been created from intermediate language (SPIR-V), but it
    // is not yet compiled.
    IL,

    // The program has been created by loading native code, but it has not yet
    // been built.  This is equivalent to an OpenCL "program executable" that
    // is loaded via clCreateProgramWithBinary().
    Native,

    // The program was notionally compiled from SPIR-V form.  However, since we
    // postpone compilation until the module is linked, the internal state
    // still represents the module as SPIR-V.
    Object,

    // The program has been built or linked, and it is represented as a Level
    // Zero module.
    Exe,

    // An error occurred during urProgramLink, but we created a
    // ur_program_handle_t
    // object anyways in order to hold the ZeBuildLog.  Note that the ZeModule
    // may or may not be nullptr in this state, depending on the error.
    Invalid
  } state;

  // A utility class that converts specialization constants into the form
  // required by the Level Zero driver.
  class SpecConstantShim {
  public:
    SpecConstantShim(ur_program_handle_t_ *Program) {
      ZeSpecConstants.numConstants = Program->SpecConstants.size();
      ZeSpecContantsIds.reserve(ZeSpecConstants.numConstants);
      ZeSpecContantsValues.reserve(ZeSpecConstants.numConstants);

      for (auto &SpecConstant : Program->SpecConstants) {
        ZeSpecContantsIds.push_back(SpecConstant.first);
        ZeSpecContantsValues.push_back(SpecConstant.second);
      }
      ZeSpecConstants.pConstantIds = ZeSpecContantsIds.data();
      ZeSpecConstants.pConstantValues = ZeSpecContantsValues.data();
    }

    const ze_module_constants_t *ze() { return &ZeSpecConstants; }

  private:
    std::vector<uint32_t> ZeSpecContantsIds;
    std::vector<const void *> ZeSpecContantsValues;
    ze_module_constants_t ZeSpecConstants;
  };

  // Construct a program in IL.
  ur_program_handle_t_(state St, ur_context_handle_t Context, const void *Input,
                       size_t Length)
      : Context{Context}, NativeProperties{nullptr}, OwnZeModule{true},
        SpirvCode{new uint8_t[Length]}, SpirvCodeLength{Length},
        InteropZeModule{nullptr} {
    std::memcpy(SpirvCode.get(), Input, Length);
    // All devices have the program in IL state.
    for (auto &Device : Context->getDevices()) {
      DeviceData &PerDevData = DeviceDataMap[Device->ZeDevice];
      PerDevData.State = St;
    }
  }

  // Construct a program in NATIVE for multiple devices.
  ur_program_handle_t_(state St, ur_context_handle_t Context,
                       const uint32_t NumDevices,
                       const ur_device_handle_t *Devices,
                       const ur_program_properties_t *Properties,
                       const uint8_t **Inputs, const size_t *Lengths)
      : Context{Context}, NativeProperties(Properties), OwnZeModule{true},
        InteropZeModule{nullptr} {
    for (uint32_t I = 0; I < NumDevices; ++I) {
      DeviceData &PerDevData = DeviceDataMap[Devices[I]->ZeDevice];
      PerDevData.State = St;
      PerDevData.Binary = std::make_pair(
          std::unique_ptr<uint8_t[]>(new uint8_t[Lengths[I]]), Lengths[I]);
      std::memcpy(PerDevData.Binary.first.get(), Inputs[I], Lengths[I]);
    }
  }

  // Construct a program in Exe or Invalid state.
  ur_program_handle_t_(state St, ur_context_handle_t Context,
                       ze_module_handle_t ZeModule,
                       ze_module_build_log_handle_t ZeBuildLog)
      : Context{Context}, NativeProperties{nullptr}, OwnZeModule{true},
        InteropZeModule{ZeModule} {
    for (auto &Device : Context->getDevices()) {
      DeviceData &PerDevData = DeviceDataMap[Device->ZeDevice];
      PerDevData.State = St;
    }
  }

  // Construct a program in Exe state (interop).
  ur_program_handle_t_(state St, ur_context_handle_t Context,
                       ze_module_handle_t ZeModule, bool OwnZeModule)
      : Context{Context}, NativeProperties{nullptr}, OwnZeModule{OwnZeModule},
        InteropZeModule{ZeModule} {
    // TODO: Currently it is not possible to understand the device associated
    // with provided ZeModule. So we can't set the state on that device to Exe.
  }

  // Construct a program in Invalid state with a custom error message.
  ur_program_handle_t_(state St, ur_context_handle_t Context,
                       const std::string &ErrorMessage)
      : Context{Context}, NativeProperties{nullptr}, OwnZeModule{true},
        ErrorMessage{ErrorMessage}, InteropZeModule{nullptr} {}

  ~ur_program_handle_t_();
  void ur_release_program_resources(bool deletion);

  // Tracks the release state of the program handle to determine if the
  // internal handle needs to be released.
  bool resourcesReleased = false;

  const ur_context_handle_t Context; // Context of the program.

  // Properties used for the Native Build
  const ur_program_properties_t *NativeProperties;

  // Indicates if we own the ZeModule or it came from interop that
  // asked to not transfer the ownership to SYCL RT.
  const bool OwnZeModule;

  // This error message is used only in Invalid state to hold a custom error
  // message from a call to urProgramLink.
  const std::string ErrorMessage;

  // In IL and Object states, this contains the SPIR-V representation of the
  // module.
  std::unique_ptr<uint8_t[]> SpirvCode; // Array containing raw IL code.
  size_t SpirvCodeLength;               // Size (bytes) of the array.

  // Used only in IL and Object states.  Contains the SPIR-V specialization
  // constants as a map from the SPIR-V "SpecID" to a buffer that contains the
  // associated value.  The caller of the PI layer is responsible for
  // maintaining the storage of this buffer.
  std::unordered_map<uint32_t, const void *> SpecConstants;

  // The Level Zero module handle for interoperability.
  // This module handle is either initialized with the handle provided to
  // interoperability UR API, or with one of the handles after building the
  // program. This handle is returned by UR API which allows to get the native
  // handle from the program.
  // TODO: Currently interoparability UR API does not support multiple devices.
  ze_module_handle_t InteropZeModule{};

  struct DeviceData {
    // Log from the result of building the program for the device using
    // zeModuleCreate().
    ze_module_build_log_handle_t ZeBuildLog{};

    // The Level Zero module handle for the device. Used primarily in Exe state.
    ze_module_handle_t ZeModule{};

    // In Native state, contains the pair of the binary code for the device and
    // its length in bytes.
    std::pair<std::unique_ptr<uint8_t[]>, size_t> Binary{nullptr, 0};

    // Build flags used for building the program for the device.
    // May be different for different devices, for example, if
    // urProgramCompileExp was called multiple times with different build flags
    // for different devices.
    std::string BuildFlags{};

    // State of the program for the device.
    state State{};
  };

  std::unordered_map<ze_device_handle_t, DeviceData> DeviceDataMap;
};
