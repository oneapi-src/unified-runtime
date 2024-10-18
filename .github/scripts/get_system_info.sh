#!/usr/bin/env bash

#  Copyright (C) 2023 Intel Corporation
#  Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
#  See LICENSE.TXT
#  SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# get_system_info.sh - Script for printing system info

function check_L0_version {
    if command -v dpkg &> /dev/null; then
        dpkg -l | grep level-zero && return
    fi

    if command -v rpm &> /dev/null; then
        rpm -qa | grep level-zero && return
    fi

    if command -v zypper &> /dev/null; then
        zypper se level-zero && return
    fi

    echo "level-zero not installed"
}

function system_info {
	echo "**********system_info**********"
	cat /etc/os-release | grep -oP "PRETTY_NAME=\K.*"
	cat /proc/version
	`which clang` --version
	md5sum `which clang`
	`which gold` --version
	md5sum `which gold`
	`which ld` --version
	md5sum /usr/bin/ar
	/usr/bin/ar --version
	echo "Archiving:"
	md5sum build/CMakeFiles/ur_common.dir/ur_util.cpp.o build/CMakeFiles/ur_common.dir/ur_util.cpp.o
	
	echo "LibURCommon:"
	md5sum build/lib/libur_common.a
	nm build/lib/libur_common.a
	echo "Deps:"
	ldd /usr/lib/llvm-14/lib/LLVMgold.so
	echo "Hashes:"
	ldd /usr/lib/llvm-14/lib/LLVMgold.so | cut -f3 -d " " | xargs md5sum
	echo "Deps of deps:"
	ldd /usr/lib/llvm-14/lib/LLVMgold.so | cut -f3 -d " " | xargs ldd
	#echo "**********cmake options**********"
	#cat build/CMakeCache.txt
	#echo "**********Binaries**********"
	#find build $(dirname "$(readlink -f "$0")")/../../build/bin -type f | xargs md5sum | sort
	#find build $(dirname "$(readlink -f "$0")")/../../build/lib -type f | xargs md5sum | sort
}

# Call the function above to print system info.
system_info
