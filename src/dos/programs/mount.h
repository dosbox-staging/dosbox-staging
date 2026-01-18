// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MOUNT_H
#define DOSBOX_PROGRAM_MOUNT_H

#include "dos/programs.h"

class MOUNT final : public Program {
    public:
	    MOUNT()
	    {
		    AddMessages();
		    help_detail = {HELP_Filter::Common,
		                   HELP_Category::Dosbox,
		                   HELP_CmdType::Program,
		                   "MOUNT"};
	    }
	    void ListMounts();
	    void Run() override;
	    bool AddWildcardPaths(const std::string& path_arg,
	                          std::vector<std::string>& paths);
	    bool MountImage(char drive, const std::vector<std::string>& paths,
	                    const std::string& type, const std::string& fstype,
	                    uint16_t* sizes, bool roflag, bool wants_ide,
	                    int8_t ide_index, bool is_second_cable_slot);

    private:
	    static void AddMessages();
	    void ShowUsage();
};

#endif // DOSBOX_PROGRAM_MOUNT_H
