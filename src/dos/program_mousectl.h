/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
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

#ifndef DOSBOX_PROGRAM_MOUSECTL_H
#define DOSBOX_PROGRAM_MOUSECTL_H

#include "programs.h"

#include "mouse.h"

class MOUSECTL final : public Program {
public:
	MOUSECTL()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "MOUSECTL"};
	}
	void Run() override;

private:
	bool ParseAndRun();
	bool ParseInterfaces(std::vector<std::string> &params);
	bool ParseSensitivity(const std::string &param, int16_t &value);
	static bool ParseIntParam(const std::string &param, int &value);
	bool CheckInterfaces();
	void FinalizeMapping();

	static const char *GetMapStatusStr(const MouseMapStatus map_status);

	// Methods below exectute specific commands requested by the user,
	// they return 'true' if DOS error code should indicate success,
	// and 'false' if we should report a failure
	bool CmdShow(const bool show_all);
	bool CmdMap(const MouseInterfaceId interface_id, const std::string &pattern);
	bool CmdMap();
	bool CmdUnMap();
	bool CmdOnOff(const bool enable);
	bool CmdReset();
	bool CmdSensitivity(const std::string &param_x, const std::string &param_y);
	bool CmdSensitivityX(const std::string &param);
	bool CmdSensitivityY(const std::string &param);
	bool CmdSensitivity();
	bool CmdSensitivityX();
	bool CmdSensitivityY();
	bool CmdMinRate(const std::string &param);
	bool CmdMinRate();

	static void AddMessages();

	std::vector<MouseInterfaceId> list_ids = {};
};

#endif
