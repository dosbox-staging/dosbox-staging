/*
 *  Copyright (C) 2002  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __VIDEO_H
#define __VIDEO_H


typedef void (* GFX_ModeCallBack)(Bitu width,Bitu height,Bitu bpp,Bitu pitch,Bitu flags);

struct GFX_PalEntry {
	Bit8u r;
	Bit8u g;
	Bit8u b;
	Bit8u unused;
};

#define GFX_FIXED_BPP	0x01
#define GFX_RESIZEABLE	0x02

#define MODE_SET 0x01
#define MODE_FULLSCREEN 0x02
#define MODE_RESIZE 0x04

void GFX_Events(void);
void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries);

Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue);
void GFX_SetSize(Bitu width,Bitu height,Bitu bpp,Bitu flags,GFX_ModeCallBack callback);

void GFX_Start(void);
void GFX_Stop(void);
void GFX_SwitchFullScreen(void);

void * GFX_StartUpdate(void);
void GFX_EndUpdate(void);

#endif

