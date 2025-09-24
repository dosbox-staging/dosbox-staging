// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_GUS_H
#define DOSBOX_GUS_H

#include "config/config.h"

void GUS_AddConfigSection(const ConfigPtr& conf);
void GUS_Init();
void GUS_Destroy();

void GUS_NotifyLockMixer();
void GUS_NotifyUnlockMixer();

#endif // DOSBOX_GUS_H
