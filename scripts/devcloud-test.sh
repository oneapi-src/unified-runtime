#! /bin/bash
# Copyright (C) 2023 Intel Corporation

# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

workspace=$1
compiler_c=$2
compiler_cxx=$3
build_type=$4

set -e
echo "Hostname: $(hostname)"

#source /opt/intel/oneapi/setvars.sh
export PATH=${workspace}/dpcpp_compiler/bin:$PATH
export CPATH=${workspace}/dpcpp_compiler/include:$CPATH
export LIBRARY_PATH=${workspace}/dpcpp_compiler/lib:$LIBRARY_PATH
export LD_LIBRARY_PATH=${workspace}/dpcpp_compiler/lib:$LD_LIBRARY_PATH
unset ONEAPI_DEVICE_SELECTOR
export ONEAPI_DEVICE_SELECTOR=level_zero:0

sycl-ls

cmake \
-B${workspace}/build \
-DCMAKE_C_COMPILER=${compiler_c} \
-DCMAKE_CXX_COMPILER=${compiler_cxx} \
-DUR_ENABLE_TRACING=ON \
-DUR_DEVELOPER_MODE=ON \
-DCMAKE_BUILD_TYPE=${build_type} \
-DUR_BUILD_TESTS=ON \
-DUR_FORMAT_CPP_STYLE=OFF \
-DUR_BUILD_ADAPTER_L0=ON

cmake --build ${workspace}/build -j $(nproc)

# Unset these variables is needed for the conformance test
unset LD_LIBRARY_PATH
unset LIBRARY_PATH
# Temporarily disabling platform test for L0, because of hang
# See issue: #824
cd ${workspace}/build
ctest -C ${build_type} --output-on-failure -L "conformance" -E "platform-adapter_level_zero" --timeout 180 -VV
