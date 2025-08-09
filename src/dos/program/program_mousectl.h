// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MOUSECTL_H
#define DOSBOX_PROGRAM_MOUSECTL_H

#include "dos/programs.h"

#include "hardware/input/mouse.h"

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
	bool CheckMappingSupported();
	bool CheckMappingPossible();
	void FinalizeMapping();

	static std::string GetMapStatusStr(const MouseMapStatus map_status);

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
