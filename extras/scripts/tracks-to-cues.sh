#!/usr/bin/env bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022-2022 kcgen <kcgen@users.noreply.github.com>

# Bash strict-mode
set -euo pipefail
IFS=$'\n\t'

print_usage() {
    readonly SCRIPT="${0##*/}"
    echo ""
    echo "usage: $SCRIPT DIR [DIR [...]]"
    echo
    echo "Generate DOS Redbook CD-DA CUE file(s) in the given"
    echo "directories made up of flac, wav, opus, ogg, and mp3s"
    echo "Track numbers starts at 2 if an ISO file is present."
    echo "Redbook's 99 minute/track limits are handled."
    echo ""
    echo "Depends on: ffprobe (part of ffmpeg), GNU find and sort."
    echo ""
}

main() {
    case ${1:--help} in
    -h | -help | --help) print_usage ;;
    *)
        check-dependencies
        generate-cues-for-dirs "$@"
        ;;
    esac
}

check-dependencies() {
    local missing_deps=0
    for dep in ffprobe find sort rm; do
        if ! command -v "$dep" &>/dev/null; then
            echo "Missing dependency: $dep could not be found in the PATH"
            missing_deps=$((missing_deps + 1))
        fi
    done
    # Were any missing?
    if ((missing_deps > 0)); then
        echo "Install the above programs and try again"
        exit 1
    fi
}

initialize-state-variables() {
    iso=""

    track=""
    track_number=1
    track_runtime=0
    track_is_first=1
    readonly track_limit=99

    cue="cdrom.cue"
    cue_number=1
    cue_runtime=0
    readonly runtime_limit=$((99 * 60))
}

generate-cues-for-dirs() {
    for dir in "$@"; do
        {
            cd "$dir"
            generate-cues-for-tracks
        }
    done
}

generate-cues-for-tracks() {
    initialize-state-variables
    iso=$(get-first-iso)
    if [[ -n $iso ]]; then
        add-iso-to-cue
    fi
    for track in $(get-audio-tracks); do
        add-track-to-cue
    done
}

add-iso-to-cue() {
    {
        echo "FILE \"$iso\" BINARY"
        echo "   TRACK 1 MODE1/2048"
        echo "   INDEX 01 00:00:00"
        echo ""
    } >>"$cue"
    increment-track-number
}

add-track-to-cue() {
    get-track-runtime
    if ((track_runtime > runtime_limit)); then
        echo "Skipping $track: $track_runtime s runtime exceeds CD-DAs maximum of $runtime_limit"
        return
    fi
    add-track-runtime-to-cue
    {
        track_type=${track##*.}
        echo "FILE \"$track\" ${track_type^^}"
        echo "  TRACK $track_number AUDIO"

        if ((track_is_first == 1)); then
            echo "  PREGAP 00:02:00"
            track_is_first=0
        fi
        echo "  INDEX 01 00:00:00"
        echo ""
    } >>"$cue"

    increment-track-number
}

increment-track-number() {
    track_number=$((track_number + 1))
    if ((track_number > track_limit)); then
        increment-cue
    fi
}

get-track-runtime() {
    track_runtime=$(ffprobe \
        -v error \
        -show_entries format=duration \
        -of default=noprint_wrappers=1:nokey=1 \
        "$track")

    # Ceil decimal portion by a full second
    track_runtime=$((${track_runtime%%.*} + 1))
}

add-track-runtime-to-cue() {
    cue_runtime=$((cue_runtime + track_runtime))
    if ((cue_runtime > runtime_limit)); then
        increment-cue
    fi
}

increment-cue() {
    if ((cue_number == 1)); then
        mv "$cue" "cdrom1.cue"
    fi
    cue_runtime=$track_runtime
    cue_number=$((cue_number + 1))
    cue="cdrom${cue_number}.cue"

    track_number=1
    track_is_first=1
}

get-first-iso() {
    find ./ -type f \
        \( -iname \*.iso \) \
        -printf '%P\n' |
        sort --ignore-case |
        head -1 ||
        true
}

get-audio-tracks() {
    find ./ -type f \
        \( -iname \*.flac \
        -o -iname \*.wav \
        -o -iname \*.opus \
        -o -iname \*.ogg \
        -o -iname \*.mp3 \) \
        -printf '%P\n' |
        sort --ignore-case ||
        true
}

main "$@"
