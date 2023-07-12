/*
    Copyright (c) 2022 Intel Corporation
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
        http://www.apache.org/licenses/LICENSE-2.0
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__
#include <numaif.h>
#include <sys/mman.h>
#endif

// TODO: expose anonymous / file-backed memory as well?

// TODO: do we want this interop with linux?
#ifdef __linux__
enum umf_mem_protection {
    ProtectionNone = PROT_NONE,
    ProtectionRead = PROT_READ,
    ProtectionWrite = PROT_WRITE,
    ProtectionExec = PROT_EXEC
};
#else
enum umf_mem_protection {
    ProtectionNone = 0,
    ProtectionRead = 2,
    ProtectionWrite = 4,
    ProtectionExec = 8
};
#endif

#ifdef __linux__
enum umf_mem_visibility {
    VisibilityShared = MAP_SHARED,
    VisibilityPrivate = MAP_PRIVATE
};
#else
enum uma_mem_visibility { VisibilityShared,
    VisibilityPrivate };
#endif

// Corresponds to arguments to mbind
#ifdef __linux__
enum umf_numa_mode {
    NumaModeDefault = MPOL_DEFAULT,
    NumaModeBind = MPOL_BIND,
    NumaModeInterleave = MPOL_INTERLEAVE,
    NumaModePreferred = MPOL_PREFERRED,
    NumaModeLocal = MPOL_LOCAL
    // TODO: error 'undeclared here'
    // NumaModeStaticNodes = MPOL_F_STATIC_NODES,
    // NumaModeRelativeNodes = MPOL_F_RELATIVE_NODES
};
enum umf_numa_flags {
    NumaFlagsStrict = MPOL_MF_STRICT,
    NumaFlagsMove = MPOL_MF_MOVE,
    NumaFlagsMoveAll = MPOL_MF_MOVE_ALL
};
#else
enum umf_numa_mode {
    NumaModeDefault = 0,
    NumaModeBind,
    NumaModeInterleave,
    NumaModePreferred,
    NumaModeLocal,
    NumaModeStaticNodes,
    NumaModeRelativeNodes
};
enum umf_numa_flags { NumaFlagsStrict = 1,
    NumaFlagsMove,
    NumaFlagsMoveAll };
#endif

typedef struct umf_os_memory_provider_config_t* umf_os_memory_provider_config_handle_t;

umf_os_memory_provider_config_handle_t umfOsMemoryProviderConfigCreate();
void umfOsMemoryProviderConfigDestroy(umf_os_memory_provider_config_handle_t);

void umfOsMemoryProviderConfigSetMemoryProtection(
    umf_os_memory_provider_config_handle_t handle, int memoryProtection);
void umfOsMemoryProviderConfigSetMemoryVisibility(
    umf_os_memory_provider_config_handle_t handle,
    enum umf_mem_visibility memoryVisibility);

// TODO: do we want to return error if numa mask is wrong (e.g. not enough
// permissions) early or just on allocation?
enum umf_result_t umfOsMemoryProviderConfigSetNumaMemBind(
    umf_os_memory_provider_config_handle_t handle,
    const unsigned long* nodemask, unsigned long maxnode, int mode, int flags);

extern struct umf_memory_provider_ops_t OS_MEMORY_PROVIDER_OPS;

#ifdef __cplusplus
}
#endif
