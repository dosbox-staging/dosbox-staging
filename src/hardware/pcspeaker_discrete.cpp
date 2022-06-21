/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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

#include "checks.h"

CHECK_NARROWING();

// Determines if the inbound wave is square by inspecting
// current and previous states.
bool PcSpeakerDiscrete::IsWaveSquare()
{
	// When compared across time, this value describe an active PIT-state
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

void PcSpeakerDiscrete::AddDelayEntry(double index, double vol)
{
	if (IsWaveSquare()) {
		vol *= sqw_scalar;
// #define DEBUG_SQUARE_WAVE 1
#ifdef DEBUG_SQUARE_WAVE
		LOG_MSG("PCSPEAKER: square-wave [prev_pos=%u, prev_port_b=%u, port_b=%u, prev_pit=%lu, pit=%lu]",
		        prev_pos,
		        prev_port_b,
		        port_b,
		        prev_pit_mode,
		        pit_mode);
	} else {
		LOG_MSG("PCSPEAKER: sine-wave [prev_pos=%u, prev_port_b=%u, port_b=%u, prev_pit=%lu, pit=%lu], ",
		        prev_pos,
		        prev_port_b,
		        port_b,
		        prev_pit_mode,
		        pit_mode);
#endif
	}
	entries.at(entries_queued) = {index, vol};
	entries_queued++;

#if 0
	// This is extremely verbose; pipe the output to a file.
	// Display the previous and current speaker modes w/ requested volume
	if (fabs(vol) > amp_neutral)
		LOG_MSG("PCSPEAKER: Adding pos=%3s, pit=%" PRIuPTR "|%" PRIuPTR
		        ", pwm=%d|%d, volume=%6.0f",
		        prev_pos > 0 ? "yes" : "no", prev_pit_mode,
		        pit_mode, prev_port_b, port_b,
		        static_cast<double>(vol));
#endif
}

void PcSpeakerDiscrete::ForwardPIT(double newindex)
{
	auto passed     = (newindex - last_index);
	auto delay_base = last_index;
	last_index      = newindex;
	switch (pit_mode) {
	case PitMode::InterruptOnTC: return;

	case PitMode::OneShot: return;

	case PitMode::RateGeneratorAlias:
	case PitMode::RateGenerator:
		while (passed > 0) {
			/* passed the initial low cycle? */
			if (pit_index >= pit_half) {
				/* Start a new low cycle */
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
			/* Determine where in the wave we're located */
			if (pit_index >= pit_half) {
				if ((pit_index + passed) >= pit_max) {
					const auto delay = pit_max - pit_index;
					delay_base += delay;
					passed -= delay;
					pit_last = amp_positive;
					if (port_b.timer2_gating_and_speaker_out.all())
						AddDelayEntry(delay_base, pit_last);
					pit_index = 0;
					/* Load the new count */
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
					/* Load the new count */
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
			/* Check if we're gonna pass the end this block */
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
		LOG_WARNING("PCSPEAKER: Unhandled PIT mode %s", PitModeToString(pit_mode));
		break;
	}
}

// PIT-mode activation
void PcSpeakerDiscrete::SetCounter(int cntr, const PitMode mode)
{
	const auto newindex = PIC_TickIndex();
	ForwardPIT(newindex);

	prev_pit_mode = pit_mode;
	pit_mode      = mode;
	switch (pit_mode) {
	case PitMode::InterruptOnTC: /* Mode 0 one shot, used with realsound */
		if (!port_b.timer2_gating_and_speaker_out.all())
			return;
		if (cntr > 80) {
			cntr = 80;
		}
		pit_last = ((double)cntr - 40) * (amp_positive / 40.0);
		AddDelayEntry(newindex, pit_last);
		pit_index = 0;
		break;

	case PitMode::OneShot:
		if (!port_b.timer2_gating_and_speaker_out.all())
			return;
		pit_last = amp_positive;
		AddDelayEntry(newindex, pit_last);
		break;

	case PitMode::RateGeneratorAlias:
	case PitMode::RateGenerator: /* Single cycle low, rest low high
	                                generator */
		pit_index = 0;
		pit_last  = amp_negative;
		AddDelayEntry(newindex, pit_last);
		pit_half = PERIOD_OF_1K_PIT_TICKS * 1;
		pit_max  = PERIOD_OF_1K_PIT_TICKS * cntr;
		break;

	case PitMode::SquareWaveAlias:
	case PitMode::SquareWave: /* Square wave generator */
		if (cntr == 0 || cntr < min_tr) {
			/* skip frequencies that can't be represented */
			pit_last = 0;
			pit_mode = PitMode::InterruptOnTC;
			return;
		}
		pit_new_max  = PERIOD_OF_1K_PIT_TICKS * cntr;
		pit_new_half = pit_new_max / 2;
		break;
	case PitMode::SoftwareStrobe: /* Software triggered strobe */
		pit_last = amp_positive;
		AddDelayEntry(newindex, pit_last);
		pit_index = 0;
		pit_max   = PERIOD_OF_1K_PIT_TICKS * cntr;
		break;
	default:
#if C_DEBUG
		LOG_WARNING("PCSPEAKER: Unhandled speaker PIT mode: %s", PitModeToString(pit_mode));
#endif
		return;
	}
	// Activate the channel after queuing new speaker entries
	channel->Enable(true);
}

// Returns the amp_neutral voltage if the speaker's  fully faded,
// otherwise returns the fallback if the speaker is active.
double PcSpeakerDiscrete::NeutralOr(double fallback)
{
	return !idle_countdown ? amp_neutral : fallback;
}

// Returns, in order of preference:
// - Neutral voltage, if the speaker's fully faded
// - The last active PIT voltage to stitch on-going playback
// - The fallback voltage to kick start a new sound pattern
double PcSpeakerDiscrete::NeutralLastPitOr(double fallback)
{
	const bool use_last = std::isgreater(fabs(pit_last), amp_neutral);
	return NeutralOr(use_last ? pit_last : fallback);
}

// PWM-mode activation
void PcSpeakerDiscrete::SetType(const PpiPortB &b)
{
	const auto newindex = PIC_TickIndex();
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

	// Activate the channel after queuing new speaker entries
	channel->Enable(true);
}

// After the speaker has idled (as defined when the number of speaker
// movements is zero) for a short time, wind down the DC-offset, and
// then halt the channel.
void PcSpeakerDiscrete::PlayOrFadeout(const uint16_t speaker_movements,
                                      size_t requested_samples, int16_t *buffer)
{
	if (speaker_movements && requested_samples) {
		static_assert(idle_grace_time_ms > 0, "Algorithm depends on a non-zero grace time");
		idle_countdown = idle_grace_time_ms;
		dc_silencer.Reset();
	} else if (idle_countdown > 0) {
		idle_countdown--;
		last_played_sample = buffer[requested_samples - 1];
	} else if (!dc_silencer.Generate(last_played_sample, requested_samples, buffer)) {
		channel->Enable(false);
	}

	channel->AddSamples_m16(check_cast<uint16_t>(requested_samples), buffer);
}

void PcSpeakerDiscrete::ChannelCallback(uint16_t frames)
{
	constexpr uint16_t render_frames = 64;
	int16_t buf[render_frames];

	ForwardPIT(1);
	last_index            = 0.0;
	uint16_t pos          = 0u;
	auto sample_base      = 0.0;
	const auto sample_add = (1.0001) / frames;

	auto remaining = frames;
	while (remaining > 0) {
		const auto todo = std::min(remaining, render_frames);
		for (auto i = 0; i < todo; ++i) {
			auto index = sample_base;
			sample_base += sample_add;
			const auto end = sample_base;
			auto value     = 0.0;
			while (index < end) {
				/* Check if there is an upcoming event */
				if (entries_queued && entries[pos].index <= index) {
					volwant = entries[pos].vol;
					pos++;
					entries_queued--;
					continue;
				}
				double vol_end;
				if (entries_queued && entries[pos].index < end) {
					vol_end = entries[pos].index;
				} else
					vol_end = end;
				const auto vol_len = vol_end - index;
				/* Check if we have to slide the volume */
				const auto vol_diff = volwant - volcur;
				if (vol_diff == 0) {
					value += volcur * vol_len;
					index += vol_len;
				} else {
					// Check how long it will take to goto
					// new level
					// TODO: describe the basis for these
					// magic numbers and their effects
					constexpr auto spkr_speed = amp_positive *
					                            2.0 / 0.070;
					const auto vol_time = fabs(vol_diff) /
					                      spkr_speed;
					if (vol_time <= vol_len) {
						/* Volume reaches endpoint in
						 * this block, calc until that
						 * point */
						value += vol_time * volcur;
						value += vol_time * vol_diff / 2;
						index += vol_time;
						volcur = volwant;
					} else {
						/* Volume still not reached in
						 * this block */
						value += volcur * vol_len;
						const auto speed_by_len = spkr_speed *
						                          vol_len;
						const auto speed_by_len_sq =
						        speed_by_len * vol_len / 2.0;
						if (vol_diff < 0) {
							value -= speed_by_len_sq;
							volcur -= speed_by_len;
						} else {
							value += speed_by_len_sq;
							volcur += speed_by_len;
						}
						index += vol_len;
					}
				}
			}
			prev_pos = pos;
			buf[i] = check_cast<int16_t>(iround(value / sample_add));
		}
		if (neutralize_dc_offset)
			PlayOrFadeout(pos, todo, buf);
		else
			channel->AddSamples_m16(todo, buf);

		remaining = check_cast<uint16_t>(remaining - todo);
	}
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
		constexpr auto hp_cutoff_freq = 120;
		channel->ConfigureHighPassFilter(hp_order, hp_cutoff_freq);
		channel->SetHighPassFilter(FilterState::On);

		constexpr auto lp_order       = 2;
		constexpr auto lp_cutoff_freq = 4800;
		channel->ConfigureLowPassFilter(lp_order, lp_cutoff_freq);
		channel->SetLowPassFilter(FilterState::On);
	} else {
		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
	}
}

PcSpeakerDiscrete::PcSpeakerDiscrete()
{
	/* Register the sound channel */
	const auto callback = std::bind(&PcSpeakerDiscrete::ChannelCallback,
	                                this,
	                                std::placeholders::_1);

	channel = MIXER_AddChannel(callback,
	                           0,
	                           device_name,
	                           {ChannelFeature::ReverbSend,
	                            ChannelFeature::ChorusSend});
	assert(channel);

	sample_rate = channel->GetSampleRate();

	min_tr = (PIT_TICK_RATE + sample_rate / 2 - 1) / (sample_rate / 2);

	channel->SetPeakAmplitude(static_cast<uint32_t>(amp_positive));

	LOG_MSG("%s: Initialized %s model", device_name, model_name);
}

PcSpeakerDiscrete::~PcSpeakerDiscrete()
{
	assert(channel);
	channel->Enable(false);
	LOG_MSG("%s: Shutting down %s model", device_name, model_name);
}
