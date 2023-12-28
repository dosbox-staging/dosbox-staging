/*
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

#ifndef DOSBOX_DEBUG_H
#define DOSBOX_DEBUG_H

#include "dosbox.h"
#include "mem.h"

void DEBUG_DrawScreen();
bool DEBUG_Breakpoint();
bool DEBUG_IntBreakpoint(uint8_t intNum);
void DEBUG_Enable(bool pressed);
void DEBUG_CheckExecuteBreakpoint(uint16_t seg, uint32_t off);
bool DEBUG_ExitLoop(void);
void DEBUG_RefreshPage(int scroll);
Bitu DEBUG_EnableDebugger();
void UpdateMemoryReadBreakpointsImpl(const PhysPt addr, std::size_t read_size);

extern Bitu cycle_count;
extern Bitu debugCallback;

#if C_HEAVY_DEBUG
bool DEBUG_HeavyIsBreakpoint();
void DEBUG_HeavyWriteLogInstruction();
#endif // C_HEAVY_DEBUG

template <typename T>
inline void DEBUG_UpdateMemoryReadBreakpoints(const PhysPt addr)
{
	static_assert(std::is_unsigned_v<T>);
	static_assert(std::is_integral_v<T>);

	UpdateMemoryReadBreakpointsImpl(addr, sizeof(T));
}

#endif
