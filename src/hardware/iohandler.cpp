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
#include "inout.h"

IO_ReadBlock IO_ReadTable[IO_MAX];
IO_WriteBlock IO_WriteTable[IO_MAX];

void IO_Write(Bitu num,Bit8u val) {
	if (num<IO_MAX) IO_WriteTable[num].handler(num,val);
	else LOG(LOG_ALL,LOG_ERROR)("IO:Out or range write %X2 to port %4X",val,num);
}

Bit8u IO_Read(Bitu num) {
	if (num<IO_MAX) return IO_ReadTable[num].handler(num);
	else LOG(LOG_ALL,LOG_ERROR)("IO:Out or range read from port %4X",num);
	return 0xff;
}


static Bit8u IO_ReadBlocked(Bit32u port) {
	return 0xff;
}

static void IO_WriteBlocked(Bit32u port,Bit8u val) {
}

static Bit8u  IO_ReadDefault(Bit32u port) {
	LOG(LOG_IO,LOG_ERROR)("Reading from undefined port %04X",port);
	IO_RegisterReadHandler(port,&IO_ReadBlocked,"Blocked Read");
	return 0xff;	
}

void IO_WriteDefault(Bit32u port,Bit8u val) {
	LOG(LOG_IO,LOG_ERROR)("Writing %02X to undefined port %04X",val,port);		
	IO_RegisterWriteHandler(port,&IO_WriteBlocked,"Blocked Write");
}


void IO_RegisterReadHandler(Bit32u port,IO_ReadHandler * handler,char * name) {
	if (port<IO_MAX) {
		IO_ReadTable[port].handler=handler;
		IO_ReadTable[port].name=name;
	}
}

void IO_RegisterWriteHandler(Bit32u port,IO_WriteHandler * handler,char * name) {
	if (port<IO_MAX) {
		IO_WriteTable[port].handler=handler;
		IO_WriteTable[port].name=name;
	}
}


void IO_FreeReadHandler(Bit32u port) {
	if (port<IO_MAX) {	
		IO_RegisterReadHandler(port,&IO_ReadDefault,"Default Read");
	}
}
void IO_FreeWriteHandler(Bit32u port) {
	if (port<IO_MAX) {
		IO_RegisterWriteHandler(port,&IO_WriteDefault,"Default Write");
	}
}


void IO_Init(Section * sect) {
	for (Bitu i=0;i<IO_MAX;i++) {
		IO_RegisterReadHandler(i,&IO_ReadDefault,"Default Read");
		IO_RegisterWriteHandler(i,&IO_WriteDefault,"Default Write");
	}
}



