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


typedef void (GFX_DrawHandler)(Bit8u * vidstart);
/* Used to reply to the renderer what size to set */
typedef void (GFX_ResizeHandler)(Bitu * width,Bitu * height);

struct GFX_PalEntry {
	Bit8u r;
	Bit8u g;
	Bit8u b;
	Bit8u unused;
};

struct GFX_Info {
	Bitu width,height,bpp,pitch;
};

extern GFX_Info gfx_info;

void GFX_Events(void);
void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries);
void GFX_SetDrawHandler(GFX_DrawHandler * handler);
void GFX_Resize(Bitu width,Bitu height,Bitu bpp,GFX_ResizeHandler * resize);

void GFX_Start(void);
void GFX_Stop(void);
void GFX_SwitchFullScreen(void);

#endif

