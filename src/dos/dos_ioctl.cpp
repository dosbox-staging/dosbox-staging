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

#include <string.h>
#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"

#define MAX_DEVICE 20
static DOS_File * dos_devices[MAX_DEVICE];


bool DOS_IOCTL(void) {
	Bitu handle;Bit8u drive;
	if (reg_al<8) {				/* call 0-7 use a file handle */
		handle=RealHandle(reg_bx);
		if (handle>=DOS_FILES) {
			DOS_SetError(DOSERR_INVALID_HANDLE);
			return false;
		}
		if (!Files[handle]) {
			DOS_SetError(DOSERR_INVALID_HANDLE);
			return false;
		}
	}
	switch(reg_al) {
	case 0x00:		/* Get Device Information */
		reg_dx=Files[handle]->GetInformation();
		return true;
	case 0x07:		/* Get Output Status */
		LOG(LOG_IOCTL,"DOS:IOCTL:07:Fakes output status is ready for handle %d",handle);
		reg_al=0xff;
		return true;
	case 0x08:		/* Check if block device removable */
		drive=reg_bl;if (!drive) drive=dos.current_drive;else drive--;
		if (Drives[drive]) {
			if (drive<2) reg_ax=0;	/* Drive a,b are removable if mounted */
			else reg_ax=1;
			return true;
		} else {
			DOS_SetError(DOSERR_INVALID_DRIVE);
			return false;
		}
	case 0x09:		/* Check if block device remote */
		drive=reg_bl;if (!drive) drive=dos.current_drive;else drive--;
		if (Drives[drive]) {
			reg_dx=0;
			//TODO Cdrom drives are remote
			//TODO Set bit 9 on drives that don't support direct I/O
			return true;
		} else {
			DOS_SetError(DOSERR_INVALID_DRIVE);
			return false;
		}
	case 0x0D:		/* Generic block device request */
		{
			PhysPt ptr	= SegPhys(ds)+reg_dx;
			drive=reg_bl;if (!drive) drive=dos.current_drive;else drive--;
			switch (reg_cl) {
			case 0x60:		/* Get Device parameters */
				mem_writeb(ptr  ,0x03);					// special function
				mem_writeb(ptr+1,(drive>=2)?0x05:0x14);	// fixed disc(5), 1.44 floppy(14)
				mem_writew(ptr+2,drive>=2);				// nonremovable ?
				mem_writew(ptr+4,0x0000);				// num of cylinders
				mem_writeb(ptr+6,0x00);					// media type (00=other type)
				break;
			default	:	
				LOG(LOG_IOCTL|LOG_ERROR,"DOS:IOCTL Call 0D:%2X Drive %2X unhandled",reg_cl,drive);
				return false;
			}
			return true;
		}
	case 0xE:		/* Get Logical Drive Map */
		drive=reg_bl;if (!drive) drive=dos.current_drive;else drive--;
		if (Drives[drive]) {
			reg_al=0;		/* Only 1 logical drive assigned */
			return true;
		} else {
			DOS_SetError(DOSERR_INVALID_DRIVE);
			return false;
		}
		break;
    case 0x06:      /* Get Input Status */
        if(reg_bx==0x00) { /* might work for other handles, but tested it only for STDIN */
            if(Files[handle]->GetInformation() & 0x40) reg_al=0x00; else
            reg_al=0xFF;
            return true;
            break;
        }
	default:
		LOG(LOG_DOSMISC|LOG_ERROR,"DOS:IOCTL Call %2X unhandled",reg_al);
		return false;
	};
	return false;
};


bool DOS_GetSTDINStatus(void) {
	Bit32u handle=RealHandle(STDIN);
	if (handle==0xFF) return false;
	if (Files[handle] && (Files[handle]->GetInformation() & 64)) return false;
	return true;
};

