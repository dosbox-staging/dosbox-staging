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


#include <string.h>
#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "bios.h"
#include "keyboard.h"
#include "regs.h"
#include "inout.h"
#include "dos_inc.h"


#define PAGEFRAME_SEG 0xe000

class device_EMS : public DOS_Device {
public:
	device_EMS();
	bool Read(Bit8u * data,Bit16u * size);
	bool Write(Bit8u * data,Bit16u * size);
	bool Seek(Bit32u * pos,Bit32u type);
	bool Close();
	Bit16u GetInformation(void);
private:
	Bit8u cache;
};

bool device_EMS::Read(Bit8u * data,Bit16u * size) {
	return false;
}

bool device_EMS::Write(Bit8u * data,Bit16u * size) {
	return false;
}

bool device_EMS::Seek(Bit32u * pos,Bit32u type) {
	return false;
}

bool device_EMS::Close() {
	return false;
}

Bit16u device_EMS::GetInformation(void) {
	return 0x8093;
};

device_EMS::device_EMS() {
	name="EMMXXXX0";
}


PageEntry ems_entries[4];

Bitu call_int67;
static Bitu INT67_Handler(void) {
	switch (reg_ah) {
	case 0x40:		/* Get Status */
		reg_ah=0;		//Status ok :)
		break;
	case 0x41:		/* Get PageFrame Segment */
		reg_bx=PAGEFRAME_SEG;
		reg_ah=0;
		break;
	case 0x42:		/* Get number of pages */
		{
//HEHE Hope this works not exactly like the specs but who cares 			
			Bit16u maxfree,total;
			EMM_GetFree(&maxfree,&total);
			reg_dx=maxfree>>2;
			reg_bx=total>>2;
			reg_ah=0;
		};
		break;
	case 0x43:		/* Get Handle and Allocate Pages */
		{
			if (!reg_bx) { reg_ah=0x89;break; }
			Bit16u handle;
			EMM_Allocate(reg_bx*4,&handle);
			if (handle) {
				reg_ah=0;
				reg_dx=handle;
			} else { reg_ah=0x88; }
			break;
		}
	case 0x44:		/* Map Memory */
		{
			if (reg_al>3) { reg_ah=0x8b;break; }
			HostPt pagestart=memory+EMM_Handles[reg_dx].phys_base+reg_bx*16*1024;
			ems_entries[reg_al].relocate=pagestart;
			reg_ah=0;
			break;
		}
	case 0x45:		/* Release handle and free pages */
		EMM_Free(reg_dx);
		reg_ah=0;
		break;		
	case 0x46:		/* Get EMM Version */
		reg_ah=0;
		reg_al=0x32;	//Only 3.2 support for now
		break;
	case 0x47:		/* Save Mapping Context */
		LOG_ERROR("EMS:47:Save Mapping Context not supported");
		reg_ah=0x8c;
		break;
	case 0xDE:		/* VCPI Functions */
		LOG_ERROR("VCPI Functions not supported");
		reg_ah=0x8c;
		break;
	default:
		LOG_ERROR("EMS:Call %2X not supported",reg_ah);
		reg_ah=0x8c;
		break;
	}
	return CBRET_NONE;
}



void EMS_Init(void) {
	call_int67=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int67,&INT67_Handler,CB_IRET);
/* Register the ems device */
	DOS_Device * newdev = new device_EMS();
	DOS_AddDevice(newdev);
/* Setup the page handlers for the page frame */
	for (Bitu i=0;i<4;i++) {
		ems_entries[i].base=(PAGEFRAME_SEG<<4)+i*16*1024;
		ems_entries[i].type=MEMORY_RELOCATE;
		ems_entries[i].relocate=memory+(PAGEFRAME_SEG<<4)+i*16*1024;
/* Place the page handlers in the ems page fram piece of the memory handler*/
		MEMORY_SetupHandler(((PAGEFRAME_SEG<<4)+i*16*1024)>>12,4,&ems_entries[i]);
	}


/* Add a little hack so it appears that there is an actual ems device installed */
	char * emsname="EMMXXXX0";
	Bit16u seg=DOS_GetMemory(2);	//We have 32 bytes
	MEM_BlockWrite(real_phys(seg,0xa),emsname,strlen(emsname)+1);
/* Copy the callback piece into the beginning */
	char buf[16];
	MEM_BlockRead(real_phys(CB_SEG,call_int67<<4),buf,0xa);
	MEM_BlockWrite(real_phys(seg,0),buf,0xa);

	RealSetVec(0x67,RealMake(seg,0));

}

