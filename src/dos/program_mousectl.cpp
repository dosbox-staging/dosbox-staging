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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "program_mousectl.h"

#include "ansi_code_markup.h"
#include "checks.h"
#include "program_more_output.h"
#include "string_utils.h"

#include <set>

CHECK_NARROWING();

void MOUSECTL::Run()
{
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_MOUSECTL_HELP_LONG"));
		output.Display();
		return;
	}

	ParseAndRun();
	// TODO: once exit codes are supported, set it according to result
}

bool MOUSECTL::ParseAndRun()
{
	// Put all the parameters into vector
	std::vector<std::string> params;
	cmd->FillVector(params);

	// Extract the list of interfaces from the vector
	list_ids.clear();
	if (!ParseInterfaces(params) || !CheckInterfaces())
		return false;

	auto param_equal = [&params](const size_t idx, const char *string) {
		if (idx >= params.size())
			return false;
		return iequals(params[idx], string);
	};

	// CmdShow
	if (list_ids.size() == 0 && params.empty())
		return CmdShow(false);
	if (list_ids.size() == 0 && params.size() == 1)
		if (param_equal(0, "-all"))
			return CmdShow(true);

	// CmdMap - by supplied host mouse name
	if (list_ids.size() == 1 && params.size() == 2)
		if (param_equal(0, "-map"))
			return CmdMap(list_ids[0], params[1]);

	// CmdMap - interactive
	if (!list_ids.empty() && params.size() == 1)
		if (param_equal(0, "-map"))
			return CmdMap();

	// CmdUnmap / CmdOnOff / CmdReset / CmdSensitivity / CmdMinRate
	if (params.size() == 1) {
		if (param_equal(0, "-unmap"))
			return CmdUnMap();
		if (param_equal(0, "-on"))
			return CmdOnOff(true);
		if (param_equal(0, "-off"))
			return CmdOnOff(false);
		if (param_equal(0, "-reset"))
			return CmdReset();
		if (param_equal(0, "-s"))
			return CmdSensitivity();
		if (param_equal(0, "-sx"))
			return CmdSensitivityX();
		if (param_equal(0, "-sy"))
			return CmdSensitivityY();
		if (param_equal(0, "-r"))
			return CmdMinRate();
	}

	// CmdSensitivity / CmdMinRate with a non-default value
	if (params.size() == 2) {
		if (param_equal(0, "-r"))
			return CmdMinRate(params[1]);

		if (param_equal(0, "-s"))
			return CmdSensitivity(params[1], params[1]);
		if (param_equal(0, "-sx"))
			return CmdSensitivityX(params[1]);
		if (param_equal(0, "-sy"))
			return CmdSensitivityY(params[1]);
	}
	if (params.size() == 3) {
		if (param_equal(0, "-s"))
			return CmdSensitivity(params[1], params[2]);
	}

	WriteOut(MSG_Get("SHELL_SYNTAX_ERROR"));
	return false;
}

bool MOUSECTL::ParseSensitivity(const std::string &param, int16_t &value)
{
	value = 0;

	int tmp = 0;
	if (!ParseIntParam(param, tmp)) {
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_SYNTAX_SENSITIVITY"));
		return false;
	}

	// Maximum allowed user sensitivity value
	constexpr int16_t sensitivity_user_max = 999;
	if (tmp < -sensitivity_user_max || tmp > sensitivity_user_max) {
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_SYNTAX_SENSITIVITY"));
		return false;
	}

	value = static_cast<int16_t>(tmp);
	return true;
}

bool MOUSECTL::ParseIntParam(const std::string &param, int &value)
{
	try {
		value = std::stoi(param);
	} catch (...) {
		value = 0;
		return false;
	}

	return true;
}

