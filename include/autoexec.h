/*
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
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

// Registers the AUTOEXEC.BAT file on the emulated Z: drive.
// From now on, the only changes to the file content which became visible on the
// guest side, are DOS code page changes.
// TODO: change this, every environment variable modification or [autoexec]
// modification by CONFIG.COM command should be reflected; this will require
// careful checking of our COMMAND.COM iomplementattion in order not break
// anything when the change happens during AUTOEXEC.BAT execution.
void AUTOEXEC_RegisterFile();

// Notify, that DOS display code page has changed, and the AUTOEXEC.BAT content
// might need to be refreshed from the original (internal) UTF-8 version.
void AUTOEXEC_NotifyNewCodePage();

// Adds/updates environment variable to the AUTOEXEC.BAT file. Empty value
// removes the variable. If a shell is already running, it the environment is
// updated accordingly.
// The 'name' and 'value' have to be a printable, 7-bit ASCII variables.
void AUTOEXEC_SetVariable(const std::string& name, const std::string& value);

void AUTOEXEC_Init(Section* sec);

#endif
