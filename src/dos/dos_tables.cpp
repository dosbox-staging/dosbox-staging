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

/* $Id: dos_tables.cpp,v 1.11 2004-06-10 08:48:53 qbix79 Exp $ */

#include "dosbox.h"
#include "mem.h"
#include "dos_inc.h"

#ifdef _MSC_VER
#pragma pack(1)
#endif
struct DOS_TableCase {	
	Bit16u size;
	Bit8u chars[256];
}
GCC_ATTRIBUTE (packed);
#ifdef _MSC_VER
#pragma pack ()
#endif

RealPt DOS_TableUpCase;
RealPt DOS_TableLowCase;

static Bit16u dos_memseg;
Bit16u sdaseg;

Bit16u DOS_GetMemory(Bit16u pages) {
	if (pages+dos_memseg>=0xe000) {
		E_Exit("DOS:Not enough memory for internal tables");
	}
	Bit16u page=dos_memseg;
	dos_memseg+=pages;
	return page;
}


void DOS_SetupTables(void) {
	dos_memseg=0xd000;
	Bit16u seg;Bitu i;
	dos.tables.mediaid=RealMake(DOS_GetMemory(2),0);
	dos.tables.tempdta=RealMake(DOS_GetMemory(4),0);
	for (i=0;i<DOS_DRIVES;i++) mem_writeb(Real2Phys(dos.tables.mediaid)+i,0);
	/* Create the DOS Info Block */
	dos_infoblock.SetLocation(0x4e); //c2woody
   
	/* Create a fake SFT, so programs think there are 100 file handles */
	seg=DOS_GetMemory(1);
	real_writed(seg,0,0xffffffff);		//Last File Table
	real_writew(seg,4,100);				//File Table supports 100 files
	dos_infoblock.SetfirstFileTable(RealMake(seg,0));
	/* Set buffers to a nice value */
	dos_infoblock.SetBuffers(50,50);
   
	/* Some weird files >20 detection routine */
	/* Possibly obselete when SFT is properly handled */
	// CON string
	real_writew(0x54,0x00+0x00, (Bit16u) 0x4f43);
	real_writew(0x54,0x00+0x02, (Bit16u) 0x204e);
	// CON string
	real_writew(0x54,0x10+0x00, (Bit16u) 0x4f43);
	real_writew(0x54,0x10+0x02, (Bit16u) 0x204e);
	//CON string
	real_writew(0x54,0x20+0x00, (Bit16u) 0x4f43);
	real_writew(0x54,0x20+0x02, (Bit16u) 0x204e);

	/* Allocate DCBS DOUBLE BYTE CHARACTER SET LEAD-BYTE TABLE */
	dos.tables.dcbs=RealMake(DOS_GetMemory(3),0);
	mem_writew(Real2Phys(dos.tables.dcbs),0); //empty table

	/* Allocate some fake memory else pharlab doesn't like the indos pointer */
	sdaseg=DOS_GetMemory(12);
	DOS_SDA(sdaseg,0).Init();
   
}