bool MOUSECTL::ParseInterfaces(std::vector<std::string> &params)
{
	auto add_if_is_interface = [&](const std::string &param) {
		for (const auto id : {
		             MouseInterfaceId::DOS,
		             MouseInterfaceId::PS2,
		             MouseInterfaceId::COM1,
		             MouseInterfaceId::COM2,
		             MouseInterfaceId::COM3,
		             MouseInterfaceId::COM4,
		     }) {
			if (iequals(param, MouseControlAPI::GetInterfaceNameStr(id))) {
				list_ids.push_back(id);
				return true;
			}
		}
		if (iequals(param, "PS2")) { // syntax sugar, easier to type
			                     // than 'PS/2'
			list_ids.push_back(MouseInterfaceId::PS2);
			return true;
		}
		return false;
	};
	while (!params.empty() && add_if_is_interface(params.front()))
		params.erase(params.begin()); // pop

	// Check that all interfaces are unique
	const std::set<MouseInterfaceId> tmp(list_ids.begin(), list_ids.end());
	if (list_ids.size() != tmp.size()) {
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_SYNTAX_DUPLICATED"));
		return false;
	}

	return true;
}

bool MOUSECTL::CheckInterfaces()
{
	if (MouseControlAPI::CheckInterfaces(list_ids))
		return true;

	if (list_ids.empty())
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_NO_INTERFACES"));
	else
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_MISSING_INTERFACES"));

	return false;
}

const char *MOUSECTL::GetMapStatusStr(const MouseMapStatus map_status)
{
	switch (map_status) {
	case MouseMapStatus::HostPointer:
		return MSG_Get("PROGRAM_MOUSECTL_TABLE_STATUS_HOST");
	case MouseMapStatus::Mapped:
		return MSG_Get("PROGRAM_MOUSECTL_TABLE_STATUS_MAPPED");
	case MouseMapStatus::Disconnected:
		return MSG_Get("PROGRAM_MOUSECTL_TABLE_STATUS_DISCONNECTED");
	case MouseMapStatus::Disabled:
		return MSG_Get("PROGRAM_MOUSECTL_TABLE_STATUS_DISABLED");
	default:
		assert(false); // missing implementation
		return nullptr;
	}
}

bool MOUSECTL::CmdShow(const bool show_all)
{
	MouseControlAPI mouse_config_api;
	const auto info_interfaces = mouse_config_api.GetInfoInterfaces();

	bool show_mapped   = false;
	bool hint_rate_com = false;
	bool hint_rate_min = false;

	// Display emulated interface list
	WriteOut("\n");
	WriteOut(MSG_Get("PROGRAM_MOUSECTL_TABLE_HEADER1"));
	WriteOut("\n");
	for (const auto &entry : info_interfaces) {
		if (!entry.IsEmulated())
			continue;
		const auto interface_id  = entry.GetInterfaceId();
		const auto rate_hz       = entry.GetRate();
		const bool rate_enforced = entry.GetMinRate();

		if (rate_enforced)
			hint_rate_min = true;

		if (interface_id == MouseInterfaceId::COM1 ||
		    interface_id == MouseInterfaceId::COM2 ||
		    interface_id == MouseInterfaceId::COM3 ||
		    interface_id == MouseInterfaceId::COM4)
			hint_rate_com = true;

		WriteOut(MSG_Get("PROGRAM_MOUSECTL_TABLE_LAYOUT1"),
		         MouseControlAPI::GetInterfaceNameStr(interface_id).c_str(),
		         entry.GetSensitivityX(),
		         entry.GetSensitivityY(),
		         rate_enforced ? "*" : "",
		         rate_hz ? std::to_string(rate_hz).c_str() : "-",
		         convert_ansi_markup(GetMapStatusStr(entry.GetMapStatus()))
		                 .c_str());
		WriteOut("\n");

		if (entry.GetMapStatus() == MouseMapStatus::Mapped)
			show_mapped = true;
	}
	WriteOut("\n");

	const bool hint = hint_rate_com || hint_rate_min;
	if (hint) {
		if (hint_rate_com) {
			WriteOut(MSG_Get("PROGRAM_MOUSECTL_TABLE_HINT_RATE_COM"));
			WriteOut("\n");
		}
		if (hint_rate_min) {
			WriteOut(MSG_Get("PROGRAM_MOUSECTL_TABLE_HINT_RATE_MIN"));
			WriteOut("\n");
		}
		WriteOut("\n");
	}

	if (!show_all && !show_mapped)
		return true;

	const auto info_physical = mouse_config_api.GetInfoPhysical();
	if (info_physical.empty()) {
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_NO_PHYSICAL_MICE"));
		WriteOut("\n\n");
		return true;
	}

	if (hint)
		WriteOut("\n");
	WriteOut(MSG_Get("PROGRAM_MOUSECTL_TABLE_HEADER2"));
	WriteOut("\n");

	// Display physical mice mapped to some interface
	bool needs_newline = false;
	for (const auto &entry : info_interfaces) {
		if (!entry.IsMapped() || entry.IsMappedDeviceDisconnected())
			continue;
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_TABLE_LAYOUT2"),
		         MouseControlAPI::GetInterfaceNameStr(entry.GetInterfaceId())
		                 .c_str(),
		         entry.GetMappedDeviceName().c_str());
		WriteOut("\n");
		needs_newline = true;
	}

	if (!show_all) {
		if (needs_newline)
			WriteOut("\n");
		return true;
	}

	// Display physical mice not mapped to any interface
	for (const auto &entry : info_physical) {
		if (entry.IsMapped() || entry.IsDeviceDisconnected())
			continue;
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_TABLE_LAYOUT2_UNMAPPED"),
		         entry.GetDeviceName().c_str());
		WriteOut("\n");
	}
	WriteOut("\n");

	return true;
}

