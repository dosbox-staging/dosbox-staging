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

