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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"
#include "inout.h"

IO_WriteHandler * io_writehandlers[3][IO_MAX];
IO_ReadHandler * io_readhandlers[3][IO_MAX];

static Bitu IO_ReadBlocked(Bitu port,Bitu iolen) {
	return (Bitu)-1;
}
static void IO_WriteBlocked(Bitu port,Bitu val,Bitu iolen) {
}

static Bitu IO_ReadDefault(Bitu port,Bitu iolen) {
	switch (iolen) {
	case 1:
		LOG(LOG_IO,LOG_WARN)("Read from port %04X",port);
		io_readhandlers[0][port]=IO_ReadBlocked;
		return 0xff;
	case 2:
		return 
			(io_readhandlers[0][port+0](port+0,1) << 0) |
			(io_readhandlers[0][port+1](port+1,1) << 8);
	case 4:
		return
			(io_readhandlers[1][port+0](port+0,2) << 0) |
			(io_readhandlers[1][port+2](port+2,2) << 16);
	}
	return 0;
}

void IO_WriteDefault(Bitu port,Bitu val,Bitu iolen) {
	switch (iolen) {
	case 1:
		LOG(LOG_IO,LOG_WARN)("Writing %02X to port %04X",val,port);		
		io_writehandlers[0][port]=IO_WriteBlocked;
		break;
	case 2:
		io_writehandlers[0][port+0](port+0,(val >> 0) & 0xff,1);
		io_writehandlers[0][port+1](port+1,(val >> 8) & 0xff,1);
		break;
	case 4:
		io_writehandlers[1][port+0](port+0,(val >> 0 ) & 0xffff,2);
		io_writehandlers[1][port+2](port+2,(val >> 16) & 0xffff,2);
		break;
	}
}

void IO_RegisterReadHandler(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range) {
	while (range--) {
		if (mask&IO_MB) io_readhandlers[0][port]=handler;
		if (mask&IO_MW) io_readhandlers[1][port]=handler;
		if (mask&IO_MD) io_readhandlers[2][port]=handler;
		port++;
	}
}
void IO_RegisterWriteHandler(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range) {
	while (range--) {
		if (mask&IO_MB) io_writehandlers[0][port]=handler;
		if (mask&IO_MW) io_writehandlers[1][port]=handler;
		if (mask&IO_MD) io_writehandlers[2][port]=handler;
		port++;
	}
}

void IO_FreeReadHandler(Bitu port,Bitu mask,Bitu range) {
	while (range--) {
		if (mask&IO_MB) io_readhandlers[0][port]=IO_ReadDefault;
		if (mask&IO_MW) io_readhandlers[1][port]=IO_ReadDefault;
		if (mask&IO_MD) io_readhandlers[2][port]=IO_ReadDefault;
		port++;
	}
}

void IO_FreeWriteHandler(Bitu port,Bitu mask,Bitu range) {
	while (range--) {
		if (mask&IO_MB) io_writehandlers[0][port]=IO_WriteDefault;
		if (mask&IO_MW) io_writehandlers[1][port]=IO_WriteDefault;
		if (mask&IO_MD) io_writehandlers[2][port]=IO_WriteDefault;
		port++;
	}
}


void IO_Init(Section * sect) {
	IO_FreeReadHandler(0,IO_MA,IO_MAX);
	IO_FreeWriteHandler(0,IO_MA,IO_MAX);
}



