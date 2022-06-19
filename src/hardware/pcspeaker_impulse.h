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
#include <deque>

#include "inout.h"
#include "setup.h"
#include "soft_limiter.h"
#include "support.h"
#include "pic.h"

class PcSpeakerImpulse final : public PcSpeaker {
public:
	PcSpeakerImpulse();
	~PcSpeakerImpulse() final;

	void SetFilterState(const FilterState filter_state) final;
	void SetCounter(const int cntr, const PitMode pit_mode) final;
	void SetPITControl(const PitMode pit_mode) final;
	void SetType(const PpiPortB &port_b_state) final;

private:
	void AddImpulse(float index, const int16_t amplitude);
	void AddPITOutput(const float index);
	void ChannelCallback(uint16_t requested_frames);
	void ForwardPIT(const float new_index);
	float CalcImpulse(const double t);
	void InitializeImpulseLUT();

	// Constants
	static constexpr char device_name[] = "PCSPEAKER";
	static constexpr char model_name[]  = "impulse";

	static constexpr int16_t positive_amplitude = 20000;
	static constexpr int16_t negative_amplitude = -positive_amplitude;
	static constexpr int16_t neutral_amplitude  = 0;

	static constexpr float ms_per_pit_tick = 1000.0f / PIT_TICK_RATE;

	// Mixer channel constants
	static constexpr uint16_t sample_rate        = 32000u;
	static constexpr uint16_t sample_rate_per_ms = sample_rate / 1000u;

	static constexpr auto lowpass_cutoff_freq = 4300;
	static constexpr auto minimum_counter     = 2 * PIT_TICK_RATE / sample_rate;

	// must be greater than 0.0f
	static constexpr float cutoff_margin = 0.2f;

	// Should be selected based on sampling rate
	static constexpr float high_pass_amount   = 0.999f;
	static constexpr uint16_t filter_quality      = 100u;
	static constexpr uint16_t oversampling_factor = 32u;
	static constexpr uint16_t filter_width        = filter_quality * oversampling_factor;

	// Compound types and containers
	struct PitState {
		// PIT starts in mode 3 (SquareWave) at ~903 Hz (pit_max) with
		// positive amplitude
		float max_ms            = ms_per_pit_tick * 1320.0f;
		float new_max_ms        = max_ms;
		float half_ms           = max_ms / 2.0f;
		float new_half_ms       = half_ms;
		float index             = 0.0f;
		float last_index        = index;
		float mode1_pending_max = 0.0f;

		// PIT boolean state
		bool mode1_waiting_for_counter = false;
		bool mode1_waiting_for_trigger = true;
		bool mode3_counting            = false;

		PitMode mode = PitMode::SquareWave;

		int16_t amplitude      = positive_amplitude;
		int16_t prev_amplitude = negative_amplitude;
	} pit = {};

	std::deque<float> waveform_deque = {};

	std::array<float, filter_width> impulse_lut = {};

	std::unique_ptr<SoftLimiter> soft_limiter = {};

	mixer_channel_t channel = {};

	PpiPortB prev_port_b_state = {};

	int tally_of_silence = 0;
};
