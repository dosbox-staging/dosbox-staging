// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "debugger_control.h"

DebugController& DebugController::Instance()
{
	static DebugController instance = {};
	return instance;
}

DebugStopState DebugController::GetStopState() const
{
	return stop_state;
}

int64_t DebugController::GetCurrentStopSequence() const
{
	return stop_sequence;
}

DebugPauseMode DebugController::GetResumePauseMode() const
{
	return resume_pause_mode;
}

void DebugController::SetResumePauseMode(const DebugPauseMode pause_mode)
{
	resume_pause_mode = pause_mode;
}

void DebugController::EnterPausedState(const DebugPauseMode pause_mode,
                                       const DebugStopReason reason,
                                       std::string description,
                                       const CpuRegisters& registers)
{
	stop_state.running            = false;
	stop_state.paused             = true;
	stop_state.pause_mode         = pause_mode;
	stop_state.reason             = reason;
	stop_state.description        = std::move(description);
	stop_state.registers          = registers;
	stop_state.stop_sequence      = ++stop_sequence;

	pause_requested    = false;
	continue_requested = false;
	step_requested     = false;
	resume_pause_mode  = pause_mode;
}

void DebugController::SetRunningState()
{
	stop_state.running            = true;
	stop_state.paused             = false;
	stop_state.reason             = DebugStopReason::None;
	stop_state.description.clear();
}

DebugResult DebugController::RequestPause(const DebugPauseMode pause_mode)
{
	requested_pause_mode = pause_mode;
	pause_requested      = true;
	resume_pause_mode    = pause_mode;
	return {};
}

DebugResult DebugController::RequestContinue()
{
	if (!stop_state.paused) {
		return {false, "debugger is not paused"};
	}
	continue_requested = true;
	return {};
}

DebugResult DebugController::RequestStep(const DebugStepMode step_mode)
{
	if (!stop_state.paused) {
		return {false, "debugger is not paused"};
	}
	if (step_mode != DebugStepMode::Into) {
		return {false, "unsupported step mode"};
	}
	requested_step_mode = step_mode;
	step_requested      = true;
	return {};
}

bool DebugController::ConsumePauseRequest(DebugPauseMode& pause_mode)
{
	if (!pause_requested) {
		return false;
	}
	pause_mode       = requested_pause_mode;
	pause_requested  = false;
	return true;
}

bool DebugController::ConsumeContinueRequest()
{
	if (!continue_requested) {
		return false;
	}
	continue_requested = false;
	return true;
}

bool DebugController::ConsumeStepRequest(DebugStepMode& step_mode)
{
	if (!step_requested) {
		return false;
	}
	step_mode      = requested_step_mode;
	step_requested = false;
	return true;
}
