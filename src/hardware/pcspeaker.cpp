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
 
#include "mixer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "dc_silencer.h"
#include "pic.h"
#include "setup.h"
#include "support.h"
#include "timer.h"

#define SPKR_ENTRIES 1024

/*
Amplitude levels for the speaker are computed as follows:
1. Start with the maximum allow level
2. Reduce it by -5 LUFS to match generally upper-bound normalization
   levels for normal audio. Knowing that the PC speaker hits peak
   amplitude, we simply take off 5 dB.
3. Given the PC speaker's PWM-mode doesn't actually produce sine
   curves and instead are stepped approximations, we drop this by the
   RMS level of a sine curve, which should provide roughly the same
   perceived loudness.
4. When the PC Speaker generates square waves, we apply the above reductions
   again plus an additional RMS factor, knowing that square wave are
   exclusively at full amplitude, and carry twice the power of sine waves.
   RMS * RMS == 0.5, so the square wave reduction becomes: - 5 dB / 2.
*/
constexpr double AMPLITUDE_MINUS_5DB = 0.562341;
constexpr double AMPLITUDE_RMS = M_SQRT1_2;
constexpr double AMPLITUDE_POSITIVE = std::numeric_limits<int16_t>::max() *
                                      AMPLITUDE_MINUS_5DB * AMPLITUDE_RMS;
constexpr double AMPLITUDE_NEGATIVE = std::numeric_limits<int16_t>::min() *
                                      AMPLITUDE_MINUS_5DB * AMPLITUDE_RMS;
constexpr double AMPLITUDE_NEUTRAL = (AMPLITUDE_POSITIVE + AMPLITUDE_NEGATIVE) / 2.0;
constexpr double AMPLITUDE_SQUARE_WAVE_REDUCER = AMPLITUDE_MINUS_5DB / 2.0;

constexpr int IDLE_GRACE_TIME_MS = 100;

#define DC_SILENCER_WAVES   5u
#define DC_SILENCER_WAVE_HZ 30u

enum SPKR_MODES { SPKR_OFF, SPKR_ON, SPKR_PIT_OFF, SPKR_PIT_ON };

struct DelayEntry {
	double index = 0.0;
	double vol = 0.0;
};

static struct {
	DelayEntry entries[SPKR_ENTRIES] = {};
	DCSilencer dc_silencer = {};
	mixer_channel_t chan = nullptr;
	SPKR_MODES prev_mode = SPKR_OFF;
	SPKR_MODES mode = SPKR_OFF;
	int prev_pit_mode = 3;
	int pit_mode = 3;
	int rate = 0;
	int min_tr = 0;
	int used = 0;
	double pit_last = 0.0;
	double pit_max = PERIOD_OF_1K_PIT_TICKS * 1320.0;
	double pit_half = pit_max / 2.0;
	double pit_new_max = pit_max;
	double pit_new_half = pit_half;
	double pit_index = 0.0;
	double volwant = 0.0;
	double volcur = 0.0;
	double last_index = 0.0;
	int16_t last_played_sample = 0;
	uint16_t prev_pos = 0u;
	int idle_countdown = IDLE_GRACE_TIME_MS;
	bool neutralize_dc_offset = true;
} spkr;

static bool SpeakerExists()
{
	// If the mixer's channel doesn't exist, then dosbox
	// hasn't created the device yet (so return false).
	return spkr.chan != nullptr;
}

