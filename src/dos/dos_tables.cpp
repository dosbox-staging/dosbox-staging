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
#include "mem.h"
#include "dos_inc.h"

#pragma pack(1)

struct DOS_TableCase {	
	Bit16u size;
	Bit8u chars[256];
};
#pragma pack()


RealPt DOS_TableUpCase;
RealPt DOS_TableLowCase;

static Bit16u dos_memseg=0xd000;
Bit16u DOS_GetMemory(Bit16u pages) {
	if (pages+dos_memseg>=0xe000) {
		E_Exit("DOS:Not enough memory for internal tables");
	}
	Bit16u page=dos_memseg;
	dos_memseg+=pages;
	return page;
}


void DOS_SetupTables(void) {
	dos.tables.indosflag=RealMake(DOS_GetMemory(1),0);
	mem_writeb(Real2Phys(dos.tables.indosflag),0);
};


	


