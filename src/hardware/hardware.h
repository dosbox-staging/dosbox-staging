// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_HARDWARE_H
#define DOSBOX_HARDWARE_H

#include "dosbox.h"

#include <cstdio>
#include <string>

#include "config/config.h"
#include "hardware/port.h"

class Section;

// IBM Music Feature Card configuration and initialisation
void IMFC_AddConfigSection(const ConfigPtr& conf);

// Innovation SSI-2001 configuration and initialisation
void INNOVATION_AddConfigSection(const ConfigPtr &conf);

void LPTDAC_NotifyLockMixer();
void LPTDAC_NotifyUnlockMixer();

#endif
