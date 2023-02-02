#!/usr/bin/python3

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2023-2023  kcgen <kcgen@users.noreply.github.com>

"""

Generates a vcpkg JSON manifest with pinned dependency versions
extracted from a local vcpkg repository.

Takes in the local project's name, version, vcpkg dependencies, and
path to a local vcpkg repository. The first three are optional (see
help for defaults).

The result is written to stdout. For example:

{
    "name": "dosbox-staging",
    "version": "0.81.0-alpha",
    "dependencies": [
        {
            "name": "fluidsynth",
            "version>=": "2.2.8#2"
        },
        {
            "name": "gtest",
            "version>=": "1.12.1"
        },
        ...
}

Conforms to the port version constraints form, specified in:
https://github.com/microsoft/vcpkg/blob/master/docs/specifications/versioning.md

"""

# pylint: disable=invalid-name
# pylint: disable=missing-docstring

import json
import os
import subprocess
import argparse


def parse_args():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter
    )

    req_args = parser.add_argument_group(title="Required arguments")
    req_args.add_argument(
        "-n",
        "--name",
        help="Project name, for example: DOSBox Staging.\n"
        "Derived from the repository, if not provided.",
        default=get_local_repo_name(),
    )
    req_args.add_argument(
        "-v",
        "--version",
        help="Project version, for example: 2.1.0.\n"
        "Derived from the most recent tag, if not provided.",
        default=get_local_repo_tag(),
    )
    req_args.add_argument(
        "-d",
        "--dep_names_file",
        help="List of project dependency names, one per line, in a file.\n"
        'Derived from "packages/vcpkg.txt" if not provided.',
        default="packages/vcpkg.txt",
    )
    req_args.add_argument(
        "-p",
        "--vcpkg_path",
        required=True,
        help="Path to vcpkg in which to fetch dependency metadata.",
    )

    return parser.parse_args()


def get_stdout_from_command(arg_line):
    result = subprocess.run(arg_line.split(), stdout=subprocess.PIPE, check=True)
    return result.stdout.decode("utf-8").strip()


def get_local_repo_name():
    git_command = "git rev-parse --show-toplevel"
    top_level = get_stdout_from_command(git_command)
    return os.path.basename(top_level)


def get_local_repo_tag():
    git_command = "git describe --abbrev=0"
    tag = get_stdout_from_command(git_command)
    is_tag_a_version = tag.startswith("v")
    return tag[1::] if is_tag_a_version else tag


def get_latest_baseline_commit(vcpkg_path):
    baseline_file = "versions/baseline.json"
    git_command = f'git -C {vcpkg_path} log -1 --format="%H" {baseline_file}'
    output = get_stdout_from_command(git_command)
    parts = output.split()
    assert len(parts) == 1, "Should have just the commit ID"
    return parts[0].strip('"')


def get_pkg_version(record):
    pkg_version = None
    for v in ["version", "version-semver", "version-date", "version-string"]:
        if v in record:
            pkg_version = record[v]

    assert pkg_version is not None, "Each pakage has one version type"
    return pkg_version


def read_dep_version(dep_name, vcpkg_path):
    dep_version = {"name": dep_name}
    dep_version_file = f"{vcpkg_path}/versions/{dep_name[0]}-/{dep_name}.json"

    with open(dep_version_file, encoding="utf-8") as json_file:
        records = json.load(json_file)

        assert len(records) >= 1, "Versions file should have some records"
        record = records["versions"][0]

        pkg_version = get_pkg_version(record)

        assert "port-version" in record, "Each package has a port version"
        port_version = record["port-version"]
        pkg_version_postfix = f"#{port_version}" if port_version > 0 else ""

        dep_version["version>="] = f"{pkg_version}{pkg_version_postfix}"

    return dep_version


def read_dep_names(dep_names_file):
    names = []
    with open(dep_names_file, encoding="utf-8") as f:
        names = f.read().splitlines()
        names.sort()
    return [n for n in names if n]


def read_dep_versions(dep_names_file, vcpkg_path):
    dep_names = read_dep_names(dep_names_file)
    return [read_dep_version(dep_name, vcpkg_path) for dep_name in dep_names]


def generate_project_json(name, version, dep_versions, builtin_baseline):
    project = {
        "name": name,
        "version": version,
        "dependencies": dep_versions,
        "builtin-baseline": builtin_baseline,
    }
    return json.dumps(project, indent=4)


def main():
    args = parse_args()

    dep_versions = read_dep_versions(args.dep_names_file, args.vcpkg_path)

    builtin_baseline = get_latest_baseline_commit(args.vcpkg_path)

    print(
        generate_project_json(args.name, args.version, dep_versions, builtin_baseline)
    )


if __name__ == "__main__":
    main()
