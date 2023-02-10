//===--- usm_pool_config.cpp -configuration for USM memory pool-------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "usm_pool_config.hpp"

#include <iomanip>
#include <iostream>
#include <string>

namespace usm_pool {
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

template <>
PoolConfigurations<typename DisjointPool::Config>
getPoolConfigurationsFor<DisjointPool>(const std::string &config, bool trace) {
    PoolConfigurations<typename DisjointPool::Config> Configs;

    Configs[PoolDescriptor(UR_USM_TYPE_HOST)].name = "Host";
    Configs[PoolDescriptor(UR_USM_TYPE_DEVICE)].name = "Device";
    Configs[PoolDescriptor(UR_USM_TYPE_SHARED)].name = "Shared";
    Configs[PoolDescriptor(UR_USM_TYPE_SHARED, true)].name = "SharedReadOnly";

    // Buckets for Host use a minimum of the cache line size of 64 bytes.
    // This prevents two separate allocations residing in the same cache line.
    // Buckets for Device and Shared allocations will use starting size of 512.
    // This is because memory compression on newer GPUs makes the
    // minimum granularity 512 bytes instead of 64.
    Configs[PoolDescriptor(UR_USM_TYPE_HOST)].MinBucketSize = 64;
    Configs[PoolDescriptor(UR_USM_TYPE_DEVICE)].MinBucketSize = 512;
    Configs[PoolDescriptor(UR_USM_TYPE_SHARED)].MinBucketSize = 512;
    Configs[PoolDescriptor(UR_USM_TYPE_SHARED, true)].MinBucketSize = 512;

    // Initialize default pool settings.
    Configs[PoolDescriptor(UR_USM_TYPE_HOST)].MaxPoolableSize = 2_MB;
    Configs[PoolDescriptor(UR_USM_TYPE_HOST)].Capacity = 4;
    Configs[PoolDescriptor(UR_USM_TYPE_HOST)].SlabMinSize = 64_KB;

    Configs[PoolDescriptor(UR_USM_TYPE_DEVICE)].MaxPoolableSize = 4_MB;
    Configs[PoolDescriptor(UR_USM_TYPE_DEVICE)].Capacity = 4;
    Configs[PoolDescriptor(UR_USM_TYPE_DEVICE)].SlabMinSize = 64_KB;

    // Disable pooling of shared USM allocations.
    Configs[PoolDescriptor(UR_USM_TYPE_SHARED)].MaxPoolableSize = 0;
    Configs[PoolDescriptor(UR_USM_TYPE_SHARED)].Capacity = 0;
    Configs[PoolDescriptor(UR_USM_TYPE_SHARED)].SlabMinSize = 2_MB;

    // Allow pooling of shared allocations that are only modified on host.
    Configs[PoolDescriptor(UR_USM_TYPE_SHARED, true)].MaxPoolableSize = 4_MB;
    Configs[PoolDescriptor(UR_USM_TYPE_SHARED, true)].Capacity = 4;
    Configs[PoolDescriptor(UR_USM_TYPE_SHARED, true)].SlabMinSize = 2_MB;

    // TODO: replace with UR ENV var parser and avoid creating a copy of
    // 'config'
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

    auto MemParser = [&Configs,
                      ParamParser](std::string &Params,
                                   PoolDescriptor poolDesc =
                                       PoolDescriptor(UR_USM_TYPE_UNKOWN)) {
        bool ParamWasSet;
        PoolDescriptor LM = poolDesc;
        if (poolDesc.type == UR_USM_TYPE_UNKOWN) {
            LM = PoolDescriptor(UR_USM_TYPE_HOST);
        }

        bool More =
            ParamParser(Params, Configs[LM].MaxPoolableSize, ParamWasSet);
        if (ParamWasSet && poolDesc.type == UR_USM_TYPE_UNKOWN) {
            for (auto &Config : Configs) {
                Config.second.MaxPoolableSize = Configs[LM].MaxPoolableSize;
            }
        }
        if (More) {
            More = ParamParser(Params, Configs[LM].Capacity, ParamWasSet);
            if (ParamWasSet && poolDesc.type == UR_USM_TYPE_UNKOWN) {
                for (auto &Config : Configs) {
                    Config.second.Capacity = Configs[LM].Capacity;
                }
            }
        }
        if (More) {
            ParamParser(Params, Configs[LM].SlabMinSize, ParamWasSet);
            if (ParamWasSet && poolDesc.type == UR_USM_TYPE_UNKOWN) {
                for (auto &Config : Configs) {
                    Config.second.SlabMinSize = Configs[LM].SlabMinSize;
                }
            }
        }
    };

    auto MemTypeParser = [&Configs, MemParser](std::string &Params) {
        int Pos = 0;
        PoolDescriptor M(UR_USM_TYPE_UNKOWN);
        if (Params.compare(0, 5, "host:") == 0) {
            Pos = 5;
            M = PoolDescriptor(UR_USM_TYPE_HOST);
        } else if (Params.compare(0, 7, "device:") == 0) {
            Pos = 7;
            M = PoolDescriptor(UR_USM_TYPE_DEVICE);
        } else if (Params.compare(0, 7, "shared:") == 0) {
            Pos = 7;
            M = PoolDescriptor(UR_USM_TYPE_SHARED);
        } else if (Params.compare(0, 17, "read_only_shared:") == 0) {
            Pos = 17;
            M = PoolDescriptor(UR_USM_TYPE_SHARED, true);
        }
        if (Pos > 0) {
            Params.erase(0, Pos);
        }
        MemParser(Params, M);
    };

    auto limits = std::make_shared<DisjointPoolConfig::SharedLimits>();

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
                    GetValue(Params, Pos, limits->MaxSize);
                }
                Params.erase(0, Pos + 1);
                do {
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
                } while (true);
            } else {
                // set MaxPoolSize for all configs
                GetValue(Params, Params.size(), limits->MaxSize);
            }
        } else {
            GetValue(Params, Params.size(), EnableBuffers);
        }
    }

    for (auto &Config : Configs) {
        Config.second.limits = limits;
        Config.second.PoolTrace = trace;
    }

    if (!EnableBuffers) {
        return {};
    }

    if (!trace) {
        return Configs;
    }

    std::cout << "USM Pool Settings (Built-in or Adjusted by Environment "
                 "Variable)"
              << std::endl;

    std::cout << std::setw(15) << "Parameter" << std::setw(12) << "Host"
              << std::setw(12) << "Device" << std::setw(12) << "Shared RW"
              << std::setw(12) << "Shared RO" << std::endl;
    std::cout << std::setw(15) << "SlabMinSize" << std::setw(12)
              << Configs[PoolDescriptor(UR_USM_TYPE_HOST)].SlabMinSize
              << std::setw(12)
              << Configs[PoolDescriptor(UR_USM_TYPE_DEVICE)].SlabMinSize
              << std::setw(12)
              << Configs[PoolDescriptor(UR_USM_TYPE_SHARED)].SlabMinSize
              << std::setw(12)
              << Configs[PoolDescriptor(UR_USM_TYPE_SHARED, true)].SlabMinSize
              << std::endl;
    std::cout
        << std::setw(15) << "MaxPoolableSize" << std::setw(12)
        << Configs[PoolDescriptor(UR_USM_TYPE_HOST)].MaxPoolableSize
        << std::setw(12)
        << Configs[PoolDescriptor(UR_USM_TYPE_DEVICE)].MaxPoolableSize
        << std::setw(12)
        << Configs[PoolDescriptor(UR_USM_TYPE_SHARED)].MaxPoolableSize
        << std::setw(12)
        << Configs[PoolDescriptor(UR_USM_TYPE_SHARED, true)].MaxPoolableSize
        << std::endl;
    std::cout << std::setw(15) << "Capacity" << std::setw(12)
              << Configs[PoolDescriptor(UR_USM_TYPE_HOST)].Capacity
              << std::setw(12)
              << Configs[PoolDescriptor(UR_USM_TYPE_DEVICE)].Capacity
              << std::setw(12)
              << Configs[PoolDescriptor(UR_USM_TYPE_SHARED)].Capacity
              << std::setw(12)
              << Configs[PoolDescriptor(UR_USM_TYPE_SHARED, true)].Capacity
              << std::endl;
    std::cout << std::setw(15) << "MaxPoolSize" << std::setw(12)
              << limits->MaxSize << std::endl;
    std::cout << std::setw(15) << "EnableBuffers" << std::setw(12)
              << EnableBuffers << std::endl
              << std::endl;

    return Configs;
}
} // namespace usm_pool
