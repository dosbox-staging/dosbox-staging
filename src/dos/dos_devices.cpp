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
#include "cpu.h"
#include "mem.h"
#include "bios.h"
#include "dos_inc.h"
#include "support.h"

#define MAX_DEVICES 10
/* Include all the devices */

#include "dev_con.h"


static DOS_Device * devices[MAX_DEVICES];
static Bit32u device_count;

Bit8u DOS_FindDevice(char * name) {
	/* loop through devices */
	Bit8u index=0;
	while (index<device_count) {
		if (devices[index]) {
			if (strcasecmp(name,devices[index]->name)==0) return index;
		}
		index++;
	}
	return 255;
}


void DOS_AddDevice(DOS_Device * adddev) {
//TODO Give the Device a real handler in low memory that responds to calls
	if (device_count<MAX_DEVICES) {
		devices[device_count]=adddev;
		device_count++;
		/* Add the device in the main file Table */
		Bit8u handle=DOS_FILES;Bit8u i;
		for (i=0;i<DOS_FILES;i++) {
			if (!Files[i]) {
				handle=i;
				Files[i]=adddev;
				break;
			}
		}
		if (handle==DOS_FILES) E_Exit("DOS:Not enough file handles for device");
		adddev->fhandle=handle;
	} else {
		E_Exit("DOS:Too many devices added");
	}
}

void DOS_SetupDevices(void) {
	device_count=0;
	DOS_Device * newdev;
	newdev=new device_CON();
	DOS_AddDevice(newdev);
}

