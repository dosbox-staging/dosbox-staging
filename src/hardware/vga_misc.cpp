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
#include "timer.h"
#include "vga.h"

static Bit8u flip=0;
static Bit32u keep_vretrace;
static bool keeping=false;
static Bit8u p3c2data;

static Bit8u read_p3da(Bit32u port) {
	vga.internal.attrindex=false;
	if (vga.config.retrace) {
		vga.config.retrace=false;
		vga.config.real_start=vga.config.display_start;
		keep_vretrace=LastTicks+1;
		keeping=true;
		flip=0;
		return 9;
	}
	if (keeping) {
		if (LastTicks>(keep_vretrace)) keeping=false;
		return 9;
	} else {
		flip++;
		if (flip>10) flip=0;
		if (flip>5)	return 1;
		return 0;
	}
};


static void write_p3d8(Bit32u port,Bit8u val) {
	return;
}

static void write_p3c2(Bit32u port,Bit8u val) {
	p3c2data=val;
}
static Bit8u read_p3c2(Bit32u port) {
	return p3c2data;
}


void VGA_SetupMisc(void) {
	IO_RegisterReadHandler(0x3da,read_p3da,"VGA Input Status 1");
//	IO_RegisterWriteHandler(0x3d8,write_p3d8,"VGA Mode Control");
	IO_RegisterWriteHandler(0x3c2,write_p3c2,"VGA Misc Output");
	IO_RegisterReadHandler(0x3c2,read_p3c2,"VGA Misc Output");
}