// Determines if the inbound wave is square by inspecting
// current and previous states.
static bool IsWaveSquare() {
	// When compared across time, this value describe an active PIT-state being toggled
	constexpr auto pit_was_toggled = SPKR_PIT_OFF + SPKR_PIT_ON;

	// The sum of the previous mode with the current mode becomes a temporal-state
	const auto temporal_pit_state = spkr.prev_pit_mode + spkr.pit_mode;
	const auto temporal_pwm_state = static_cast<int>(spkr.prev_mode) +
	                                static_cast<int>(spkr.mode);

	// We have a sine-wave if the PIT was steadily off and ...
	if (temporal_pit_state == SPKR_OFF)
		// The PWM toggled an ongoing PIT state or turned on PIT-mode from an off-state
		if (temporal_pwm_state == pit_was_toggled || temporal_pwm_state == SPKR_PIT_ON)
			return false;

	// We have a sine-wave if the PIT was steadily on and ...
	if (temporal_pit_state == SPKR_PIT_ON)
		// the PWM was turned on from an off-state
		if (temporal_pwm_state == SPKR_ON)
			return false;

	return true;
}

static void AddDelayEntry(double index, double vol)
{
	if (spkr.used == SPKR_ENTRIES) {
		return;
	}
	spkr.entries[spkr.used].index=index;

	if (IsWaveSquare()) {
		vol *= AMPLITUDE_SQUARE_WAVE_REDUCER;
// #define DEBUG_SQUARE_WAVE 1
#ifdef DEBUG_SQUARE_WAVE
		LOG_MSG("SPEAKER: square-wave [prev_pos=%u, prev_mode=%u, mode=%u, prev_pit=%lu, pit=%lu]",
		        spkr.prev_pos, spkr.prev_mode, spkr.mode,
		        spkr.prev_pit_mode, spkr.pit_mode);
	} else {
		LOG_MSG("SPEAKER: sine-wave [prev_pos=%u, prev_mode=%u, mode=%u, prev_pit=%lu, pit=%lu], ",
		        spkr.prev_pos, spkr.prev_mode, spkr.mode,
		        spkr.prev_pit_mode, spkr.pit_mode);
#endif
	}

	spkr.entries[spkr.used].vol = vol;
	spkr.used++;

#if 0
	// This is extremely verbose; pipe the output to a file.
	// Display the previous and current speaker modes w/ requested volume
	if (fabs(vol) > AMPLITUDE_NEUTRAL)
		LOG_MSG("SPEAKER: Adding pos=%3s, pit=%" PRIuPTR "|%" PRIuPTR
		        ", pwm=%d|%d, volume=%6.0f",
		        spkr.prev_pos > 0 ? "yes" : "no", spkr.prev_pit_mode,
		        spkr.pit_mode, spkr.prev_mode, spkr.mode,
		        static_cast<double>(vol));
#endif
}

static void ForwardPIT(double newindex)
{
	auto passed = (newindex - spkr.last_index);
	auto delay_base = spkr.last_index;
	spkr.last_index = newindex;
	switch (spkr.pit_mode) {
	case 0:
		return;
	case 1:
		return;
	case 2:
		while (passed>0) {
			/* passed the initial low cycle? */
			if (spkr.pit_index>=spkr.pit_half) {
				/* Start a new low cycle */
				if ((spkr.pit_index+passed)>=spkr.pit_max) {
					const auto delay = spkr.pit_max -
					                   spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_last = AMPLITUDE_NEGATIVE;
					if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);
					spkr.pit_index=0;
				} else {
					spkr.pit_index+=passed;
					return;
				}
			} else {
				if ((spkr.pit_index+passed)>=spkr.pit_half) {
					const auto delay = spkr.pit_half -
					                   spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_last = AMPLITUDE_POSITIVE;
					if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);
					spkr.pit_index=spkr.pit_half;
				} else {
					spkr.pit_index+=passed;
					return;
				}
			}
		}
		break;
		//END CASE 2
	case 3:
		while (passed>0) {
			/* Determine where in the wave we're located */
			if (spkr.pit_index>=spkr.pit_half) {
				if ((spkr.pit_index+passed)>=spkr.pit_max) {
					const auto delay = spkr.pit_max -
					                   spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_last = AMPLITUDE_POSITIVE;
					if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);
					spkr.pit_index=0;
					/* Load the new count */
					spkr.pit_half=spkr.pit_new_half;
					spkr.pit_max=spkr.pit_new_max;
				} else {
					spkr.pit_index+=passed;
					return;
				}
			} else {
				if ((spkr.pit_index+passed)>=spkr.pit_half) {
					const auto delay = spkr.pit_half -
					                   spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_last = AMPLITUDE_NEGATIVE;
					if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);
					spkr.pit_index=spkr.pit_half;
					/* Load the new count */
					spkr.pit_half=spkr.pit_new_half;
					spkr.pit_max=spkr.pit_new_max;
				} else {
					spkr.pit_index+=passed;
					return;
				}
			}
		}
		break;
		//END CASE 3
	case 4:
		if (spkr.pit_index<spkr.pit_max) {
			/* Check if we're gonna pass the end this block */
			if (spkr.pit_index+passed>=spkr.pit_max) {
				const auto delay = spkr.pit_max - spkr.pit_index;
				delay_base+=delay;passed-=delay;
				spkr.pit_last = AMPLITUDE_NEGATIVE;
				if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);				//No new events unless reprogrammed
				spkr.pit_index=spkr.pit_max;
			} else spkr.pit_index+=passed;
		}
		break;
		//END CASE 4
	}
}

