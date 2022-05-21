/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
 *  Copyright (C) 2002-2019  The DOSBox Team
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

// NOTE: a lot of this code assumes that the callback is called every emulated
// millisecond

#include "dosbox.h"

// #define SPKR_DEBUGGING
// #define REFERENCE
#include <algorithm>
#include <array>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <deque>

#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "soft_limiter.h"
#include "support.h"
#include "pic.h"

// constants
constexpr int16_t SPKR_POSITIVE_AMPLITUDE = 20000;
constexpr int16_t SPKR_NEGATIVE_AMPLITUDE = -SPKR_POSITIVE_AMPLITUDE;
constexpr int16_t SPKR_NEUTRAL_AMPLITUDE = 0;

constexpr auto SPKR_FILTER_QUALITY = 100;
constexpr auto SPKR_OVERSAMPLING = 32;
constexpr auto SPKR_CUTOFF_MARGIN = 0.2f; // must be greater than 0.0f
constexpr auto SPKR_FILTER_WIDTH = (SPKR_FILTER_QUALITY * SPKR_OVERSAMPLING);
constexpr auto SPKR_HIGHPASS = 0.999f; // Should be selected based on
                                       // sampling rate
constexpr auto ms_per_pit_tick = 1000.0f / PIT_TICK_RATE;

// PIT starts in mode 3 at ~903 Hz (pit_max) with positive amplitude
static struct {
	// Complex types and containers
	std::deque<float> waveform_deque = {};
	std::array<float, SPKR_FILTER_WIDTH> impulse_lut = {};
	std::unique_ptr<SoftLimiter> soft_limiter = {};
	mixer_channel_t chan = nullptr;

	// time values
	float pit_max = ms_per_pit_tick * 1320;
	float pit_new_max = pit_max;
	float pit_half = pit_max / 2;
	float pit_new_half = pit_half;
	float pit_index = 0.0f;
	float last_index = 0.0f;
	float pit_mode1_pending_max = 0.0f;

	// Playback rate (and derived)
	float rate_as_float = 0.0f;
	float rate_per_ms = 0.0f;
	int rate = 0;

	int minimum_counter = 0; // based on channel rate rate

	// Toggles
	bool pit_clock_gate_enabled = false;
	bool pit_mode1_waiting_for_counter = false;
	bool pit_mode1_waiting_for_trigger = true;
	bool pit_mode3_counting = false;
	bool pit_output_enabled = false;

	// PIT mode and apl
	int16_t pit_amplitude = SPKR_POSITIVE_AMPLITUDE;
	int16_t pit_prev_amplitude = SPKR_NEGATIVE_AMPLITUDE;
	int16_t tally_of_silence = 0;

	uint8_t pit_mode = 3;

} spkr = {};

static void AddImpulse(float index, const int16_t amplitude);
static void AddPITOutput(const float index)
{
	if (spkr.pit_output_enabled) {
		AddImpulse(index, spkr.pit_amplitude);
	}
}

