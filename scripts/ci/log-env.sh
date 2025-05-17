#!/bin/bash

# To use this script in GitHub Actions job:
#
#  - name: "Log environment"
#    shell: bash
#    run: ./scripts/log-env.sh
#
# Line "shell: bash" is optional for Linux and MacOS,
# but required for Windows; see also script: log-env.ps1

set -x
uname -a
uname -s
uname -m
env
