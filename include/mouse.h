/*
 *  Copyright (C) 2022       The DOSBox Staging Team
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

#ifndef DOSBOX_MOUSE_H
#define DOSBOX_MOUSE_H

#include "dosbox.h"

// ***************************************************************************
// Notifications from external subsystems - all should go via these methods
// ***************************************************************************

void MOUSE_EventMoved(const float x_rel, const float y_rel,
                      const uint16_t x_abs, const uint16_t y_abs);
void MOUSE_EventPressed(const uint8_t idx);
void MOUSE_EventReleased(const uint8_t idx);
void MOUSE_EventWheel(const int16_t w_rel);

void MOUSE_SetConfig(const bool raw_input);
void MOUSE_NewScreenParams(const uint16_t clip_x, const uint16_t clip_y,
                           const uint16_t res_x, const uint16_t res_y,
                           const bool fullscreen, const uint16_t x_abs,
                           const uint16_t y_abs);

// ***************************************************************************
// Common structures and variables
// ***************************************************************************

// If driver with seamless pointer support is running
extern bool mouse_seamless_driver;
// Suggestion to GUI to show host pointer despite other conditions
extern bool mouse_suggest_show; // TODO: use this information

// ***************************************************************************
// Serial mouse
// ***************************************************************************

class CSerialMouse;

void MOUSESERIAL_RegisterListener(CSerialMouse &listener);
void MOUSESERIAL_UnRegisterListener(CSerialMouse &listener);

// ***************************************************************************
// BIOS mouse interface for PS/2 mouse
// ***************************************************************************

bool MOUSEBIOS_SetState(const bool use);
void MOUSEBIOS_SetCallback(const uint16_t pseg, const uint16_t pofs);
void MOUSEBIOS_Reset();
bool MOUSEBIOS_SetPacketSize(const uint8_t packet_size);
bool MOUSEBIOS_SetSampleRate(const uint8_t rate_id);
void MOUSEBIOS_SetScaling21(const bool enable);
bool MOUSEBIOS_SetResolution(const uint8_t res_id);
uint8_t MOUSEBIOS_GetType();
uint8_t MOUSEBIOS_GetStatus();
uint8_t MOUSEBIOS_GetResolution();
uint8_t MOUSEBIOS_GetSampleRate();

// ***************************************************************************
// DOS mouse driver
// ***************************************************************************

void MOUSEDOS_BeforeNewVideoMode();
void MOUSEDOS_AfterNewVideoMode(const bool setmode);

#endif // DOSBOX_MOUSE_H
