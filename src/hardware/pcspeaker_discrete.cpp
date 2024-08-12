/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "pcspeaker_discrete.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <mutex>

#include "checks.h"

CHECK_NARROWING();

// Determines if the inbound wave is square by inspecting
// current and previous states.
bool PcSpeakerDiscrete::IsWaveSquare() const
{
	// When compared across time, this value describes an active PIT-state
	// being toggled
	constexpr auto was_toggled     = 0b10 + 0b11;
	constexpr auto was_steadily_on = 0b11;
	constexpr auto was_toggled_on  = 0b01;

	// The sum of the previous port_b with the current port_b
	// becomes a temporal-state
	const auto temporal_pit_state = static_cast<int>(prev_pit_mode) + static_cast<int>(pit_mode);
	const auto temporal_pwm_state =
	        static_cast<int>(prev_port_b.timer2_gating_and_speaker_out) +
	        static_cast<int>(port_b.timer2_gating_and_speaker_out);

	// We have a sine-wave if the PIT was steadily off and ...
	if (temporal_pit_state == 0)
		// The PWM toggled an ongoing PIT state or turned on PIT-mode
		// from an off-state
		if (temporal_pwm_state == was_toggled || temporal_pwm_state == was_steadily_on)
			return false;

	// We have a sine-wave if the PIT was steadily on and ...
	if (temporal_pit_state == was_steadily_on)
		// the PWM was turned on from an off-state
		if (temporal_pwm_state == was_toggled_on)
			return false;

	return true;
}

void PcSpeakerDiscrete::AddDelayEntry(const float index, float vol)
{
	if (IsWaveSquare()) {
		vol *= sqw_scalar;
// #define DEBUG_SQUARE_WAVE 1
#ifdef DEBUG_SQUARE_WAVE
		LOG_MSG("PCSPEAKER: square-wave [vol=%f, prev_port_b=%u, port_b=%u, prev_pit=%lu, pit=%lu]",
		        vol,
		        prev_port_b,
		        port_b,
		        prev_pit_mode,
		        pit_mode);
	} else {
		LOG_MSG("PCSPEAKER: sine-wave [vol=%f, prev_port_b=%u, port_b=%u, prev_pit=%lu, pit=%lu], ",
		        vol,
		        prev_port_b,
		        port_b,
		        prev_pit_mode,
		        pit_mode);
#endif
	}
	const auto entry = DelayEntry{index, vol};
	entries.emplace(entry);
#if 0
	// This is extremely verbose; pipe the output to a file.
	// Display the previous and current speaker modes w/ requested volume
	if (fabsf(vol) > amp_neutral)
		LOG_MSG("PCSPEAKER: Adding pit=%" PRIuPTR "|%" PRIuPTR
		        ", pwm=%d|%d, volume=%6.0f",
		        prev_pit_mode,
		        pit_mode, prev_port_b, port_b,
		        static_cast<float>(vol));
#endif
}

