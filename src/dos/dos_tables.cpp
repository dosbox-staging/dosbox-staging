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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: dos_tables.cpp,v 1.14 2004-08-04 09:12:53 qbix79 Exp $ */

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
	Bit16u seg,seg2;Bitu i;
	dos.tables.mediaid=RealMake(DOS_GetMemory(2),0);
	dos.tables.tempdta=RealMake(DOS_GetMemory(4),0);
	for (i=0;i<DOS_DRIVES;i++) mem_writeb(Real2Phys(dos.tables.mediaid)+i,0);
	/* Create the DOS Info Block */
	dos_infoblock.SetLocation(0x50); //c2woody
   
	/* create SDA */
	sdaseg=0x5a;
	DOS_SDA(sdaseg,0).Init();

	/* Some weird files >20 detection routine */
	/* Possibly obselete when SFT is properly handled */
	real_writed(0x5d,0x0a,0x204e4f43);
	real_writed(0x5d,0x1a,0x204e4f43);
	real_writed(0x5d,0x2a,0x204e4f43);

	/* create a CON device driver */
	seg=0x60;
 	real_writed(seg,0x00,0xffffffff);	// next ptr
 	real_writew(seg,0x04,0x8013);		// attributes
  	real_writed(seg,0x06,0xffffffff);	// strategy routine
  	real_writed(seg,0x0a,0x204e4f43);	// driver name
  	real_writed(seg,0x0e,0x20202020);	// driver name
	dos_infoblock.SetDeviceChainStart(RealMake(seg,0));
   
	/* Create a fake SFT, so programs think there are 100 file handles */
	seg=0x62;
	seg2=0x63;
	real_writed(seg,0,seg2<<16);		//Next File Table
	real_writew(seg,4,100);				//File Table supports 100 files
	real_writed(seg2,0,0xffffffff);		//Last File Table
	real_writew(seg2,4,100);			//File Table supports 100 files
	dos_infoblock.SetfirstFileTable(RealMake(seg,0));
   
	/* Create a fake CDS */
	seg=0x64;
	real_writed(seg,0x00,0x005c3a43);
	dos_infoblock.SetCurDirStruct(RealMake(seg,0));



	/* Allocate DCBS DOUBLE BYTE CHARACTER SET LEAD-BYTE TABLE */
	dos.tables.dcbs=RealMake(DOS_GetMemory(12),0);
	mem_writew(Real2Phys(dos.tables.dcbs),0); //empty table

	/* Create a fake FCB SFT */
	seg=DOS_GetMemory(4);
	real_writed(seg,0,0xffffffff);		//Last File Table
	real_writew(seg,4,100);				//File Table supports 100 files
	dos_infoblock.SetFCBTable(RealMake(seg,0));

	/* Create a fake disk info buffer */
	seg=DOS_GetMemory(6);
	seg2=DOS_GetMemory(6);
	real_writed(seg,0x00,seg2<<16);
	real_writed(seg2,0x00,0xffffffff);
	real_writed(seg2,0x0d,0xffffffff);
	dos_infoblock.SetDiskInfoBuffer(RealMake(seg,0));

	/* Set buffers to a nice value */
	dos_infoblock.SetBuffers(50,50);
   
}
