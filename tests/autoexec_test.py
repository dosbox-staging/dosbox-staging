# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2021-2021  The DOSBox Staging Team
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

""" backend for performing autoexec-based dosbox tests.

Collection of functions that can be used for performing automatic autoexec
testing of DOSBox Staging emulator.

"""

from itertools import zip_longest
from typing import Iterator
import os
import pathlib
import subprocess
import sys

DOSBOX_TEST_ENV = {
    'XDG_CONFIG_HOME': 'xdg-config-home-test',
    'SDL_VIDEODRIVER': 'dummy',
    'DOSBOX_DOSBOX_STARTUP_VERBOSITY': 'low',
    'DOSBOX_MIXER_NOSOUND': 'true',
    'DOSBOX_MIDI_MIDIDEVICE': 'none',
}

def get_test_name() -> str:
    r""" get_test_name() -> str

    Obtain test name based on current test script filename.

    Example: t1234_test_name.py -> t1234_test_name

    """
    exe_path = pathlib.Path(sys.argv[0])
    return exe_path.stem


def get_test_num() -> str:
    r""" get_test_num() -> str

    Obtain test number id based on current test script filename.

    Example: t1234_test_name.py -> t1234

    """
    exe_path = pathlib.Path(sys.argv[0])
    return exe_path.stem.split('_')[0]


def _sanitize_file_lines(dos_output_file: str) -> Iterator[str]:
    for line in open(dos_output_file, 'r').readlines():
        yield line[:-1]  # strip newline char
    # end with empty line to make it easier to compare with
    # multiline string split by newline char
    yield ''


def run_autoexec_test(conf_txt: str, expected_output: str) -> None:
    r""" run_autoexec_test(conf_txt: str, expected_output: str)

    Start dosbox binary (passed through the first script argument) using test
    environment (see DOSBOX_TEST_ENV) and generated .conf file.

    conf_txt parameter should contain text to be injected into the .conf file
    (notably, including an autoexec section ending with 'exit' command).

    You can use {out} in conf_txt as placeholder for file used to store
    output redirected from DOS commands/programs.  After dosbox exits,
    the text redirected to {out} is compared line-by-line to text passed
    through expected_output parameter.

    This function always ends with an exit, with one of following codes:

    0 - test passed
    1 - dosbox process exited with non-zero return code
    2 - expected output is different than text stored in {out} file

    """
    dosbox_bin = sys.argv[1]
    test_name = get_test_name()
    test_conf = test_name + '.conf'
    out_file = get_test_num() + '.txt'

    print('---')
    print('running test', test_name)
    print('in', os.getcwd())

    with open(test_conf, 'w') as conf_file:
        conf_file.write(conf_txt.format(out = out_file))

    result = subprocess.run([dosbox_bin, '-conf', test_conf],
                            env = DOSBOX_TEST_ENV,
                            check = True)
    print('---')
    print(f'dosbox stopped with error code: {result.returncode}')
    if result.returncode != 0:
        sys.exit(1) # test failed - unexpected error code

    out_file_name = out_file.upper()
    out_lines = _sanitize_file_lines(out_file_name)
    exp_lines = expected_output.split('\n')
    line_no = 0
    for out_line, exp_line in zip_longest(out_lines, exp_lines, fillvalue='<missing line>'):
        line_no += 1
        if exp_line != out_line:
            print(f'unexpected DOS shell output in line {out_file_name}:{line_no}')
            print(f'expecting: "{exp_line}"')
            print(f'found:     "{out_line}"')
            sys.exit(2) # test failed - unexpected DOS shell output

    sys.exit(0)