// PIT-mode activation
void PCSPEAKER_SetCounter(int cntr, int mode)
{
	if (!SpeakerExists())
		return;

	const auto newindex = PIC_TickIndex();
	ForwardPIT(newindex);
	spkr.prev_pit_mode = spkr.pit_mode;
	spkr.pit_mode = mode;
	switch (mode) {
	case 0:		/* Mode 0 one shot, used with realsound */
		if (spkr.mode!=SPKR_PIT_ON) return;
		if (cntr>80) { 
			cntr=80;
		}
		spkr.pit_last = ((double)cntr - 40) * (AMPLITUDE_POSITIVE / 40.0);
		AddDelayEntry(newindex, spkr.pit_last);
		spkr.pit_index = 0;
		break;
	case 1:
		if (spkr.mode!=SPKR_PIT_ON) return;
		spkr.pit_last = AMPLITUDE_POSITIVE;
		AddDelayEntry(newindex, spkr.pit_last);
		break;
	case 2:			/* Single cycle low, rest low high generator */
		spkr.pit_index=0;
		spkr.pit_last = AMPLITUDE_NEGATIVE;
		AddDelayEntry(newindex, spkr.pit_last);
		spkr.pit_half = PERIOD_OF_1K_PIT_TICKS * 1;
		spkr.pit_max = PERIOD_OF_1K_PIT_TICKS * cntr;
		break;
	case 3:		/* Square wave generator */
		if (cntr==0 || cntr<spkr.min_tr) {
			/* skip frequencies that can't be represented */
			spkr.pit_last=0;
			spkr.pit_mode=0;
			return;
		}
		spkr.pit_new_max = PERIOD_OF_1K_PIT_TICKS * cntr;
		spkr.pit_new_half=spkr.pit_new_max/2;
		break;
	case 4:		/* Software triggered strobe */
		spkr.pit_last = AMPLITUDE_POSITIVE;
		AddDelayEntry(newindex,spkr.pit_last);
		spkr.pit_index=0;
		spkr.pit_max = PERIOD_OF_1K_PIT_TICKS * cntr;
		break;
	default:
#if C_DEBUG
		LOG_MSG("Unhandled speaker mode %u", mode);
#endif
		return;
	}
	// Activate the channel after queuing new speaker entries
	spkr.chan->Enable(true);
}

// Returns the AMPLITUDE_NEUTRAL voltage if the speaker's  fully faded,
// otherwise returns the fallback if the speaker is active.
static double NeutralOr(double fallback)
{
	return !spkr.idle_countdown ? AMPLITUDE_NEUTRAL : fallback;
}

