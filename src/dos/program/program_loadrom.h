// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_LOADROM_H
#define DOSBOX_PROGRAM_LOADROM_H

#include "dos/programs.h"

class LOADROM final : public Program {
    public:
	    LOADROM()
	    {
		    AddMessages();
		    help_detail = {HELP_Filter::All,
		                   HELP_Category::Dosbox,
		                   HELP_CmdType::Program,
		                   "LOADROM"};
	    }
	    void Run(void) override;
    private:
        static void AddMessages();
};

#endif // DOSBOX_PROGRAM_LOADROM_H
