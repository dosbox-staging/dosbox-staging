/*
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

void  Mouse_EventMoved(int32_t x_rel, int32_t y_rel, int32_t x_abs, int32_t y_abs, bool is_captured);
void  Mouse_EventPressed(uint8_t idx);
void  Mouse_EventReleased(uint8_t idx);
void  Mouse_EventWheel(int32_t w_rel);

void  Mouse_SetSensitivity(int32_t sensitivity_x, int32_t sensitivity_y);
void  Mouse_NewScreenParams(uint16_t clip_x, uint16_t clip_y, uint16_t res_x, uint16_t res_y);

// ***************************************************************************
// Common structures, please only update via functions above
// ***************************************************************************

typedef struct {

    float  sensitivity_x = 0.3f;     // sensitivity, might depend on the GUI/GFX 
    float  sensitivity_y = 0.3f;     // for scaling all relative mouse movements

} MouseInfoConfig;

typedef struct {

    bool     fullscreen = false;
    uint16_t res_x      = 320;       // resolution to which guest image is scaled, excluding black borders
    uint16_t res_y      = 200;
    uint16_t clip_x     = 0;         // clipping = size of black border (one side)
    uint16_t clip_y     = 0;         // clipping value - size of black border (one side)

} MouseInfoVideo;

extern MouseInfoConfig mouse_config;
extern MouseInfoVideo  mouse_video;

// ***************************************************************************
// BIOS interface for PS/2 mouse
// ***************************************************************************

bool Mouse_SetPS2State(bool use);
void Mouse_ChangePS2Callback(uint16_t pseg, uint16_t pofs);

// ***************************************************************************
// DOS virtual mouse driver
// ***************************************************************************

void Mouse_BeforeNewVideoMode();
void Mouse_AfterNewVideoMode(bool setmode);

#endif
