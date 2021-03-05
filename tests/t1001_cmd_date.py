#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2021-2021  The DOSBox Staging Team

"""
Test if setting date works and if leap year calculation is correct.
"""

import autoexec_test as at

CMD_DATE_TESTS = '''
[autoexec]
mount C .
C:

echo.> {out}
echo testing date output >> {out}
date 12-31-1999
date >> {out}

echo.>> {out}
echo testing err on non-leap year >> {out}
date 02-29-2001
date >> {out}

echo.>> {out}
echo testing success on leap year >> {out}
date 02-29-2004
date >> {out}

echo.>> {out}
echo testing success on leap year 2000 >> {out}
date 02-29-2000
date >> {out}

rem echo.>> {out}
rem echo testing err on leap year 2100 >> {out}
rem date 02-29-2100
rem date >> {out}

echo.>> {out}
echo testing success on year 2038 problem >> {out}
date 01-20-2038
date >> {out}

exit
'''

EXPECTED_OUT = '''
testing date output
Current date: Fri 12/31/1999
Type 'date MM-DD-YYYY' to change.

testing err on non-leap year
Current date: Fri 12/31/1999
Type 'date MM-DD-YYYY' to change.

testing success on leap year
Current date: Sun 02/29/2004
Type 'date MM-DD-YYYY' to change.

testing success on leap year 2000
Current date: Tue 02/29/2000
Type 'date MM-DD-YYYY' to change.

testing success on year 2038 problem
Current date: Wed 01/20/2038
Type 'date MM-DD-YYYY' to change.
'''

at.run_autoexec_test(CMD_DATE_TESTS, EXPECTED_OUT)
