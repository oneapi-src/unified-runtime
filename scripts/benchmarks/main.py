#!/usr/bin/env python3

# Copyright (C) 2024 Intel Corporation
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

from utils.utils import prepare_workdir, load_benchmark_results, save_benchmark_results;
from benches.api_overhead import APIOverheadSYCL
from benches.hashtable import Hashtable
from benches.bitcracker import Bitcracker
from benches.cudaSift import CudaSift
from benches.easywave import Easywave
from benches.quicksilver import QuickSilver
from benches.SobelFilter import SobelFilter
from benches.velocity import VelocityBench
from benches.options import options
from output import generate_markdown
import argparse
import re

# Update this if you are changing the layout of the results files
INTERNAL_WORKDIR_VERSION = '1.3'

def main(directory, additional_env_vars, save_name, compare_names, filter):
    variants = [
        ({'UR_L0_USE_IMMEDIATE_COMMANDLISTS': '0'}, "Imm-CmdLists-OFF"),
        ({'UR_L0_USE_IMMEDIATE_COMMANDLISTS': '1'}, ""),
    ]

    prepare_workdir(directory, INTERNAL_WORKDIR_VERSION)

    vb = VelocityBench(directory)

    benchmarks = [
        APIOverheadSYCL(0, directory),
        APIOverheadSYCL(1, directory),
        Hashtable(vb),
        Bitcracker(vb),
        #CudaSift(vb), TODO: the benchmark is passing, but is outputting "Failed to allocate device data"
        Easywave(vb),
        QuickSilver(vb),
        SobelFilter(vb)
    ]

    if filter:
        benchmarks = [benchmark for benchmark in benchmarks if filter.search(benchmark.name())]

    for benchmark in benchmarks:
        print(f"setting up {benchmark.name()}... ", end='', flush=True)
        benchmark.setup()
        print("complete.")

    results = []
    for benchmark in benchmarks:
        for env_vars, extra_label in variants:
            merged_env_vars = {**env_vars, **additional_env_vars}
            iteration_results = []
            for iter in range(options.iterations):
                print(f"running {benchmark.name()} {extra_label}, iteration {iter}... ", end='', flush=True)
                bench_results = benchmark.run(merged_env_vars)
                print(f"complete ({bench_results.value} {bench_results.unit}).")
                iteration_results.append(bench_results)

            iteration_results.sort(key=lambda res: res.value)
            median_index = len(iteration_results) // 2
            median_result = iteration_results[median_index]

            median_result.unit = benchmark.unit()
            median_result.name = benchmark.name()
            median_result.label += f" {extra_label}"

            results.append(median_result)

    for benchmark in benchmarks:
        print(f"tearing down {benchmark.name()}... ", end='', flush=True)
        benchmark.teardown()
        print("complete.")

    chart_data = {"This PR" : results}

    for name in compare_names:
        compare_result = load_benchmark_results(directory, name)
        if compare_result:
            chart_data[name] = compare_result

    if save_name:
        save_benchmark_results(directory, save_name, results)

    markdown_content = generate_markdown(chart_data)

    with open('benchmark_results.md', 'w') as file:
        file.write(markdown_content)

    print("Markdown with benchmark results has been written to benchmark_results.md")

def validate_and_parse_env_args(env_args):
    env_vars = {}
    for arg in env_args:
        if '=' not in arg:
            raise ValueError(f"Environment variable argument '{arg}' is not in the form Variable=Value.")
        key, value = arg.split('=', 1)
        env_vars[key] = value
    return env_vars

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Unified Runtime Benchmark Runner')
    parser.add_argument('benchmark_directory', type=str, help='Working directory to setup benchmarks.')
    parser.add_argument('sycl', type=str, help='Root directory of the SYCL compiler.')
    parser.add_argument("--no-rebuild", help='Rebuild the benchmarks from scratch.', action="store_true")
    parser.add_argument("--env", type=str, help='Use env variable for a benchmark run.', action="append", default=[])
    parser.add_argument("--save", type=str, help='Save the results for comparison under a specified name.')
    parser.add_argument("--compare", type=str, help='Compare results against previously saved data.', action="append", default=["baseline"])
    parser.add_argument("--iterations", type=int, help='Number of times to run each benchmark to select a median value.', default=3)
    parser.add_argument("--timeout", type=int, help='Timeout for individual benchmarks in seconds.', default=600)
    parser.add_argument("--filter", type=str, help='Regex pattern to filter benchmarks by name.', default=None)

    args = parser.parse_args()
    additional_env_vars = validate_and_parse_env_args(args.env)

    options.rebuild = not args.no_rebuild
    options.sycl = args.sycl
    options.iterations = args.iterations
    options.timeout = args.timeout

    benchmark_filter = re.compile(args.filter) if args.filter else None

    main(args.benchmark_directory, additional_env_vars, args.save, args.compare, benchmark_filter)
