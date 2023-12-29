/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include <memory>
#include <string_view>

// A class providing Lua with an API into the debugger needed to implement its own library.
class LuaDebugInterface {
public:
    virtual ~LuaDebugInterface() = default;
    virtual void WriteToConsole(std::string_view text) = 0;
};

class LuaInterpreter
{
public:
    static std::unique_ptr<LuaInterpreter> Create(LuaDebugInterface* debug_interface);

    virtual ~LuaInterpreter() = default;

    virtual bool RunCommand(std::string_view command) = 0;
};
