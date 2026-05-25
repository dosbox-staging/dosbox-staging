// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PCSPEAKER_PIT_H
#define DOSBOX_PCSPEAKER_PIT_H

#include "hardware/timer.h"

#include <vector>

enum class OutputState { High, Low };

// Pure hardware emulation of PIT counter 2 as wired to the PC speaker.
//
// Tracks output transitions in continuous time (milliseconds). Callers drive
// it with WriteControl / WriteCount / EnableGate, interleaved with Advance()
// calls to collect the transitions that occurred during each time window.
//
// All offsets in Transition are relative to the start of the Advance() call
// that produced them.
class PitCounter {
public:
	struct Transition {
		float offset_ms    = 0.0f;
		OutputState output = OutputState::High;
	};

	// Control word written. Output changes immediately; no count loaded yet.
	std::vector<Transition> WriteControl(const PitMode mode);

	// Count register written. May start counting depending on mode + gate.
	std::vector<Transition> WriteCount(const int count);

	// Gate signal changed (port B bit 0).
	std::vector<Transition> EnableGate(const bool gate);

	// Advance by duration_ms, returning transitions that occurred within.
	std::vector<Transition> Advance(const float duration_ms);

	// Stop counting without changing mode. Used when a count arrives that
	// can't be represented (e.g. ultrasonic mode 3 reload).
	void Invalidate();

	OutputState GetOutputState() const
	{
		return output_state;
	}

	PitMode GetMode() const
	{
		return mode;
	}

	bool IsMode3Active() const
	{
		return mode3_active;
	}

	bool IsGateEnabled() const
	{
		return gate;
	}

private:
	void Emit(const OutputState state, const float offset_ms,
	          std::vector<Transition>& out);

	// Per-mode Advance() helpers (dispatched on the canonical mode).
	void AdvanceCountdown(const float duration_ms, std::vector<Transition>& out);
	void AdvanceStrobe(const float duration_ms, std::vector<Transition>& out);
	void AdvanceOscillator(float duration_ms, std::vector<Transition>& out,
	                       const bool square);

	// Modes 6 and 7 are hardware aliases of modes 2 and 3; fold them so the
	// rest of the class only ever deals with RateGenerator / SquareWave.
	static PitMode Canonical(const PitMode m);

	// A programmed count of 0 reloads the full 16-bit range (65536 ticks).
	static constexpr int MaxDecCount = 0x10000;

	static constexpr float MsPerTick   = 1000.0f / PIT_TICK_RATE;
	static constexpr float MaxPeriodMs = MsPerTick * MaxDecCount;

	PitMode mode             = PitMode::SquareWave;
	bool gate                = false;
	OutputState output_state = OutputState::High;
	bool counting            = false;

	float phase_ms      = 0.0f;
	float period_ms     = MaxPeriodMs;
	float half_ms       = MaxPeriodMs / 2.0f;
	float new_period_ms = MaxPeriodMs;
	float new_half_ms   = MaxPeriodMs / 2.0f;

	// Mode 1 (OneShot)
	bool mode1_awaiting_count   = false;
	bool mode1_awaiting_trigger = false;
	float mode1_pending_ms      = 0.0f;

	// Mode 3 (SquareWave): gate must have been high to begin oscillating
	bool mode3_active = false;
	// Set by Invalidate() so WriteCount can resume without resetting phase
	bool mode3_retain_phase = false;
};

#endif // DOSBOX_PCSPEAKER_PIT_H
