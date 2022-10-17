#!/usr/bin/env bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022-2022  kcgen <kcgen@users.noreply.github.com>

# Bash strict-mode
set -euo pipefail
IFS=$'\n\t'

SCRIPT=$(basename "$0")
readonly SCRIPT

print_usage() {
    echo ""
    echo "usage: $SCRIPT URI [URI [...]]"
    echo
    echo "Convert a YouTube video with chapters into CDDA+CUE format."
    echo " - Multiple videos can be provided; each will be converted."
    echo " - URIs can be the full https:// URL or just the identifier."
    echo ""
    echo "Depends on:"
    echo " - yt-dlp (install with pip3 install yt-dlp)"
    echo " - ffmpeg: install with package manager"
    echo " - basename: install with package manager"
    echo ""
}

main() {
    case ${1:-} in
    -h | -help | --help) print_usage ;;
    *)
        check-dependencies
        convert-chapters-to-cdda "$@"
        ;;
    esac
}

check-dependencies() {
    local missing_deps=0
    for dep in yt-dlp ffmpeg find sort rm basename; do
        if ! command -v "$dep" &>/dev/null; then
            echo "Missing dependency: $dep could not be found in the PATH"
            ((missing_deps++))
        fi
    done
    # Were any missing?
    if [[ "$missing_deps" -gt 0 ]]; then
        echo "Install the above programs and try again"
        exit 1
    fi
}

download-chapters-from-uri() {
    local uri="$1"

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
        "$uri"
}

extract-opus-from-webm() {
    local webm="$1"
    local opus="${1%.webm}.opus"

    if [[ -f "$webm" && ! -f "$opus" ]]; then
        # Extract the Opus stream from the WebM
        ffmpeg -hide_banner -loglevel error \
            -i "$webm" \
            -vn -acodec copy "$opus"
        # Delete the WebM source if we've got the Opus
        if [[ -f "$opus" ]]; then
            rm "$webm"
        fi
    fi
}

add-track-to-cue() {
    local dir
    local webm
    local opus
    dir=$(basename "${1%/*}")
    webm=$(basename "$1")
    opus=$(basename "$webm" webm)opus

    local track_number="$2"

    # Ensure this directory and the track exists
    if [[ ! -d "$dir" || ! -f "$dir/$opus" ]]; then
        echo "Problem finding the CDDA directory ($dir) and/or track ($dir/$opus)"
        exit 1
    fi

    # Add a CUE entry for the track
    cue_path="$dir/cdrom.cue"
    {
        echo "FILE \"$opus\" OPUS"
        echo "  TRACK $track_number AUDIO"
        # Add pre-gap just for the first track
        if [[ "$track_number" == "1" ]]; then
            echo "  PREGAP 00:02:00"
        fi
        echo "  INDEX 01 00:00:00"
        echo ""
    } >>"$cue_path"
}

convert-local-webm-to-cdda() {
    local count=1
    for webm in $(find . -maxdepth 2 -mindepth 2 -name '*.webm' | sort); do
        extract-opus-from-webm "$webm"
        add-track-to-cue "$webm" "$count"
        ((count += 1))
    done
}

print-dosbox-imgmount-line() {
    local cue="$1"
    if [[ -n "$cue" ]]; then
        echo "imgmount d \"$cue\" -t cdrom"
    fi
}

convert-chapters-to-cdda() {
    for uri in "$@"; do
        cue_path=""
        download-chapters-from-uri "$uri"
        convert-local-webm-to-cdda
        print-dosbox-imgmount-line "$cue_path"
    done
}

main "$@"
