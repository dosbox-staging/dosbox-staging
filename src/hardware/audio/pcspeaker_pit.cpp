// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/pcspeaker_pit.h"

#include "utils/checks.h"

#include <algorithm>

CHECK_NARROWING();

void PitCounter::Emit(const OutputState state, const float offset_ms,
                      std::vector<Transition>& out)
{
	if (state == output_state) {
		return;
	}
	output_state = state;
	out.emplace_back(Transition{offset_ms, state});
}

// Intel 8254 / 82C54 datasheet, mode definitions:
//
//   Mode 0: output LOW on control word write; counting starts after count
//           load (gate high). Output goes HIGH at terminal count and latches.
//   Mode 1: output HIGH on control word write; counting started by gate
//           rising edge, not by count load. Output LOW during count, HIGH at TC.
//   Mode 2: output HIGH on control word write; LOW pulse (1 clock) at
//           terminal count every N clocks. Gate low forces output HIGH and
//           freezes count; gate rising edge restarts.
//   Mode 3: output HIGH on control word write; square wave, HIGH for ceil(N/2)
//           clocks, LOW for floor(N/2) clocks. New count takes effect at next
//           half-period boundary. Gate same as mode 2.
//   Mode 4: output HIGH on control word write; like mode 0 start, but
//           generates a single 1-clock LOW pulse at TC instead of latching.
//   Mode 5: gate-triggered like mode 1, output like mode 4.

void PitCounter::Invalidate()
{
	// Like a gate-low event: mode 3 forces output HIGH when counting stops.
	// Without this, output_state is frozen mid-cycle (often LOW), which
	// causes AddImpulse to drop the next rising edge via deduplication.
	if (mode3_active) {
		output_state = OutputState::High;
	}
	mode3_active       = false;
	mode3_retain_phase = true;
	counting           = false;
}

PitMode PitCounter::Canonical(const PitMode m)
{
	switch (m) {
	case PitMode::RateGeneratorAlias: return PitMode::RateGenerator;
	case PitMode::SquareWaveAlias: return PitMode::SquareWave;
	default: return m;
	}
}

std::vector<PitCounter::Transition> PitCounter::WriteControl(const PitMode new_mode)
{
	std::vector<Transition> out;
	mode     = Canonical(new_mode);
	counting = false;
	phase_ms = 0.0f;

	switch (mode) {
	case PitMode::InterruptOnTerminalCount:
		// Output goes LOW immediately on control word write
		Emit(OutputState::Low, 0.0f, out);
		break;

	case PitMode::OneShot:
	case PitMode::HardwareStrobe:
		// Output goes HIGH immediately; count load arms the trigger
		Emit(OutputState::High, 0.0f, out);
		mode1_awaiting_count   = true;
		mode1_awaiting_trigger = false;
		break;

	case PitMode::RateGenerator:
	case PitMode::SquareWave:
	case PitMode::SoftwareStrobe:
		// Output goes HIGH immediately
		Emit(OutputState::High, 0.0f, out);
		mode3_active       = false;
		mode3_retain_phase = false;
		break;

	default: break;
	}

	return out;
}

