/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "pcspeaker.h"

#include <array>

#include "mixer.h"
#include "pic.h"
#include "setup.h"
#include "support.h"
#include "timer.h"

class PcSpeakerDiscrete final : public PcSpeaker {
public:
	PcSpeakerDiscrete();
	~PcSpeakerDiscrete() final;

	void SetFilterState(const FilterState filter_state) final;
	void SetCounter(const int cntr, const PitMode m) final;
	void SetPITControl(const PitMode) final {}
	void SetType(const PpiPortB &b) final;

private:
	void ChannelCallback(const uint16_t len);
	void AddDelayEntry(const double index, double vol);
	void ForwardPIT(const double newindex);
	bool IsWaveSquare() const;
	double NeutralOr(const double fallback) const;
	double NeutralLastPitOr(const double fallback) const;
	void PlayOrSleep(const uint16_t speaker_movements,
	                 size_t requested_samples, int16_t *buffer);

	// Constants
	static constexpr char device_name[] = "PCSPEAKER";
	static constexpr char model_name[]  = "discrete";

	// RMS levels for the pulse-width-modulation (PWM) and the square wave
	static constexpr double pwm_scalar = 0.9 * M_SQRT1_2;
	static constexpr double sqw_scalar = pwm_scalar / 2.0;

	// Amplitude constants
	static constexpr double amp_positive = MAX_AUDIO * pwm_scalar;
	static constexpr double amp_negative = MIN_AUDIO * pwm_scalar;
	static constexpr double amp_neutral = (amp_positive + amp_negative) / 2.0;

	static constexpr int idle_grace_time_ms = 100;

	struct DelayEntry {
		double index = 0.0;
		double vol   = 0.0;
	};

	std::array<DelayEntry, 1024> entries = {};

	mixer_channel_t channel = nullptr;

	PpiPortB port_b      = {};
	PpiPortB prev_port_b = {};

	PitMode pit_mode      = PitMode::SquareWave;
	PitMode prev_pit_mode = PitMode::SquareWave;

	double pit_last     = 0.0;
	double pit_max      = period_of_1k_pit_ticks * 1320.0;
	double pit_half     = pit_max / 2.0;
	double pit_new_max  = pit_max;
	double pit_new_half = pit_half;
	double pit_index    = 0.0;
	double volwant      = 0.0;
	double volcur       = 0.0;
	double last_index   = 0.0;

	int min_tr      = 0;
	int sample_rate = 0;

	int idle_countdown = idle_grace_time_ms;

	uint16_t entries_queued = 0u;
	uint16_t prev_pos       = 0u;

	int16_t last_played_sample = 0;
};
