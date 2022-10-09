/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022  The DOSBox Staging Team
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
#include "video.h"

#include <set>

CHECK_NARROWING();


void MOUSECTL::Run()
{
    if (HelpRequested()) {
        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_HELP_LONG"));
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
    std::vector<MouseInterfaceId> list_ids;
    if (!ParseInterfaces(params, list_ids) || !CheckInterfaces(list_ids))
        return false;

    auto param_equal = [&params](const size_t idx, const char *string)
    {
        if (idx >= params.size())
            return false;
        return (0 == strcasecmp(params[idx].c_str(), string));
    };

    // CmdShow
    if (list_ids.size() == 0 && params.size() == 0)
        return CmdShow(false);
    if (list_ids.size() == 0 && params.size() == 1)
        if (param_equal(0, "-all"))
            return CmdShow(true);

    // CmdMap - by supplied host mouse name
    if (list_ids.size() == 1 && params.size() == 2)
        if (param_equal(0, "-map"))
            return CmdMap(list_ids[0], params[1]);

    // CmdMap - interactive
    if (list_ids.size() >= 1 && params.size() == 1)
        if (param_equal(0, "-map"))
            return CmdMap(list_ids);

    // CmdUnmap / CmdOnOff / CmdReset / CmdSensitivity / CmdMinRate
    if (params.size() == 1) {
        if (param_equal(0, "-unmap"))
            return CmdUnMap(list_ids);
        if (param_equal(0, "-on"))
            return CmdOnOff(list_ids, true);
        if (param_equal(0, "-off"))
            return CmdOnOff(list_ids, false);
        if (param_equal(0, "-reset"))
            return CmdReset(list_ids);
        if (param_equal(0, "-s"))
            return CmdSensitivity(list_ids);
        if (param_equal(0, "-sx"))
            return CmdSensitivityX(list_ids);
        if (param_equal(0, "-sy"))
            return CmdSensitivityY(list_ids);
        if (param_equal(0, "-r"))
            return CmdMinRate(list_ids);
    }

    // CmdSensitivity / CmdMinRate with a non-default value
    if (params.size() == 2) {
        if (param_equal(0, "-r"))
            return CmdMinRate(list_ids, params[1]);

        const auto value = std::atoi(params[1].c_str());
        if (value < -99 || value > 99) {
            WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_SYNTAX_SENSITIVITY"));
            return false;
        }
        if (param_equal(0, "-s"))
            return CmdSensitivity(list_ids, static_cast<int8_t>(value));
        if (param_equal(0, "-sx"))
            return CmdSensitivityX(list_ids, static_cast<int8_t>(value));
        if (param_equal(0, "-sy"))
            return CmdSensitivityY(list_ids, static_cast<int8_t>(value));
    }

    WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_SYNTAX"));
    return false;
}

bool MOUSECTL::ParseInterfaces(std::vector<std::string> &params,
                               std::vector<MouseInterfaceId> &list_ids)
{
    while (params.size()) {
        auto compare = [&params](const char *string)
        {
            return (0 == strcasecmp(params[0].c_str(), string));
        };

        // Check if first parameter is one of supported mouse interfaces
        if (compare("DOS"))
            list_ids.push_back(MouseInterfaceId::DOS);
        else if (compare("PS/2"))
            list_ids.push_back(MouseInterfaceId::PS2);
        else if (compare("PS2"))
            list_ids.push_back(MouseInterfaceId::PS2);
        else if (compare("COM1"))
            list_ids.push_back(MouseInterfaceId::COM1);
        else if (compare("COM2"))
            list_ids.push_back(MouseInterfaceId::COM2);
        else if (compare("COM3"))
            list_ids.push_back(MouseInterfaceId::COM3);
        else if (compare("COM4"))
            list_ids.push_back(MouseInterfaceId::COM4);
        else
            break; // It isn't - end of interface list

        // First parameter is consummed, remove it
        params.erase(params.begin());
    }

    // Check that all interfaces are unique
    std::set<MouseInterfaceId> tmp(list_ids.begin(), list_ids.end());
    if (list_ids.size() != tmp.size()) {
        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_SYNTAX_DUPLICATED"));
        return false;
    }

    return true;
}

bool MOUSECTL::CheckInterfaces(const std::vector<MouseInterfaceId> &list_ids)
{
    if (!MouseConfigAPI::CheckInterfaces(list_ids))
    {
        if (list_ids.empty())
            WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_NO_INTERFACES"));
        else
            WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_MISSING_INTERFACES"));
        return false;
    }

    return true;
}

const char *MOUSECTL::GetInterfaceStr(const MouseInterfaceId interface_id) const
{
    switch (interface_id) {
    case MouseInterfaceId::DOS:  return "DOS";
    case MouseInterfaceId::PS2:  return "PS/2";
    case MouseInterfaceId::COM1: return "COM1";
    case MouseInterfaceId::COM2: return "COM2";
    case MouseInterfaceId::COM3: return "COM3";
    case MouseInterfaceId::COM4: return "COM4";
    case MouseInterfaceId::None: return "";
    default:
        assert(false); // missing implementation
        return nullptr;
    }
}

const char *MOUSECTL::GetMapStatusStr(const MouseMapStatus map_status) const
{
    switch (map_status) {
    case MouseMapStatus::HostPointer:
        return MSG_Get("SHELL_CMD_MOUSECTL_TABLE_STATUS_HOST");
    case MouseMapStatus::Mapped:
        return MSG_Get("SHELL_CMD_MOUSECTL_TABLE_STATUS_MAPPED");
    case MouseMapStatus::Disconnected:
        return MSG_Get("SHELL_CMD_MOUSECTL_TABLE_STATUS_DISCONNECTED");
    case MouseMapStatus::Disabled:
        return MSG_Get("SHELL_CMD_MOUSECTL_TABLE_STATUS_DISABLED");
    default:
        assert(false); // missing implementation
        return nullptr;
    }
}

bool MOUSECTL::CmdShow(const bool show_all)
{
    MouseConfigAPI mouse_config_api;
    const auto info_interfaces = mouse_config_api.GetInfoInterfaces();

    bool show_mapped   = false;
    bool hint_rate_com = false;
    bool hint_rate_min = false;

    // Display emulated interface list
    WriteOut("\n");
    WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_TABLE_HEADER1"));
    WriteOut("\n");
    for (const auto &entry : info_interfaces) {
        if (!entry.IsEmulated())
            continue;
        const auto interface_id = entry.GetInterfaceId();
        const auto rate_hz = entry.GetRate();

        if (entry.GetMinRate())
            hint_rate_min = true;

        if (interface_id == MouseInterfaceId::COM1 ||
            interface_id == MouseInterfaceId::COM2 ||
            interface_id == MouseInterfaceId::COM3 ||
            interface_id == MouseInterfaceId::COM4)
            hint_rate_com = true;

        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_TABLE_LAYOUT1"),
                 GetInterfaceStr(interface_id),
                 entry.GetSensitivityX(),
                 entry.GetSensitivityY(),
                 hint_rate_min ? "*" : "",
                 rate_hz ? std::to_string(rate_hz).c_str() : "-",
                 convert_ansi_markup(GetMapStatusStr(entry.GetMapStatus())).c_str());
        WriteOut("\n");

        if (entry.GetMapStatus() == MouseMapStatus::Mapped)
            show_mapped = true;
    }
    WriteOut("\n");

    const bool hint = hint_rate_com || hint_rate_min;
    if (hint)
    {
        if (hint_rate_com) {
            WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_TABLE_HINT_RATE_COM"));
            WriteOut("\n");
        }
        if (hint_rate_min) {
            WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_TABLE_HINT_RATE_MIN"));
            WriteOut("\n");
        }
        WriteOut("\n");
    }

    if (!show_all && !show_mapped)
        return true;

    const auto info_physical = mouse_config_api.GetInfoPhysical();
    if (info_physical.empty()) {
        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_NO_PHYSICAL_MICE"));
        WriteOut("\n\n");       
        return true;
    }

    if (hint)
        WriteOut("\n");
    WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_TABLE_HEADER2"));
    WriteOut("\n");

    // Display physical mice mapped to some interface
    for (const auto &entry : info_interfaces) {
        if (!entry.IsMapped() || entry.IsMappedDeviceDisconnected())
            continue;
        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_TABLE_LAYOUT2"),
                 GetInterfaceStr(entry.GetInterfaceId()),
                 entry.GetMappedDeviceName().c_str());
        WriteOut("\n");
    }

    if (!show_all)
        return true;

    // Display physical mice not mapped to any interface
    for (const auto &entry : info_physical) {
        if (entry.IsMapped() || entry.IsDeviceDisconnected())
            continue;
        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_TABLE_LAYOUT2_UNMAPPED"),
                 entry.GetDeviceName().c_str());
        WriteOut("\n");
    }
    WriteOut("\n");

    return true;
}

