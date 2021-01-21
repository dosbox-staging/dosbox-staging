#!/usr/bin/env python

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2021-2021  DOSBox Staging team

# pylint: disable=invalid-name

"""
This script interleaves Roland IC dump pairs
into ROM files suitable for use with mt32emu.

"""

from hashlib import sha1
from itertools import chain
from mmap import ACCESS_READ, mmap
from os import path
import sys

def check_python_version():
    """Check if python meets this scripts' needs"""

    MIN_PYTHON = (3, 6)
    if sys.version_info < MIN_PYTHON:
        sys.exit("Python %s.%s or later is required.\n" % MIN_PYTHON)


def open_as_byte_stream(filename):
    """Open the file as a raw byte stream"""

    with open(filename, 'rb') as f:
        return mmap(f.fileno(), 0,
                    access=ACCESS_READ)


def check_usage():
    """Check usage and print help"""

    if len(sys.argv) > 1:
        return
    script = __file__
    sys.exit('Usage: %s IC-prefix [IC-prefix [..]]\n\n'\
          'Where IC-Prefix.ic27.bin and IC-prefix.ic26.bin files exist.\n\n'\
          'For example, given IC dumps:\n'\
          '  mt32_1.0.6.ic26.bin\n'\
          '  mt32_1.0.6.ic27.bin\n'\
          '  mt32_1.0.7.ic26.bin\n'\
          '  mt32_1.0.7.ic27.bin\n\n'\
          'Run %s mt32_1.0.6 mt32_1.0.7\n' % \
          (script, script))


def construct_rom(ic_prefix):
    """Byte-interleave a pair of IC dumps to ROM format"""

    # Contruct filenames from prefix
    ic27 = ic_prefix + '.ic27.bin'
    ic26 = ic_prefix + '.ic26.bin'
    rom = ic_prefix + '.rom'

    isfile = path.isfile
    if not isfile(ic27) or not isfile(ic26):
        print(f'Missing {ic27} and/or {ic26}, skipping')
        return

    # Read and interleave content from ICs
    ichain = chain.from_iterable
    with open_as_byte_stream(ic27) as s27,\
         open_as_byte_stream(ic26) as s26:
        interleaved = ichain(zip(s27, s26))
        content = b''.join(interleaved)

    # Write byte-content to ROM
    with open(rom, 'wb') as rom_file:
        rom_file.write(content)

    # Display SHA1 hash od content
    checksum = sha1(content).hexdigest()
    print(f'{rom} SHA1: {checksum}')

if __name__ == "__main__":
    check_python_version()
    check_usage()

    for prefix in sys.argv[1:]:
        construct_rom(prefix)
