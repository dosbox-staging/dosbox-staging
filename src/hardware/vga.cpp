/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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
Bit32u FillTable[16];

static void EndRetrace(void) {
	/* start the actual display update now */
	RENDER_DoUpdate();
	vga.config.retrace=false;
}

static void VGA_BlankTimer() {
	PIC_AddEvent(VGA_BlankTimer,vga.draw.blank);
	PIC_AddEvent(EndRetrace,667);
	/* Setup a timer to destroy the vertical retrace bit in a few microseconds */
	vga.config.real_start=vga.config.display_start;
	vga.config.retrace=true;
}

static void VGA_DrawHandler(RENDER_Part_Handler part_handler) {
	Bit8u * buf,* bufsplit;
	/* Draw the current frame */
	if (!vga.draw.resizing) {
		if (vga.config.line_compare<vga.draw.lines) {
			Bitu stop=vga.config.line_compare;
			if (vga.draw.double_height) stop/=2;
			if (stop>=vga.draw.height){
				LOG(LOG_VGAGFX,"Split at %d",stop);
				goto drawnormal;
			}
			switch (vga.mode) {
			case GFX_16:
				buf=&vga.buffer[vga.config.real_start*8+vga.config.pel_panning];
				bufsplit=vga.buffer;
				break;
			case GFX_256U:
				buf=&vga.mem.linear[vga.config.real_start*4+vga.config.pel_panning/2];
				bufsplit=vga.mem.linear;
				break;
			case GFX_256C:
				buf=memory+0xa0000;
				bufsplit=memory+0xa0000;
				break;
			default:
				LOG(LOG_VGAGFX,"VGA:Unhandled split screen mode %d",vga.mode);
				goto norender;
			}
			part_handler(buf,0,0,vga.draw.width,stop);
			part_handler(bufsplit,0,stop,vga.draw.width,vga.draw.height-stop);
		} else {
drawnormal:
			switch (vga.mode) {
			case GFX_2:
				VGA_DrawGFX2_Fast(vga.buffer,vga.draw.width);
				buf=vga.buffer;
				break;
			case GFX_4:
				VGA_DrawGFX4_Fast(vga.buffer,vga.draw.width);
				buf=vga.buffer;
				break;
			case TEXT_16:
				VGA_DrawTEXT(vga.buffer,vga.draw.width);
				buf=vga.buffer;
				break;
			case GFX_16:
				buf=&vga.buffer[vga.config.real_start*8+vga.config.pel_panning];
				break;
			case GFX_256C:
				buf=memory+0xa0000;
				break;
			case GFX_256U:
				buf=&vga.mem.linear[vga.config.real_start*4+vga.config.pel_panning/2];
				break;
			}
			part_handler(buf,0,0,vga.draw.width,vga.draw.height);
		}
norender:;
	}

}

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
			if (vga.seq.clocking_mode & 0x8) {
				vga.mode=GFX_4;
//					MEM_SetupPageHandlers(PAGE_COUNT(0x0b8000),PAGE_COUNT(0x10000),&VGA_GFX_4_ReadHandler,&VGA_GFX_4_WriteHandler);
			} else vga.mode=GFX_2;
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
	/* Calculate the FPS for this screen */
	double fps;
	Bitu vtotal=2 + (vga.crtc.vertical_total | ((vga.crtc.overflow & 1) << 8) | ((vga.crtc.overflow & 0x20) << 4) );
	Bitu htotal=5 + vga.crtc.horizontal_total;
	Bitu vdispend = 1 + (vga.crtc.vertical_display_end | ((vga.crtc.overflow & 2)<<7) | ((vga.crtc.overflow & 0x40) << 3) );
	Bitu hdispend = 1 + (vga.crtc.horizontal_display_end);
	//TODO Maybe check if blanking comes before display_end

	double clock=(double)vga.config.clock;
	/* Check for 8 for 9 character clock mode */
	if (vga.seq.clocking_mode & 1 ) clock/=8; else clock/=9;
	/* Check for pixel doubling, master clock/2 */
	if (vga.seq.clocking_mode & 0x8) clock/=2;

	LOG(LOG_VGA,"H total %d, V Total %d",htotal,vtotal);
	LOG(LOG_VGA,"H D End %d, V D End %d",hdispend,vdispend);
	fps=clock/(vtotal*htotal);

	vga.draw.resizing=false;
	Bitu width,height,pitch,flags;

	flags=0;
	vga.draw.lines=height=vdispend;
	width=hdispend;
	vga.draw.double_height=vga.config.vline_double;
	vga.draw.double_width=(vga.seq.clocking_mode & 0x8)>0;
	vga.draw.font_height=vga.config.vline_height+1;
	switch (vga.mode) {
	case GFX_256C:
	case GFX_256U:
		vga.draw.double_width=true;		//Hack since 256 color modes use 2 clocks for a pixel
		/* Don't know might do this different sometime, will have to do for now */
		if (!vga.draw.double_height) {
			if (vga.config.vline_height&1) {
				vga.draw.double_height=true;
				vga.draw.font_height/=2;
			}
		}
		width<<=2;
		pitch=vga.config.scan_len*8;
		break;
	case GFX_16:
		width<<=3;
		pitch=vga.config.scan_len*16;
		break;
	case GFX_4:
		width<<=3;
		pitch=width;
		break;
	case GFX_2:
		width<<=3;
		pitch=width;
		break;
	case TEXT_16:
		/* probably a 16-color text mode, got to detect mono mode somehow */
		width<<=3;				/* 8 bit wide text font */
		if (width>640) width=640;
		if (height>480) height=480;
		pitch=width;
	};
	if (vga.draw.double_height) {
		flags|=DoubleHeight;
		height/=2;
	}
	if (vga.draw.double_width) {
		flags|=DoubleWidth;
		/* Double width is dividing main clock, the width should be correct already for this */
	}
	if (( width != vga.draw.width) || (height != vga.draw.height) || (pitch != vga.draw.pitch)) {
		PIC_RemoveEvents(VGA_BlankTimer);
		vga.draw.width=width;
		vga.draw.height=height;
		vga.draw.pitch=pitch;

		LOG(LOG_VGA,"Width %d, Height %d",width,height);
		LOG(LOG_VGA,"Flags %X, fps %f",flags,fps);
		RENDER_SetSize(width,height,8,pitch,((float)width/(float)height),flags,&VGA_DrawHandler);
		vga.draw.blank=(Bitu)(1000000/fps);
		PIC_AddEvent(VGA_BlankTimer,vga.draw.blank);
	}
};

