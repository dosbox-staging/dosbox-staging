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

#include <stdlib.h>
#include <string.h>

#include "dosbox.h"
#include "video.h"
#include "pic.h"
#include "render.h"
#include "timer.h"
#include "vga.h"

VGA_Type vga;
Bit32u CGAWriteTable[256];
Bit32u ExpandTable[256];
Bit32u Expand16Table[4][16];
Bit32u Expand16BigTable[0x10000];

Bit32u FillTable[16]={	
	0x00000000,0x000000ff,0x0000ff00,0x0000ffff,
	0x00ff0000,0x00ff00ff,0x00ffff00,0x00ffffff,
	0xff000000,0xff0000ff,0xff00ff00,0xff00ffff,
	0xffff0000,0xffff00ff,0xffffff00,0xffffffff
};



void VGA_Render_GFX_2(Bit8u * * data);
void VGA_Render_GFX_4(Bit8u * * data);
void VGA_Render_GFX_16(Bit8u * * data);
void VGA_Render_GFX_256C(Bit8u * * data);
void VGA_Render_GFX_256U(Bit8u * * data);
void VGA_Render_TEXT_16(Bit8u * * data);

void VGA_FindSettings(void) {
	/* Sets up the correct memory handler from the vga.mode setting */
	MEM_ClearPageHandlers(PAGE_COUNT(0xa0000),PAGE_COUNT(0x20000));
	/* Detect the kind of video mode this is */
	if (vga.config.gfxmode) {
		if (vga.config.vga_enabled) {
			if (vga.config.chained) {
			/* 256 color chained vga */
				vga.mode=GFX_256C;
				//Doesn't need a memory handler
			} else {
			/* 256 color unchained vga */
				vga.mode=GFX_256U;
				MEM_SetupPageHandlers(PAGE_COUNT(0xa0000),PAGE_COUNT(0x10000),
					&VGA_NormalReadHandler,&VGA_GFX_256U_WriteHandler);
			}
		} else if (vga.config.cga_enabled) {
			/* 4 color cga */
			//TODO Detect hercules modes, probably set them up in bios too
			if (vga.config.pixel_double) vga.mode=GFX_4;
			else vga.mode=GFX_2;
			//TODO Maybe also use a page handler for cga mode
		} else {
			/* 16 color ega */
			vga.mode=GFX_16;
			MEM_SetupPageHandlers(PAGE_COUNT(0xa0000),PAGE_COUNT(0x10000),
					&VGA_NormalReadHandler,&VGA_GFX_16_WriteHandler);
		}
	} else {
		vga.mode=TEXT_16;
	}
	VGA_StartResize();
}

static void VGA_DoResize(void) {
	vga.draw.resizing=false;
	Bitu width,height,pitch;
	RENDER_Handler * renderer;

	height=vga.config.vdisplayend+1;
	if (vga.config.vline_height>0) {
		height/=(vga.config.vline_height+1);
	}
	if (vga.config.vline_double) height>>=1;
	width=vga.config.hdisplayend;
	switch (vga.mode) {
	case GFX_256C:
		renderer=&VGA_Render_GFX_256C;
		width<<=2;
		pitch=vga.config.scan_len*8;
		break;
	case GFX_256U:
		width<<=2;
		pitch=vga.config.scan_len*8;
		renderer=&VGA_Render_GFX_256U;
		break;
	case GFX_16:
		width<<=3;
		pitch=vga.config.scan_len*16;
		renderer=&VGA_Render_GFX_16;
		break;
	case GFX_4:
		width<<=3;
		height<<=1;
		pitch=width;
		renderer=&VGA_Render_GFX_4;
		break;
	case GFX_2:
		width<<=3;
		height<<=1;
		pitch=width;
		renderer=&VGA_Render_GFX_2;
		break;
	case TEXT_16:
		/* probably a 16-color text mode, got to detect mono mode somehow */
		width<<=3;		/* 8 bit wide text font */
		height<<=4;		/* 16 bit font height */
		if (width>640) width=640;
		if (height>480) height=480;
		pitch=width;
		renderer=&VGA_Render_TEXT_16;
	};

	vga.draw.width=width;
	vga.draw.height=height;
	RENDER_SetSize(width,height,8,pitch,((float)width/(float)height),0,renderer);

};

void VGA_StartResize(void) {
	if (!vga.draw.resizing) {
		vga.draw.resizing=true;
		/* Start a resize after 50 ms */
		TIMER_RegisterDelayHandler(VGA_DoResize,50);
	}
}


void VGA_Init() {
	vga.draw.resizing=false;
	VGA_SetupMemory();
	VGA_SetupMisc();
	VGA_SetupDAC();
	VGA_SetupGFX();
	VGA_SetupSEQ();
	VGA_SetupAttr();
/* Generate tables */
	Bitu i,j;
	for (i=0;i<256;i++) {
		ExpandTable[i]=i | (i << 8)| (i <<16) | (i << 24);
		CGAWriteTable[i]=((i>>6)&3) | (((i>>4)&3) << 8)| (((i>>2)&3) <<16) | (((i>>0)&3) << 24);
	}
	for (j=0;j<4;j++) {
		for (i=0;i<16;i++) {
			Expand16Table[j][i] =
				((i & 1) ? 1 << (24 + j) : 0) |
				((i & 2) ? 1 << (16 + j) : 0) |
				((i & 4) ? 1 << (8 + j) : 0) |
				((i & 8) ? 1 << j : 0);
		}
	}
	for (i=0;i<0x10000;i++) {
		Bit32u val=0;
		if (i & 0x1) val|=0x1 << 24;
		if (i & 0x2) val|=0x1 << 16;
		if (i & 0x4) val|=0x1 << 8;
		if (i & 0x8) val|=0x1 << 0;

		if (i & 0x10) val|=0x4 << 24;
		if (i & 0x20) val|=0x4 << 16;
		if (i & 0x40) val|=0x4 << 8;
		if (i & 0x80) val|=0x4 << 0;

		if (i & 0x100) val|=0x2 << 24;
		if (i & 0x200) val|=0x2 << 16;
		if (i & 0x400) val|=0x2 << 8;
		if (i & 0x800) val|=0x2 << 0;

		if (i & 0x1000) val|=0x8 << 24;
		if (i & 0x2000) val|=0x8 << 16;
		if (i & 0x4000) val|=0x8 << 8;
		if (i & 0x8000) val|=0x8 << 0;
		Expand16BigTable[i]=val;
	}
}

