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

bool PS1AUDIO_IsEnabled();

bool SB_GetAddress(uint16_t &sbaddr, uint8_t &sbirq, uint8_t &sbdma);

// Sound Blaster and ESS configuration and initialisation
void SB_AddConfigSection(const ConfigPtr &conf);

// IBM Music Feature Card configuration and initialisation
void IMFC_AddConfigSection(const ConfigPtr& conf);

// Innovation SSI-2001 configuration and initialisation
void INNOVATION_AddConfigSection(const ConfigPtr &conf);

// Disk noise emulation configuration and initialisation
void DISKNOISE_AddConfigSection(const ConfigPtr& conf);

// Common lock notification calls used by the mixer
void PCSPEAKER_NotifyLockMixer();
void PCSPEAKER_NotifyUnlockMixer();

void PS1DAC_NotifyLockMixer();
void PS1DAC_NotifyUnlockMixer();

void SBLASTER_NotifyLockMixer();
void SBLASTER_NotifyUnlockMixer();

void REELMAGIC_NotifyLockMixer();
void REELMAGIC_NotifyUnlockMixer();

void LPTDAC_NotifyLockMixer();
void LPTDAC_NotifyUnlockMixer();

#endif
