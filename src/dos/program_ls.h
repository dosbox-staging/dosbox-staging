/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
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

#ifndef DOSBOX_PROGRAM_LS_H
#define DOSBOX_PROGRAM_LS_H

#include "dos_inc.h"
#include "programs.h"

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