std::vector<PitCounter::Transition> PitCounter::WriteCount(const int count)
{
	std::vector<Transition> out;

	// Per 8254 spec (Figure 22): count 0 is equivalent to 65536 (2^16)
	const int effective_count = (count == 0) ? 0x10000 : count;
	const auto duration_ms = MsPerTick * static_cast<float>(effective_count);

	switch (mode) {
	case PitMode::InterruptOnTerminalCount:
		// Per 8254: after TC, a new count write forces output LOW again.
		if (output_state == OutputState::High) {
			Emit(OutputState::Low, 0.0f, out);
		}
		phase_ms  = 0.0f;
		period_ms = duration_ms;
		counting  = gate;
		break;

	case PitMode::SoftwareStrobe:
		phase_ms  = 0.0f;
		period_ms = duration_ms;
		counting  = gate;
		break;

	case PitMode::OneShot:
	case PitMode::HardwareStrobe:
		mode1_pending_ms = duration_ms;
		if (mode1_awaiting_count) {
			mode1_awaiting_count   = false;
			mode1_awaiting_trigger = true;
		}
		break;

	case PitMode::RateGenerator:
		new_period_ms = duration_ms;
		if (!counting) {
			period_ms = new_period_ms;
			half_ms   = std::max(period_ms - MsPerTick, 0.0f);
			if (gate) {
				phase_ms = 0.0f;
				counting = true;
				Emit(OutputState::High, 0.0f, out);
			}
		}
		// If already counting, new period takes effect at the next
		// period boundary via Advance (same deferred-update pattern as
		// Mode 3).
		break;

	case PitMode::SquareWave:
		new_period_ms = duration_ms;
		new_half_ms   = MsPerTick * static_cast<float>((count + 1) / 2);
		if (!mode3_active) {
			period_ms = new_period_ms;
			half_ms   = new_half_ms;
			if (gate) {
				if (!mode3_retain_phase) {
					phase_ms = 0.0f;
					Emit(OutputState::High, 0.0f, out);
				}
				mode3_active       = true;
				mode3_retain_phase = false;
			}
		}
		break;

	default: break;
	}

	return out;
}

std::vector<PitCounter::Transition> PitCounter::EnableGate(const bool new_gate)
{
	std::vector<Transition> out;
	const bool rising  = !gate && new_gate;
	const bool falling = gate && !new_gate;
	gate               = new_gate;

	switch (mode) {
	case PitMode::InterruptOnTerminalCount:
	case PitMode::SoftwareStrobe:
		// Gate low freezes count; gate high resumes
		if (rising && !counting && period_ms > 0.0f) {
			counting = true;
		} else if (falling) {
			counting = false;
		}
		break;

	case PitMode::OneShot:
		// Gate rising edge triggers / retriggers; OUT goes LOW immediately
		if (rising && !mode1_awaiting_count && !mode1_awaiting_trigger) {
			phase_ms  = 0.0f;
			period_ms = mode1_pending_ms;
			counting  = true;
			Emit(OutputState::Low, 0.0f, out);
		} else if (rising && mode1_awaiting_trigger) {
			phase_ms               = 0.0f;
			period_ms              = mode1_pending_ms;
			mode1_awaiting_trigger = false;
			counting               = true;
			Emit(OutputState::Low, 0.0f, out);
		}
		// Gate falling has no effect once counting
		break;

	case PitMode::HardwareStrobe:
		// Gate rising edge triggers / retriggers; OUT stays HIGH until
		// TC (spec: "GATE has no effect on OUT" for mode 5)
		if (rising && !mode1_awaiting_count && !mode1_awaiting_trigger) {
			phase_ms  = 0.0f;
			period_ms = mode1_pending_ms;
			counting  = true;
		} else if (rising && mode1_awaiting_trigger) {
			phase_ms               = 0.0f;
			period_ms              = mode1_pending_ms;
			mode1_awaiting_trigger = false;
			counting               = true;
		}
		// Gate falling has no effect once counting
		break;

	case PitMode::RateGenerator:
		if (falling) {
			// Gate low forces output HIGH and freezes count
			counting = false;
			Emit(OutputState::High, 0.0f, out);
		} else if (rising) {
			// Gate rising restarts from the beginning of the
			// period; pick up any count written while gate was low.
			phase_ms  = 0.0f;
			period_ms = new_period_ms;
			half_ms   = std::max(period_ms - MsPerTick, 0.0f);
			counting  = true;
			Emit(OutputState::High, 0.0f, out);
		}
		break;

	case PitMode::SquareWave:
		if (falling) {
			mode3_active       = false;
			mode3_retain_phase = false;
			Emit(OutputState::High, 0.0f, out);
		} else if (rising) {
			phase_ms           = 0.0f;
			period_ms          = new_period_ms;
			half_ms            = new_half_ms;
			mode3_active       = true;
			mode3_retain_phase = false;
			Emit(OutputState::High, 0.0f, out);
		}
		break;

	default: break;
	}

	return out;
}

