// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_ATTRIB_H
#define DOSBOX_PROGRAM_ATTRIB_H

#include "programs.h"

#include <string>

class ATTRIB final : public Program {
public:
	ATTRIB()
	{
		help_detail = {HELP_Filter::All,
		               HELP_Category::File,
		               HELP_CmdType::Program,
		               "ATTRIB"};
	}
	void Run() override;
};

#endif
