// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_TANDY_SOUND_H
#define DOSBOX_TANDY_SOUND_H

#include "config/setup.h"
#include "dosbox.h"

void TANDYSOUND_AddConfigSection(Section* sec);

void TANDYSOUND_Destroy(Section* sec);

bool TANDYSOUND_GetAddress(Bitu& tsaddr, Bitu& tsirq, Bitu& tsdma);

void TANDYDAC_NotifyLockMixer();
void TANDYDAC_NotifyUnlockMixer();

#endif // DOSBOX_TANDY_SOUND_H