static void ForwardPIT(const float newindex)
{
	assert(newindex >= -FLT_EPSILON && newindex <= 1.0f + FLT_EPSILON);
	float passed = (newindex - spkr.last_index);
	float delay_base = spkr.last_index;
	spkr.last_index = newindex;
	switch (spkr.pit_mode) {
	case 6: // dummy
		return;
	case 0:
		if (spkr.pit_index >= spkr.pit_max) {
			return; // counter reached zero before previous call so
			        // do nothing
		}
		spkr.pit_index += passed;
		if (spkr.pit_index >= spkr.pit_max) {
			// counter reached zero between previous and this call
			const float delay = delay_base + spkr.pit_max -
			                    spkr.pit_index + passed;
			spkr.pit_amplitude = SPKR_POSITIVE_AMPLITUDE;
			AddPITOutput(delay);
		}
		return;
	case 1:
		if (spkr.pit_mode1_waiting_for_counter) {
			// assert output amplitude is high
			return; // counter not written yet
		}
		if (spkr.pit_mode1_waiting_for_trigger) {
			// assert output amplitude is high
			return; // no pulse yet
		}
		if (spkr.pit_index >= spkr.pit_max) {
			return; // counter reached zero before previous call so
			        // do nothing
		}
		spkr.pit_index += passed;
		if (spkr.pit_index >= spkr.pit_max) {
			// counter reached zero between previous and this call
			const float delay = delay_base + spkr.pit_max -
			                    spkr.pit_index + passed;
			spkr.pit_amplitude = SPKR_POSITIVE_AMPLITUDE;
			AddPITOutput(delay);
			// finished with this pulse
			spkr.pit_mode1_waiting_for_trigger = 1;
		}
		return;
	case 2:
		while (passed > 0) {
			/* passed the initial low cycle? */
			if (spkr.pit_index >= spkr.pit_half) {
				/* Start a new low cycle */
				if ((spkr.pit_index + passed) >= spkr.pit_max) {
					const float delay = spkr.pit_max -
					                    spkr.pit_index;
					delay_base += delay;
					passed -= delay;
					spkr.pit_amplitude = SPKR_NEGATIVE_AMPLITUDE;
					AddPITOutput(delay_base);
					spkr.pit_index = 0;
				} else {
					spkr.pit_index += passed;
					return;
				}
			} else {
				if ((spkr.pit_index + passed) >= spkr.pit_half) {
					const float delay = spkr.pit_half -
					                    spkr.pit_index;
					delay_base += delay;
					passed -= delay;
					spkr.pit_amplitude = SPKR_POSITIVE_AMPLITUDE;
					AddPITOutput(delay_base);
					spkr.pit_index = spkr.pit_half;
				} else {
					spkr.pit_index += passed;
					return;
				}
			}
		}
		break;
		// END CASE 2
	case 3:
		if (!spkr.pit_mode3_counting)
			break;
		while (passed > 0) {
			/* Determine where in the wave we're located */
			if (spkr.pit_index >= spkr.pit_half) {
				if ((spkr.pit_index + passed) >= spkr.pit_max) {
					const float delay = spkr.pit_max -
					                    spkr.pit_index;
					delay_base += delay;
					passed -= delay;
					spkr.pit_amplitude = SPKR_POSITIVE_AMPLITUDE;
					AddPITOutput(delay_base);
					spkr.pit_index = 0;
					/* Load the new count */
					spkr.pit_half = spkr.pit_new_half;
					spkr.pit_max = spkr.pit_new_max;
				} else {
					spkr.pit_index += passed;
					return;
				}
			} else {
				if ((spkr.pit_index + passed) >= spkr.pit_half) {
					const float delay = spkr.pit_half -
					                    spkr.pit_index;
					delay_base += delay;
					passed -= delay;
					spkr.pit_amplitude = SPKR_NEGATIVE_AMPLITUDE;
					AddPITOutput(delay_base);
					spkr.pit_index = spkr.pit_half;
					/* Load the new count */
					spkr.pit_half = spkr.pit_new_half;
					spkr.pit_max = spkr.pit_new_max;
				} else {
					spkr.pit_index += passed;
					return;
				}
			}
		}
		break;
		// END CASE 3
	case 4:
		if (spkr.pit_index < spkr.pit_max) {
			/* Check if we're gonna pass the end this block */
			if (spkr.pit_index + passed >= spkr.pit_max) {
				const float delay = spkr.pit_max - spkr.pit_index;
				delay_base += delay;
				passed -= delay;
				spkr.pit_amplitude = SPKR_NEGATIVE_AMPLITUDE;
				AddPITOutput(delay_base); // No new events
				                          // unless reprogrammed
				spkr.pit_index = spkr.pit_max;
			} else
				spkr.pit_index += passed;
		}
		break;
		// END CASE 4
	}
}

void PCSPEAKER_SetPITControl(const uint8_t mode)
{
	const auto newindex = static_cast<float>(PIC_TickIndex());
	ForwardPIT(newindex);
#ifdef SPKR_DEBUGGING
	LOG_INFO("PCSPEAKER: %f pit command: %u", PIC_FullIndex(), mode);
#endif
	// TODO: implement all modes
	switch (mode) {
	case 1:
		spkr.pit_mode = 1;
		spkr.pit_mode1_waiting_for_counter = 1;
		spkr.pit_mode1_waiting_for_trigger = 0;
		spkr.pit_amplitude = SPKR_POSITIVE_AMPLITUDE;
		break;
	case 3:
		spkr.pit_mode = 3;
		spkr.pit_mode3_counting = false;
		spkr.pit_amplitude = SPKR_POSITIVE_AMPLITUDE;
		break;
	default: return;
	}
	AddPITOutput(newindex);
}

