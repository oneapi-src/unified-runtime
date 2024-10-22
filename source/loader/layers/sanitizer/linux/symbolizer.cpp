/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 * See LICENSE.TXT
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */
#include "common.hpp"
#include "llvm/DebugInfo/Symbolize/DIPrinter.h"
#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include <link.h>

namespace ur_sanitizer_layer {

llvm::symbolize::LLVMSymbolizer *GetSymbolizer() {
    static llvm::symbolize::LLVMSymbolizer Symbolizer;
    return &Symbolizer;
}

llvm::symbolize::PrinterConfig GetPrinterConfig() {
    llvm::symbolize::PrinterConfig Config;
    Config.Pretty = false;
    Config.PrintAddress = false;
    Config.PrintFunctions = true;
    Config.SourceContextLines = 0;
    Config.Verbose = false;
    return Config;
}

uptr GetModuleBase(const char *ModuleName) {
    uptr Data = (uptr)ModuleName;
    int Result = dl_iterate_phdr(
        [](struct dl_phdr_info *Info, size_t, void *Arg) {
            uptr *Data = (uptr *)Arg;
            const char *ModuleName = (const char *)(*Data);
            if (strstr(Info->dlpi_name, ModuleName)) {
                *Data = (uptr)Info->dlpi_addr;
                return 1;
            }
            return 0;
        },
        (void *)&Data);

    // If dl_iterate_phdr return 0, it means the module is main executable,
    // its base address should be 0.
    if (!Result) {
        return 0;
    }
    return Data;
}

} // namespace ur_sanitizer_layer

extern "C" {

void SymbolizeCode(const char *ModuleName, uint64_t ModuleOffset,
                   char *ResultString, size_t ResultSize, size_t *RetSize) {
    std::string Result;
    llvm::raw_string_ostream OS(Result);
    llvm::symbolize::Request Request{ModuleName, ModuleOffset};
    llvm::symbolize::PrinterConfig Config =
        ur_sanitizer_layer::GetPrinterConfig();
    llvm::symbolize::ErrorHandler EH = [&](const llvm::ErrorInfoBase &ErrorInfo,
                                           llvm::StringRef ErrorBanner) {
        OS << ErrorBanner;
        ErrorInfo.log(OS);
        OS << '\n';
    };
    auto Printer =
        std::make_unique<llvm::symbolize::LLVMPrinter>(OS, EH, Config);

    auto ResOrErr = ur_sanitizer_layer::GetSymbolizer()->symbolizeInlinedCode(
        ModuleName,
        {ModuleOffset - ur_sanitizer_layer::GetModuleBase(ModuleName),
         llvm::object::SectionedAddress::UndefSection});

    if (!ResOrErr) {
        return;
    }
    Printer->print(Request, *ResOrErr);
    ur_sanitizer_layer::GetSymbolizer()->pruneCache();
    if (RetSize) {
        *RetSize = Result.size() + 1;
    }
    if (ResultString) {
        std::strncpy(ResultString, Result.c_str(), ResultSize);
        ResultString[ResultSize - 1] = '\0';
    }
}
}
