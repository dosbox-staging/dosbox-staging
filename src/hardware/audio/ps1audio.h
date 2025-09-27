// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PS1AUDIO_H
#define DOSBOX_PS1AUDIO_H

#include "config/setup.h"

void PS1AUDIO_AddConfigSection(Section* sec);

void PS1AUDIO_Init(Section* sec);
void PS1AUDIO_Destroy(Section* sec);

void PS1AUDIO_NotifySettingUpdated(SectionProp* section, const std::string& prop_name);

bool PS1AUDIO_IsEnabled();

void PS1DAC_NotifyLockMixer();
void PS1DAC_NotifyUnlockMixer();

#endif // DOSBOX_PS1AUDIO_H
