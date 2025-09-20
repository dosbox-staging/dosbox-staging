// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SBLASTER_H
#define DOSBOX_SBLASTER_H

#include "config/setup.h"

bool SB_GetAddress(uint16_t &sbaddr, uint8_t &sbirq, uint8_t &sbdma);
void SB_AddConfigSection(const ConfigPtr &conf);

void SBLASTER_NotifyLockMixer();
void SBLASTER_NotifyUnlockMixer();

#endif // DOSBOX_SBLASTER_H
