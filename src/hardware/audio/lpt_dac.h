// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_LPT_DAC_H
#define DOSBOX_LPT_DAC_H

#include "config/setup.h"

void LPT_DAC_AddConfigSection(Section* sec);

void LPT_DAC_Init(Section* sec);
void LPT_DAC_Destroy(Section* sec);

void LPT_DAC_NotifySettingUpdated(SectionProp* section, const std::string& prop_name);

void LPTDAC_NotifyLockMixer();
void LPTDAC_NotifyUnlockMixer();

#endif // DOSBOX_LPT_DAC_H
