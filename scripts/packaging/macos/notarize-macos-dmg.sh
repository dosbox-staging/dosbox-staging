#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2024-2024	The DOSBox Staging Team

# This script notarizes a macOS DMG produced from the CI builds by
# extracting the DMG, signing the app bundle, using the rest of the files
# and the newly-signed app bundle to create a new DMG, then signing and 
# notarizing the entire new DMG. Xcode must be installed.

# Ensure developer identity and keychain profile name are set as environment variables
# For example:
# export DEVELOPER_IDENTITY="Developer ID Application: Valued Developer (xxxxxxxxxxx)"
# export KEYCHAIN_PROFILE="Valued Developer Personal"
#
# Apple's notarization documentation: 
# https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution

if [[ -z "$DEVELOPER_IDENTITY" || -z "$KEYCHAIN_PROFILE" ]]; then
    echo "Error: DEVELOPER_IDENTITY and KEYCHAIN_PROFILE environment variables must be set."
    exit 1
fi

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 INPUT_DMG OUTPUT_DMG"
    exit 1
fi

SCRIPT_DIR=$(pwd)
INPUT_DMG="$1"
OUTPUT_DMG="$2"
TEMP_DIR=$(mktemp -d)

ATTACHED_VOLUME_PATH=$(hdiutil attach "$INPUT_DMG" | grep "Volumes" | awk 'BEGIN {FS="\t"}; {print $NF}')
ATTACHED_VOLUME_NAME=$(basename "$ATTACHED_VOLUME_PATH")

if [ -z "$ATTACHED_VOLUME_NAME" ]; then
    echo "Error attaching DMG."
    exit 1
fi

ditto "$ATTACHED_VOLUME_PATH" "$TEMP_DIR"

cd "$TEMP_DIR" || { echo "Failed to change to $TEMP_DIR"; exit 1; }

# Remove accidental sed backup files from older DMGs. These will cause
# verification failures, e.g.:
# spctl -a -vvv -t install "/Volumes/DOSBox Staging/DOSBox Staging.app"
rm "DOSBox Staging.app/Contents/Info.plist-e"
rm "DOSBox Staging.app/Contents/SharedSupport/README-e"

codesign -s "$DEVELOPER_IDENTITY" -fv --timestamp -o runtime --entitlements "${SCRIPT_DIR}/extras/macos/Entitlements.plist" "DOSBox Staging.app"

hdiutil create -volname "$ATTACHED_VOLUME_NAME" -srcfolder "$TEMP_DIR" -ov -format UDZO "$OUTPUT_DMG"

codesign -s "$DEVELOPER_IDENTITY" -fv --timestamp "$OUTPUT_DMG"

xcrun notarytool submit "$OUTPUT_DMG" --keychain-profile "$KEYCHAIN_PROFILE" --wait
xcrun stapler staple "$OUTPUT_DMG"

hdiutil detach "$ATTACHED_VOLUME_PATH"
rm -rf "$TEMP_DIR"

