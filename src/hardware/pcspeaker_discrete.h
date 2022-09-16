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

#include <queue>
#include <string>

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
	bool TryParseAndSetCustomFilter(const std::string &filter_choice) final;
	void SetCounter(const int cntr, const PitMode m) final;
	void SetPITControl(const PitMode) final {}
	void SetType(const PpiPortB &b) final;

private:
	void ChannelCallback(const uint16_t len);
	void AddDelayEntry(const float index, float vol);
	void ForwardPIT(const float newindex);
	bool IsWaveSquare() const;
	float NeutralOr(const float fallback) const;
	float NeutralLastPitOr(const float fallback) const;

	// Constants
	static constexpr char device_name[] = "PCSPEAKER";
	static constexpr char model_name[]  = "discrete";

	// The discrete PWM scalar was manually adjusted to roughly match voltage
	// levels recorded from a hardware PC Speaker 
	// Ref:https://github.com/dosbox-staging/dosbox-staging/files/9494469/3.audio.samples.zip
	static constexpr float pwm_scalar = 0.75f;
	static constexpr float sqw_scalar = pwm_scalar / 2.0f;

	// Amplitude constants
	static constexpr float amp_positive = MAX_AUDIO * pwm_scalar;
	static constexpr float amp_negative = MIN_AUDIO * pwm_scalar;
	static constexpr float amp_neutral = (amp_positive + amp_negative) / 2.0f;

	struct DelayEntry {
		float index = 0.0f;
		float vol   = 0.0f;
	};

	std::queue<DelayEntry> entries = {};

	mixer_channel_t channel = nullptr;

	PpiPortB port_b      = {};
	PpiPortB prev_port_b = {};

	PitMode pit_mode      = PitMode::SquareWave;
	PitMode prev_pit_mode = PitMode::SquareWave;

	float pit_last     = 0.0f;
	float pit_max      = period_of_1k_pit_ticks_f * 1320.0f;
	float pit_half     = pit_max / 2.0f;
	float pit_new_max  = pit_max;
	float pit_new_half = pit_half;
	float pit_index    = 0.0f;
	float volwant      = 0.0f;
	float volcur       = 0.0f;
	float last_index   = 0.0f;

	int sample_rate       = 0;
	int minimum_tick_rate = 0;
};
