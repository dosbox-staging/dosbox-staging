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

typedef Bit8u  (IO_ReadBHandler)(Bit32u port);
typedef Bit16u (IO_ReadWHandler)(Bit32u port);
typedef Bit32u (IO_ReadDHandler)(Bit32u port);
typedef void (IO_WriteBHandler)(Bit32u port,Bit8u value);
typedef void (IO_WriteWHandler)(Bit32u port,Bit16u value);
typedef void (IO_WriteDHandler)(Bit32u port,Bit32u value);

void IO_RegisterReadBHandler(Bitu port,IO_ReadBHandler * handler);
void IO_RegisterReadWHandler(Bitu port,IO_ReadWHandler * handler);
void IO_RegisterReadDHandler(Bitu port,IO_ReadDHandler * handler);

void IO_RegisterWriteBHandler(Bitu port,IO_WriteBHandler * handler);
void IO_RegisterWriteWHandler(Bitu port,IO_WriteWHandler * handler);
void IO_RegisterWriteDHandler(Bitu port,IO_WriteDHandler * handler);

void IO_FreeReadHandler(Bitu port);
void IO_FreeWriteHandler(Bitu port);

void IO_WriteB(Bitu port,Bit8u val);
Bit8u IO_ReadB(Bitu port);
void IO_WriteW(Bitu port,Bit16u val);
Bit16u IO_ReadW(Bitu port);
void IO_WriteD(Bitu port,Bit32u val);
Bit32u IO_ReadD(Bitu port);

INLINE void IO_Write(Bitu port,Bit8u val) {
	IO_WriteB(port,val);
}
INLINE Bit8u IO_Read(Bitu port){
	return IO_ReadB(port);
}

INLINE void IO_RegisterReadHandler(Bitu port,IO_ReadBHandler * handler,char * name) {
	IO_RegisterReadBHandler(port,handler);
}
INLINE void IO_RegisterWriteHandler(Bitu port,IO_WriteBHandler * handler,char * name) {
	IO_RegisterWriteBHandler(port,handler);
}


