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
void DEBUG_Destroy(Section* section);

void DEBUG_DrawScreen();
bool DEBUG_Breakpoint();
bool DEBUG_IntBreakpoint(uint8_t intNum);
void DEBUG_Enable(bool pressed);
void DEBUG_CheckExecuteBreakpoint(uint16_t seg, uint32_t off);
bool DEBUG_ExitLoop(void);
void DEBUG_RefreshPage(int scroll);
Bitu DEBUG_EnableDebugger();

void LOG_StartUp();

extern Bitu cycle_count;
extern Bitu debugCallback;

#endif // C_DEBUGGER

#if C_DEBUGGER && C_HEAVY_DEBUGGER
bool DEBUG_HeavyIsBreakpoint();
void DEBUG_HeavyWriteLogInstruction();

template <typename T>
void DEBUG_UpdateMemoryReadBreakpoints(const PhysPt addr);

#else

template <typename T>
constexpr void DEBUG_UpdateMemoryReadBreakpoints(const PhysPt)
{
	// no-op
}
#endif // C_DEBUGGER && C_HEAVY_DEBUGGER

#endif // DOSBOX_DEBUG_H
