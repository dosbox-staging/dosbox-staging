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

#include <string.h>
#include "dosbox.h"
#include "callback.h"
#include "regs.h"
#include "mem.h"
#include "bios.h"
#include "dos_inc.h"
#include "support.h"

#define MAX_DEVICES 10
/* Include all the devices */

#include "dev_con.h"


static DOS_Device * devices[MAX_DEVICES];
static Bit32u device_count;

class device_NUL : public DOS_Device {
public:
	device_NUL() { name="NUL"; };
	bool Read(Bit8u * data,Bit16u * size) {
		for(Bitu i = 0; i < *size;i++) 
			data[i]=0; 
		LOG(LOG_IOCTL,LOG_NORMAL)("NUL:READ");	   
		return true;
	}
	bool Write(Bit8u * data,Bit16u * size) {
		LOG(LOG_IOCTL,LOG_NORMAL)("NUL:WRITE");
		return true;
	}
	bool Seek(Bit32u * pos,Bit32u type) {
		LOG(LOG_IOCTL,LOG_NORMAL)("NUL:SEEK");
		return true;
	}
	bool Close() { return true; }
	Bit16u GetInformation(void) { return 0x8004; }
};

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
   	DOS_Device * newdev2;
	newdev2=new device_NUL();
	DOS_AddDevice(newdev2);
}

