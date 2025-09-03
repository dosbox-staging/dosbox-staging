// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_LS_H
#define DOSBOX_PROGRAM_LS_H

#include "dos/dos.h"
#include "dos/programs.h"

#include <string>

class LS final : public Program {
public:
	LS()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::File,
		               HELP_CmdType::Program,
		               "LS"};
	}
	void Run() override;

private:
	// Map a vector of dir contents to a vector of word widths
	static std::vector<uint8_t> GetFileNameLengths(
	        const std::vector<DOS_DTA::Result>& dir_contents,
	        const uint8_t padding = 0);
	static std::vector<uint8_t> GetColumnWidths(const std::vector<uint8_t>& name_widths,
	        const uint8_t min_column_width);

	static void AddMessages();
};

#endif
