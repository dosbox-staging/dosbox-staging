/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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


enum GFX_MODES {
	GFX_8BPP=0,
	GFX_15BPP=1,
	GFX_16BPP=2,
	GFX_24BPP=3,
	GFX_32BPP=4,
	GFX_YUV=5,
	GFX_MODE_SIZE=6
};

typedef void (* GFX_ResetCallBack)(void);

typedef void (* GFX_RenderCallBack)(Bit8u * data,Bitu pitch);

struct GFX_PalEntry {
	Bit8u r;
	Bit8u g;
	Bit8u b;
	Bit8u unused;
};

#define GFX_HASSCALING	0x0001
#define GFX_HASCONVERT	0x0002

void GFX_Events(void);
void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries);
GFX_MODES GFX_GetBestMode(Bitu bpp,Bitu & gfx_flags);

Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue);
void GFX_SetSize(Bitu width,Bitu height,GFX_MODES gfx_mode,double scalex,double scaley,GFX_ResetCallBack cb_reset, GFX_RenderCallBack cb_render);

void GFX_Start(void);
void GFX_Stop(void);
void GFX_SwitchFullScreen(void);

void GFX_Render_Blit(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy);

void GFX_DoUpdate(void);

#endif

