// Copyright (C) 2022-2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

struct FileHelper {
    static std::vector<uint8_t> loadFile(const std::string &filePath,
                                         std::ios_base::openmode openMode);
    static std::vector<uint8_t> loadBinaryFile(const std::string &filePath);
};
