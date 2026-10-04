// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DEBUGGER_MODELS_H
#define DOSBOX_DEBUGGER_MODELS_H

#include <cstdint>
#include <string>
#include <vector>

#include "cpu/cpu_snapshot.h"

enum class DebugPauseMode {
	Interactive,
	Headless,
};

enum class DebugStopReason {
	None,
	StepComplete,
	PauseRequested,
};

enum class DebugStepMode {
	Into,
};

struct DebugResult {
	bool ok          = true;
	std::string error = {};
};

struct DebugStopState {
	bool running              = true;
	bool paused               = false;
	DebugPauseMode pause_mode = DebugPauseMode::Interactive;
	int64_t stop_sequence     = 0;
	DebugStopReason reason    = DebugStopReason::None;
	std::string description   = {};
	CpuRegisters registers    = {};
};

#endif // DOSBOX_DEBUGGER_MODELS_H
