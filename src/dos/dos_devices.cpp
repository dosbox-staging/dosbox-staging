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

/* $Id: dos_devices.cpp,v 1.8 2004-10-25 21:08:47 qbix79 Exp $ */ 

#include <string.h>
#include "dosbox.h"
#include "callback.h"
#include "regs.h"
#include "mem.h"
#include "bios.h"
#include "dos_inc.h"
#include "support.h"
#include "drives.h" //Wildcmp
/* Include all the devices */

#include "dev_con.h"


DOS_Device * Devices[DOS_DEVICES];
static Bitu device_count;

class device_NUL : public DOS_Device {
public:
	device_NUL() { SetName("NUL"); };
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
	Bit16u GetInformation(void) { return 0x8084; }
};

bool DOS_Device::Read(Bit8u * data,Bit16u * size) {
	return Devices[devnum]->Read(data,size);
}

bool DOS_Device::Write(Bit8u * data,Bit16u * size) {
	return Devices[devnum]->Write(data,size);
}

bool DOS_Device::Seek(Bit32u * pos,Bit32u type) {
	return Devices[devnum]->Seek(pos,type);
}

bool DOS_Device::Close() {
	return Devices[devnum]->Close();
}

Bit16u DOS_Device::GetInformation(void) { 
	return Devices[devnum]->GetInformation();
}

DOS_File::DOS_File(const DOS_File& orig) {
	type=orig.type;
	flags=orig.flags;
	time=orig.time;
	date=orig.date;
	attr=orig.attr;
	size=orig.size;
	refCtr=orig.refCtr;
	open=orig.open;
	name=0;
	if(orig.name) {
		name=new char [strlen(orig.name) + 1];strcpy(name,orig.name);
	}
}

DOS_File & DOS_File::operator= (const DOS_File & orig) {
	type=orig.type;
	flags=orig.flags;
	time=orig.time;
	date=orig.date;
	attr=orig.attr;
	size=orig.size;
	refCtr=orig.refCtr;
	open=orig.open;
	if(name) {
		delete [] name; name=0;
	}
	if(orig.name) {
		name=new char [strlen(orig.name) + 1];strcpy(name,orig.name);
	}
	return *this;
}

Bit8u DOS_FindDevice(char * name) {
	/* should only check for the names before the dot and spacepadded */
	char temp[CROSS_LEN];//TODOD
	if(!name || !(*name)) return DOS_DEVICES;
	strcpy(temp,name);
	char* dot= strrchr(temp,'.');
	if(dot && *dot) dot=0; //no ext checking

	/* loop through devices */
	Bit8u index=0;
	while (index<device_count) {
		if (Devices[index]) {
			if (WildFileCmp(temp,Devices[index]->name)) return index;
		}
		index++;
	}
	return DOS_DEVICES;
}


void DOS_AddDevice(DOS_Device * adddev) {
//TODO Give the Device a real handler in low memory that responds to calls
	if (device_count<DOS_DEVICES) {
		Devices[device_count]=adddev;
		Devices[device_count]->SetDeviceNumber(device_count);
		device_count++;
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

