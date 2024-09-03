# Copyright (C) 2024 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import os
import csv
import io
from utils.utils import run, git_clone, create_build_path
from .base import Benchmark
from .result import Result
from .options import options

class SyclBench:
    def __init__(self, directory):
        self.directory = directory
        # self.built = False
        # self.adapter_short_name = {'level_zero' : 'L0', "level_zero_v2" : 'L0_V2'}
        self.setup()
        return

    def setup(self):
        # if self.built:
        #     return

        repo_path = git_clone(self.directory, "sycl-bench-repo", "https://github.com/mateuszpn/sycl-bench.git", "1e6ab2cfd004a72c5336c26945965017e06eab71")
        build_path = create_build_path(self.directory, 'sycl-bench-build')

        configure_command = [
            "cmake",
            f"-B {build_path}",
            f"-S {repo_path}",
            f"-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_CXX_COMPILER={options.sycl}/bin/clang++",
            f"-DCMAKE_C_COMPILER={options.sycl}/bin/clang",
            f"-DSYCL_IMPL=dpcpp"
            # f"-DBUILD_SYCL=ON",
            # f"-DSYCL_COMPILER_ROOT={options.sycl}",
            # f"-DALLOW_WARNINGS=ON",
            # f"-DBUILD_UR=ON",
            # f"-DUR_BUILD_TESTS=OFF",
            # f"-DUR_BUILD_ADAPTER_L0=ON",
            # f"-DUR_BUILD_TESTS=OFF",
            # f"-DUMF_DISABLE_HWLOC=ON",
            # f"-DBENCHMARK_UR_SOURCE_DIR={options.ur_dir}",
            # f"-DUR_BUILD_ADAPTER_{self.adapter_short_name[options.ur_adapter_name]}=ON"
        ]
        run(configure_command, add_sycl=True)

        run(f"cmake --build {build_path} -j", add_sycl=True)

        # self.built = True
        # self.bins = os.path.join(build_path, 'bin')
        # self.libs = os.path.join(build_path, 'lib')
        self.bins = build_path
        # self.libs = os.path.join(build_path, 'lib')