void PcSpeakerDiscrete::ForwardPIT(const float newindex)
{
	auto passed     = (newindex - last_index);
	auto delay_base = last_index;
	last_index      = newindex;
	switch (pit_mode) {
	case PitMode::InterruptOnTerminalCount: return;

	case PitMode::OneShot: return;

	case PitMode::RateGeneratorAlias:
	case PitMode::RateGenerator:
		while (passed > 0) {
			// passed the initial low cycle?
			if (pit_index >= pit_half) {
				// Start a new low cycle
				if ((pit_index + passed) >= pit_max) {
					const auto delay = pit_max - pit_index;
					delay_base += delay;
					passed -= delay;
					pit_last = amp_negative;
					if (port_b.timer2_gating_and_speaker_out.all())
						AddDelayEntry(delay_base, pit_last);
					pit_index = 0;
				} else {
					pit_index += passed;
					return;
				}
			} else {
				if ((pit_index + passed) >= pit_half) {
					const auto delay = pit_half - pit_index;
					delay_base += delay;
					passed -= delay;
					pit_last = amp_positive;
					if (port_b.timer2_gating_and_speaker_out.all())
						AddDelayEntry(delay_base, pit_last);
					pit_index = pit_half;
				} else {
					pit_index += passed;
					return;
				}
			}
		}
		break;
		// END CASE 2

	case PitMode::SquareWaveAlias:
	case PitMode::SquareWave:
		while (passed > 0) {
			// Determine where in the wave we're located
			if (pit_index >= pit_half) {
				if ((pit_index + passed) >= pit_max) {
					const auto delay = pit_max - pit_index;
					delay_base += delay;
					passed -= delay;
					pit_last = amp_positive;
					if (port_b.timer2_gating_and_speaker_out.all())
						AddDelayEntry(delay_base, pit_last);
					pit_index = 0;
					// Load the new count
					pit_half = pit_new_half;
					pit_max  = pit_new_max;
				} else {
					pit_index += passed;
					return;
				}
			} else {
				if ((pit_index + passed) >= pit_half) {
					const auto delay = pit_half - pit_index;
					delay_base += delay;
					passed -= delay;
					pit_last = amp_negative;
					if (port_b.timer2_gating_and_speaker_out.all())
						AddDelayEntry(delay_base, pit_last);
					pit_index = pit_half;
					// Load the new count
					pit_half = pit_new_half;
					pit_max  = pit_new_max;
				} else {
					pit_index += passed;
					return;
				}
			}
		}
		break;
		// END CASE 3

	case PitMode::SoftwareStrobe:
		if (pit_index < pit_max) {
			// Check if we're gonna pass the end this block
			if (pit_index + passed >= pit_max) {
				const auto delay = pit_max - pit_index;
				delay_base += delay;
				// passed -= delay; // not currently used
				pit_last = amp_negative;
				if (port_b.timer2_gating_and_speaker_out.all())
					AddDelayEntry(delay_base,
					              pit_last); // No new events
					                         // unless
					                         // reprogrammed
				pit_index = pit_max;
			} else
				pit_index += passed;
		}
		break;
		// END CASE 4
	case PitMode::HardwareStrobe:
	case PitMode::Inactive:
	default:
		LOG_WARNING("PCSPEAKER: Unhandled PIT mode: '%s'", pit_mode_to_string(pit_mode));
		break;
	}
}

// PIT-mode activation
void PcSpeakerDiscrete::SetCounter(int count, const PitMode mode)
{
	const auto newindex = static_cast<float>(PIC_TickIndex());
	ForwardPIT(newindex);

	prev_pit_mode = pit_mode;
	pit_mode      = mode;

	// more documentation needed for these constants
	constexpr auto max_terminal_count = 80.0f;
	constexpr auto last_count_offset  = 40.0f;
	constexpr auto last_count_scalar  = amp_positive / last_count_offset;

	auto count_f = static_cast<float>(count);

	switch (pit_mode) {
	// Mode 0 one shot, used with realsound
	case PitMode::InterruptOnTerminalCount:
		if (!port_b.timer2_gating_and_speaker_out.all())
			return;

		count_f = std::min(count_f, max_terminal_count);

		pit_last = (count_f - max_terminal_count) * last_count_scalar;

		AddDelayEntry(newindex, pit_last);
		pit_index = 0;
		break;

	case PitMode::OneShot:
		if (!port_b.timer2_gating_and_speaker_out.all())
			return;
		pit_last = amp_positive;
		AddDelayEntry(newindex, pit_last);
		break;

	// The RateGenerator is a single cycle low, rest low high generator
	case PitMode::RateGeneratorAlias:
	case PitMode::RateGenerator:
		pit_index = 0;
		pit_last  = amp_negative;
		AddDelayEntry(newindex, pit_last);
		pit_half = period_of_1k_pit_ticks_f * 1;
		pit_max  = period_of_1k_pit_ticks_f * count_f;
		break;

	case PitMode::SquareWaveAlias:
	case PitMode::SquareWave:
		if (count == 0 || count < minimum_tick_rate) {
			// skip frequencies that can't be represented
			pit_last = 0;
			pit_mode = PitMode::InterruptOnTerminalCount;
			return;
		}
		pit_new_max  = period_of_1k_pit_ticks_f * count_f;
		pit_new_half = pit_new_max / 2;
		break;

	case PitMode::SoftwareStrobe:
		pit_last = amp_positive;
		AddDelayEntry(newindex, pit_last);
		pit_index = 0;
		pit_max   = period_of_1k_pit_ticks_f * count_f;
		break;
	default:
#if C_DEBUG
		LOG_WARNING("PCSPEAKER: Unhandled speaker PIT mode: '%s'", pit_mode_to_string(pit_mode));
#endif
		return;
	}
	// Activate the channel after queuing new speaker entries
	// We don't care about the return-code, so explicitly ignore it.
	(void)channel->WakeUp();
}

