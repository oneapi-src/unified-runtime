//===--- disjoint_pool_config_parser.cpp -configuration for USM memory pool-==//
//
// Copyright (C) 2023-2024 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "disjoint_pool_config_parser.hpp"
#include "logger/ur_logger.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

// Don't expect any failures in setting params; the only 2 reasons for failure
// are not enough memory for allocating 'name' or a NULL params handle.
#define UMF_CALL(RET)                                                          \
    if (RET != UMF_RESULT_SUCCESS) {                                           \
        if (RET == UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY) {                      \
            throw std::runtime_error("DisjointPool name allocation failed");   \
        }                                                                      \
        throw std::runtime_error("DisjointPool setting params failed");        \
    }

namespace usm {
constexpr auto operator""_B(unsigned long long x) -> size_t { return x; }
constexpr auto operator""_KB(unsigned long long x) -> size_t {
    return x * 1024;
}
constexpr auto operator""_MB(unsigned long long x) -> size_t {
    return x * 1024 * 1024;
}
constexpr auto operator""_GB(unsigned long long x) -> size_t {
    return x * 1024 * 1024 * 1024;
}

DisjointPoolAllConfigs::~DisjointPoolAllConfigs() {
    for (auto &Config : Configs) {
        umfDisjointPoolParamsDestroy(Config);
    }
}

DisjointPoolAllConfigs::DisjointPoolAllConfigs(int trace) {
    try {
        for (auto &Config : Configs) {
            umf_disjoint_pool_params_handle_t params = NULL;
            if (umfDisjointPoolParamsCreate(&params) != UMF_RESULT_SUCCESS) {
                logger::error("DisjointPool params create failed");
                assert(0);
            }
            UMF_CALL(umfDisjointPoolParamsSetTrace(params, trace));

            Config = params;
        }

        UMF_CALL(umfDisjointPoolParamsSetName(
            Configs[DisjointPoolMemType::Host], "Host"));
        UMF_CALL(umfDisjointPoolParamsSetName(
            Configs[DisjointPoolMemType::Device], "Device"));
        UMF_CALL(umfDisjointPoolParamsSetName(
            Configs[DisjointPoolMemType::Shared], "Shared"));
        UMF_CALL(umfDisjointPoolParamsSetName(
            Configs[DisjointPoolMemType::SharedReadOnly], "SharedReadOnly"));

        // Buckets for Host use a minimum of the cache line size of 64 bytes.
        // This prevents two separate allocations residing in the same cache line.
        // Buckets for Device and Shared allocations will use starting size of 512.
        // This is because memory compression on newer GPUs makes the
        // minimum granularity 512 bytes instead of 64.
        UMF_CALL(umfDisjointPoolParamsSetMinBucketSize(
            Configs[DisjointPoolMemType::Host], 64));
        UMF_CALL(umfDisjointPoolParamsSetMinBucketSize(
            Configs[DisjointPoolMemType::Device], 512));
        UMF_CALL(umfDisjointPoolParamsSetMinBucketSize(
            Configs[DisjointPoolMemType::Shared], 512));
        UMF_CALL(umfDisjointPoolParamsSetMinBucketSize(
            Configs[DisjointPoolMemType::SharedReadOnly], 512));

        // Initialize default pool settings.
        UMF_CALL(umfDisjointPoolParamsSetMaxPoolableSize(
            Configs[DisjointPoolMemType::Host], 2_MB));
        UMF_CALL(umfDisjointPoolParamsSetCapacity(
            Configs[DisjointPoolMemType::Host], 4));
        UMF_CALL(umfDisjointPoolParamsSetSlabMinSize(
            Configs[DisjointPoolMemType::Host], 64_KB));

        UMF_CALL(umfDisjointPoolParamsSetMaxPoolableSize(
            Configs[DisjointPoolMemType::Device], 4_MB));
        UMF_CALL(umfDisjointPoolParamsSetCapacity(
            Configs[DisjointPoolMemType::Device], 4));
        UMF_CALL(umfDisjointPoolParamsSetSlabMinSize(
            Configs[DisjointPoolMemType::Device], 64_KB));

        // Disable pooling of shared USM allocations.
        UMF_CALL(umfDisjointPoolParamsSetMaxPoolableSize(
            Configs[DisjointPoolMemType::Shared], 0));
        UMF_CALL(umfDisjointPoolParamsSetCapacity(
            Configs[DisjointPoolMemType::Shared], 0));
        UMF_CALL(umfDisjointPoolParamsSetSlabMinSize(
            Configs[DisjointPoolMemType::Shared], 2_MB));

        // Allow pooling of shared allocations that are only modified on host.
        UMF_CALL(umfDisjointPoolParamsSetMaxPoolableSize(
            Configs[DisjointPoolMemType::SharedReadOnly], 4_MB));
        UMF_CALL(umfDisjointPoolParamsSetCapacity(
            Configs[DisjointPoolMemType::SharedReadOnly], 4));
        UMF_CALL(umfDisjointPoolParamsSetSlabMinSize(
            Configs[DisjointPoolMemType::SharedReadOnly], 2_MB));
    } catch (const std::runtime_error &e) {
        logger::error(e.what());
        assert(0);
    }
}

DisjointPoolAllConfigs parseDisjointPoolConfig(const std::string &config,
                                               int trace) {
    DisjointPoolAllConfigs AllConfigs;

    // TODO: replace with UR ENV var parser and avoid creating a copy of 'config'
    auto GetValue = [](std::string &Param, size_t Length, size_t &Setting) {
        size_t Multiplier = 1;
        if (tolower(Param[Length - 1]) == 'k') {
            Length--;
            Multiplier = 1_KB;
        }
        if (tolower(Param[Length - 1]) == 'm') {
            Length--;
            Multiplier = 1_MB;
        }
        if (tolower(Param[Length - 1]) == 'g') {
            Length--;
            Multiplier = 1_GB;
        }
        std::string TheNumber = Param.substr(0, Length);
        if (TheNumber.find_first_not_of("0123456789") == std::string::npos) {
            Setting = std::stoi(TheNumber) * Multiplier;
        }
    };

    auto ParamParser = [GetValue](std::string &Params, size_t &Setting,
                                  bool &ParamWasSet) {
        bool More;
        if (Params.size() == 0) {
            ParamWasSet = false;
            return false;
        }
        size_t Pos = Params.find(',');
        if (Pos != std::string::npos) {
            if (Pos > 0) {
                GetValue(Params, Pos, Setting);
                ParamWasSet = true;
            }
            Params.erase(0, Pos + 1);
            More = true;
        } else {
            GetValue(Params, Params.size(), Setting);
            ParamWasSet = true;
            More = false;
        }
        return More;
    };

    auto MemParser = [&AllConfigs, ParamParser](std::string &Params,
                                                DisjointPoolMemType memType =
                                                    DisjointPoolMemType::All) {
        bool ParamWasSet;
        size_t TmpValue = 0;
        DisjointPoolMemType LM = memType;
        if (memType == DisjointPoolMemType::All) {
            LM = DisjointPoolMemType::Host;
        }

        bool More = ParamParser(Params, TmpValue, ParamWasSet);
        if (ParamWasSet && memType == DisjointPoolMemType::All) {
            for (auto &Config : AllConfigs.Configs) {
                UMF_CALL(
                    umfDisjointPoolParamsSetMaxPoolableSize(Config, TmpValue));
            }
        } else if (ParamWasSet) {
            UMF_CALL(umfDisjointPoolParamsSetMaxPoolableSize(
                AllConfigs.Configs[LM], TmpValue));
        }

        if (More) {
            More = ParamParser(Params, TmpValue, ParamWasSet);
            if (ParamWasSet && memType == DisjointPoolMemType::All) {
                for (auto &Config : AllConfigs.Configs) {
                    UMF_CALL(
                        umfDisjointPoolParamsSetCapacity(Config, TmpValue));
                }
            } else if (ParamWasSet) {
                UMF_CALL(umfDisjointPoolParamsSetCapacity(
                    AllConfigs.Configs[LM], TmpValue));
            }
        }
        if (More) {
            ParamParser(Params, TmpValue, ParamWasSet);
            if (ParamWasSet && memType == DisjointPoolMemType::All) {
                for (auto &Config : AllConfigs.Configs) {
                    UMF_CALL(
                        umfDisjointPoolParamsSetSlabMinSize(Config, TmpValue));
                }
            } else if (ParamWasSet) {
                UMF_CALL(umfDisjointPoolParamsSetSlabMinSize(
                    AllConfigs.Configs[LM], TmpValue));
            }
        }
    };

    auto MemTypeParser = [MemParser](std::string &Params) {
        int Pos = 0;
        DisjointPoolMemType M(DisjointPoolMemType::All);
        if (Params.compare(0, 5, "host:") == 0) {
            Pos = 5;
            M = DisjointPoolMemType::Host;
        } else if (Params.compare(0, 7, "device:") == 0) {
            Pos = 7;
            M = DisjointPoolMemType::Device;
        } else if (Params.compare(0, 7, "shared:") == 0) {
            Pos = 7;
            M = DisjointPoolMemType::Shared;
        } else if (Params.compare(0, 17, "read_only_shared:") == 0) {
            Pos = 17;
            M = DisjointPoolMemType::SharedReadOnly;
        }
        if (Pos > 0) {
            Params.erase(0, Pos);
        }
        MemParser(Params, M);
    };

    size_t MaxSize = (std::numeric_limits<size_t>::max)();

    // Update pool settings if specified in environment.
    size_t EnableBuffers = 1;
    if (config != "") {
        std::string Params = config;
        size_t Pos = Params.find(';');
        if (Pos != std::string::npos) {
            if (Pos > 0) {
                GetValue(Params, Pos, EnableBuffers);
            }
            Params.erase(0, Pos + 1);
            size_t Pos = Params.find(';');
            if (Pos != std::string::npos) {
                if (Pos > 0) {
                    GetValue(Params, Pos, MaxSize);
                }
                Params.erase(0, Pos + 1);
                do {
                    try {
                        size_t Pos = Params.find(';');
                        if (Pos != std::string::npos) {
                            if (Pos > 0) {
                                std::string MemParams = Params.substr(0, Pos);
                                MemTypeParser(MemParams);
                            }
                            Params.erase(0, Pos + 1);
                            if (Params.size() == 0) {
                                break;
                            }
                        } else {
                            MemTypeParser(Params);
                            break;
                        }
                    } catch (const std::runtime_error &e) {
                        logger::error("Error in parsing DisjointPool config, "
                                      "returning default config values. ",
                                      e.what());
                        AllConfigs = DisjointPoolAllConfigs(trace);
                        break;
                    }
                } while (true);
            } else {
                // set MaxPoolSize for all configs
                GetValue(Params, Params.size(), MaxSize);
            }
        } else {
            GetValue(Params, Params.size(), EnableBuffers);
        }
    }

    AllConfigs.EnableBuffers = EnableBuffers;

    AllConfigs.limits = std::shared_ptr<umf_disjoint_pool_shared_limits_t>(
        umfDisjointPoolSharedLimitsCreate(MaxSize),
        umfDisjointPoolSharedLimitsDestroy);

    for (auto &Config : AllConfigs.Configs) {
        UMF_CALL(umfDisjointPoolParamsSetSharedLimits(Config,
                                                      AllConfigs.limits.get()));
        UMF_CALL(umfDisjointPoolParamsSetTrace(Config, trace));
    }

    if (!trace) {
        return AllConfigs;
    }

    std::cout << "USM Pool Settings (Built-in or Adjusted by Environment "
                 "Variable)"
              << std::endl;

    // TODO: fixme, accessing config values directly is no longer allowed - API's changed
    // std::cout << std::setw(15) << "Parameter" << std::setw(12) << "Host"
    //           << std::setw(12) << "Device" << std::setw(12) << "Shared RW"
    //           << std::setw(12) << "Shared RO" << std::endl;
    // std::cout
    //     << std::setw(15) << "SlabMinSize" << std::setw(12)
    //     << umfDisjointPoolParamsGetSlabMinSize(AllConfigs.Configs[DisjointPoolMemType::Host])
    //     << std::setw(12)
    //     << umfDisjointPoolParamsGetSlabMinSize(AllConfigs.Configs[DisjointPoolMemType::Device])
    //     << std::setw(12)
    //     << umfDisjointPoolParamsGetSlabMinSize(AllConfigs.Configs[DisjointPoolMemType::Shared])
    //     << std::setw(12)
    //     << umfDisjointPoolParamsGetSlabMinSize(AllConfigs.Configs[DisjointPoolMemType::SharedReadOnly])
    //     << std::endl;
    // std::cout << std::setw(15) << "MaxPoolableSize" << std::setw(12)
    //           << umfDisjointPoolParamsGetMaxPoolableSize(AllConfigs.Configs[DisjointPoolMemType::Host])
    //           << std::setw(12)
    //           << umfDisjointPoolParamsGetMaxPoolableSize(AllConfigs.Configs[DisjointPoolMemType::Device])
    //           << std::setw(12)
    //           << umfDisjointPoolParamsGetMaxPoolableSize(AllConfigs.Configs[DisjointPoolMemType::Shared])
    //           << std::setw(12)
    //           << umfDisjointPoolParamsGetMaxPoolableSize(AllConfigs.Configs[DisjointPoolMemType::SharedReadOnly])
    //           << std::endl;
    // std::cout
    //     << std::setw(15) << "Capacity" << std::setw(12)
    //     << umfDisjointPoolParamsGetCapacity(AllConfigs.Configs[DisjointPoolMemType::Host])
    //     << std::setw(12)
    //     << umfDisjointPoolParamsGetCapacity(AllConfigs.Configs[DisjointPoolMemType::Device])
    //     << std::setw(12)
    //     << umfDisjointPoolParamsGetCapacity(AllConfigs.Configs[DisjointPoolMemType::Shared])
    //     << std::setw(12)
    //     << umfDisjointPoolParamsGetCapacity(AllConfigs.Configs[DisjointPoolMemType::SharedReadOnly])
    //     << std::endl;
    std::cout << std::setw(15) << "MaxPoolSize" << std::setw(12) << MaxSize
              << std::endl;
    std::cout << std::setw(15) << "EnableBuffers" << std::setw(12)
              << EnableBuffers << std::endl
              << std::endl;

    return AllConfigs;
}
} // namespace usm