// Returns, in order of preference:
// - Neutral voltage, if the speaker's fully faded
// - The last active PIT voltage to stitch on-going playback
// - The fallback voltage to kick start a new sound pattern
static double NeutralLastPitOr(double fallback)
{
	const bool use_last = std::isgreater(fabs(spkr.pit_last),
	                                     AMPLITUDE_NEUTRAL);
	return NeutralOr(use_last ? spkr.pit_last : fallback);
}

// PWM-mode activation
void PCSPEAKER_SetType(int mode)
{
	if (!SpeakerExists())
		return;

	const auto newindex = PIC_TickIndex();
	ForwardPIT(newindex);
	spkr.prev_mode = spkr.mode;
	switch (mode) {
	case 0:
		spkr.mode=SPKR_OFF;
		AddDelayEntry(newindex, NeutralOr(AMPLITUDE_NEGATIVE));
		break;
	case 1:
		spkr.mode=SPKR_PIT_OFF;
		AddDelayEntry(newindex, NeutralOr(AMPLITUDE_NEGATIVE));
		break;
	case 2:
		spkr.mode=SPKR_ON;
		AddDelayEntry(newindex, NeutralOr(AMPLITUDE_POSITIVE));
		break;
	case 3:
		spkr.mode = SPKR_PIT_ON;
		AddDelayEntry(newindex,
		              NeutralLastPitOr(AMPLITUDE_POSITIVE));
		break;
	};

	// Activate the channel after queuing new speaker entries
	spkr.chan->Enable(true);
}

// After the speaker has idled (as defined when the number of speaker
// movements is zero) for a short time, wind down the DC-offset, and
// then halt the channel.
static void PlayOrFadeout(const uint16_t speaker_movements,
                          size_t requested_samples,
                          int16_t *buffer)
{
	if (speaker_movements && requested_samples) {
		static_assert(IDLE_GRACE_TIME_MS > 0,
		              "Algorithm depends on a non-zero grace time");
		spkr.idle_countdown = IDLE_GRACE_TIME_MS;
		spkr.dc_silencer.Reset();
	} else if (spkr.idle_countdown > 0) {
		spkr.idle_countdown--;
		spkr.last_played_sample = buffer[requested_samples - 1];
	} else if (!spkr.dc_silencer.Generate(spkr.last_played_sample,
	                                      requested_samples, buffer)) {
		spkr.chan->Enable(false);
	}

	spkr.chan->AddSamples_m16(check_cast<uint16_t>(requested_samples), buffer);
}

