// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_AUTOTYPE_H
#define DOSBOX_PROGRAM_AUTOTYPE_H

#include "programs.h"

#include <string>

class AUTOTYPE final : public Program {
public:
	AUTOTYPE()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "AUTOTYPE"};
	}
	void Run() override;
private:
	static void AddMessages();
	void PrintUsage();
	void PrintKeys();
	bool ReadDoubleArg(const std::string &name,
	                   const char *flag,
	                   const double def_value,
	                   const double min_value,
	                   const double max_value,
	                   double &value);
};

#endif