void PCSPEAKER_SetCounter(const int cntr, const uint8_t mode)
{
#ifdef SPKR_DEBUGGING
	LOG_INFO("PCSPEAKER: %f counter: %u, mode: %u", PIC_FullIndex(), cntr, mode);
#endif
	const auto newindex = static_cast<float>(PIC_TickIndex());
	const auto duration_of_count_ms = ms_per_pit_tick * static_cast<float>(cntr);
	ForwardPIT(newindex);
	switch (mode) {
	case 0: /* Mode 0 one shot, used with "realsound" (PWM) */
		// if (cntr>80) {
		//	cntr=80;
		// }
		// spkr.pit_amplitude=((float)cntr-40)*(SPKR_VOLUME/40.0f);
		spkr.pit_amplitude = SPKR_NEGATIVE_AMPLITUDE;
		spkr.pit_index = 0;
		spkr.pit_max = duration_of_count_ms;
		AddPITOutput(newindex);
		break;
	case 1: // retriggerable one-shot, used by Star Control 1
		spkr.pit_mode1_pending_max = duration_of_count_ms;
		if (spkr.pit_mode1_waiting_for_counter) {
			// assert output amplitude is high
			spkr.pit_mode1_waiting_for_counter = 0;
			spkr.pit_mode1_waiting_for_trigger = 1;
		}
		break;
	case 2: /* Single cycle low, rest low high generator */
		spkr.pit_index = 0;
		spkr.pit_amplitude = SPKR_NEGATIVE_AMPLITUDE;
		AddPITOutput(newindex);
		spkr.pit_half = ms_per_pit_tick;
		spkr.pit_max = duration_of_count_ms;
		break;
	case 3: /* Square wave generator */
		if (cntr < spkr.minimum_counter) {
			// avoid breaking digger music
			spkr.pit_amplitude = SPKR_POSITIVE_AMPLITUDE;
			spkr.pit_mode = 6; // dummy mode with constant high output
			AddPITOutput(newindex);
			return;
		}
		spkr.pit_new_max = duration_of_count_ms;
		spkr.pit_new_half = spkr.pit_new_max / 2;
		if (!spkr.pit_mode3_counting) {
			spkr.pit_index = 0;
			spkr.pit_max = spkr.pit_new_max;
			spkr.pit_half = spkr.pit_new_half;
			if (spkr.pit_clock_gate_enabled) {
				spkr.pit_mode3_counting = true;
				// probably not necessary
				spkr.pit_amplitude = SPKR_POSITIVE_AMPLITUDE;
				AddPITOutput(newindex);
			}
		}
		break;
	case 4: /* Software triggered strobe */
		spkr.pit_amplitude = SPKR_POSITIVE_AMPLITUDE;
		AddPITOutput(newindex);
		spkr.pit_index = 0;
		spkr.pit_max = duration_of_count_ms;
		break;
	default:
#ifdef SPKR_DEBUGGING
		LOG_MSG("Unhandled speaker mode %d at %f", mode, PIC_FullIndex());
#endif
		return;
	}
	spkr.pit_mode = mode;
}

void PCSPEAKER_SetType(const bool pit_clock_gate_enabled, const bool pit_output_enabled)
{
#ifdef SPKR_DEBUGGING
	LOG_INFO("PCSPEAKER: %f output: %s, clock gate %s",
	        PIC_FullIndex(),
	        pit_output_enabled ? "pit" : "forced low",
	        pit_clock_gate_enabled ? "on" : "off");
#endif
	const float newindex = static_cast<float>(PIC_TickIndex());
	ForwardPIT(newindex);
	// pit clock gate enable rising edge is a trigger
	const bool pit_trigger = pit_clock_gate_enabled &&
	                         !spkr.pit_clock_gate_enabled;
	spkr.pit_clock_gate_enabled = pit_clock_gate_enabled;
	spkr.pit_output_enabled = pit_output_enabled;
	if (pit_trigger) {
		switch (spkr.pit_mode) {
		case 1:
			if (spkr.pit_mode1_waiting_for_counter) {
				// assert output amplitude is high
				break;
			}
			spkr.pit_amplitude = SPKR_NEGATIVE_AMPLITUDE;
			spkr.pit_index = 0;
			spkr.pit_max = spkr.pit_mode1_pending_max;
			spkr.pit_mode1_waiting_for_trigger = 0;
			break;
		case 3:
			spkr.pit_mode3_counting = true;
			// spkr.pit_new_max = spkr.pit_new_max; // typo or bug?
			spkr.pit_new_half = spkr.pit_new_max / 2;
			spkr.pit_index = 0;
			spkr.pit_max = spkr.pit_new_max;
			spkr.pit_half = spkr.pit_new_half;
			spkr.pit_amplitude = SPKR_POSITIVE_AMPLITUDE;
			break;
		default:
			// TODO: implement other modes
			break;
		}
	} else if (!pit_clock_gate_enabled) {
		switch (spkr.pit_mode) {
		case 1:
			// gate amplitude does not affect mode1
			break;
		case 3:
			// low gate forces pit output high
			spkr.pit_amplitude = SPKR_POSITIVE_AMPLITUDE;
			spkr.pit_mode3_counting = false;
			break;
		default:
			// TODO: implement other modes
			break;
		}
	}
	if (pit_output_enabled) {
		AddImpulse(newindex, spkr.pit_amplitude);
	} else {
		AddImpulse(newindex, SPKR_NEGATIVE_AMPLITUDE);
	}
}

