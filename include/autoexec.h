/*
 *  Copyright (C) 2020-2025  The DOSBox Staging Team
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

#ifndef DOSBOX_AUTOEXEC_H
#define DOSBOX_AUTOEXEC_H

#include "setup.h"

#include <string>

// Creates or refreshes the AUTOEXEC.BAT file on the emulated Z: drive.
void AUTOEXEC_RefreshFile();

// Adds/updates environment variable to the AUTOEXEC.BAT file. Empty value
// removes the variable. If a shell is already running, it the environment is
// updated accordingly.
// The 'name' and 'value' have to be a printable, 7-bit ASCII variables.
void AUTOEXEC_SetVariable(const std::string& name, const std::string& value);

void AUTOEXEC_Init(Section* sec);

#endif
