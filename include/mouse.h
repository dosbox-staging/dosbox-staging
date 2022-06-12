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

void MOUSE_EventMoved(const int16_t x_rel, const int16_t y_rel,
                      const uint16_t x_abs, const uint16_t y_abs,
                      const bool is_captured);
void MOUSE_EventPressed(const uint8_t idx);
void MOUSE_EventReleased(const uint8_t idx);
void MOUSE_EventWheel(const int16_t w_rel);

void MOUSE_SetSensitivity(const int32_t sensitivity_x, const int32_t sensitivity_y);
void MOUSE_NewScreenParams(const uint16_t clip_x, const uint16_t clip_y,
                           const uint16_t res_x, const uint16_t res_y,
                           const bool fullscreen,
                           const uint16_t x_abs, const uint16_t y_abs);
void MOUSE_NotifyMovedFake(); // for VMware protocol support

// ***************************************************************************
// Common structures, please only update via functions above
// ***************************************************************************

class MouseInfoConfig {
public:
	float sensitivity_x = 0.3f; // sensitivity, might depend on the GUI/GFX
	float sensitivity_y = 0.3f; // for scaling all relative mouse movements
};

class MouseInfoVideo {
public:
	bool fullscreen = true;
	uint16_t res_x  = 640; // resolution to which guest image is scaled,
	                       // excluding black borders
	uint16_t res_y  = 400;
	uint16_t clip_x = 0; // clipping = size of black border (one side)
	uint16_t clip_y = 0; // clipping value - size of black border (one side)
};

extern MouseInfoConfig mouse_config;
extern MouseInfoVideo  mouse_video;
extern bool            mouse_vmware; // true = VMware driver took over the mouse

// ***************************************************************************
// Serial mouse
// ***************************************************************************

class CSerialMouse;

void MOUSESERIAL_RegisterListener(CSerialMouse &listener);
void MOUSESERIAL_UnRegisterListener(CSerialMouse &listener);

// - needs relative movements
// - understands up to 3 buttons
// - needs index of button which changed state

void MOUSESERIAL_NotifyMoved(const int16_t x_rel, const int16_t y_rel);
void MOUSESERIAL_NotifyPressed(const uint8_t buttons_12S, const uint8_t idx);
void MOUSESERIAL_NotifyReleased(const uint8_t buttons_12S, const uint8_t idx);
void MOUSESERIAL_NotifyWheel(const int16_t w_rel);

// ***************************************************************************
// BIOS interface for PS/2 mouse
// ***************************************************************************

bool MOUSE_SetPS2State(const bool use);
void MOUSE_ChangePS2Callback(const uint16_t pseg, const uint16_t pofs);

// ***************************************************************************
// VMware protocol extension for PS/2 mouse
// ***************************************************************************

void MOUSEVMWARE_Init();
void MOUSEVMWARE_NewScreenParams(const uint16_t x_abs, const uint16_t y_abs);

// - needs absolute mouse position
// - understands up to 3 buttons

bool MOUSEVMWARE_NotifyMoved(const uint16_t x_abs, const uint16_t y_abs);
void MOUSEVMWARE_NotifyPressedReleased(const uint8_t buttons_12S);
void MOUSEVMWARE_NotifyWheel(const int16_t w_rel);

// ***************************************************************************
// DOS virtual mouse driver
// ***************************************************************************

void MOUSE_BeforeNewVideoMode();
void MOUSE_AfterNewVideoMode(const bool setmode);

#endif