// TODO: check if this is accurate
static double sinc(const double t)
{
	constexpr auto SINC_ACCURACY = 20;
	double result = 1;
	for (auto k = 1; k < SINC_ACCURACY; ++k) {
		result *= cos(t / pow(2.0, k));
	}
	return result;
}

static float impulse(const double t)
{
	// raised-cosine-windowed sinc function
	const double fs = spkr.rate;
	const auto fc = fs / (2 + static_cast<double>(SPKR_CUTOFF_MARGIN));
	const auto q = static_cast<double>(SPKR_FILTER_QUALITY);
	if ((0 < t) && (t * fs < q)) {
		const auto window = 1.0 + cos(2 * fs * M_PI * (q / (2 * fs) - t) / q);
		const auto amplitude = window * (sinc(2 * fc * M_PI * (t - q / (2 * fs)))) / 2.0;
		return static_cast<float>(amplitude);
	} else
		return 0.0f;
}

static void AddImpulse(float index, const int16_t amplitude)
{
	// Did the amplitude change?
	if (amplitude == spkr.pit_prev_amplitude)
		return;
	spkr.pit_prev_amplitude = amplitude;

	// Wake the channel if it was sleeping
	if (!spkr.chan->is_enabled) {
		spkr.chan->ReactivateEnvelope();
		spkr.chan->Enable(true);
	}

	// Make sure the time index is valid
	index = clamp(index, 0.0f, 1.0f);
#ifndef REFERENCE
	const auto samples_in_impulse = index * spkr.rate_per_ms;
	auto offset = static_cast<uint32_t>(samples_in_impulse);
	auto phase = static_cast<uint32_t>(samples_in_impulse * SPKR_OVERSAMPLING) %
	             SPKR_OVERSAMPLING;
	if (phase != 0) {
		offset++;
		phase = SPKR_OVERSAMPLING - phase;
	}
	for (uint16_t i = 0; i < SPKR_FILTER_QUALITY; ++i) {
		assertm(offset + i < spkr.waveform_deque.size(),
		        "index into spkr.waveform_deque too high");
		assertm(phase + SPKR_OVERSAMPLING * i < SPKR_FILTER_WIDTH,
		        "index into spkr.impulse_lut too high");
		spkr.waveform_deque[offset + i] +=
		        amplitude * spkr.impulse_lut[phase + i * SPKR_OVERSAMPLING];
	}
}
#else
	const auto portion_of_ms = static_cast < double(index) / 1000.0;
	for (size_t i = 0; i < spkr.waveform_deque.size(); ++i) {
		const auto impulse_time = static_cast<double>(i) / spkr.rate -
		                          portion_of_ms;
		spkr.waveform_deque[i] += amplitude * impulse(impulse_time);
	}
}
#endif

static void PCSPEAKER_CallBack(uint16_t requested_frames)
{
	ForwardPIT(1.0f);
	spkr.last_index = 0;

	constexpr auto num_channels = 2;
	// Static vectors and samples re-used for all callbacks
	static std::vector<float> in_buffer(num_channels);
	static std::vector<int16_t> out_buffer(num_channels);
	static auto &out_sample = out_buffer.front();
	static auto &in_sample = in_buffer.front();

	while (requested_frames > 0 && spkr.waveform_deque.size()) {
		// Pop the first sample off the waveform
		in_sample += spkr.waveform_deque.front();
		spkr.waveform_deque.pop_front();
		spkr.waveform_deque.push_back(0.0f);

		// Soft-limit the sample, if needed
		spkr.soft_limiter->Process(in_buffer, 1, out_buffer);

		// Pass the sample to the mixer and decrement the requested frames
		spkr.chan->AddSamples_m16(1, &out_sample);
		--requested_frames;

		// Keep a tally of sequential silence so we can sleep the channel
		spkr.tally_of_silence = out_sample ? 0 : spkr.tally_of_silence + 1;

		// Scale down the running volume amplitude. Eventually it will
		// hit 0 if no other waveforms are generated.
		in_sample *= SPKR_HIGHPASS;
	}

	// Write silence if the waveform deque ran out
	while (requested_frames) {
		spkr.chan->AddSamples_m16(1, &SPKR_NEUTRAL_AMPLITUDE);
		++spkr.tally_of_silence;
		--requested_frames;
	}

	// Maybe put the channel to sleep
	if (spkr.tally_of_silence > 1000) {
		spkr.pit_prev_amplitude = SPKR_NEUTRAL_AMPLITUDE;
		spkr.chan->Enable(false);
	}
}

