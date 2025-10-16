// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_VMWARE_H
#define DOSBOX_VMWARE_H

#include <string>

#include "config/setup.h"

void VMWARE_Init();
void VMWARE_Destroy();

void VMWARE_NotifyBooting();
void VMWARE_NotifyProgramName(const std::string& segment_name);

// Returns true if Intel 8042 port read should be taken over by the VMware API
bool VMWARE_I8042_ReadTakeover();

// Intel 8042 port read handlers
uint32_t VMWARE_I8042_ReadStatusRegister();
uint32_t VMWARE_I8042_ReadDataPort();

// Intel 8042 port write handler, returns true if port write has been taken over
// by the VMware API
bool VMWARE_I8042_WriteCommandPort(const uint32_t value);

#endif // DOSBOX_VMWARE_H
