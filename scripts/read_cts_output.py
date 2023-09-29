"""
 Copyright (C) 2023 Intel Corporation

 Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
 See LICENSE.TXT
 SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""
# Print conformance test output from gtest, which is captured by the cts_exe.py script.

import os
import sys

def print_file_content(folder_path, conformance_folder):
    file_path = os.path.join(folder_path, "output.txt")
    print(f"#### {conformance_folder} ####")
    
    if os.path.isfile(file_path):
        with open(file_path, "r") as file:
            print(file.read())
    else:
        print(f"Warning: File {file_path} does not exist.")

if __name__ == '__main__':
    build_dir = sys.argv[1]
    conformance_path = os.path.join(build_dir, "test/conformance/")

    # Folder expected not to have CTS output. We expect output for all CTS binaries.
    exclude_folders = ["CmakeFiles", "testing"]

    for conformance_folder in os.listdir(conformance_path):
        if conformance_folder not in exclude_folders:
            folder_path = os.path.join(conformance_path, conformance_folder)
            print_file_content(folder_path, conformance_folder)
