// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DEBUG_H
#define DOSBOX_DEBUG_H

#include "dosbox.h"
#include "hardware/memory.h"

#if C_DEBUG
void DEBUG_DrawScreen();
bool DEBUG_Breakpoint();
bool DEBUG_IntBreakpoint(uint8_t intNum);
void DEBUG_Enable(bool pressed);
void DEBUG_CheckExecuteBreakpoint(uint16_t seg, uint32_t off);
bool DEBUG_ExitLoop(void);
void DEBUG_RefreshPage(int scroll);
Bitu DEBUG_EnableDebugger();

extern Bitu cycle_count;
extern Bitu debugCallback;
#else  // Empty debugging replacements
#endif // C_DEBUG

#if C_DEBUG && C_HEAVY_DEBUG
bool DEBUG_HeavyIsBreakpoint();
void DEBUG_HeavyWriteLogInstruction();
template <typename T>
void DEBUG_UpdateMemoryReadBreakpoints(const PhysPt addr);
#else  // Empty heavy debugging replacements
template <typename T>
constexpr void DEBUG_UpdateMemoryReadBreakpoints(const PhysPt)
{ /* no-op */
}
#endif // C_DEBUG && C_HEAVY_DEBUG

#endif // DOSBOX_DEBUG_H
