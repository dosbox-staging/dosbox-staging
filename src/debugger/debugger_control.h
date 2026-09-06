// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DEBUGGER_CONTROL_H
#define DOSBOX_DEBUGGER_CONTROL_H

#include "debugger_models.h"

// Tracks the current debugger stop state plus the next control request to
// consume when execution or a paused loop re-enters the debugger.
class DebugController {
public:
	static DebugController& Instance();

	DebugStopState GetStopState() const;

	int64_t GetCurrentStopSequence() const;
	DebugPauseMode GetResumePauseMode() const;

	void SetResumePauseMode(DebugPauseMode pause_mode);
	void EnterPausedState(DebugPauseMode pause_mode,
	                      DebugStopReason reason,
	                      std::string description,
	                      const CpuRegisters& registers);
	void SetRunningState();

	DebugResult RequestPause(DebugPauseMode pause_mode);
	DebugResult RequestContinue();
	DebugResult RequestStep(DebugStepMode step_mode);

	bool ConsumePauseRequest(DebugPauseMode& pause_mode);
	bool ConsumeContinueRequest();
	bool ConsumeStepRequest(DebugStepMode& step_mode);

private:
	DebugStopState stop_state = {};
	int64_t stop_sequence      = 0;
	DebugPauseMode resume_pause_mode = DebugPauseMode::Interactive;
	bool pause_requested             = false;
	bool continue_requested          = false;
	bool step_requested              = false;
	DebugPauseMode requested_pause_mode = DebugPauseMode::Headless;
	DebugStepMode requested_step_mode   = DebugStepMode::Into;
};

#endif // DOSBOX_DEBUGGER_CONTROL_H