void MOUSECTL::FinalizeMapping()
{
    WriteOut("\n");
    WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_MAP_HINT"));
    WriteOut("\n\n");

    GFX_MouseCaptureAfterMapping();    
}

bool MOUSECTL::CmdMap(const MouseInterfaceId interface_id,
                      const std::string &pattern)
{
    std::regex regex;
    if (!MouseConfigAPI::PatternToRegex(pattern, regex)) {
        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_SYNTAX_PATTERN"));
        return false;
    }

    if (MouseConfigAPI::IsNoMouseMode()) {
        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_MAPPING_NO_MOUSE"));
        return false;
    }

    MouseConfigAPI mouse_config_api;
    if (!mouse_config_api.Map(interface_id, regex)) {
        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_NO_MATCH"));
        return false;
    }

    FinalizeMapping();
    return true;
}

bool MOUSECTL::CmdMap(const std::vector<MouseInterfaceId> &list_ids)
{
    assert(list_ids.size() >= 1);

    if (MouseConfigAPI::IsNoMouseMode()) {
        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_MAPPING_NO_MOUSE"));
        return false;
    }

    MouseConfigAPI mouse_config_api;
    const auto info_physical = mouse_config_api.GetInfoPhysical();

    if (info_physical.empty()) {
        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_NO_PHYSICAL_MICE"));
        WriteOut("\n\n");
        return false;
    }

    // Clear the current mapping before starting interactive mapper
    std::vector<MouseInterfaceId> empty;
    mouse_config_api.UnMap(empty);

    WriteOut("\n");
    WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_MAP_ADVICE"));
    WriteOut("\n\n");

    for (const auto &interface_id : list_ids) {
        WriteOut(convert_ansi_markup("[color=cyan]%-4s[reset]   ?").c_str(),
                 GetInterfaceStr(interface_id));

        uint8_t device_id = 0;
        if (!mouse_config_api.ProbeForMapping(device_id)) {
            mouse_config_api.UnMap(empty);
            WriteOut("\b");
            WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_MAP_CANCEL"));
            WriteOut("\n\n");
            return false;
        }

        WriteOut("\b");
        WriteOut(info_physical[device_id].GetDeviceName().c_str());
        WriteOut("\n");
        mouse_config_api.Map(interface_id, device_id);
    }

    FinalizeMapping();
    return true;
}

