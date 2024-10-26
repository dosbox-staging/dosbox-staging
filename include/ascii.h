/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_ASCII_H
#define DOSBOX_ASCII_H

#include <cstdint>

namespace Ascii {

enum ControlCode : uint8_t {
	// Control characters
	Null           = 0x00,
	CtrlC          = 0x03,
	Backspace      = 0x08,
	LineFeed       = 0x0a,
	FormFeed       = 0x0c,
	CarriageReturn = 0x0d,
	Escape         = 0x1b,
	Delete         = 0x7f,
	Extended       = 0xe0,
};

}

#endif
