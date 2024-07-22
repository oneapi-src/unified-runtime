// Copyright (C) 2022-2023 Intel Corporation
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "file.hpp"

std::vector<uint8_t> FileHelper::loadFile(const std::string &filePath,
                                          std::ios_base::openmode openMode) {
    std::ifstream stream(filePath, openMode);
    if (!stream.good()) {
        return {};
    }

    stream.seekg(0, stream.end);
    const size_t length = static_cast<size_t>(stream.tellg());
    stream.seekg(0, stream.beg);

    std::vector<uint8_t> content(length);
    stream.read(reinterpret_cast<char *>(content.data()), length);
    return content;
}

std::vector<uint8_t> FileHelper::loadBinaryFile(const std::string &filePath) {
    return loadFile(filePath, std::ios::in | std::ios::binary);
}
