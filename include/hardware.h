/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_HARDWARE_H
#define DOSBOX_HARDWARE_H

#include "dosbox.h"

#include <cstdio>
#include <string>

class Section;

enum class OplMode { None, Opl2, DualOpl2, Opl3, Opl3Gold, Esfm };

void OPL_Init(Section *sec, OplMode mode);
void OPL_ShutDown(Section* sec = nullptr);

void CMS_Init(Section *sec);
void CMS_ShutDown(Section* sec = nullptr);

bool PS1AUDIO_IsEnabled();
bool SB_GetAddress(uint16_t &sbaddr, uint8_t &sbirq, uint8_t &sbdma);

bool TANDYSOUND_GetAddress(Bitu& tsaddr, Bitu& tsirq, Bitu& tsdma);

extern uint8_t adlib_commandreg;

// Gravis UltraSound configuration and initialization
void GUS_AddConfigSection(const config_ptr_t &conf);

// IBM Music Feature Card configuration and initialization
void IMFC_AddConfigSection(const config_ptr_t& conf);

// Innovation SSI-2001 configuration and initialization
void INNOVATION_AddConfigSection(const config_ptr_t &conf);

#endif
