// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_VIRTUALBOX_H
#define DOSBOX_VIRTUALBOX_H

#include <string>

#include "config/setup.h"

void VIRTUALBOX_Init(Section* sec);

void VIRTUALBOX_NotifySettingUpdated(Section* sec, const std::string& prop_name);

void VIRTUALBOX_NotifyBooting();

#endif // DOSBOX_VIRTUALBOX_H
