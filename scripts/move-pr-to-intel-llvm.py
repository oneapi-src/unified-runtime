#!/usr/bin/env python3

# Copyright (C) 2024 Intel Corporation
#
# Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM Exceptions.
# See LICENSE.TXT
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""
Use this script to move an existing pull request in the
https://github.com/oneapi-src/unified-runtime project to a pull request
targeting the top-level unified-runtime directory of the
https://github.com/intel/llvm project. During use confirmation will be
requested before each action unless the -y/--yes option is provided.

Git operations will be performed on both repositories. Checks are performed to
ensure no data loss however please take care as they may not cover every corner
case.

GitHub interactions are performed using the gh command-line tool which must
reside on the PATH and be authenticated, see https://cli.github.com for
install instructions.

It is recommended to run this script in the root of your local unified-runtime
repository with the feature branch relating to your pull request checked out.
This behaviour can be changed using the flags specified below.

Examples:

    $ export $UR_DIR=/path/to/oneapi-src/unified-runtime
    $ export $INTEL_LLVM_DIR=/path/to/intel/llvm
    $ export $INTEL_LLVM_PUSH_REMOTE=origin

When the UR change does not have an existing intel/llvm PR:

    $ python $UR_DIR/scripts/move-pr-to-intel-llvm.py \\
        --ur-branch <ur-feature-branch> \\
        $INTEL_LLVM_DIR $INTEL_LLVM_PUSH_REMOTE

When the UR change has existing an intel/llvm PR:

    $ python $UR_DIR/scripts/move-pr-to-intel-llvm.py \\
        --ur-branch <ur-feature-branch> \\
        --intel-llvm-feature-branch <intel-llvm-feature-branch> \\
        $INTEL_LLVM_DIR $INTEL_LLVM_PUSH_REMOTE
