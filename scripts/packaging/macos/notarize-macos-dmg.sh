#!/bin/bash

# SPDX-FileCopyrightText:  2024-2026 The DOSBox Staging Team
# SPDX-License-Identifier: GPL-2.0-or-later

# This script takes an unsigned DOSBox Staging DMG from the GitHub CI build,
# signs all binaries inside it with a Developer ID certificate, notarizes it
# with Apple, and staples the notarization ticket to the final DMG.
#
# See docs/build-macos.dmg for instructions on setting up the certificates for
# code signing on your development machine.
#
# Required environment variables:
#
#   DEVELOPER_IDENTITY - Your Developer ID cert, e.g:
#                        "Developer ID Application: John Novak (944SS92GAH)"
#
#   KEYCHAIN_PROFILE   - The notarytool keychain profile name created via:
#                        xcrun notarytool store-credentials <profile-name> ...
#
# Usage:
#   ./notarize-macos-dmg.sh INPUT_DMG OUTPUT_DMG

set -e

if [[ -z "$DEVELOPER_IDENTITY" || -z "$KEYCHAIN_PROFILE" ]]; then
    echo "Error: DEVELOPER_IDENTITY and KEYCHAIN_PROFILE environment variables must be set."
    exit 1
fi

if [[ "$#" -ne 2 ]]; then
    echo "Usage: $0 INPUT_DMG OUTPUT_DMG"
    exit 1
fi

SCRIPT_DIR=$(pwd)
INPUT_DMG="$1"

# Resolve OUTPUT_DMG to an absolute path before we cd into TEMP_DIR,
# otherwise the relative path will point inside TEMP_DIR and get deleted
# during cleanup.
OUTPUT_DMG="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"

TEMP_DIR=$(mktemp -d)

# Mount the input DMG and capture the volume path
ATTACHED_VOLUME_PATH=$(hdiutil attach "$INPUT_DMG" | grep "Volumes" | awk 'BEGIN {FS="\t"}; {print $NF}')
ATTACHED_VOLUME_NAME=$(basename "$ATTACHED_VOLUME_PATH")

if [[ -z "$ATTACHED_VOLUME_NAME" ]]; then
    echo "Error attaching DMG."
    exit 1
fi

# Copy the mounted volume's contents into a temp directory so we can modify
# them.
ditto "$ATTACHED_VOLUME_PATH" "$TEMP_DIR"
cd "$TEMP_DIR" || { echo "Failed to change to $TEMP_DIR"; exit 1; }

APP="DOSBox Staging.app"
CERT="$DEVELOPER_IDENTITY"

# Remove accidental sed backup files left over from older DMG builds.
# These corrupt the app bundle signature and cause spctl verification to fail.
#
rm -f "$APP/Contents/Info.plist-e"
rm -f "$APP/Contents/SharedSupport/README-e"

# -------------------------------------------------------------------
# IMPORTANT!
# ===================================================================
# Signing must be done inside-out: every binary inside the app bundle
# must be signed *before* the bundle itself is signed. This is because
# the app bundle signature is a hash of its contents — if anything
# inside is signed or modified after the bundle is signed, the bundle
# signature becomes invalid.
# -------------------------------------------------------------------

# Sign all bundled dylibs individually. These are third-party libraries
# (glib, slirp, etc.) that we inject at CI-build time, so they are not signed
# by their upstream sources. Therefore, we must sign them ourselves with our
# Developer ID.
#
find "$APP/Contents/MacOS/lib" -name "*.dylib" | while read -r dylib; do
    echo "Signing dylib: $dylib"
    codesign -s "$CERT" -fv --timestamp -o runtime "$dylib"
done

# Sign the bundled Nuked-SC55 CLAP plugin. CLAP plugins are bundles, so we
# must sign the inner binary first, then the outer bundle.
#
if [[ -f "$APP/Contents/PlugIns/Nuked-SC55.clap/Contents/MacOS/Nuked-SC55" ]]; then
    echo "Signing Nuked-SC55 binary..."
    codesign -s "$CERT" -fv --timestamp -o runtime \
        "$APP/Contents/PlugIns/Nuked-SC55.clap/Contents/MacOS/Nuked-SC55"

    echo "Signing Nuked-SC55.clap bundle..."
    codesign -s "$CERT" -fv --timestamp -o runtime \
        "$APP/Contents/PlugIns/Nuked-SC55.clap"
fi

# Catch-all: sign any other executable files in PlugIns that aren't
# part of a .clap bundle (future-proofing for additional plugins).
#
find "$APP/Contents/PlugIns" -name "*.clap" -prune -o -type f -perm +111 -print | while read -r bin; do
    echo "Signing plugin binary: $bin"
    codesign -s "$CERT" -fv --timestamp -o runtime "$bin"
done

# Sign the app bundle last, with our entitlements. The entitlements file
# declares capabilities the app needs (e.g., hardened runtime exceptions).
echo "Signing app bundle..."

codesign -s "$CERT" -fv --timestamp -o runtime \
    --entitlements "${SCRIPT_DIR}/extras/macos/Entitlements.plist" "$APP"

# Verify the signature is valid before we bother packaging and uploading.
# Catches problems early and saves a wasted notarization submission.
echo "Verifying signature..."
codesign --verify --deep --strict --verbose=2 "$APP"

# Repackage the signed app into a new DMG using UDZO (zlib) compression.
echo "Creating output DMG..."
hdiutil create -volname "$ATTACHED_VOLUME_NAME" -srcfolder "$TEMP_DIR" \
	-ov -format UDZO "$OUTPUT_DMG"

# The DMG itself must also be signed — Apple requires all submitted
# files to be signed, not just the app inside them.
echo "Signing DMG..."
codesign -s "$CERT" -fv --timestamp "$OUTPUT_DMG"

# Submit the DMG to Apple's notary service. --wait blocks until Apple
# returns a result rather than requiring a separate polling step.
echo "Submitting for notarization..."
xcrun notarytool submit "$OUTPUT_DMG" \
	--keychain-profile "$KEYCHAIN_PROFILE" --wait

# Staple the notarization ticket to the DMG. This embeds the ticket
# so Gatekeeper can verify it offline, without contacting Apple's servers.
echo "Stapling notarization ticket..."
xcrun stapler staple "$OUTPUT_DMG"

# Cleanup: detach the original mounted volume and delete the temp directory.
hdiutil detach "$ATTACHED_VOLUME_PATH"
rm -rf "$TEMP_DIR"

echo "Done: $OUTPUT_DMG"