bool MOUSECTL::CmdUnMap(const std::vector<MouseInterfaceId> &list_ids)
{
    MouseConfigAPI mouse_config_api;
    mouse_config_api.UnMap(list_ids);
    return true;
}

bool MOUSECTL::CmdOnOff(const std::vector<MouseInterfaceId> &list_ids,
                        const bool enable)
{
    MouseConfigAPI mouse_config_api;
    mouse_config_api.OnOff(list_ids, enable);
    return true;
}

bool MOUSECTL::CmdReset(const std::vector<MouseInterfaceId> &list_ids)
{
    MouseConfigAPI mouse_config_api;
    mouse_config_api.Reset(list_ids);
    return true;
}

bool MOUSECTL::CmdSensitivity(const std::vector<MouseInterfaceId> &list_ids,
                              const int8_t value)
{
    MouseConfigAPI mouse_config_api;
    mouse_config_api.SetSensitivity(list_ids, value, value);
    return true;
}

bool MOUSECTL::CmdSensitivityX(const std::vector<MouseInterfaceId> &list_ids,
                               const int8_t value)
{
    MouseConfigAPI mouse_config_api;
    mouse_config_api.SetSensitivityX(list_ids, value);
    return true;
}

bool MOUSECTL::CmdSensitivityY(const std::vector<MouseInterfaceId> &list_ids,
                               const int8_t value)
{
    MouseConfigAPI mouse_config_api;
    mouse_config_api.SetSensitivityY(list_ids, value);
    return true;
}

