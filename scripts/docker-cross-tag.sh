#!/usr/bin/env bash

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2024-2024  The DOSBox Staging Team

# This script requires the following programs to be installed as prerequisites:
#   - curl
#   - jq
#   - sha256sum

usage() {
    printf "%s\n" "\
Usage: $0 FUNC OPTIONS

Utility script to assist with docker image tags

FUNC must be one of:
  tag-exists         Test if image tag exists in the GitHub Container Registry (GHCR).
                     Takes two options: IMAGE_NAME TAG.

  hash-files         Compute a hash of specified files for use in a docker image tag
                     docker image tag. Command takes one or more file paths as options.
"
}

test_prerequisites() {
    for prog in curl jq sha256sum
    do
        if ! command -v "$prog" &> /dev/null ; then
            printf "%s not found!\n" "$prog" && exit 1
        fi
    done
}

tag_exists_in_ghcr() {
    local image="$1"
    local tag="$2"

    # Get an anonymous token from Github to make further API calls.
    # No need for accounts as the images are public.
    token=$(curl -s https://ghcr.io/token\?scope="repository:dosbox-staging/${image}:pull" | jq -r .token)
    # Get the list of tags from the registry, and attempt to find provided tag
    # in the list using jq. Prints 'true' if tag found, or 'false' otherwise
    curl -s -H "Authorization: Bearer $token" "https://ghcr.io/v2/dosbox-staging/${image}/tags/list" \
        | jq --arg T "$tag" '.tags | any(.==$T)'
}

files_hash() {
    # Hash the contents of the provided list of files.
    # Print only the first 12 characters of the hash. If that is 
    # good enough for the Linux kernel, it's good enough for us.
    cat "$@" | sha256sum | cut -c -12
}

cmd="$1"

test_prerequisites

case "$cmd" in
    tag-exists) tag_exists_in_ghcr "$2" "$3" ;;
    hash-files) files_hash "${@:2}" ;;
    *) usage ;;
esac
