#!/usr/bin/python3

# Copyright (C) 2019-2020  Patryk Obara <patryk.obara@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# pylint: disable=invalid-name
# pylint: disable=line-too-long

r"""
This is a wrapper for MSYS2 bash inside Windows GitHub CI environment.

Usage:

- name:  Step Name
  shell: python scripts\msys-bash.py {0}
  run:   echo "Hello"

GitHub documentation for specifying shell is here: [1], although it
leaves several VERY important details out:

- The first parameter (program name) in template shell invocation needs to be
  in PATH; absolute path will result in obscure C# exception.
- Name of temporary script file ends with a hardcoded extension matching the
  first parameter.
- There is some additional undocumented trickery about expansion of {0} in
  template (most probably it's something about formatting strings in C#).
- For some shells, an additional first line is prepended to the temporary
  script (but not for python!)

[1] https://help.github.com/en/github/automating-your-workflow-with-github-actions/workflow-syntax-for-github-actions#using-a-specific-shell
"""

import shutil
import subprocess
import sys

if __name__ == "__main__":
    script_py = sys.argv[1]
    script_sh = script_py + '.sh'
    shutil.copy(script_py, script_sh)
    bash = [r'C:\tools\msys64\usr\bin\bash', '-eo', 'pipefail', '-l']
    rcode = subprocess.call(bash + [script_sh])
    sys.exit(rcode)
