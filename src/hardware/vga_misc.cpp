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
#include "inout.h"
#include "pic.h"
#include "vga.h"


static Bit8u flip=0;
static Bit32u keep_vretrace;
static bool keeping=false;
static Bit8u p3c2data=0;

void write_p3d4(Bit32u port,Bit8u val);
Bit8u read_p3d4(Bit32u port);
void write_p3d5(Bit32u port,Bit8u val);
Bit8u read_p3d5(Bit32u port);

static Bit8u read_p3da(Bit32u port) {
	vga.internal.attrindex=false;
	if (vga.config.retrace) {
		return 9;
	}
	flip++;
	if (flip>10) flip=0;
	if (flip>5)	return 1;
	return 0;
	/*
		0	Either Vertical or Horizontal Retrace active if set
		3	Vertical Retrace in progress if set
	*/
}


static void write_p3d8(Bit32u port,Bit8u val) {
	LOG_DEBUG("Write %2X to 3da",val);
	/*
		3	Vertical Sync Select. If set Vertical Sync to the monitor is the
			logical OR of the vertical sync and the vertical display enable.
	*/
}

static void write_p3c2(Bit32u port,Bit8u val) {
	p3c2data=val;
	if (val & 1) {
		IO_RegisterWriteHandler(0x3d4,write_p3d4,"VGA:CRTC Index Select");
		IO_RegisterReadHandler(0x3d4,read_p3d4,"VGA:CRTC Index Select");
		IO_RegisterWriteHandler(0x3d5,write_p3d5,"VGA:CRTC Data Register");
		IO_RegisterReadHandler(0x3d5,read_p3d5,"VGA:CRTC Data Register");
		IO_FreeWriteHandler(0x3b4);
		IO_FreeReadHandler(0x3b4);
		IO_FreeWriteHandler(0x3b5);
		IO_FreeReadHandler(0x3b5);
	} else {
		IO_RegisterWriteHandler(0x3b4,write_p3d4,"VGA:CRTC Index Select");
		IO_RegisterReadHandler(0x3b4,read_p3d4,"VGA:CRTC Index Select");
		IO_RegisterWriteHandler(0x3b5,write_p3d5,"VGA:CRTC Data Register");
		IO_RegisterReadHandler(0x3b5,read_p3d5,"VGA:CRTC Data Register");
		IO_FreeWriteHandler(0x3d4);
		IO_FreeReadHandler(0x3d4);
		IO_FreeWriteHandler(0x3d5);
		IO_FreeReadHandler(0x3d5);
	}
	/*
		0	If set Color Emulation. Base Address=3Dxh else Mono Emulation. Base Address=3Bxh.
		2-3	Clock Select. 0: 25MHz, 1: 28MHz
		5	When in Odd/Even modes Select High 64k bank if set
		6	Horizontal Sync Polarity. Negative if set
		7	Vertical Sync Polarity. Negative if set
			Bit 6-7 indicates the number of lines on the display:
			1:  400, 2: 350, 3: 480
			Note: Set to all zero on a hardware reset.
			Note: This register can be read from port 3CCh.
	*/
}


static Bit8u read_p3cc(Bit32u port) {
	return p3c2data;
}


static void EndRetrace(void) {
	vga.config.retrace=false;
}

void VGA_StartRetrace(void) {
	/* Setup a timer to destroy the vertical retrace bit in a few microseconds */
	vga.config.real_start=vga.config.display_start;
	vga.config.retrace=true;
	PIC_AddEvent(EndRetrace,667);
}

void VGA_SetupMisc(void) {
	IO_RegisterReadHandler(0x3da,read_p3da,"VGA Input Status 1");
	IO_RegisterReadHandler(0x3ba,read_p3da,"VGA Input Status 1");

	IO_RegisterWriteHandler(0x3d8,write_p3d8,"VGA Feature Control Register");
	IO_RegisterWriteHandler(0x3c2,write_p3c2,"VGA Misc Output");
	IO_RegisterReadHandler(0x3cc,read_p3cc,"VGA Misc Output");
}


