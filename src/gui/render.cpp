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

#include "dosbox.h"
#include "video.h"
#include "render.h"



#define MAX_RES 2048

struct PalData {
	struct { 
		Bit8u red;
		Bit8u green;
		Bit8u blue;
		Bit8u unused;
	} rgb[256];
	Bitu first;
	Bitu last;
};


static struct {
	struct {
		Bitu width;
		Bitu height;
		Bitu bpp;
		Bitu pitch;
		float ratio;
	} src;
	Bitu flags;
	RENDER_Handler * handler;	
	Bitu stretch_x[MAX_RES];
	Bitu stretch_y[MAX_RES];
	PalData pal;
	bool remake;
	bool enlarge;
} render;

/* This could go kinda bad with multiple threads */
static void Check_Palette(void) {
	if (render.pal.first>render.pal.last) return;
	
	GFX_SetPalette(render.pal.first,render.pal.last-render.pal.first+1,(GFX_PalEntry *)&render.pal.rgb[render.pal.first]);
	render.pal.first=256;
	render.pal.last=0;
}

static void MakeTables(void) {
	//The stretching tables 
	Bitu i;Bit32u c,a;
	c=0;a=(render.src.width<<16)/gfx_info.width;
	for (i=0;i<gfx_info.width;i++) {
		c=(c&0xffff)+a;
		render.stretch_x[i]=c>> 16;
	}
	c=0;a=(render.src.height<<16)/gfx_info.height;
	for (i=0;i<gfx_info.height;i++) {
		c=(c&0xffff)+a;
		render.stretch_y[i]=(c>>16)*render.src.pitch;
	}
}

static void Draw_8_Normal(Bit8u * src_data,Bit8u * dst_data) {
	for (Bitu y=0;y<gfx_info.height;y++) {
		Bit8u * line_src=src_data;
		Bit8u * line_dest=dst_data;
		for (Bitu x=0;x<gfx_info.width;x++) {
			*line_dest++=*line_src;
			line_src+=render.stretch_x[x];
		}
		src_data+=render.stretch_y[y];
		dst_data+=gfx_info.pitch;
	}
}

void RENDER_Draw(Bit8u * bitdata) {
	Bit8u * src_data;
	Check_Palette();
	if (render.remake) {
		MakeTables();
		render.remake=false;
	}
	render.handler(&src_data);
	Draw_8_Normal(src_data,bitdata);

}


void RENDER_SetPal(Bit8u entry,Bit8u red,Bit8u green,Bit8u blue) {
	render.pal.rgb[entry].red=red;
	render.pal.rgb[entry].green=green;
	render.pal.rgb[entry].blue=blue;
	if (render.pal.first>entry) render.pal.first=entry;
	if (render.pal.last<entry) render.pal.last=entry;
}

static void RENDER_Resize(Bitu * width,Bitu * height) {
	/* Calculate the new size the window should be */
	if (!*width && !*height) {
		/* Special command to reset any resizing for fullscreen */
		*width=render.src.width;
		*height=render.src.height;
	} else {
		if ((*width/render.src.ratio)<*height) *height=(Bitu)(*width/render.src.ratio);
		else *width=(Bitu)(*height*render.src.ratio);
	}
	render.remake=true;
}

void RENDER_SetSize(Bitu width,Bitu height,Bitu bpp,Bitu pitch,float ratio,Bitu flags, RENDER_Handler * handler) {

	if (!width) return;
	if (!height) return;
	GFX_Stop();
	render.src.width=width;
	render.src.height=height;
	render.src.bpp=bpp;
	render.src.ratio=ratio;
	render.src.pitch=pitch;
	render.handler=handler;

	switch (bpp) {
	case 8:
		GFX_Resize(width,height,bpp,&RENDER_Resize);
		GFX_SetDrawHandler(RENDER_Draw);
		break;	
	default:
		E_Exit("RENDER:Illegal bpp %d",bpp);
	};
	/* Setup the internal render structures to correctly render this screen */
	MakeTables();

	GFX_Start();

}


void RENDER_Init(void) {
	render.pal.first=256;
	render.pal.last=0;
	render.enlarge=false;	
}

