#!/usr/bin/env python3

"""
Mirror commits from the unified-runtime top level directory in the
https://github.com/intel/llvm repository to the root of the
https://github.com/oneapi-src/unified-runtime repository.
"""

from argparse import ArgumentParser, RawTextHelpFormatter
from subprocess import run, PIPE
import os


REPO_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
INTEL_LLVM_MIRROR_BASE_COMMIT_FILE = os.path.join(
    REPO_DIR, ".github", "intel-llvm-mirror-base-commit"
)


def main():
    cli = ArgumentParser(description=__doc__, formatter_class=RawTextHelpFormatter)

    # Positional arguments
    cli.add_argument(
        "unified_runtime_dir",
        help="path to a oneapi-src/unified-runtime clone",
    )
    cli.add_argument(
        "intel_llvm_dir",
        help="path to an intel/llvm clone",
    )

    # Optional arguments
    cli.add_argument(
        "--unified-runtime-remote",
        default="origin",
        help="upstream remote to use in the oneapi-src/unified-runtime clone",
    )
    cli.add_argument(
        "--intel-llvm-remote",
        default="origin",
        help="upstream remote to use in the intel/llvm clone",
    )
    cli.add_argument(
        "--intel-llvm-mirror-base-commit-file",
        default=INTEL_LLVM_MIRROR_BASE_COMMIT_FILE,
        help="path to a file containing the intel/llvm mirror base commit",
    )

    args = cli.parse_args()

    with open(args.intel_llvm_mirror_base_commit_file, "r") as base_commit_file:
        intel_llvm_mirror_base_commit = base_commit_file.read().strip()

    print(f"Using intel/llvm mirror base commit: {intel_llvm_mirror_base_commit}")

    run(["git", "-C", args.intel_llvm_dir, "fetch", args.intel_llvm_remote])

    result = run(
        [
            "git",
            "-C",
            args.intel_llvm_dir,
            "format-patch",
            "--relative=unified-runtime",
            f"--output-directory={args.unified_runtime_dir}",
            f"{intel_llvm_mirror_base_commit}..{args.intel_llvm_remote}/sycl",
            "unified-runtime",
        ],
        stdout=PIPE,
    )

    patches = result.stdout.decode().splitlines()
    if len(patches) == 0:
        print("There are no patches to apply, exiting.")
        exit(0)

    print(f"Applying the following patches:\n{'\n'.join(patches)}")

    run(["git", "-C", args.unified_runtime_dir, "am"] + patches)

    result = run(
        [
            "git",
            "-C",
            args.intel_llvm_dir,
            "log",
            "-1",
            "--format=%H",
            "unified-runtime",
        ],
        stdout=PIPE,
    )

    new_intel_llvm_mirror_base_commit = result.stdout.decode().strip()
    print(
        f"Using new intel/llvm mirror base commit: {new_intel_llvm_mirror_base_commit}"
    )
    with open(args.intel_llvm_mirror_base_commit_file, "w") as base_commit_file:
        base_commit_file.write(f"{new_intel_llvm_mirror_base_commit}\n")

    print("Cleaning up patch files.")
    for patch in patches:
        os.remove(patch)

    run(
        [
            "git",
            "-C",
            args.unified_runtime_dir,
            "commit",
            args.intel_llvm_mirror_base_commit_file,
            "-m",
            f"Update intel/llvm mirror base commit to {new_intel_llvm_mirror_base_commit[:8]}",
        ]
    )


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        exit(130)
