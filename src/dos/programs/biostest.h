// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

// BIOSTEST.COM is a debugger-only internal DOS command for testing custom
// BIOS implementations. It loads a BIOS ROM image (up to 64 KB) from disk
// into the BIOS memory area at 0xF000:0 and starts CPU execution at the
// standard BIOS entry point CS:F000, IP:FFF0. Only compiled and registered
// when C_DEBUGGER is enabled.

#ifndef DOSBOX_PROGRAM_BIOSTEST_H
#define DOSBOX_PROGRAM_BIOSTEST_H

#include "dosbox.h"
#include "dos/programs.h"

class BIOSTEST final : public Program {
    public:
	    BIOSTEST()
	    {
		    help_detail = {HELP_Filter::All,
		                   HELP_Category::Misc,
		                   HELP_CmdType::Program,
		                   "BIOSTEST"};
	    }
	void Run(void) override;
};

#endif // DOSBOX_PROGRAM_BIOSTEST_H