bool MOUSECTL::CmdSensitivity(const std::vector<MouseInterfaceId> &list_ids)
{
    MouseConfigAPI mouse_config_api;
    mouse_config_api.ResetSensitivity(list_ids);
    return true;
}

bool MOUSECTL::CmdSensitivityX(const std::vector<MouseInterfaceId> &list_ids)
{
    MouseConfigAPI mouse_config_api;
    mouse_config_api.ResetSensitivityX(list_ids);
    return true;
}

bool MOUSECTL::CmdSensitivityY(const std::vector<MouseInterfaceId> &list_ids)
{
    MouseConfigAPI mouse_config_api;
    mouse_config_api.ResetSensitivityY(list_ids);
    return true;
}

bool MOUSECTL::CmdMinRate(const std::vector<MouseInterfaceId> &list_ids,
                          const std::string &param)
{
    const auto &valid_list = MouseConfigAPI::GetValidMinRateList();
    const auto &valid_str  = MouseConfigAPI::GetValidMinRateStr();

    const auto tmp = std::atoi(param.c_str());
    if (tmp < 0 || tmp > UINT16_MAX) {
        // Parameter way out of range
        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_SYNTAX_MIN_RATE"), valid_str.c_str());
        return false;
    }

    uint16_t value_hz = static_cast<uint16_t>(tmp);
    if (std::find(valid_list.begin(), valid_list.end(), value_hz) == valid_list.end()) {
        // Parameter not in the list of allowed values
        WriteOut(MSG_Get("SHELL_CMD_MOUSECTL_SYNTAX_MIN_RATE"), valid_str.c_str());
        return false;
    }

    MouseConfigAPI mouse_config_api;
    mouse_config_api.SetMinRate(list_ids, value_hz);
    return true;
}

bool MOUSECTL::CmdMinRate(const std::vector<MouseInterfaceId> &list_ids)
{
    MouseConfigAPI mouse_config_api;
    mouse_config_api.ResetMinRate(list_ids);
    return true;
}

