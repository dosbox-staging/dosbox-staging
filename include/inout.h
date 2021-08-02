/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "dosbox.h"

#include <functional>

using io_port_t = uint16_t; // DOS only supports 16-bit port addresses
using io_val_t = uint32_t; // Handling exists up to a dword (or less)

void IO_WriteB(io_port_t port, uint8_t val);
void IO_WriteW(io_port_t port, uint16_t val);
void IO_WriteD(io_port_t port, uint32_t val);

uint8_t IO_ReadB(io_port_t port);
uint16_t IO_ReadW(io_port_t port);
uint32_t IO_ReadD(io_port_t port);

// type-sized IO handler API
enum class io_width_t : uint8_t {
	byte = sizeof(uint8_t),
	word = sizeof(uint16_t),
	dword = sizeof(uint32_t),
};
constexpr int io_widths = 3; // byte, word, and dword

using io_read_f = std::function<io_val_t(io_port_t port, io_width_t width)>;
using io_write_f = std::function<void(io_port_t port, io_val_t val, io_width_t width)>;

void IO_RegisterReadHandler(io_port_t port,
                            io_read_f handler,
                            io_width_t max_width,
                            io_port_t range = 1);

void IO_RegisterWriteHandler(io_port_t port,
                             io_write_f handler,
                             io_width_t max_width,
                             io_port_t range = 1);

void IO_FreeReadHandler(io_port_t port,
                        io_width_t max_width,
                        io_port_t range = 1);

void IO_FreeWriteHandler(io_port_t port,
                         io_width_t max_width,
                         io_port_t range = 1);

/* Classes to manage the IO objects created by the various devices.
 * The io objects will remove itself on destruction.*/
class IO_Base{
protected:
	bool installed = false;
	io_port_t m_port = 0u;
	io_width_t m_width = io_width_t::byte;
	io_port_t m_range = 0u;
};

class IO_ReadHandleObject: private IO_Base{
public:
	void Install(io_port_t port,
	             io_read_f handler,
	             io_width_t max_width,
	             io_port_t range = 1);

	void Uninstall();
	~IO_ReadHandleObject();
};
class IO_WriteHandleObject: private IO_Base{
public:
	void Install(io_port_t port,
	             io_write_f handler,
	             io_width_t max_width,
	             io_port_t range = 1);

	void Uninstall();
	~IO_WriteHandleObject();
};

static INLINE void IO_Write(io_port_t port, Bit8u val)
{
	IO_WriteB(port,val);
}

static INLINE Bit8u IO_Read(io_port_t port){
	// cast to be dropped after deprecating the Bitu IO handler API
	return (Bit8u)IO_ReadB(port);
}

#endif
