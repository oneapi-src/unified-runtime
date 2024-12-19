#!/usr/bin/env python3

# Copyright (C) 2024 Intel Corporation
#
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""
Use this script to move an existing pull request in the
https://github.com/oneapi-src/unified-runtime project to a new pull request
targeting the top-level unified-runtime directory of the
https://github.com/intel/llvm project.

Git operations are performed on both repositories.

GitHub interactions are performed using the gh command-line tool which must
reside on the PATH and be authenticated.
"""

import argparse
from os import remove
import shutil
import subprocess
from subprocess import PIPE

verbose = False


def run(command, **kwargs):
    if verbose:
        print(command)
    result = subprocess.run(command, **kwargs)
    result.check_returncode()
    return result


def main():
    global verbose

    cli = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter
    )
    cli.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="enable printing verbose output",
    )
    cli.add_argument(
        "--ur-dir",
        default=".",
        help="local directory containing a clone of oneapi-src/unified-runtime",
    )
    cli.add_argument(
        "--ur-remote",
        default="origin",
        help="unified-runtime remote to fetch from, defaults to 'origin'",
    )
    cli.add_argument(
        "--ur-branch",
        default=None,
        help="unified-runtime branch to move, defaults to current branch",
    )
    cli.add_argument(
        "intel_llvm_dir",
        help="local directory containing a clone of intel/llvm",
    )
    cli.add_argument(
        "--intel-llvm-remote",
        default="origin",
        help="intel/llvm remote to fetch from, defaults to 'origin'",
    )
    cli.add_argument(
        "--intel-llvm-base-branch",
        default=None,
        help="intel/llvm branch to base new branch upon, defaults to 'sycl'",
    )
    cli.add_argument(
        "--intel-llvm-branch",
        default=None,
        help="intel/llvm remote branch to create, defaults to --ur-branch",
    )

    args = cli.parse_args()
    verbose = args.verbose

    git = shutil.which("git")
    if not git:
        raise FileNotFoundError("git command not found")
    if not args.ur_branch:
        ur_branch = run(
            [git, "-C", args.ur_dir, "rev-parse", "--abbrev-ref", "HEAD"],
            stdout=PIPE,
        )
        args.ur_branch = ur_branch.stdout.decode().strip()
    if not args.intel_llvm_branch:
        args.intel_llvm_branch = args.ur_branch

    gh = shutil.which("gh")
    if not gh:
        raise FileNotFoundError("gh command not found")
    gh_auth_status = run([gh, "auth", "status"], stdout=PIPE)
    if "Logged in" not in gh_auth_status.stdout.decode():
        raise ValueError("gh is not authenticated, run gh auth login")

    # TODO: check if ur local branch and remote tracking branch are in sync
    # TODO: check how far behind origin/main the feature branch is, error out if a rebase is required

    run([git, "-C", args.ur_dir, "fetch", args.ur_remote])

    # Use git format-patch to create a list of patches from the feature branch
    patches = run(
        [
            git,
            "-C",
            args.ur_dir,
            "format-patch",
            "--output-directory",
            args.intel_llvm_dir,
            f"{args.ur_remote}/main..{args.ur_branch}",
        ],
        stdout=PIPE,
    )
    patches = patches.stdout.decode().strip()
    print(patches)
    patches = patches.split("\n")

    # TODO: Sycl with upstream
    # run([git, "-C", args.intel_llvm_dir, "fetch", args.intel_llvm_remote])

    # TODO: check if feature branch already exists

    # create feature branch on intel/llvm
    run(
        [
            git,
            "-C",
            args.intel_llvm_dir,
            "branch",
            args.intel_llvm_branch,
            f"{args.intel_llvm_remote}/{args.intel_llvm_base_branch}",
        ]
    )
    run([git, "-C", args.intel_llvm_dir, "checkout", args.intel_llvm_branch])

    # Use git am to apply list of patches to intel/llvm
    run(
        [git, "-C", args.intel_llvm_dir, "am", "--directory=unified-runtime"]
        + patches
    )

    # clean up patch files
    for patch in patches:
        remove(patch)

    # TODO: create pull request with feature branch

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        exit(130)
