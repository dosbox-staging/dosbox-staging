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
#include "timer.h"
#include "vga.h"
#include "inout.h"

VGA_Type vga;

Bit32u CGA_4_Table[256];
Bit32u ExpandTable[256];
Bit32u Expand16Table[4][16];
Bit32u Expand16BigTable[0x10000];
Bit32u FillTable[16];


void VGA_SetMode(VGAModes mode) {
	vga.mode=mode;
	VGA_SetupHandlers();
	VGA_StartResize();
}

void VGA_StartResize(void) {
	if (!vga.draw.resizing) {
		vga.draw.resizing=true;
		/* Start a resize after 50 ms */
		PIC_AddEvent(VGA_SetupDrawing,50000);
	}
}

void VGA_SetClock(Bitu which,Bitu target) {
	struct{
		Bitu n,m;
		Bits err;
	} best;
	best.err=target;
	Bitu n,m,r;

	for (r = 0; r <= 3; r++) {
		Bitu f_vco = target * (1 << r);
		if (MIN_VCO <= f_vco && f_vco < MAX_VCO) break;
    }
	for (n=1;n<=31;n++) {
		m=(target * (n + 2) * (1 << r) + (S3_CLOCK_REF/2)) / S3_CLOCK_REF - 2;
		if (0 <= m && m <= 127)	{
			Bitu temp_target = S3_CLOCK(m,n,r);
			Bits err = target - temp_target;
			if (err < 0) err = -err;
			if (err < best.err) {
				best.err = err;
				best.m = m;
				best.n = n;
			}
		}
    }
	/* Program the s3 clock chip */
	vga.s3.clk[which].m=best.m;
	vga.s3.clk[which].r=r;
	vga.s3.clk[which].n=best.n;
	VGA_StartResize();
}

void VGA_Init(Section* sec) {
	vga.draw.resizing=false;
	VGA_SetupMemory();
	VGA_SetupMisc();
	VGA_SetupDAC();
	VGA_SetupGFX();
	VGA_SetupSEQ();
	VGA_SetupAttr();
	VGA_SetClock(0,CLK_25);
	VGA_SetClock(1,CLK_28);
/* Generate tables */
	Bitu i,j;
	for (i=0;i<256;i++) {
		ExpandTable[i]=i | (i << 8)| (i <<16) | (i << 24);
#ifdef WORDS_BIGENDIAN
		CGA_4_Table[i]=((i>>0)&3) | (((i>>2)&3) << 8)| (((i>>4)&3) <<16) | (((i>>6)&3) << 24);
#else
		CGA_4_Table[i]=((i>>6)&3) | (((i>>4)&3) << 8)| (((i>>2)&3) <<16) | (((i>>0)&3) << 24);
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

