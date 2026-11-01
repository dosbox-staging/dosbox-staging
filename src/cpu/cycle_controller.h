// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CYCLE_CONTROLLER_H
#define DOSBOX_CYCLE_CONTROLLER_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

// All durations are microseconds. Observations cover disjoint host-time
// intervals; guest_us counts completed guest ticks, not scheduled ticks.
// Work includes main-thread device/rendering work and involuntary stalls,
// but excludes explicit pacing waits. Audio on other threads is deliberately
// absent: this controller estimates the capacity of the emulation thread.
struct CycleObservation {
	int64_t elapsed_us     = 0;
	int64_t work_us        = 0;
	int64_t guest_us       = 0;
	int64_t budget_cycles  = 0;
	int64_t removed_cycles = 0;
	int64_t dropped_us     = 0;
};

struct CycleAdjustment {
	CycleObservation observation = {};
	double load                  = 0.0;
	double guest_rate            = 0.0;
	double active_fraction       = 0.0;
	int cycles                   = 0;
};

// Pure feedback policy, independent of clocks, sleeping, SDL, and CPU globals.
// The caller resets it when changing cores, modes, or cycle settings.
class CycleController {
public:
	void Reset()
	{
		window             = {};
		overloaded_windows = 0;
	}

	std::optional<CycleAdjustment> Observe(const CycleObservation& sample,
	                                       const int cycles,
	                                       const int requested_percent,
	                                       const int cycle_limit,
	                                       const int hard_limit)
	{
		window.elapsed_us += sample.elapsed_us;
		window.work_us += sample.work_us;
		window.guest_us += sample.guest_us;
		window.budget_cycles += sample.budget_cycles;
		window.removed_cycles += sample.removed_cycles;
		window.dropped_us += sample.dropped_us;

		// Host-time windows still expire when emulation is overloaded.
		if (window.elapsed_us < WindowUs || window.guest_us <= 0) {
			return {};
		}

		CycleAdjustment result = {};
		result.observation     = window;
		result.load = static_cast<double>(window.work_us) / window.guest_us;
		result.guest_rate = static_cast<double>(window.guest_us) /
		                    window.elapsed_us;
		result.active_fraction =
		        window.budget_cycles > 0
		                ? std::clamp(1.0 - static_cast<double>(
		                                           window.removed_cycles) /
				                             window.budget_cycles,
				             0.0,
				             1.0)
				: 0.0;

		// Reserve 10% for scheduling jitter and asynchronous consumers.
		// Lower legacy percentage requests retain their requested budget.
		const auto target = std::min(0.90,
		                             std::clamp(requested_percent, 0, 100) /
		                                     100.0);
		const auto max_cycles = cycle_limit > 0
		                              ? std::min(cycle_limit, hard_limit)
		                              : hard_limit;
		const auto min_cycles = std::min(200, max_cycles);
		result.cycles = std::clamp(cycles, min_cycles, max_cycles);

		// A 2% relative dead band prevents chasing measurement noise.
		// Progress is a separate guard: a blocking presentation may
		// have been excluded from work, yet still prevent real-time
		// emulation.
		const auto overloaded = result.load > target * (1.0 + DeadBand) ||
		                        result.guest_rate < MinGuestRate;
		overloaded_windows = overloaded
		                           ? std::min(overloaded_windows + 1, 2)
		                           : 0;

		double factor = 1.0;
		if (overloaded_windows >= 2) {
			factor = result.load > 0.0 ? target / result.load : 1.0;
			if (result.guest_rate < MinGuestRate) {
				factor = std::min(factor, result.guest_rate);
			}
			// Even extreme sustained overload is actionable; never
			// discard it as an assumed external interruption.
			factor = std::clamp(factor, 0.5, 1.0);
		} else if (!overloaded && result.load < target * (1.0 - DeadBand) &&
		           result.active_fraction >= MinActiveFraction &&
		           window.dropped_us == 0) {
			// Idle or I/O-delay-heavy guests cannot tell us how
			// much additional execution the host could sustain. Do
			// not inflate their cycle budget in anticipation of
			// gameplay.
			// Below three quarters of the target load, probe up to
			// twice the current budget. The proportional estimate
			// still caps the increase at the predicted target.
			// This avoids spending seconds climbing from real-mode
			// cycles to the capacity of a modern dynamic core.
			// Near capacity, retain the smaller tracking steps.
			const auto max_increase = result.load < target * ProbeThreshold
			                                ? MaxProbeIncrease
			                                : MaxTrackingIncrease;
			factor = result.load > 0.0 ? target / result.load
			                           : max_increase;
			factor = std::clamp(factor, 1.0, max_increase);
		}

		result.cycles = std::clamp(static_cast<int>(std::lround(
		                                   result.cycles * factor)),
		                           min_cycles,
		                           max_cycles);
		window        = {};
		return result;
	}

private:
	static constexpr int64_t WindowUs           = 100'000;
	static constexpr double DeadBand            = 0.02;
	static constexpr double MinGuestRate        = 0.95;
	static constexpr double MinActiveFraction   = 0.8;
	static constexpr double ProbeThreshold      = 0.75;
	static constexpr double MaxProbeIncrease    = 2.0;
	static constexpr double MaxTrackingIncrease = 1.25;

	CycleObservation window = {};
	int overloaded_windows  = 0;
};

#endif // DOSBOX_CYCLE_CONTROLLER_H