void MOUSECTL::FinalizeMapping()
{
	WriteOut("\n");
	WriteOut(MSG_Get("PROGRAM_MOUSECTL_MAP_HINT"));
	WriteOut("\n\n");
}

bool MOUSECTL::CmdMap(const MouseInterfaceId interface_id, const std::string &pattern)
{
	std::regex regex;
	if (!MouseControlAPI::PatternToRegex(pattern, regex)) {
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_SYNTAX_PATTERN"));
		return false;
	}

	if (MouseControlAPI::IsNoMouseMode()) {
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_MAPPING_NO_MOUSE"));
		return false;
	}

	MouseControlAPI mouse_config_api;
	if (!mouse_config_api.Map(interface_id, regex)) {
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_NO_MATCH"));
		return false;
	}

	FinalizeMapping();
	return true;
}

bool MOUSECTL::CmdMap()
{
	assert(!list_ids.empty());

	if (MouseControlAPI::IsNoMouseMode()) {
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_MAPPING_NO_MOUSE"));
		return false;
	}

	MouseControlAPI mouse_config_api;
	const auto info_physical = mouse_config_api.GetInfoPhysical();

	if (info_physical.empty()) {
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_NO_PHYSICAL_MICE"));
		WriteOut("\n\n");
		return false;
	}

	// Clear the current mapping before starting interactive mapper
	std::vector<MouseInterfaceId> empty;
	mouse_config_api.UnMap(empty);

	WriteOut("\n");
	WriteOut(MSG_Get("PROGRAM_MOUSECTL_MAP_ADVICE"));
	WriteOut("\n\n");

	for (const auto &interface_id : list_ids) {
		WriteOut(convert_ansi_markup("[color=cyan]%-4s[reset]   ?").c_str(),
		         MouseControlAPI::GetInterfaceNameStr(interface_id).c_str());

		uint8_t device_idx = 0;
		if (!mouse_config_api.MapInteractively(interface_id, device_idx)) {
			mouse_config_api.UnMap(empty);
			WriteOut("\b");
			WriteOut(MSG_Get("PROGRAM_MOUSECTL_MAP_CANCEL"));
			WriteOut("\n\n");
			return false;
		}

		WriteOut("\b");
		WriteOut(info_physical[device_idx].GetDeviceName().c_str());
		WriteOut("\n");
	}

	FinalizeMapping();
	return true;
}

bool MOUSECTL::CmdUnMap()
{
	MouseControlAPI mouse_config_api;
	mouse_config_api.UnMap(list_ids);
	return true;
}

bool MOUSECTL::CmdOnOff(const bool enable)
{
	MouseControlAPI mouse_config_api;
	mouse_config_api.OnOff(list_ids, enable);
	return true;
}

bool MOUSECTL::CmdReset()
{
	MouseControlAPI mouse_config_api;
	mouse_config_api.Reset(list_ids);
	return true;
}

