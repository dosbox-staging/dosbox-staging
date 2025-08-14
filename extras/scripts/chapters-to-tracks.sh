#!/usr/bin/env bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022-2022 kcgen <kcgen@users.noreply.github.com>

# Bash strict-mode
set -euo pipefail
IFS=$'\n\t'

print_usage() {
    local script="${0##*/}"
    echo ""
    echo "usage: $script URI [URI [...]]"
    echo
    echo "Convert a YouTube video with chapters into CD-DA tracks."
    echo " - Multiple videos can be provided; each will be converted."
    echo " - URIs can be the full https:// URL or just the identifier."
    echo ""
    echo "Depends on:"
    echo " - yt-dlp (install with pip3 install yt-dlp)"
    echo " - ffmpeg, and GNU find and sort: install with package manager"
    echo ""
}

main() {
    case ${1:--help} in
    -h | -help | --help) print_usage ;;
    *)
        check-dependencies
        convert-chapters-to-tracks "$@"
        ;;
    esac
}

check-dependencies() {
    local missing_deps=0
    for dep in yt-dlp ffmpeg find sort rm; do
        if ! command -v "$dep" &>/dev/null; then
            echo "Missing dependency: $dep could not be found in the PATH"
            ((missing_deps++))
        fi
    done
    # Were any missing?
    if [[ $missing_deps -gt 0 ]]; then
        echo "Install the above programs and try again"
        exit 1
    fi
}

download-chapters-from-uri() {
    # YouTube Opus format identifiers in order of quality
    local webm_formats="338/251/250/249"

    # Directory/##-Track output layout
    local cdda_ouput="chapter:%(title)s/%(section_number)02d-%(section_title)s.%(ext)s"

    # Fetch the track and split it on chapters
    yt-dlp \
        --split-chapters \
        --restrict-filenames \
        --format "$webm_formats" \
        --output "$cdda_ouput" \
        "$uri" || true
}

extract-opus-from-webm() {
    local opus="${webm%.webm}.opus"

    ffmpeg \
        -hide_banner \
        -loglevel error \
        -i "$webm" \
        -vn -acodec copy "$opus"

    # Delete the WebM source if we've got the Opus
    if [[ -f $opus ]]; then
        rm "$webm"
    fi
}

convert-chapters-to-tracks() {
    for uri in "$@"; do
        download-chapters-from-uri
    done
    for webm in $(find . -maxdepth 2 -mindepth 2 -name '*.webm' | sort || true); do
        extract-opus-from-webm
    done
}

main "$@"