static void InitializeImpulseLUT()
{
	assert(spkr.impulse_lut.size() == SPKR_FILTER_WIDTH);
	for (uint16_t i = 0; i < SPKR_FILTER_WIDTH; ++i)
		spkr.impulse_lut[i] = impulse(
		        i / (static_cast<double>(spkr.rate) * SPKR_OVERSAMPLING));
}

class PCSPEAKER final : public Module_base {
public:
	PCSPEAKER(Section *configuration) : Module_base(configuration)
	{
		Section_prop *section = static_cast<Section_prop *>(configuration);
		if (!section->Get_bool("pcspeaker"))
			return;

		// Get the playback rate and calculate related constants
		spkr.rate = std::max(section->Get_int("pcrate"), 8000);
		spkr.rate_as_float = static_cast<float>(spkr.rate);
		spkr.rate_per_ms = spkr.rate_as_float / 1000.0f;
		spkr.minimum_counter = 2 * PIT_TICK_RATE / spkr.rate;

		InitializeImpulseLUT();

		// Size the waveform queue
		// +1 to compensate for rounding down of the division
		static auto waveform_size = SPKR_FILTER_QUALITY +
		                            spkr.rate / 1000 + 1;
		spkr.waveform_deque.resize(check_cast<uint16_t>(waveform_size), 0.0f);

		// Setup the soft limiter
		constexpr auto channel_name = "SPKR";
		spkr.soft_limiter = std::make_unique<SoftLimiter>(channel_name);
		assert(spkr.soft_limiter);

		// Register the sound channel
		spkr.chan = MIXER_AddChannel(&PCSPEAKER_CallBack,
		                             spkr.rate,
		                             "SPKR",
		                             {ChannelFeature::ReverbSend,
		                              ChannelFeature::ChorusSend});
		assert(spkr.chan);
		spkr.chan->SetPeakAmplitude(
		        static_cast<uint32_t>(AMPLITUDE_POSITIVE));

		// Setup filters
		std::string filter_pref = section->Get_string("pcspeaker_filter");

		if (filter_pref == "on") {
			// The filters are meant to emulate the bandwidth limited
			// sound of the small PC speaker. This more accurately
			// reflects people's actual experience of the PC speaker
			// sound than the raw unfiltered output, and it's a lot
			// more pleasant to listen to, especially in headphones.
			constexpr auto hp_order       = 3;
			constexpr auto hp_cutoff_freq = 120;
			spkr.chan->ConfigureHighPassFilter(hp_order, hp_cutoff_freq);
			spkr.chan->SetHighPassFilter(FilterState::On);

			constexpr auto lp_order       = 2;
			constexpr auto lp_cutoff_freq = 4800;
			spkr.chan->ConfigureLowPassFilter(lp_order, lp_cutoff_freq);
			spkr.chan->SetLowPassFilter(FilterState::On);
		} else {
			if (filter_pref != "off")
				LOG_WARNING("PCSPEAKER: Invalid filter setting '%s', using off",
				            filter_pref.c_str());

			spkr.chan->SetHighPassFilter(FilterState::Off);
			spkr.chan->SetLowPassFilter(FilterState::Off);
		}
		spkr.chan->Enable(true);
	}
	~PCSPEAKER()
	{
		Section_prop *section = static_cast<Section_prop *>(m_configuration);
		if (!section->Get_bool("pcspeaker"))
			return;
	}
};
static PCSPEAKER *test;

void PCSPEAKER_ShutDown([[maybe_unused]] Section *sec)
{
	delete test;
}

void PCSPEAKER_Init(Section *sec)
{
	test = new PCSPEAKER(sec);
	sec->AddDestroyFunction(&PCSPEAKER_ShutDown, true);
}
