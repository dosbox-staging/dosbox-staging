// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_VMWARE_H
#define DOSBOX_VMWARE_H

#include <string>

#include "config/setup.h"

void VMWARE_Init(Section* sec);

void VMWARE_NotifySettingUpdated(Section* sec, const std::string& prop_name);

void VMWARE_NotifyBooting();
void VMWARE_NotifyProgramName(const std::string& segment_name);

#endif // DOSBOX_VMWARE_H
