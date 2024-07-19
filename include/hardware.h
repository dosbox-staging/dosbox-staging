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

#include "control.h"
#include "inout.h"

class Section;

enum class OplMode { None, Opl2, DualOpl2, Opl3, Opl3Gold, Esfm };

void OPL_Init(Section *sec, OplMode mode);
void OPL_ShutDown(Section* sec = nullptr);

void CMS_Init(Section *sec);
void CMS_ShutDown(Section* sec = nullptr);

bool PS1AUDIO_IsEnabled();

bool SB_GetAddress(uint16_t &sbaddr, uint8_t &sbirq, uint8_t &sbdma);

// Sound Blaster and ESS configuration and initialisation
void SB_AddConfigSection(const ConfigPtr &conf);

// CMS/Game Blaster, OPL, and ESFM configuration and initialisation
void OPL_AddConfigSettings(const ConfigPtr &conf);

bool TANDYSOUND_GetAddress(Bitu& tsaddr, Bitu& tsirq, Bitu& tsdma);

// Gravis UltraSound configuration and initialisation
void GUS_AddConfigSection(const ConfigPtr &conf);

// The Gravis UltraSound mirrors the AdLib's command register (written to the
// command port 388h) on its own port 20Ah. This function passes the command
// onto the GUS (if available)
void GUS_MirrorAdLibCommandPortWrite(const io_port_t port, const io_val_t reg_value,
                                     const io_width_t = io_width_t::byte);

// IBM Music Feature Card configuration and initialisation
void IMFC_AddConfigSection(const ConfigPtr& conf);

// Innovation SSI-2001 configuration and initialisation
void INNOVATION_AddConfigSection(const ConfigPtr &conf);

#endif
