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
#include "cpu.h"
#include "dos_inc.h"

#define MAX_DEVICE 20
static DOS_File * dos_devices[MAX_DEVICE];

bool DOS_IOCTL(Bit8u call,Bit16u entry) {
	Bit32u handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	switch(reg_al) {
	case 0x00:		/* Get Device Information */
		reg_dx=Files[handle]->GetInformation();
		return true;
	case 0x07:		/* Get Output Status */
		LOG_DEBUG("DOS:IOCTL:07:Fakes output status is ready for handle %d",handle);
		reg_al=0xff;
		return true;
	case 0x0D: {
		PhysPt ptr	= SegPhys(ds)+reg_dx;
		Bit8u drive = reg_bl ? reg_bl : DOS_GetDefaultDrive()+1; // A=1, B=2, C=3...
		switch (reg_cl) {
			case 0x60 :	mem_writeb(ptr  ,0x03);					// special function
						mem_writeb(ptr+1,(drive>=3)?0x05:0x14);	// fixed disc(5), 1.44 floppy(14)
						mem_writew(ptr+2,drive>=3);				// nonremovable ?
						mem_writew(ptr+4,0x0000);				// num of cylinders
						mem_writeb(ptr+6,0x00);					// media type (00=other type)
						break;
			default	:	LOG_ERROR("DOS:IOCTL Call 0D:%2X Drive %2X unhandled",reg_cl,drive);
						return false;
		}
		return true;
		}
	default:
		LOG_ERROR("DOS:IOCTL Call %2X Handle %2X unhandled",reg_al,handle);
		return false;
	};
	return false;
};


bool DOS_GetSTDINStatus(void) {
	Bit32u handle=RealHandle(STDIN);
	if (Files[handle]->GetInformation() & 64) return false;
	return true;
};

