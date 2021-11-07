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

#ifndef DOSBOX_VIDEO_H
#define DOSBOX_VIDEO_H

#include "types.h"

#define REDUCE_JOYSTICK_POLLING

typedef enum {
	GFX_CallBackReset,
	GFX_CallBackStop,
	GFX_CallBackRedraw
} GFX_CallBackFunctions_t;

typedef void (*GFX_CallBack_t)( GFX_CallBackFunctions_t function );

#define GFX_CAN_8   0x0001
#define GFX_CAN_15  0x0002
#define GFX_CAN_16  0x0004
#define GFX_CAN_32  0x0008

#define GFX_LOVE_8  0x0010
#define GFX_LOVE_15 0x0020
#define GFX_LOVE_16 0x0040
#define GFX_LOVE_32 0x0080

#define GFX_RGBONLY 0x0100
#define GFX_DBL_H   0x0200 /* double-width  flag */
#define GFX_DBL_W   0x0400 /* double-height flag */

#define GFX_SCALING		0x1000
#define GFX_HARDWARE	0x2000

#define GFX_CAN_RANDOM  0x4000 //If the interface can also do random access surface
#define GFX_UNITY_SCALE 0x8000 /* turn of all scaling in render.cpp */

// return code of:
// - true means event loop can keep running.
// - false means event loop wants to quit.
bool GFX_Events();

Bitu GFX_GetBestMode(Bitu flags);
int8_t GFX_GetDisplayRefreshRate();
Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue);
void GFX_SetShader(const char* src);
Bitu GFX_SetSize(int width, int height, Bitu flags,
                 double scalex, double scaley,
                 GFX_CallBack_t callback,
                 double pixel_aspect);

void GFX_ResetScreen(void);
void GFX_Start(void);
void GFX_Stop(void);
void GFX_SwitchFullScreen(void);
bool GFX_StartUpdate(uint8_t * &pixels, int &pitch);
void GFX_EndUpdate( const Bit16u *changedLines );
void GFX_GetSize(int &width, int &height, bool &fullscreen);
void GFX_LosingFocus(void);

#if defined (REDUCE_JOYSTICK_POLLING)
void MAPPER_UpdateJoysticks(void);
#endif

#endif
