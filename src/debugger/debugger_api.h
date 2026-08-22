// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DEBUGGER_API_H
#define DOSBOX_DEBUGGER_API_H

#include "config/config.h"
#include "debugger_models.h"

#if C_DEBUGGER

DebugStopState DEBUG_GetStopState();
DebugResult DEBUG_RequestPause(DebugPauseMode pause_mode);
DebugResult DEBUG_RequestContinue();
DebugResult DEBUG_RequestStep(DebugStepMode step_mode);

#endif // C_DEBUGGER

#endif // DOSBOX_DEBUGGER_API_H
