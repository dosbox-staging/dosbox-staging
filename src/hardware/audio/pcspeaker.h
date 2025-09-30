// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PCSPEAKER_H
#define DOSBOX_PCSPEAKER_H

#include "config/setup.h"

void PCSPEAKER_AddConfigSection(Section* sec);

void PCSPEAKER_Init(SectionProp& sec);
void PCSPEAKER_Destroy();

void PCSPEAKER_NotifySettingUpdated(SectionProp& section,
                                    const std::string& prop_name);

void PCSPEAKER_NotifyLockMixer();
void PCSPEAKER_NotifyUnlockMixer();

#endif // DOSBOX_PCSPEAKER_H
