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

#define IO_MAX (64*1024+3)

#define IO_MB	0x1
#define IO_MW	0x2
#define IO_MD	0x4
#define IO_MA	(IO_MB | IO_MW | IO_MD )

typedef Bitu IO_ReadHandler(Bitu port,Bitu iolen);
typedef void IO_WriteHandler(Bitu port,Bitu val,Bitu iolen);

extern IO_WriteHandler * io_writehandlers[3][IO_MAX];
extern IO_ReadHandler * io_readhandlers[3][IO_MAX];

void IO_RegisterReadHandler(Bitu port,IO_ReadHandler * handler,Bitu mask,Bitu range=1);
void IO_RegisterWriteHandler(Bitu port,IO_WriteHandler * handler,Bitu mask,Bitu range=1);

void IO_FreeReadHandler(Bitu port,Bitu mask,Bitu range=0);
void IO_FreeWriteHandler(Bitu port,Bitu mask,Bitu range=0);

INLINE void IO_WriteB(Bitu port,Bitu val) {
	io_writehandlers[0][port](port,val,1);
};
INLINE void IO_WriteW(Bitu port,Bitu val) {
	io_writehandlers[1][port](port,val,2);
};
INLINE void IO_WriteD(Bitu port,Bitu val) {
	io_writehandlers[2][port](port,val,4);
};

INLINE Bitu IO_ReadB(Bitu port) {
	return io_readhandlers[0][port](port,1);
}
INLINE Bitu IO_ReadW(Bitu port) {
	return io_readhandlers[1][port](port,2);
}
INLINE Bitu IO_ReadD(Bitu port) {
	return io_readhandlers[2][port](port,4);
}

INLINE void IO_Write(Bitu port,Bit8u val) {
	IO_WriteB(port,val);
}
INLINE Bit8u IO_Read(Bitu port){
	return IO_ReadB(port);
}