class SyclBenchmark(Benchmark):
    def __init__(self, bench, name, test):
        self.bench = bench
        self.bench_name = name
        self.test = test
        super().__init__(bench.directory)

    def bin_args(self) -> list[str]:
        return []

    def extra_env_vars(self) -> dict:
        return {}

    def unit(self):
        return "μs"

    def setup(self):
        self.bench.setup()
        self.benchmark_bin = os.path.join(self.bench.bins, self.bench_name)

    def run(self, env_vars) -> Result:
        outputfile = f"{self.bench.directory}/{self.test}.csv"
        command = [
            f"{self.benchmark_bin}",
            f"--warmup-run",
            f"--num-runs=6",
            f"--output={outputfile}"
        ]

        command += self.bin_args()
        env_vars.update(self.extra_env_vars())

        print(f"Running {self.test} -> {outputfile} with env vars {env_vars}")

        result = self.run_bench(command, env_vars)
        (label, mean) = self.parse_output(outputfile)
        return Result(label=label, value=mean, command=command, env=env_vars, stdout=result)

    def parse_output(self, output):
        # csv_file = io.StringIO(output)
        # reader = csv.reader(csv_file)
        # next(reader, None)
        # data_row = next(reader, None)
        # if data_row is None:
        #     raise ValueError("Benchmark output does not contain data.")
        # try:
        #     label = data_row[0]
        #     mean = float(data_row[10])
        #     return (label, mean)
        # except (ValueError, IndexError) as e:
        #     raise ValueError(f"Error parsing output: {e}")
        with open(output, 'r') as f:
            reader = csv.reader(f)
            res_list = []
            for row in reader:
                if row[0] == self.test:
                    # print(row)
                    res_list.append(float(row[12]))
            
            sorted_res_list = sorted(res_list)
            n = len(sorted_res_list)
            if n % 2 == 0:
                median1 = sorted_res_list[n//2]
                median2 = sorted_res_list[n//2 - 1]
                median = (median1 + median2) / 2
            else:    
                median = sorted_res_list[n//2]
            
            print(f"Median: {median}")
            return (self.test, median)


    def teardown(self):
        return

    def name(self):
        return self.test

class Arith(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "arith", "MicroBench_Arith_int32_512")

    def bin_args(self) -> list[str]:
        return [
            f"--size=16384",
        ]

class TwoDConvolution(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "2DConvolution", "Polybench_2DConvolution")

class Two_mm(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "2mm", "Polybench_2mm")

    def bin_args(self) -> list[str]:
        return [
            f"--size=512",
        ]

class Three_mm(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "3mm", "Polybench_3mm")

    def bin_args(self) -> list[str]:
        return [
            f"--size=512",
        ]

class Atax(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "atax", "Polybench_Atax")

    def bin_args(self) -> list[str]:
        return [
            f"--size=24576",
        ]

class Atomic_reduction(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "atomic_reduction", "ReductionAtomic_fp64")

class Bicg(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "bicg", "Polybench_Bicg")

    def bin_args(self) -> list[str]:
        return [
            f"--size=16384",
        ]
    
# class Blocked_transform(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "blocked_transform", "BlockedTransform")

#     def bin_args(self) -> list[str]:
#         return [
#             f"--size=512",
#         ]

class Correlation(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "correlation", "Polybench_Correlation")

    def bin_args(self) -> list[str]:
        return [
            f"--size=512",
        ]

class Covariance(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "covariance", "Polybench_Covariance")

    def bin_args(self) -> list[str]:
        return [
            f"--size=768",
        ]

# class DagTaskI(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "dag_task_throughput_independent", "Runtime_IndependentDAGTaskThroughput_")

#     def bin_args(self) -> list[str]:
#         return [
#             f"--size=512",
#         ]
    
# class DagTaskS(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "dag_task_throughput_sequential", "Runtime_DAGTaskThroughput_")

#     def bin_args(self) -> list[str]:
#         return [
#             f"--size=512",
#         ]

class Gemm(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "gemm", "Polybench_Gemm")

    def bin_args(self) -> list[str]:
        return [
            f"--size=1024",
        ]
    
class Gesumv(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "gesummv", "Polybench_Gesummv")

    def bin_args(self) -> list[str]:
        return [
            f"--size=12294",
        ]

class Gramschmidt(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "gramschmidt", "Polybench_Gramschmidt")

    def bin_args(self) -> list[str]:
        return [
            f"--size=768",
        ]

# class HostDevBandwidth (SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "host_device_bandwidth", "MicroBench_HostDeviceBandwidth_")

class KMeans(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "kmeans", "Kmeans_fp32")

    def bin_args(self) -> list[str]:
        return [
            f"--size=67108864",
        ]

class LinRegCoeff(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "lin_reg_coeff", "LinearRegressionCoeff_fp32")

class LinRegError(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "lin_reg_error", "LinearRegression_fp32")

class LocalMem(SyclBenchmark):
    def __init__(self, bench, type):
        if type != "int32" and type != "fp32":
            raise ValueError("Invalid type for LocalMem benchmark")
        super().__init__(bench, "local_mem", f"MicroBench_LocalMem_{type}_4096")

    def bin_args(self) -> list[str]:
        return [
            f"--size=512",
        ]
class MatmulChain(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "matmulchain", "MatmulChain")

    def bin_args(self) -> list[str]:
        return [
            f"--size=1536",
        ]

# ** bad input file path **
# 
# class Median(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "median", "MedianFilter")
# 
#     def bin_args(self) -> list[str]:
#         return [
#             f"--size=512",
#         ]

class MolDyn(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "mol_dyn", "MolecularDynamics")


class Mvt(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "mvt", "Polybench_Mvt")

    def bin_args(self) -> list[str]:
        return [
            f"--size=16384",
        ]
    
# ** verification fail **
# 
# class NBody(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "nbody", "NBody_")

#     def bin_args(self) -> list[str]:
#         return [
#             f"--size=32768",
#         ]

# class Pattern_L2(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "pattern_L2", "MicroBench_L2_")

# class Reduction(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "reduction", "Pattern_Reduction_")

# class ScalarProd(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "scalar_prod", "ScalarProduct_")

# class SegmentReduction(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "segment_reduction", "Pattern_SegmentedReduction_")

class Sf(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "sf", "MicroBench_sf_fp32_16")

    def bin_args(self) -> list[str]:
        return [
            f"--size=--size=100000000",
        ]
    
# bad input file path
# 
# class SobelX(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "sobel", "SobelFilter")

class Syr2k(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "syr2k", "Polybench_Syr2k")

    def bin_args(self) -> list[str]:
        return [
            f"--size=1536",
        ]
    
class Syrk(SyclBenchmark):
    def __init__(self, bench):
        super().__init__(bench, "syrk", "Polybench_Syrk")

    def bin_args(self) -> list[str]:
        return [
            f"--size=1536",
        ]
    
# class UsmAccLatency(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "usm_accessors_latency", "USM_Latency_")

# class UsmAllocLatency(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "usm_alloc_latency", "USM_Allocation_latency_fp32_")

# class UsmInstrMix(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "usm_instr_mix", " USM_Instr_Mix_")

# class UsmPinnedOverhead(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "usm_pinned_overhead", "USM_Pinned_Overhead_")

# class VecAdd(SyclBenchmark):
#     def __init__(self, bench):
#         super().__init__(bench, "vec_add", "VectorAddition_")