bool MOUSECTL::CmdSensitivity(const std::string &param_x, const std::string &param_y)
{
	int16_t value_x = 0;
	int16_t value_y = 0;
	if (!ParseSensitivity(param_x, value_x) || !ParseSensitivity(param_y, value_y))
		return false;

	MouseControlAPI mouse_config_api;
	mouse_config_api.SetSensitivity(list_ids, value_x, value_y);
	return true;
}

bool MOUSECTL::CmdSensitivityX(const std::string &param)
{
	int16_t value = 0;
	if (!ParseSensitivity(param, value))
		return false;

	MouseControlAPI mouse_config_api;
	mouse_config_api.SetSensitivityX(list_ids, value);
	return true;
}

bool MOUSECTL::CmdSensitivityY(const std::string &param)
{
	int16_t value = 0;
	if (!ParseSensitivity(param, value))
		return false;

	MouseControlAPI mouse_config_api;
	mouse_config_api.SetSensitivityY(list_ids, value);
	return true;
}

bool MOUSECTL::CmdSensitivity()
{
	MouseControlAPI mouse_config_api;
	mouse_config_api.ResetSensitivity(list_ids);
	return true;
}

bool MOUSECTL::CmdSensitivityX()
{
	MouseControlAPI mouse_config_api;
	mouse_config_api.ResetSensitivityX(list_ids);
	return true;
}

bool MOUSECTL::CmdSensitivityY()
{
	MouseControlAPI mouse_config_api;
	mouse_config_api.ResetSensitivityY(list_ids);
	return true;
}

bool MOUSECTL::CmdMinRate(const std::string &param)
{
	const auto &valid_list = MouseControlAPI::GetValidMinRateList();
	const auto &valid_str  = MouseControlAPI::GetValidMinRateStr();

	int tmp = 0;
	if (!ParseIntParam(param, tmp)) {
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_SYNTAX_MIN_RATE"),
		         valid_str.c_str());
		return false;
	}

	if (tmp < 0 || tmp > UINT16_MAX) {
		// Parameter way out of range
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_SYNTAX_MIN_RATE"),
		         valid_str.c_str());
		return false;
	}

	const auto value_hz = static_cast<uint16_t>(tmp);
	if (!contains(valid_list, value_hz)) {
		// Parameter not in the list of allowed values
		WriteOut(MSG_Get("PROGRAM_MOUSECTL_SYNTAX_MIN_RATE"),
		         valid_str.c_str());
		return false;
	}

	MouseControlAPI mouse_config_api;
	mouse_config_api.SetMinRate(list_ids, value_hz);
	return true;
}

bool MOUSECTL::CmdMinRate()
{
	MouseControlAPI mouse_config_api;
	mouse_config_api.ResetMinRate(list_ids);
	return true;
}