"""

import argparse
import os
import shutil
import subprocess
from subprocess import PIPE, CompletedProcess
import sys
from typing import List, NoReturn, Tuple
import json
import platform

verbose = False
yes = False
allow_dirty = False

ur_default_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
unified_runtime_slug = "oneapi-src/unified-runtime"
intel_llvm_slug = "intel/llvm"


def main():
    global verbose, yes, allow_dirty

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
        "-y",
        "--yes",
        action="store_true",
        help="assume yes when prompted for input",
    )
    cli.add_argument(
        "--ur-dir",
        default=ur_default_dir,
        help="local directory containing a clone of oneapi-src/unified-runtime",
    )
    cli.add_argument(
        "--ur-remote",
        default="origin",
        help="unified-runtime remote to fetch main from, defaults to 'origin'",
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
        "intel_llvm_push_remote",
        help="intel/llvm remote to push the feature branch to",
    )
    cli.add_argument(
        "--intel-llvm-remote",
        default="origin",
        help="intel/llvm remote to fetch from, defaults to 'origin'",
    )
    cli.add_argument(
        "--intel-llvm-base-branch",
        default="sycl",
        help="intel/llvm branch to base new branch upon, defaults to 'sycl'",
    )
    cli.add_argument(
        "--intel-llvm-feature-branch",
        default=None,
        help="intel/llvm remote branch to create, defaults to --ur-branch",
    )
    cli.add_argument(
        "--allow-dirty",
        default=False,
        action="store_true",
        help="Skip checks for a clean workspace. Please check any generated patch files before applying",
    )
    args = cli.parse_args()

    if platform.system() != "Linux":
        if not confirm("This script has only been tested on Linux, continue?"):
            exit(1)

    verbose = args.verbose
    yes = args.yes
    allow_dirty = args.allow_dirty

    git = get_git_executable()
    gh = get_gh_executable()

    check_remote_url(
        git, args.ur_dir, args.ur_remote, unified_runtime_slug, "--ur-remote"
    )
    check_remote_url(
        git,
        args.intel_llvm_dir,
        args.intel_llvm_remote,
        intel_llvm_slug,
        "--intel-llvm-remote",
    )

    check_worktree_is_clean(git, args.ur_dir)
    check_worktree_is_clean(git, args.intel_llvm_dir)

    if args.intel_llvm_remote == args.intel_llvm_push_remote:
        print("The intel_llvm_push_remote and --intel-llvm-remote values match.")
        if not confirm(f"Are you sure you want to push directly to {intel_llvm_slug}?"):
            exit(1)

    if not confirm("Fetch latest changes from remotes?"):
        exit(1)
    fetch_remote(git, args.ur_dir, args.ur_remote)
    fetch_remote(git, args.intel_llvm_dir, args.intel_llvm_remote)

    if not args.ur_branch:
        args.ur_branch = get_current_branch(git, args.ur_dir)
    if not args.intel_llvm_feature_branch:
        args.intel_llvm_feature_branch = args.ur_branch
    ur_pr = get_ur_pr_for_branch(gh, args.ur_branch)
    check_ur_branch_is_updated(git, args.ur_dir, args.ur_remote, args.ur_branch)

    locally, remotely = check_intel_llvm_feature_branch(
        git,
        args.intel_llvm_dir,
        args.intel_llvm_feature_branch,
        args.intel_llvm_push_remote,
    )
    if (
        (
            locally
            and remotely
            and not confirm(
                f"LLVM feature branch '{args.intel_llvm_feature_branch}' exists "
                "locally and remotely, proceed with applying patches on top "
                "of this branch?"
            )
        )
        or (
            not locally
            and remotely
            and not confirm(
                f"LLVM feature branch '{args.intel_llvm_feature_branch}' exists "
                "remotely but not locally, proceed with applying patches on "
                "top of this branch?"
            )
        )
        or (
            locally
            and not remotely
            and not confirm(
                f"LLVM feature branch '{args.intel_llvm_feature_branch}' exists "
                "locally but not remotely, proceed with applying patches on "
                "top of this branch?"
            )
        )
    ):
        print(
            "Please use --intel-llvm-feature-branch to specify an "
            "alternative branch name."
        )
        exit(1)
    if not locally and not remotely:
        if not confirm(
            f"Create feature branch '{args.intel_llvm_feature_branch}' "
            f"in {os.path.abspath(args.intel_llvm_dir)}?"
        ):
            exit(1)
        create_intel_llvm_feature_branch(
            git,
            args.intel_llvm_dir,
            args.intel_llvm_feature_branch,
            args.intel_llvm_remote,
            args.intel_llvm_base_branch,
        )
    elif remotely and not locally or locally and not remotely:
        checkout_branch(git, args.intel_llvm_dir, args.intel_llvm_feature_branch)

    patches = get_patch_files(
        git, args.ur_dir, args.intel_llvm_dir, args.ur_remote, args.ur_branch
    )

    if confirm("Apply patches to feature branch?"):
        apply_patches(git, args.intel_llvm_dir, patches)

    if confirm("Clean up patch files?"):
        for patch in patches:
            os.remove(patch)

    if not confirm(
        f"Push '{args.intel_llvm_feature_branch}' to '{args.intel_llvm_push_remote}'?"
    ):
        print("Please push the changes and create your pull request manually.")
        exit(0)
    push_branch(
        git,
        args.intel_llvm_dir,
        args.intel_llvm_push_remote,
        args.intel_llvm_feature_branch,
    )

    if not confirm(f"Create '{intel_llvm_slug}' pull request?"):
        print("Please create an intel/llvm pull request manually.")
        exit()
    title = ur_pr["title"]
    if not title.startswith("[UR]"):
        title = f"[UR] {title}"
    body = [f"Migrated from {ur_pr['url']}", ""] + ur_pr["body"].split("\\r\\n")
    create_pull_request(
        gh,
        args.intel_llvm_dir,
        intel_llvm_slug,
        args.intel_llvm_base_branch,
        title,
        "\n".join(body),
    )

    if not confirm(f"Close {ur_pr['url']}?"):
        print("Please create your pull request manually.")
        exit()
    close_pull_request(gh, args.ur_dir, unified_runtime_slug, str(ur_pr["number"]))


def get_git_executable() -> str:
    git = shutil.which("git")
    if not git:
        error("git command not found")
    return git


def get_gh_executable() -> str:
    gh = shutil.which("gh")
    if not gh:
        error(
            "gh command not found, see https://cli.github.com for install instructions",
        )
    # check gh is usable
    gh_auth_status = run([gh, "auth", "status"], stdout=PIPE)
    if "Logged in" not in gh_auth_status.stdout.decode():
        error("gh is not authenticated, please run 'gh auth login'")
    return gh


def fetch_remote(git: str, dir: str, remote: str) -> None:
    print(f"Fetching remote '{remote}' in '{os.path.abspath(dir)}' repository")
    run([git, "-C", dir, "fetch", remote])


def check_remote_url(git: str, dir, remote: str, slug: str, option: str) -> None:
    result = run(
        [
            git,
            "-C",
            dir,
            "config",
            "--get",
            f"remote.{remote}.url",
        ],
        stdout=PIPE,
    )
    url = result.stdout.decode().strip()
    if slug not in url:
        error(
            f"'{remote}' remote URL in the '{os.path.abspath(dir)}' "
            " repository does not contain",
            f"'{slug}', specify the correct remote using",
            f"the {option} command-line option",
        )


def get_current_branch(git, dir) -> str:
    ur_branch = run(
        [git, "-C", dir, "rev-parse", "--abbrev-ref", "HEAD"],
        stdout=PIPE,
    )
    return ur_branch.stdout.decode().strip()


def check_worktree_is_clean(git, dir):
    if allow_dirty:
        return
    result = run([git, "-C", dir, "status", "--porcelain"], stdout=PIPE)
    stdout = result.stdout.decode()
    if verbose:
        print(result)
    if len(stdout.splitlines()) > 0:
        error(f"worktree is not clean for repo: {os.path.abspath(dir)}")


def get_ur_pr_for_branch(gh, branch) -> dict:
    result = run(
        [
            gh,
            "pr",
            "list",
            "--json",
            "number,state,title,body,url",
            "--search",
            f"is:open head:{branch}",
        ],
        stdout=PIPE,
    )
    prs = json.loads(result.stdout.decode())
    if verbose:
        print(prs)
    if len(prs) == 0:
        error(f"no pull request found for branch '{branch}'")
    # Assum there is only one open PR with this branch name
    pr = prs[0]
    if pr["state"] != "OPEN":
        if not confirm(
            f"The Pull request associated with branch '{branch}' has "
            f"state '{pr['state']}', are you sure you want to continue?"
        ):
            exit(1)
    return pr


def check_ur_branch_is_updated(git: str, dir: str, remote: str, branch: str) -> None:
    rev_list = run(
        [
            git,
            "-C",
            dir,
            "rev-list",
            "--left-right",
            "--count",
            f"{remote}/main...{branch}",
        ],
        stdout=PIPE,
    )
    rev_list = rev_list.stdout.decode().strip().split()
    assert len(rev_list) == 2
    behind, _ = rev_list
    if int(behind) > 0:
        error(
            f"rebase required, feature branch '{branch}' is {behind}",
            f"commits behind '{remote}/main'",
        )


def check_intel_llvm_feature_branch(
    git: str, dir: str, branch: str, remote: str
) -> Tuple[bool, bool]:
    # Check if feature branch exists locally
    locally = False
    remotely = False
    command = [git, "-C", dir, "rev-parse", "--verify", branch]
    if verbose:
        print(command)
    result = subprocess.run(
        command,
        stdout=PIPE,
        stderr=PIPE,
    )
    if result.returncode == 0:
        locally = True
    # Check if feature branch exists on the remote
    result = run(
        [
            git,
            "-C",
            dir,
            "ls-remote",
            "--heads",
            remote,
            f"refs/heads/{branch}",
        ],
        stdout=PIPE,
    )
    if len(result.stdout.decode().splitlines()) > 0:
        remotely = True
    return locally, remotely


def create_intel_llvm_feature_branch(
    git: str, dir: str, branch: str, remote: str, base: str
) -> None:
    # create feature branch on intel/llvm
    run(
        [
            git,
            "-C",
            dir,
            "branch",
            branch,
            f"{remote}/{base}",
        ]
    )
    run([git, "-C", dir, "checkout", branch])


def checkout_branch(git: str, dir: str, branch: str) -> None:
    run(
        [
            git,
            "-C",
            dir,
            "checkout",
            branch,
        ]
    )


def get_patch_files(
    git: str, dir: str, output_dir: str, remote: str, branch: str
) -> List[str]:
    # Use git format-patch to create a list of patches from the feature branch
    patches = run(
        [
            git,
            "-C",
            dir,
            "format-patch",
            "--output-directory",
            output_dir,
            f"{remote}/main..{branch}",
        ],
        stdout=PIPE,
    )
    patches = patches.stdout.decode().strip().splitlines()
    print("List of patch files:")
    for patch in patches:
        print(f"  {patch}")
    return patches


def apply_patches(git: str, dir: str, patches: List[str]) -> None:
    # Use git am to apply list of patches to intel/llvm
    run([git, "-C", dir, "am", "--directory=unified-runtime"] + patches)


def push_branch(git: str, dir: str, remote: str, branch: str) -> None:
    run([git, "-C", dir, "push", "-u", remote, branch])


def create_pull_request(
    gh: str, dir: str, repo: str, base_branch: str, title: str, body: str
) -> None:
    run(
        [
            gh,
            "pr",
            "create",
            "--repo",
            repo,
            "--base",
            base_branch,
            "--title",
            title,
            "--body-file",
            "-",
        ],
        input=body.encode("utf-8"),
        cwd=dir,
    )


def close_pull_request(gh: str, dir: str, repo: str, number: str) -> None:
    run(
        [
            gh,
            "pr",
            "close",
            "--repo",
            repo,
            number,
        ],
        cwd=dir,
    )


def run(command: List[str], **kwargs) -> CompletedProcess:
    if verbose:
        print(command)
    result = subprocess.run(command, **kwargs)
    if result.returncode != 0:
        error(f"command '{' '.join(command)}' returned {result.returncode}")
    return result


def confirm(message: str) -> bool:
    if yes:
        return True
    while True:
        response = input(f"\033[36m{message} [Y/n]:\033[0m ").lower()
        if response in ["", "y", "yes"]:
            return True
        elif response in ["n", "no"]:
            return False
        continue


def error(*args, **kwargs) -> NoReturn:
    print("\033[31merror:\033[0m", *args, file=sys.stderr, **kwargs)
    exit(1)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print()
        exit(130)