void VGA_StartResize(void) {
	if (!vga.draw.resizing) {
		vga.draw.resizing=true;
		/* Start a resize after 50 ms */
		PIC_AddEvent(VGA_DoResize,50000);
	}
}

void VGA_Init(Section* sec) {
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
#ifdef WORDS_BIGENDIAN
		CGAWriteTable[i]=((i>>0)&3) | (((i>>2)&3) << 8)| (((i>>4)&3) <<16) | (((i>>6)&3) << 24);
#else
		CGAWriteTable[i]=((i>>6)&3) | (((i>>4)&3) << 8)| (((i>>2)&3) <<16) | (((i>>0)&3) << 24);
#endif
	}
	for (i=0;i<16;i++) {
#ifdef WORDS_BIGENDIAN
		FillTable[i]=	((i & 1) ? 0xff000000 : 0) |
						((i & 2) ? 0x00ff0000 : 0) |
						((i & 4) ? 0x0000ff00 : 0) |
						((i & 8) ? 0x000000ff : 0) ;
#else 
		FillTable[i]=	((i & 1) ? 0x000000ff : 0) |
						((i & 2) ? 0x0000ff00 : 0) |
						((i & 4) ? 0x00ff0000 : 0) |
						((i & 8) ? 0xff000000 : 0) ;
#endif
	}
	for (j=0;j<4;j++) {
		for (i=0;i<16;i++) {
#ifdef WORDS_BIGENDIAN
			Expand16Table[j][i] =
				((i & 1) ? 1 << j : 0) |
				((i & 2) ? 1 << (8 + j) : 0) |
				((i & 4) ? 1 << (16 + j) : 0) |
				((i & 8) ? 1 << (24 + j) : 0);
#else
			Expand16Table[j][i] =
				((i & 1) ? 1 << (24 + j) : 0) |
				((i & 2) ? 1 << (16 + j) : 0) |
				((i & 4) ? 1 << (8 + j) : 0) |
				((i & 8) ? 1 << j : 0);
#endif
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