void MOUSECTL::AddMessages()
{
	MSG_Add("PROGRAM_MOUSECTL_HELP_LONG",
	        "Manages physical and logical mice.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]mousectl[reset] [-all]\n"
	        "  [color=green]mousectl[reset] [color=white]INTERFACE[reset] -map [color=cyan]NAME[reset]\n"
	        "  [color=green]mousectl[reset] [color=white]INTERFACE[reset] [[color=white]INTERFACE[reset] ...] -map\n"
	        "  [color=green]mousectl[reset] [[color=white]INTERFACE[reset] ...] -unmap | -on | -off | -reset\n"
	        "  [color=green]mousectl[reset] [[color=white]INTERFACE[reset] ...] -s | -sx | -sy [sensitivity]\n"
	        "  [color=green]mousectl[reset] [[color=white]INTERFACE[reset] ...] -s sensitivity_x sensitivity_y\n"
	        "  [color=green]mousectl[reset] [[color=white]INTERFACE[reset] ...] -r [min_rate]\n"
	        "\n"
	        "Where:\n"
	        "  [color=white]INTERFACE[reset]      one of [color=white]DOS[reset], [color=white]PS/2[reset], [color=white]COM1[reset], [color=white]COM2[reset], [color=white]COM3[reset], [color=white]COM4[reset]\n"
	        "  -map -unmap    maps/unmaps physical mouse, honors DOS wildcards in [color=cyan]NAME[reset]\n"
	        "  -s -sx -sy     sets sensitivity / for x axis / for y axis, from -999 to +999\n"
	        "  -r             sets minimum mouse sampling rate\n"
	        "  -on -off       enables or disables mouse on the given interface\n"
	        "  -reset         restores all mouse settings from the configuration file\n"
	        "\n"
	        "Notes:\n"
	        "  If sensitivity or rate is omitted, it is reset to default value.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]mousectl[reset] [color=white]DOS[reset] [color=white]COM1[reset] -map    ; asks user to select mice for a two player game");

	MSG_Add("PROGRAM_MOUSECTL_SYNTAX_PATTERN",
	        "Incorrect syntax, only ASCII characters allowed in pattern.\n");
	MSG_Add("PROGRAM_MOUSECTL_SYNTAX_SENSITIVITY",
	        "Incorrect syntax, sensitivity needs to be in -999 to +999 range.\n");
	MSG_Add("PROGRAM_MOUSECTL_SYNTAX_DUPLICATED",
	        "Incorrect syntax, duplicated mouse interfaces.\n");
	MSG_Add("PROGRAM_MOUSECTL_SYNTAX_MIN_RATE",
	        "Incorrect syntax, sampling rate has to be one of:\n%s\n");

	MSG_Add("PROGRAM_MOUSECTL_MAPPING_NO_MOUSE",
	        "Mapping not available in no-mouse mode.\n");
	MSG_Add("PROGRAM_MOUSECTL_NO_INTERFACES",
	        "No mouse interfaces available.\n");
	MSG_Add("PROGRAM_MOUSECTL_MISSING_INTERFACES",
	        "Mouse interface not available.\n");
	MSG_Add("PROGRAM_MOUSECTL_NO_PHYSICAL_MICE",
	        "No physical mice detected.\n");
	MSG_Add("PROGRAM_MOUSECTL_NO_MATCH",
	        "No available mouse found matching the pattern.\n");

	MSG_Add("PROGRAM_MOUSECTL_TABLE_HEADER1",
	        "[color=white]Interface      Sensitivity      Rate (Hz)     Status[reset]");
	MSG_Add("PROGRAM_MOUSECTL_TABLE_LAYOUT1",
	        "[color=cyan]%-4s[reset]          X:%+.3d Y:%+.3d       %1s %3s       %s");

	MSG_Add("PROGRAM_MOUSECTL_TABLE_HEADER2",
	        "[color=white]Interface     Mouse Name[reset]");
	MSG_Add("PROGRAM_MOUSECTL_TABLE_LAYOUT2",
	        "[color=cyan]%-4s[reset]          %s");
	MSG_Add("PROGRAM_MOUSECTL_TABLE_LAYOUT2_UNMAPPED", "not mapped    %s");

	MSG_Add("PROGRAM_MOUSECTL_TABLE_STATUS_HOST", "uses system pointer");
	MSG_Add("PROGRAM_MOUSECTL_TABLE_STATUS_MAPPED", "mapped physical mouse");
	MSG_Add("PROGRAM_MOUSECTL_TABLE_STATUS_DISCONNECTED",
	        "[color=red]mapped mouse disconnected[reset]");
	MSG_Add("PROGRAM_MOUSECTL_TABLE_STATUS_DISABLED", "disabled");

	MSG_Add("PROGRAM_MOUSECTL_TABLE_HINT_RATE_COM",
	        "Sampling rates for mice on [color=cyan]COM[reset] interfaces are estimations only.");
	MSG_Add("PROGRAM_MOUSECTL_TABLE_HINT_RATE_MIN",
	        "Sampling rates with minimum value set are marked with '*'.");

	MSG_Add("PROGRAM_MOUSECTL_MAP_ADVICE",
	        "Click [color=white]left[reset] mouse button to map the physical mouse to the interface. Clicking\n"
	        "any other button cancels the mapping and assigns system pointer to all the\n"
	        "mouse interfaces.");
	MSG_Add("PROGRAM_MOUSECTL_MAP_CANCEL", "(mapping cancelled)");
	MSG_Add("PROGRAM_MOUSECTL_MAP_HINT",
	        "Seamless mouse integration is always disabled while mapping is in effect\n"
	        "and mapped mice always receive raw input events.");
}
