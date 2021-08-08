#!/usr/bin/python3

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2021-2021  Patryk Obara <patryk.obara@gmail.com>

# pylint: disable=invalid-name

"""
Verify if there are no problematic runtime dependencies for this macOS binary.
"""

import sys
import subprocess


if __name__ == '__main__':
    result = subprocess.run(['otool', '-L', sys.argv[1]],
                            stdout=subprocess.PIPE,
                            check=True)
    output = result.stdout.decode('utf-8').split('\n')

    allowed_list = [
        '/System/Library/Frameworks/AppKit.framework/',
        '/System/Library/Frameworks/AudioToolbox.framework/',
        '/System/Library/Frameworks/AudioUnit.framework/',
        '/System/Library/Frameworks/Carbon.framework/',
        '/System/Library/Frameworks/CoreAudio.framework/',
        '/System/Library/Frameworks/CoreFoundation.framework/',
        '/System/Library/Frameworks/CoreGraphics.framework/',
        '/System/Library/Frameworks/CoreHaptics.framework/',
        '/System/Library/Frameworks/CoreMIDI.framework/',
        '/System/Library/Frameworks/CoreServices.framework/',
        '/System/Library/Frameworks/CoreVideo.framework/',
        '/System/Library/Frameworks/ForceFeedback.framework/',
        '/System/Library/Frameworks/Foundation.framework/',
        '/System/Library/Frameworks/GameController.framework/',
        '/System/Library/Frameworks/IOKit.framework/',
        '/System/Library/Frameworks/Metal.framework/',
        '/System/Library/Frameworks/OpenGL.framework/',
        '/usr/lib/',
    ]

    # skip first line (program name)
    for desc in (line.strip() for line in output[1:]):
        if not desc: # skip over empty string
            continue
        if any(desc.startswith(pfx) for pfx in allowed_list):
            continue
        print(f"Problematic runtime dependency: '{desc}'")
        print("Is it installed by default on macOS system?")
        sys.exit(1)

    sys.exit(0)
