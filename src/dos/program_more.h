/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_PROGRAM_MORE_H
#define DOSBOX_PROGRAM_MORE_H

#include "programs.h"

#include <vector>

class MORE final : public Program {
public:
	MORE()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "MORE"};
	}
	void Run();

private:
	bool ParseCommandLine();
	bool FindInputFiles(const std::vector<std::string> &params);

	void DisplayInputFiles();
	void DisplayInputStream();
	UserDecision DisplaySingleStream();
	UserDecision PromptUser();

	std::string GetShortName(const std::string &file_name, const char *msg_id);
	static uint8_t GetCurrentColumn();
	static uint8_t GetCurrentRow();
	bool GetCharacter(char &code);

	void AddMessages();

	struct InputFile {
		std::string name = ""; // file name with path
		bool is_device   = false;
	};

	std::vector<InputFile> input_files = {};

	uint16_t max_lines    = 0;
	uint16_t max_columns  = 0;
	uint16_t line_counter = 0;

	uint8_t tab_size       = 8;
	uint8_t tabs_remaining = 0;
	bool skip_next_cr      = false;
	bool skip_next_lf      = false;

	uint16_t input_handle = 0;  // DOS handle of the input stream
	bool ctrl_c_enable = false; // if CTRL+C in the input stream should quit
};

#endif
