/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_INTEL_8042_H
#define DOSBOX_INTEL_8042_H

#include <cstdint>
#include <vector>

void I8042_Init();

// Functions for keyboard and mouse emulation code, to provide
// command responses ('AddByte') and key scancodes / mouse updates
// ('AddFrame')

void I8042_AddAuxByte(const uint8_t byte);
void I8042_AddAuxFrame(const std::vector<uint8_t>& bytes);
void I8042_AddKbdByte(const uint8_t byte);
void I8042_AddKbdFrame(const std::vector<uint8_t>& bytes);

bool I8042_IsReadyForAuxFrame();
bool I8042_IsReadyForKbdFrame();

// The following routines needs to be implemented by keyboard/mouse
// emulation code:

void KEYBOARD_PortWrite(const uint8_t byte);
bool MOUSEPS2_PortWrite(const uint8_t byte); // false if mouse not connected

void KEYBOARD_NotifyReadyForFrame();
void MOUSEPS2_NotifyReadyForFrame();

#endif // DOSBOX_INTEL_8042_H
