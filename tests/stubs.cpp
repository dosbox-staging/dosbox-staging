// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include "config.h"
#include "logging.h"

// During testing we never want to log to stdout/stderr, as it could
// negatively affect test harness.

void DEBUG_ShowMsg(const char *, ...) {}

void DEBUG_HeavyWriteLogInstruction() {}

#if C_DEBUG
void LOG::operator()([[maybe_unused]] const char* buf, ...)
{
	(void)d_type;     // Deliberately unused.
	(void)d_severity; // Deliberately unused.
}
#endif
