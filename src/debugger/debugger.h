// SPDX-FileCopyrightText:  2002-2025 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DEBUG_H
#define DOSBOX_DEBUG_H

#include "config/config.h"
#include "config/setup.h"
#include "dosbox.h"
#include "hardware/memory.h"

#if C_DEBUGGER

void DEBUG_AddConfigSection(const ConfigPtr& conf);
void DEBUG_Init();
void DEBUG_Destroy();

void DEBUG_DrawScreen();
bool DEBUG_Breakpoint();
bool DEBUG_IntBreakpoint(uint8_t intNum);
void DEBUG_Enable(bool pressed);
void DEBUG_CheckExecuteBreakpoint(uint16_t seg, uint32_t off);
bool DEBUG_ExitLoop(void);
bool DEBUG_IsDebugging();
void DEBUG_RefreshPage(int scroll);
Bitu DEBUG_EnableDebugger();

void LOG_StartUp();
void LOG_Init();
void LOG_Destroy();

extern Bitu cycle_count;
extern Bitu debugCallback;

#endif // C_DEBUGGER

#if C_DEBUGGER && C_HEAVY_DEBUGGER
// True only when something the heavy debugger cares about is armed (a
// breakpoint, instruction/CPU logging, zero-code protection, ...). It lets the
// per-instruction and per-memory-read checks below stay a single inlined branch
// in the CPU cores when nothing is set, instead of an out-of-line call.
extern bool DEBUG_heavy_active;

bool DEBUG_HeavyIsBreakpointImpl();
void DEBUG_HeavyWriteLogInstruction();

template <typename T>
void DEBUG_UpdateMemoryReadBreakpointsImpl(const PhysPt addr);

// Hot path: called once per instruction by the CPU cores.
inline bool DEBUG_HeavyIsBreakpoint()
{
	return DEBUG_heavy_active && DEBUG_HeavyIsBreakpointImpl();
}

// Hot path: called on every guarded memory read.
template <typename T>
inline void DEBUG_UpdateMemoryReadBreakpoints(const PhysPt addr)
{
	if (DEBUG_heavy_active) {
		DEBUG_UpdateMemoryReadBreakpointsImpl<T>(addr);
	}
}

#else

template <typename T>
constexpr void DEBUG_UpdateMemoryReadBreakpoints(const PhysPt)
{
	// no-op
}
#endif // C_DEBUGGER && C_HEAVY_DEBUGGER

#endif // DOSBOX_DEBUG_H
