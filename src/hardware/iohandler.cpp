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

#define IO_MAX 1024

static struct IO_Block {
	IO_WriteBHandler * write_b[IO_MAX];
	IO_WriteWHandler * write_w[IO_MAX];
	IO_WriteDHandler * write_d[IO_MAX];

	IO_ReadBHandler * read_b[IO_MAX];
	IO_ReadWHandler * read_w[IO_MAX];
	IO_ReadDHandler * read_d[IO_MAX];
} io;

void IO_WriteB(Bitu port,Bit8u val) {
	if (port<IO_MAX) io.write_b[port](port,val);
	else LOG(LOG_IO,LOG_WARN)("WriteB:Out or range write %X to port %4X",val,port);
}
void IO_WriteW(Bitu port,Bit16u val) {
	if (port<(IO_MAX & ~1)) io.write_w[port](port,val);
	else LOG(LOG_IO,LOG_WARN)("WriteW:Out or range write %X to port %4X",val,port);
}
void IO_WriteD(Bitu port,Bit32u val) {
	if (port<(IO_MAX & ~3)) io.write_d[port](port,val);
	else LOG(LOG_IO,LOG_WARN)("WriteD:Out or range write %X to port %4X",val,port);
}

Bit8u IO_ReadB(Bitu port) {
	if (port<IO_MAX) return io.read_b[port](port);
	else LOG(LOG_IO,LOG_WARN)("ReadB:Out or range read from port %4X",port);
	return 0xff;
}
Bit16u IO_ReadW(Bitu port) {
	if (port<(IO_MAX & ~1)) return io.read_w[port](port);
	else LOG(LOG_IO,LOG_WARN)("ReadW:Out or range read from port %4X",port);
	return 0xffff;
}
Bit32u IO_ReadD(Bitu port) {
	if (port<(IO_MAX & ~3)) return io.read_d[port](port);
	else LOG(LOG_IO,LOG_WARN)("ReadD:Out or range read from port %4X",port);
	return 0xffffffff;
}

static Bit8u IO_ReadBBlocked(Bit32u port) {
	return 0xff;
}
static void IO_WriteBBlocked(Bit32u port,Bit8u val) {
}

static Bit8u IO_ReadDefaultB(Bit32u port) {
	LOG(LOG_IO,LOG_WARN)("Reading from undefined port %04X",port);
	io.read_b[port]=IO_ReadBBlocked;
	return 0xff;	
}
static Bit16u IO_ReadDefaultW(Bit32u port) {
	return io.read_b[port](port) | (io.read_b[port+1](port+1) << 8);
}
static Bit32u IO_ReadDefaultD(Bit32u port) {
	return io.read_b[port](port) | (io.read_b[port+1](port+1) << 8) |
		(io.read_b[port+2](port+2) << 16) | (io.read_b[port+3](port+3) << 24);
}

void IO_WriteDefaultB(Bit32u port,Bit8u val) {
	LOG(LOG_IO,LOG_WARN)("Writing %02X to undefined port %04X",static_cast<Bit32u>(val),port);		
	io.write_b[port]=IO_WriteBBlocked;
}
void IO_WriteDefaultW(Bit32u port,Bit16u val) {
	io.write_b[port](port,(Bit8u)val);
	io.write_b[port+1](port+1,(Bit8u)(val>>8));
}
void IO_WriteDefaultD(Bit32u port,Bit32u val) {
	io.write_b[port](port,(Bit8u)val);
	io.write_b[port+1](port+1,(Bit8u)(val>>8));
	io.write_b[port+2](port+2,(Bit8u)(val>>16));
	io.write_b[port+3](port+3,(Bit8u)(val>>24));
}

void IO_RegisterReadBHandler(Bitu port,IO_ReadBHandler * handler) {
	if (port>=IO_MAX) return;
	io.read_b[port]=handler;
}
void IO_RegisterReadWHandler(Bitu port,IO_ReadWHandler * handler) {
	if (port>=IO_MAX) return;
	io.read_w[port]=handler;
}
void IO_RegisterReadDHandler(Bitu port,IO_ReadDHandler * handler) {
	if (port>=IO_MAX) return;
	io.read_d[port]=handler;
}
void IO_RegisterWriteBHandler(Bitu port,IO_WriteBHandler * handler) {
	if (port>=IO_MAX) return;
	io.write_b[port]=handler;
}
void IO_RegisterWriteWHandler(Bitu port,IO_WriteWHandler * handler) {
	if (port>=IO_MAX) return;
	io.write_w[port]=handler;
}
void IO_RegisterWriteDHandler(Bitu port,IO_WriteDHandler * handler) {
	if (port>=IO_MAX) return;
	io.write_d[port]=handler;
}


void IO_FreeReadHandler(Bitu port) {
	if (port>=IO_MAX) return;
	io.read_b[port]=IO_ReadDefaultB;
	io.read_w[port]=IO_ReadDefaultW;
	io.read_d[port]=IO_ReadDefaultD;
}
void IO_FreeWriteHandler(Bitu port) {
	if (port>=IO_MAX) return;
	io.write_b[port]=IO_WriteDefaultB;
	io.write_w[port]=IO_WriteDefaultW;
	io.write_d[port]=IO_WriteDefaultD;
}

void IO_Init(Section * sect) {
	for (Bitu i=0;i<IO_MAX;i++) {
		io.read_b[i]=IO_ReadDefaultB;
		io.read_w[i]=IO_ReadDefaultW;
		io.read_d[i]=IO_ReadDefaultD;
		io.write_b[i]=IO_WriteDefaultB;
		io.write_w[i]=IO_WriteDefaultW;
		io.write_d[i]=IO_WriteDefaultD;
	}
}



