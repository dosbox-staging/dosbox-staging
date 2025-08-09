// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_AUTOEXEC_H
#define DOSBOX_AUTOEXEC_H

#include "config/setup.h"

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
