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

#include "dosbox.h"
#include "callback.h"
#include "bios.h"
#include "regs.h"
#include "mem.h"
#include "dos_inc.h" /* for Drives[] */

static Bitu call_int13;
static BIOS_Disk * Floppys[2];
static BIOS_Disk * Harddisks[BIOS_MAX_DISK];
static Bit8u last_status;


static Bitu INT13_SmallHandler(void) {
	switch (reg_ah) {
    case 0x0:
        reg_ah=0x00;
        CALLBACK_SCF(false);
        LOG(LOG_BIOS,LOG_NORMAL)("reset disk return succesfull");
        break;
	case 0x02:	/* Read Disk Sectors */
		LOG(LOG_BIOS,LOG_NORMAL)("INT13:02:Read Disk Sectors not supported failing");
		reg_ah=0x80;
		CALLBACK_SCF(true);
		break;
    case 0x04:
        if(Drives[reg_dl]!=NULL) {
            reg_ah=0;
            CALLBACK_SCF(false);
        }                     
        else{
            reg_ah=0x80;
            CALLBACK_SCF(true);
        }
        LOG(LOG_BIOS,LOG_NORMAL)("INT 13:04 Verify sector used on %d, with result %d",reg_dl,reg_ah);
        break;
          
	case 0x08:	/* Get Drive Parameters */
		LOG(LOG_BIOS,LOG_NORMAL)("INT13:08:Get Drive parameters not supported failing");
		reg_ah=0xff;
		CALLBACK_SCF(true);
		break;
	case 0xff:
	default:
		LOG(LOG_BIOS,LOG_ERROR)("Illegal int 13h call %2X Fail it",reg_ah);
		reg_ah=0xff;
		CALLBACK_SCF(true);
	}
	return CBRET_NONE;
}

void BIOS_SetupDisks(void) {
/* TODO Start the time correctly */
	call_int13=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int13,&INT13_SmallHandler,CB_IRET);
	RealSetVec(0x13,CALLBACK_RealPointer(call_int13));
/* Init the Disk Tables */
	last_status=0;
/* Setup the Bios Area */
	mem_writeb(BIOS_HARDDISK_COUNT,0);
};

