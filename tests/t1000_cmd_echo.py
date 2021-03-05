#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2021-2021  The DOSBox Staging Team

"""
Test echo command.

MS-DOS echo behaves very differently and unfriendly compared to UNIX echo,
but we need to support these for backwards-compatibility.

"""

import autoexec_test as at

CMD_ECHO_TESTS = '''
[autoexec]
mount C .
C:

# Output text, no spaces around.
#
echo foo > {out}

# (MS-DOS behaviour) DOS echo does not support quoting
#
echo 'bar' >> {out}
echo "bar" >> {out}
echo ''    >> {out}
echo ""    >> {out}

# Output text, surprisingly with spaces.
# TODO check - MS-DOS was really this stupid?
#
echo    baz >> {out}

# (MS-DOS behaviour) Test if echo off/on changes echo status.
#
echo OFF >> {out}
echo     >> {out}
echo ON  >> {out}
echo     >> {out}
echo off >> {out}
echo     >> {out}
echo on  >> {out}
echo     >> {out}

# (MS-DOS behaviour) Echo the text "ON" or "OFF" instead of turning the
# command printing on or off (as in previous test).
# This can be done using . or / instead of space.
#
echo.ON  >> {out}
echo.OFF >> {out}
echo/ON  >> {out}
echo/OFF >> {out}
echo     >> {out}

# (MS-DOS behaviour) ... but slash after space is part of the message.
#
echo .    >> {out}
echo /    >> {out}
echo .msg >> {out}
echo /msg >> {out}

# (MS-DOS behaviour) dot and slash can be used to echo an empty line,
#
echo. >> {out}
echo/ >> {out}

echo bye! >> {out}

exit
'''

EXPECTED_OUT = '''foo
'bar'
"bar"
''
""
   baz
ECHO is off.
ECHO is on.
ECHO is off.
ECHO is on.
ON
OFF
ON
OFF
ECHO is on.
.
/
.msg
/msg


bye!
'''

at.run_autoexec_test(CMD_ECHO_TESTS, EXPECTED_OUT)