// Returns the amp_neutral voltage if the speaker's  fully faded,
// otherwise returns the fallback if the speaker is active.
float PcSpeakerDiscrete::NeutralOr(const float fallback) const
{
	return channel->is_enabled ? fallback : amp_neutral;
}

// Returns, in order of preference:
// - Neutral voltage, if the speaker's fully faded
// - The last active PIT voltage to stitch on-going playback
// - The fallback voltage to kick start a new sound pattern
float PcSpeakerDiscrete::NeutralLastPitOr(const float fallback) const
{
	const bool use_last = std::isgreater(fabsf(pit_last), amp_neutral);
	return NeutralOr(use_last ? pit_last : fallback);
}

// PWM-mode activation
void PcSpeakerDiscrete::SetType(const PpiPortB &b)
{
	const auto newindex = static_cast<float>(PIC_TickIndex());
	ForwardPIT(newindex);

	prev_port_b.data = port_b.data;

	switch (b.timer2_gating_and_speaker_out) {
	case 0b00:
		port_b.timer2_gating  = false;
		port_b.speaker_output = false;
		AddDelayEntry(newindex, NeutralOr(amp_negative));
		break;
	case 0b01:
		port_b.timer2_gating  = false;
		port_b.speaker_output = true;
		AddDelayEntry(newindex, NeutralOr(amp_negative));
		break;
	case 0b10:
		port_b.timer2_gating  = true;
		port_b.speaker_output = false;
		AddDelayEntry(newindex, NeutralOr(amp_positive));
		break;
	case 0b11:
		port_b.timer2_gating  = true;
		port_b.speaker_output = true;
		AddDelayEntry(newindex, NeutralLastPitOr(amp_positive));
		break;
	};
	// We don't care about the return-code, so explicitly ignore it.
	(void)channel->WakeUp();
}

