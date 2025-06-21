// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_BOOT_H
#define DOSBOX_PROGRAM_BOOT_H

#include "dosbox.h"
#include "programs.h"

class BOOT final : public Program {
    public:
	    BOOT()
	    {
		    AddMessages();
		    help_detail = {HELP_Filter::All,
		                   HELP_Category::Dosbox,
		                   HELP_CmdType::Program,
		                   "BOOT"};
	    }
	    void Run(void) override;

    private:
        static void AddMessages();
	FILE* getFSFile_mounted(const char* filename, uint32_t* ksize,
	                        uint32_t* bsize, uint8_t* error);
	FILE* getFSFile(const char* filename, uint32_t* ksize, uint32_t* bsize,
	                bool tryload = false);
	void printError();
	void disable_umb_ems_xms();
	void NotifyBooting();
};

#endif // DOSBOX_PROGRAM_BOOT_H