void PitCounter::AdvanceCountdown(const float duration_ms,
                                  std::vector<Transition>& out)
{
	// Modes 0 and 1: output goes HIGH once at terminal count and latches.
	if (!counting) {
		return;
	}
	if (phase_ms + duration_ms >= period_ms) {
		const auto delay = period_ms - phase_ms;
		Emit(OutputState::High, delay, out);
		counting = false;
		phase_ms = period_ms;
	} else {
		phase_ms += duration_ms;
	}
}

void PitCounter::AdvanceStrobe(const float duration_ms, std::vector<Transition>& out)
{
	// Modes 4 and 5: a single one-clock LOW pulse at terminal count, then
	// the output returns HIGH. Counting stops afterwards either way.
	if (!counting) {
		return;
	}
	if (phase_ms + duration_ms >= period_ms) {
		const auto delay = period_ms - phase_ms;
		Emit(OutputState::Low, delay, out);              // LOW strobe
		Emit(OutputState::High, delay + MsPerTick, out); // return HIGH
		counting = false;
		phase_ms = period_ms;
	} else {
		phase_ms += duration_ms;
	}
}

void PitCounter::AdvanceOscillator(float duration_ms,
                                   std::vector<Transition>& out, const bool square)
{
	// Modes 2 (rate generator) and 3 (square wave) share the same two-phase
	// structure: a HIGH segment of half_ms followed by a LOW segment until
	// period_ms. They differ only in how a pending count reload is applied:
	//   - Mode 2: reloads at the full-period boundary, and half_ms is fixed
	//     at period_ms - one clock (the LOW segment is a single clock pulse).
	//   - Mode 3: reloads at either half-period boundary, with an explicit
	//     new_half_ms (a true square wave).
	const bool active = square ? mode3_active : counting;
	if (!active) {
		return;
	}

	auto time_base = 0.0f;
	while (duration_ms > 0.0f) {
		if (phase_ms >= half_ms) {
			// LOW segment, ending at the full-period boundary
			if (phase_ms + duration_ms >= period_ms) {
				const auto delay = period_ms - phase_ms;
				time_base += delay;
				duration_ms -= delay;
				phase_ms  = 0.0f;
				period_ms = new_period_ms;
				half_ms = square ? new_half_ms
				                 : std::max(period_ms - MsPerTick,
				                            0.0f);
				Emit(OutputState::High, time_base, out); // go HIGH
			} else {
				phase_ms += duration_ms;
				duration_ms = 0.0f;
			}
		} else {
			// HIGH segment, ending at the half-period boundary
			if (phase_ms + duration_ms >= half_ms) {
				const auto delay = half_ms - phase_ms;
				time_base += delay;
				duration_ms -= delay;
				phase_ms = half_ms;
				if (square) {
					period_ms = new_period_ms;
					half_ms   = new_half_ms;
				}
				Emit(OutputState::Low, time_base, out); // go LOW
			} else {
				phase_ms += duration_ms;
				duration_ms = 0.0f;
			}
		}
	}
}

std::vector<PitCounter::Transition> PitCounter::Advance(const float duration_ms)
{
	std::vector<Transition> out;

	switch (mode) {
	case PitMode::InterruptOnTerminalCount:
	case PitMode::OneShot: AdvanceCountdown(duration_ms, out); break;

	case PitMode::HardwareStrobe:
	case PitMode::SoftwareStrobe: AdvanceStrobe(duration_ms, out); break;

	case PitMode::RateGenerator:
		AdvanceOscillator(duration_ms, out, /*square=*/false);
		break;

	case PitMode::SquareWave:
		AdvanceOscillator(duration_ms, out, /*square=*/true);
		break;

	default: break;
	}

	return out;
}
