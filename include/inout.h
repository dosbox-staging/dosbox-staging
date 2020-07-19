/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef DOSBOX_INOUT_H
#define DOSBOX_INOUT_H

#include <functional>
#include <unordered_map>

#define IO_MB	0x1 // Byte (8-bit)
#define IO_MW	0x2 // Word (16-bit)
#define IO_MD	0x4 // DWord (32-bit)
#define IO_MA	(IO_MB | IO_MW | IO_MD ) // All three
#define IO_SIZES 3 // byte, word, and dword

// Existing type sizes
using io_port_t = Bitu;
using io_val_t = Bitu;

// Proposed type types
using io_port_t_proposed = uint16_t; // DOS only supports 16-bit port addresses
using io_val_t_proposed = uint32_t; // Handling exists up to a dword (or less)

using IO_ReadHandler = std::function<Bitu(io_port_t port, Bitu iolen)>;
using IO_WriteHandler = std::function<void(io_port_t port, io_val_t val, Bitu iolen)>;

extern std::unordered_map<io_port_t, IO_WriteHandler> io_writehandlers[IO_SIZES];
extern std::unordered_map<io_port_t, IO_ReadHandler> io_readhandlers[IO_SIZES];

void IO_RegisterReadHandler(io_port_t port, IO_ReadHandler handler, Bitu mask, Bitu range = 1);
void IO_RegisterWriteHandler(io_port_t port,
                             IO_WriteHandler handler,
                             Bitu mask,
                             Bitu range = 1);

void IO_FreeReadHandler(io_port_t port, Bitu mask, Bitu range = 1);
void IO_FreeWriteHandler(io_port_t port, Bitu mask, Bitu range = 1);

void IO_WriteB(io_port_t port, io_val_t val);
void IO_WriteW(io_port_t port, io_val_t val);
void IO_WriteD(io_port_t port, io_val_t val);

io_val_t IO_ReadB(io_port_t port);
io_val_t IO_ReadW(io_port_t port);
io_val_t IO_ReadD(io_port_t port);

/* Classes to manage the IO objects created by the various devices.
 * The io objects will remove itself on destruction.*/
class IO_Base{
protected:
	bool installed = false;
	io_port_t m_port = 0u;
	Bitu m_mask = 0u;
	Bitu m_range = 0u;
};

class IO_ReadHandleObject: private IO_Base{
public:
	void Install(io_port_t port, IO_ReadHandler handler, Bitu mask, Bitu range = 1);
	void Uninstall();
	~IO_ReadHandleObject();
};
class IO_WriteHandleObject: private IO_Base{
public:
	void Install(io_port_t port, IO_WriteHandler handler, Bitu mask, Bitu range = 1);
	void Uninstall();
	~IO_WriteHandleObject();
};

static INLINE void IO_Write(io_port_t port, Bit8u val)
{
	IO_WriteB(port,val);
}
static INLINE Bit8u IO_Read(io_port_t port){
	return (Bit8u)IO_ReadB(port);
}

#endif