void PcSpeakerDiscrete::PicCallback(const int requested_frames)
{
	std::vector<float> output = {};
	output.reserve(requested_frames);

	ForwardPIT(1);
	last_index       = 0.0f;
	auto sample_base = 0.0f;

	const auto period_per_frame_ms = FLT_EPSILON + 1.0f / static_cast<float>(requested_frames);
	// The addition of epsilon ensures that queued entries
	// having time indexes at the end of the tick cycle (ie: .index == ~1.0)
	// will still be accepted in the comparison below:
	//    "entries.front().index <= index"
	//
	// Without epsilon, then we risk some of these comparisons failing, in
	// which case these marginal end-of-cycle entries won't be processed
	// this round and instead will stay queued for processing next round. If
	// this pattern persists for some number of seconds, then more and more
	// entries can be queued versus processed.

	// An example of this behavior can be seen in Narco Police, where
	// repeated gunfire from enemy soldiers during the first wave can drive
	// the entry queue into the thousands (if epsilon is removed).

	// Therefore, it's better to err on the side of accepting and processing
	// entries versus queuing, to keep the queue under control.

	for (auto i = 0; i < requested_frames; ++i) {
		auto index = sample_base;
		sample_base += period_per_frame_ms;
		const auto end = sample_base;

		auto value = 0.0f;

		while (index < end) {
			// Check if there is an upcoming event
			const auto has_entries = !entries.empty();
			const auto first_index = has_entries ? entries.front().index : 0.0f;

			if (has_entries && first_index <= index) {
				volwant = entries.front().vol;
				entries.pop();
				continue;
			}
			const auto vol_end = (has_entries && first_index < end) ? first_index : end;
			const auto vol_len = vol_end - index;
			// Check if we have to slide the volume
			const auto vol_diff = volwant - volcur;
			if (vol_diff == 0) {
				value += volcur * vol_len;
				index += vol_len;
			} else {
				// Check how long it will take to goto
				// new level
				// TODO: describe the basis for these
				// magic numbers and their effects
				constexpr float spkr_speed = amp_positive * 2.0f / 0.070f;
				const auto vol_time = fabsf(vol_diff) / spkr_speed;
				if (vol_time <= vol_len) {
					// Volume reaches endpoint in
					// this block, calc until that
					// point
					value += vol_time * volcur;
					value += vol_time * vol_diff / 2;
					index += vol_time;
					volcur = volwant;
				} else {
					// Volume still not reached in
					// this block
					value += volcur * vol_len;

					const auto vol_cur_delta = spkr_speed * vol_len;
					volcur += std::copysign(vol_cur_delta, vol_diff);

					const auto value_delta = vol_cur_delta * vol_len / 2.0f;
					value += std::copysign(value_delta, vol_diff);
					index += vol_len;
				}
			}
		}
		output.push_back(value / period_per_frame_ms);
	}

	output_queue.NonblockingBulkEnqueue(output);
}

void PcSpeakerDiscrete::SetFilterState(const FilterState filter_state)
{
	assert(channel);

	// Setup filters
	if (filter_state == FilterState::On) {
		// The filters are meant to emulate the bandwidth limited
		// sound of the small PC speaker. This more accurately
		// reflects people's actual experience of the PC speaker
		// sound than the raw unfiltered output, and it's a lot
		// more pleasant to listen to, especially in headphones.
		constexpr auto hp_order       = 3;
		constexpr auto hp_cutoff_freq_hz = 120;
		channel->ConfigureHighPassFilter(hp_order, hp_cutoff_freq_hz);
		channel->SetHighPassFilter(FilterState::On);

		constexpr auto lp_order       = 2;
		constexpr auto lp_cutoff_freq_hz = 4800;
		channel->ConfigureLowPassFilter(lp_order, lp_cutoff_freq_hz);
		channel->SetLowPassFilter(FilterState::On);
	} else {
		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
	}
}

bool PcSpeakerDiscrete::TryParseAndSetCustomFilter(const std::string& filter_choice)
{
	assert(channel);
	return channel->TryParseAndSetCustomFilter(filter_choice);
}

PcSpeakerDiscrete::PcSpeakerDiscrete()
{
	// Register the sound channel
	constexpr bool Stereo = false;
	constexpr bool SignedData = true;
	constexpr bool NativeOrder = true;
	const auto callback = std::bind(MIXER_PullFromQueueCallback<PcSpeakerDiscrete, float, Stereo, SignedData, NativeOrder>,
	                                std::placeholders::_1,
	                                this);

	channel = MIXER_AddChannel(callback,
	                           UseMixerRate,
	                           device_name,
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::Synthesizer});
	assert(channel);

	sample_rate_hz = channel->GetSampleRate();

	minimum_tick_rate = (PIT_TICK_RATE + sample_rate_hz / 2 - 1) /
	                    (sample_rate_hz / 2);

	channel->SetPeakAmplitude(static_cast<uint32_t>(amp_positive));

	LOG_MSG("%s: Initialised %s model", device_name, model_name);
}

PcSpeakerDiscrete::~PcSpeakerDiscrete()
{
	LOG_MSG("%s: Shutting down %s model", device_name, model_name);

	// Deregister the mixer channel, after which it's cleaned up
	assert(channel);
	MIXER_DeregisterChannel(channel);
}