void MOUSECTL::AddMessages()
{
    MSG_Add("SHELL_CMD_MOUSECTL_HELP_LONG",
            "Manages physical and logical mice.\n"
            "\n"
            "Usage:\n"
            "  [color=green]mousectl[reset] [-all]\n"
            "  [color=green]mousectl[reset] [color=white]INTERFACE[reset] -map name\n"
            "  [color=green]mousectl[reset] [color=white]INTERFACE[reset] [[color=white]INTERFACE[reset] ...] -map\n"
            "  [color=green]mousectl[reset] [[color=white]INTERFACE[reset] ...] -unmap | -on | -off | -reset\n"
            "  [color=green]mousectl[reset] [[color=white]INTERFACE[reset] ...] -s | -sx | -sy [sensitivity]\n"
            "  [color=green]mousectl[reset] [[color=white]INTERFACE[reset] ...] -r [min_rate]\n"
            "\n"
            "Where:\n"
            "  [color=white]INTERFACE[reset]      one of [color=white]DOS[reset], [color=white]PS/2[reset], [color=white]COM1[reset], [color=white]COM2[reset], [color=white]COM3[reset], [color=white]COM4[reset]\n"
            "  -map -unmap    maps/unmaps physical mouse, honors DOS wildcards in name\n"
            "  -s -sx -sy     sets sensitivity / for x axis / for y axis, from -99 to +99\n"
            "  -r             sets minimum mouse sampling rate\n"
            "  -on -off       enables or disables mouse on the given interface\n"
            "  -reset         restores all mouse settings from the configuration file\n"
            "\n"
            "Notes:\n"
            "  - if host mouse name is omitted, it is mapped in interactive mode\n"
            "  - if sensitivity or rate is omitted, it is reset to default value\n"
            "\n"
            "Examples:\n"
            "  [color=green]mousectl[reset] [color=white]DOS[reset] [color=white]COM1[reset] -map    ; asks user to select mice for a two player game");

    MSG_Add("SHELL_CMD_MOUSECTL_SYNTAX",             "Wrong command syntax.");
    MSG_Add("SHELL_CMD_MOUSECTL_SYNTAX_PATTERN",     "Wrong syntax, only ASCII characters allowed in pattern.");
    MSG_Add("SHELL_CMD_MOUSECTL_SYNTAX_SENSITIVITY", "Wrong syntax, sensitivity needs to be in -99 to +99 range.");
    MSG_Add("SHELL_CMD_MOUSECTL_SYNTAX_DUPLICATED",  "Wrong syntax, duplicated mouse interfaces.");
    MSG_Add("SHELL_CMD_MOUSECTL_SYNTAX_MIN_RATE",    "Wrong syntax, sampling rate has to be one of:\n%s");

    MSG_Add("SHELL_CMD_MOUSECTL_MAPPING_NO_MOUSE",   "Mapping not available in NoMouse mode.");
    MSG_Add("SHELL_CMD_MOUSECTL_NO_INTERFACES",      "No mouse interfaces available.");
    MSG_Add("SHELL_CMD_MOUSECTL_MISSING_INTERFACES", "Mouse interface not available.");
    MSG_Add("SHELL_CMD_MOUSECTL_NO_PHYSICAL_MICE",   "No physical mice detected.");
    MSG_Add("SHELL_CMD_MOUSECTL_NO_MATCH",           "No available mouse matching the pattern found.");

    MSG_Add("SHELL_CMD_MOUSECTL_TABLE_HEADER1", "[color=white]Interface     Sensitivity     Rate (Hz)     Status[reset]");
    MSG_Add("SHELL_CMD_MOUSECTL_TABLE_LAYOUT1", "[color=cyan]%-4s[reset]          X:%+.2d Y:%+.2d       %1s %3s       %s");

    MSG_Add("SHELL_CMD_MOUSECTL_TABLE_HEADER2", "[color=white]Interface     Mouse Name[reset]");
    MSG_Add("SHELL_CMD_MOUSECTL_TABLE_LAYOUT2", "[color=cyan]%-4s[reset]          %s");
    MSG_Add("SHELL_CMD_MOUSECTL_TABLE_LAYOUT2_UNMAPPED",  "not mapped    %s");

    MSG_Add("SHELL_CMD_MOUSECTL_TABLE_STATUS_HOST",         "uses system pointer");
    MSG_Add("SHELL_CMD_MOUSECTL_TABLE_STATUS_MAPPED",       "mapped physical mouse");
    MSG_Add("SHELL_CMD_MOUSECTL_TABLE_STATUS_DISCONNECTED", "[color=red]mapped mouse disconnected[reset]");
    MSG_Add("SHELL_CMD_MOUSECTL_TABLE_STATUS_DISABLED",     "disabled");

    MSG_Add("SHELL_CMD_MOUSECTL_TABLE_HINT_RATE_COM", "Sampling rates for mice on [color=cyan]COM[reset] interfaces are estimations only.");
    MSG_Add("SHELL_CMD_MOUSECTL_TABLE_HINT_RATE_MIN", "Sampling rates with minimum value set are marked with '*'.");

    MSG_Add("SHELL_CMD_MOUSECTL_MAP_ADVICE",
            "Click [color=white]left[reset] mouse button to map the physical mouse to the interface. Clicking\n"
            "any other button cancels the mapping and assigns system pointer to all the\n"
            "mouse interfaces.");
    MSG_Add("SHELL_CMD_MOUSECTL_MAP_CANCEL", "(mapping cancelled)");
    MSG_Add("SHELL_CMD_MOUSECTL_MAP_HINT",
            "Mouse captured. Seamless mouse integration is always disabled while mapping is\n"
            "in effect and mapped mice always receive raw input events.");
}
