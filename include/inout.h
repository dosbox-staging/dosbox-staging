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

typedef Bit8u (IO_ReadHandler)(Bit32u port);
typedef void (IO_WriteHandler)(Bit32u port,Bit8u value);

#define IO_MAX 1024

struct IO_ReadBlock{
	IO_ReadHandler * handler;
	char * name;
};

struct IO_WriteBlock{
	IO_WriteHandler * handler;
	char * name;
};

extern IO_ReadBlock IO_ReadTable[IO_MAX];
extern IO_WriteBlock IO_WriteTable[IO_MAX];



void IO_Write(Bitu num,Bit8u val);
Bit8u IO_Read(Bitu num);

void IO_RegisterReadHandler(Bit32u port,IO_ReadHandler * handler,char * name);
void IO_RegisterWriteHandler(Bit32u port,IO_WriteHandler * handler,char * name);

void IO_FreeReadHandler(Bit32u port);
void IO_FreeWriteHandler(Bit32u port);


