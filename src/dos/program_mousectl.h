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
    void Run();

private:

    bool ParseAndRun();
    bool ParseInterfaces(std::vector<std::string> &params);
    bool CheckInterfaces();
    void FinalizeMapping();

    const char *GetInterfaceStr(const MouseInterfaceId interface_id) const;
    const char *GetMapStatusStr(const MouseMapStatus map_status) const;

    // Methods below exectute specific commands requested by the user,
    // they return 'true' if DOS error code should indicate success,
    // and 'false' if we should report a failure
    bool CmdShow(const bool show_all);
    bool CmdMap(const MouseInterfaceId interface_id,
                const std::string &pattern);
    bool CmdMap();
    bool CmdUnMap();
    bool CmdOnOff(const bool enable);
    bool CmdReset();
    bool CmdSensitivity(const int8_t value);
    bool CmdSensitivityX(const int8_t value);
    bool CmdSensitivityY(const int8_t value);
    bool CmdSensitivity();
    bool CmdSensitivityX();
    bool CmdSensitivityY();
    bool CmdMinRate(const std::string &param);
    bool CmdMinRate();

    void AddMessages();

    std::vector<MouseInterfaceId> list_ids = {};
};

#endif
