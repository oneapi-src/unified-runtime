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
import os
import shutil
import subprocess
from subprocess import PIPE
import sys

verbose = False


def error(*args, **kwargs):
    print("\033[31merror:\033[0m", *args, file=sys.stderr, **kwargs)
    exit(1)


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
    # TODO: cli.add_argument("--ur-pr", default=None, help="unified-runtime pull request id to move")
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

    # validate git is usable
    git = shutil.which("git")
    if not git:
        error("git command not found")

    # validate gh is usable
    gh = shutil.which("gh")
    if not gh:
        error(
            "gh command not found, see https://cli.github.com/ for install instructions",
        )
    gh_auth_status = run([gh, "auth", "status"], stdout=PIPE)
    if "Logged in" not in gh_auth_status.stdout.decode():
        error("gh is not authenticated, please run 'gh auth login'")

    # get UR branch if not already set
    if not args.ur_branch:
        ur_branch = run(
            [git, "-C", args.ur_dir, "rev-parse", "--abbrev-ref", "HEAD"],
            stdout=PIPE,
        )
        args.ur_branch = ur_branch.stdout.decode().strip()
    if not args.intel_llvm_branch:
        args.intel_llvm_branch = args.ur_branch

    # validate args.ur_remote
    ur_remote_url = run(
        [
            git,
            "-C",
            args.ur_dir,
            "config",
            "--get",
            f"remote.{args.ur_remote}.url",
        ],
        stdout=PIPE,
    )
    ur_remote_url = ur_remote_url.stdout.decode().strip()
    if "oneapi-src/unified-runtime" in ur_remote_url:
        error(
            f"'{args.ur_remote}' remote URL does not contain",
            "'oneapi-src/unified-runtime', specify the correct remote using",
            "the --ur-remote command-line option",
        )

    # TODO: check if ur local branch and remote tracking branch are in sync

    # check how far behind origin/main the feature branch is
    rev_list = run(
        [
            git,
            "-C",
            args.ur_dir,
            "rev-list",
            "--left-right",
            "--count",
            f"{args.ur_remote}/main...{args.ur_branch}",
        ],
        stdout=PIPE,
    )
    rev_list = rev_list.stdout.decode().strip().split()
    assert len(rev_list) == 2
    behind, ahead = rev_list
    if int(behind) > 0:
        error(
            f"rebase required, feature branch '{args.ur_branch}' is {behind}",
            f"commits behind '{args.ur_remote}/main'",
        )

    return

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

    # TODO: sync with upstream
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
        os.remove(patch)

    # TODO: create pull request with feature branch


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        exit(130)