static void PCSPEAKER_CallBack(uint16_t len)
{
	if (!SpeakerExists())
		return;

	int16_t * stream=(int16_t*)MixTemp;
	ForwardPIT(1);
	spkr.last_index=0;
	auto count = len;
	uint16_t pos = 0;
	auto sample_base = 0.0;
	const auto sample_add = (1.0001) / len;
	while (count--) {
		auto index = sample_base;
		sample_base+=sample_add;
		const auto end = sample_base;
		auto value = 0.0;
		while (index < end) {
			/* Check if there is an upcoming event */
			if (spkr.used && spkr.entries[pos].index<=index) {
				spkr.volwant=spkr.entries[pos].vol;
				pos++;spkr.used--;
				continue;
			}
			double vol_end;
			if (spkr.used && spkr.entries[pos].index<end) {
				vol_end=spkr.entries[pos].index;
			} else vol_end=end;
			const auto vol_len = vol_end - index;
			/* Check if we have to slide the volume */
			const auto vol_diff = spkr.volwant - spkr.volcur;
			if (vol_diff == 0) {
				value+=spkr.volcur*vol_len;
				index+=vol_len;
			} else {
				// Check how long it will take to goto new level
				// TODO: describe the basis for these magic numbers and their effects
				constexpr auto spkr_speed = AMPLITUDE_POSITIVE *
				                            2.0 / 0.070;
				const auto vol_time = fabs(vol_diff) / spkr_speed;
				if (vol_time <= vol_len) {
					/* Volume reaches endpoint in this block, calc until that point */
					value+=vol_time*spkr.volcur;
					value+=vol_time*vol_diff/2;
					index+=vol_time;
					spkr.volcur=spkr.volwant;
				} else {
					/* Volume still not reached in this block */
					value += spkr.volcur * vol_len;
					const auto speed_by_len = spkr_speed * vol_len;
					const auto speed_by_len_sq = speed_by_len *
					                             vol_len / 2.0;
					if (vol_diff < 0) {
						value -= speed_by_len_sq;
						spkr.volcur -= speed_by_len;
					} else {
						value += speed_by_len_sq;
						spkr.volcur += speed_by_len;
					}
					index+=vol_len;
				}
			}
		}
		spkr.prev_pos = pos;
		*stream++=(int16_t)(value/sample_add);
	}

	int16_t *buffer = reinterpret_cast<int16_t *>(MixTemp);
	if (spkr.neutralize_dc_offset)
		PlayOrFadeout(pos, len, buffer);
	else
		spkr.chan->AddSamples_m16(len, buffer);
}
class PCSPEAKER final : public Module_base {
public:
	PCSPEAKER(Section *configuration) : Module_base(configuration)
	{
		Section_prop * section=static_cast<Section_prop *>(configuration);
		if (!section->Get_bool("pcspeaker"))
			return;
		spkr.rate = std::max(section->Get_int("pcrate"), 8000);

		std::string dc_offset_pref = section->Get_string("zero_offset");
		if (dc_offset_pref == "auto")
#if !defined(WIN32)
			spkr.neutralize_dc_offset = true;
#else
			spkr.neutralize_dc_offset = false;
#endif
		else
			spkr.neutralize_dc_offset = (dc_offset_pref == "true");

		spkr.dc_silencer.Configure(static_cast<uint32_t>(spkr.rate),
		                           DC_SILENCER_WAVES, DC_SILENCER_WAVE_HZ);

		spkr.min_tr = (PIT_TICK_RATE + spkr.rate / 2 - 1) / (spkr.rate / 2);

		/* Register the sound channel */
		spkr.chan = MIXER_AddChannel(&PCSPEAKER_CallBack,
		                             spkr.rate,
		                             "SPKR",
		                             {ChannelFeature::ReverbSend,
		                              ChannelFeature::ChorusSend});
		spkr.chan->SetPeakAmplitude(
		        static_cast<uint32_t>(AMPLITUDE_POSITIVE));


		// Setup filters
		std::string filter_pref = section->Get_string("pcspeaker_filter");

		if (filter_pref == "on") {
			constexpr auto hp_order       = 3;
			constexpr auto hp_cutoff_freq = 120.0f;
			spkr.chan->ConfigureHighPassFilter(hp_order, hp_cutoff_freq);
			spkr.chan->SetHighPassFilter(FilterState::On);

			constexpr auto lp_order       = 2;
			constexpr auto lp_cutoff_freq = 4800.0f;
			spkr.chan->ConfigureLowPassFilter(lp_order, lp_cutoff_freq);
			spkr.chan->SetLowPassFilter(FilterState::On);
		} else {
			if (filter_pref != "off")
				LOG_WARNING("PCSPEAKER: Invalid filter setting '%s', using off",
				            filter_pref.c_str());

			spkr.chan->SetHighPassFilter(FilterState::Off);
			spkr.chan->SetLowPassFilter(FilterState::Off);
		}
	}

	PCSPEAKER(const PCSPEAKER&) = delete; // prevent copying
	PCSPEAKER& operator= (const PCSPEAKER&) = delete; // prevent assignment
};
static PCSPEAKER* test;

void PCSPEAKER_ShutDown(Section* sec){
	(void) sec; // unused
	delete test;
}

void PCSPEAKER_Init(Section* sec) {
	test = new PCSPEAKER(sec);
	sec->AddDestroyFunction(&PCSPEAKER_ShutDown,true);
}
