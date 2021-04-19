/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

#ifndef DOSBOX_PROGRAM_AUTOTYPE_H
#define DOSBOX_PROGRAM_AUTOTYPE_H

#include "programs.h"

#include <string>

class AUTOTYPE final : public Program {
public:
	void Run();

private:
	void PrintUsage();
	void PrintKeys();
	bool ReadDoubleArg(const std::string &name,
	                   const char *flag,
	                   const double &def_value,
	                   const double &min_value,
	                   const double &max_value,
	                   double &value);
};

void AUTOTYPE_ProgramStart(Program **make);

#endif
