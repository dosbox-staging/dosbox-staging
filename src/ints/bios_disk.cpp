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
#include "callback.h"
#include "bios.h"
#include "regs.h"
#include "mem.h"


static Bitu call_int13;
static BIOS_Disk * Floppys[2];
static BIOS_Disk * Harddisks[BIOS_MAX_DISK];
static Bit8u last_status;

static Bitu INT13_FullHandler(void) {
	/* Check for disk numbers */
	BIOS_Disk * disk=Floppys[0];
	switch (reg_ah) {
	case 0x00:
		last_status=reg_ah=0;
		break;
	case 0x01:			/* Get Status of last operation */
		reg_ah=last_status;
		break;
	case 0x02:			/* Read Sectors into Memory */
		last_status=reg_ah=disk->Read_Sector(&reg_al,reg_dh,reg_ch,(reg_cl & 0x3f)-1,real_off(Segs[es].value,reg_bx));
		CALLBACK_SCF(false);
		break;
	case 0x03:			/* Write Sectors from Memory */
		last_status=reg_ah=disk->Write_Sector(&reg_al,reg_dh,reg_ch,(reg_cl & 0x3f)-1,real_off(Segs[es].value,reg_bx));
		CALLBACK_SCF(false);
		break;
	default:
		LOG_DEBUG("INT13:Illegal call %2X",reg_ah);
		reg_ah=0xff;
		CALLBACK_SCF(true);
	}
	return CBRET_NONE;
};


static Bitu INT13_SmallHandler(void) {
	switch (reg_ah) {
	case 0x02:	/* Read Disk Sectors */
		LOG_DEBUG("INT13:02:Read Disk Sectors not supported failing");
		reg_ah=0xff;
		CALLBACK_SCF(true);
		break;
	case 0x08:	/* Get Drive Parameters */
		LOG_DEBUG("INT13:08:Get Drive parameters not supported failing");
		reg_ah=0xff;
		CALLBACK_SCF(true);
		break;
	case 0xff:
	default:
		LOG_WARN("Illegal int 13h call %2X Fail it",reg_ah);
		reg_ah=0xff;
		CALLBACK_SCF(true);
	}
	return CBRET_NONE;
}

void BIOS_SetupDisks(void) {
/* TODO Start the time correctly */
	call_int13=CALLBACK_Allocate();	
#ifdef C_IMAGE
	Floppys[0]=new imageDisk("c:\\test.img");
	for (Bit32u i=0;i<BIOS_MAX_DISK;i++) Harddisks[i]=0;
	CALLBACK_Setup(call_int13,&INT13_FullHandler,CB_IRET);
#else
	CALLBACK_Setup(call_int13,&INT13_SmallHandler,CB_IRET);
#endif
	RealSetVec(0x13,CALLBACK_RealPointer(call_int13));
/* Init the Disk Tables */
	last_status=0;
/* Setup the Bios Area */
	mem_writeb(BIOS_HARDDISK_COUNT,0);
};

