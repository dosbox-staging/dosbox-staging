/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
#include "timer.h"
#include "setup.h"
#include "pic.h"

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
constexpr float AMPLITUDE_MINUS_5DB = 0.562341f;
constexpr float AMPLITUDE_RMS = static_cast<float>(M_SQRT1_2);
constexpr float AMPLITUDE_POSITIVE = std::numeric_limits<int16_t>::max() *
                                     AMPLITUDE_MINUS_5DB * AMPLITUDE_RMS;
constexpr float AMPLITUDE_NEGATIVE = std::numeric_limits<int16_t>::min() *
                                     AMPLITUDE_MINUS_5DB * AMPLITUDE_RMS;
constexpr float AMPLITUDE_NEUTRAL = (AMPLITUDE_POSITIVE + AMPLITUDE_NEGATIVE) / 2;
constexpr float AMPLITUDE_SQUARE_WAVE_REDUCER = AMPLITUDE_MINUS_5DB / 2;

#define DC_SILENCER_WAVES   5u
#define DC_SILENCER_WAVE_HZ 30u

enum SPKR_MODES { SPKR_OFF, SPKR_ON, SPKR_PIT_OFF, SPKR_PIT_ON };

struct DelayEntry {
	float index = 0.0f;
	float vol = 0.0f;
};

static struct {
	DelayEntry entries[SPKR_ENTRIES] = {};
	DCSilencer dc_silencer = {};
	MixerChannel *chan = nullptr;
	SPKR_MODES prev_mode = SPKR_OFF;
	SPKR_MODES mode = SPKR_OFF;
	Bitu prev_pit_mode = 3;
	Bitu pit_mode = 3;
	Bitu rate = 0u;
	Bitu min_tr = 0u;
	Bitu used = 0u;
	float pit_last = 0.0f;
	float pit_max = (1000.0f / PIT_TICK_RATE) * 1320;
	float pit_half = pit_max / 2;
	float pit_new_max = pit_max;
	float pit_new_half = pit_half;
	float pit_index = 0.0f;
	float volwant = 0.0f;
	float volcur = 0.0f;
	float last_index = 0.0f;
	int16_t last_played_sample = 0;
	uint16_t prev_pos = 0u;
	uint8_t idle_countdown = 0u;
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

	// We have a square wave if either are true:

	// 1. PIT mode is set, and the speaker ...
	if (spkr.pit_mode) {
		// previously generated a square wave
		if (spkr.prev_mode == SPKR_PIT_ON)
			return true;
		// is currently generating a square wave
		if (spkr.mode == SPKR_PIT_ON)
			return true;
	}

	// 2. Speaker was in a neutral position and is transitioning ...
	if (!spkr.prev_pos) {
		// into a square wave
		if (spkr.prev_mode != SPKR_PIT_ON && spkr.mode == SPKR_PIT_ON)
			return true;
		// out of a square wave
		if (spkr.prev_mode == SPKR_PIT_ON && spkr.mode != SPKR_PIT_ON)
			return true;
	}
	return false;
}

static void AddDelayEntry(float index,float vol) {
	if (spkr.used==SPKR_ENTRIES) {
		return;
	}
	spkr.entries[spkr.used].index=index;

	if (IsWaveSquare())
		vol *= AMPLITUDE_SQUARE_WAVE_REDUCER;

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

static void ForwardPIT(float newindex) {
	float passed=(newindex-spkr.last_index);
	float delay_base=spkr.last_index;
	spkr.last_index=newindex;
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
					float delay=spkr.pit_max-spkr.pit_index;
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
					float delay=spkr.pit_half-spkr.pit_index;
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
					float delay=spkr.pit_max-spkr.pit_index;
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
					float delay=spkr.pit_half-spkr.pit_index;
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
				float delay=spkr.pit_max-spkr.pit_index;
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
void PCSPEAKER_SetCounter(Bitu cntr, Bitu mode)
{
	if (!SpeakerExists())
		return;

	float newindex=PIC_TickIndex();
	ForwardPIT(newindex);
	spkr.prev_pit_mode = spkr.pit_mode;
	spkr.pit_mode = mode;
	switch (mode) {
	case 0:		/* Mode 0 one shot, used with realsound */
		if (spkr.mode!=SPKR_PIT_ON) return;
		if (cntr>80) { 
			cntr=80;
		}
		spkr.pit_last = ((float)cntr - 40) *
		                (AMPLITUDE_POSITIVE / 40.0f);
		AddDelayEntry(newindex,spkr.pit_last);
		spkr.pit_index=0;
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
		spkr.pit_half = (1000.0f / PIT_TICK_RATE) * 1;
		spkr.pit_max = (1000.0f / PIT_TICK_RATE) * cntr;
		break;
	case 3:		/* Square wave generator */
		if (cntr==0 || cntr<spkr.min_tr) {
			/* skip frequencies that can't be represented */
			spkr.pit_last=0;
			spkr.pit_mode=0;
			return;
		}
		spkr.pit_new_max=(1000.0f/PIT_TICK_RATE)*cntr;
		spkr.pit_new_half=spkr.pit_new_max/2;
		break;
	case 4:		/* Software triggered strobe */
		spkr.pit_last = AMPLITUDE_POSITIVE;
		AddDelayEntry(newindex,spkr.pit_last);
		spkr.pit_index=0;
		spkr.pit_max=(1000.0f/PIT_TICK_RATE)*cntr;
		break;
	default:
#if C_DEBUG
		LOG_MSG("Unhandled speaker mode %d",mode);
#endif
		return;
	}
	// Activate the channel after queuing new speaker entries
	spkr.chan->Enable(true);
}

// Returns the AMPLITUDE_NEUTRAL voltage if the speaker's  fully faded,
// otherwise returns the fallback if the speaker is active.
static float NeutralOr(float fallback)
{
	return !spkr.idle_countdown ? AMPLITUDE_NEUTRAL : fallback;
}

// Returns, in order of preference:
// - Neutral voltage, if the speaker's fully faded
// - The last active PIT voltage to stitch on-going playback
// - The fallback voltage to kick start a new sound pattern 
static float NeutralLastPitOr(float fallback)
{
	const bool use_last = std::isgreater(fabs(spkr.pit_last),
	                                     AMPLITUDE_NEUTRAL);
	return NeutralOr(use_last ? spkr.pit_last : fallback);
}

// PWM-mode activation
void PCSPEAKER_SetType(Bitu mode)
{
	if (!SpeakerExists())
		return;

	float newindex = PIC_TickIndex();
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
	constexpr uint8_t grace_time_ms = 100u;
	static_assert(grace_time_ms > 0,
	              "Algorithm depends on a non-zero grace time");

	if (speaker_movements && requested_samples) {
		spkr.idle_countdown = grace_time_ms;
		spkr.dc_silencer.Reset();
	} else if (spkr.idle_countdown) {
		spkr.idle_countdown--;
		spkr.last_played_sample = buffer[requested_samples - 1];
	} else if (!spkr.dc_silencer.Generate(spkr.last_played_sample,
	                                      requested_samples, buffer)) {
		spkr.chan->Enable(false);
	}

	spkr.chan->AddSamples_m16(requested_samples, buffer);
}

static void PCSPEAKER_CallBack(Bitu len)
{
	if (!SpeakerExists())
		return;

	Bit16s * stream=(Bit16s*)MixTemp;
	ForwardPIT(1);
	spkr.last_index=0;
	Bitu count=len;
	uint16_t pos = 0;
	float sample_base=0;
	float sample_add=(1.0001f)/len;
	while (count--) {
		float index=sample_base;
		sample_base+=sample_add;
		float end=sample_base;
		float value = 0;
		while(index<end) {
			/* Check if there is an upcoming event */
			if (spkr.used && spkr.entries[pos].index<=index) {
				spkr.volwant=spkr.entries[pos].vol;
				pos++;spkr.used--;
				continue;
			}
			float vol_end;
			if (spkr.used && spkr.entries[pos].index<end) {
				vol_end=spkr.entries[pos].index;
			} else vol_end=end;
			float vol_len=vol_end-index;
            /* Check if we have to slide the volume */
			float vol_diff=spkr.volwant-spkr.volcur;
			if (vol_diff == 0) {
				value+=spkr.volcur*vol_len;
				index+=vol_len;
			} else {
				// Check how long it will take to goto new level
				// TODO: describe the basis for these magic numbers and their effects
				constexpr float spkr_speed = AMPLITUDE_POSITIVE * 2.0f / 0.070f;
				const float vol_time = fabsf(vol_diff) / spkr_speed;
				if (vol_time <= vol_len) {
					/* Volume reaches endpoint in this block, calc until that point */
					value+=vol_time*spkr.volcur;
					value+=vol_time*vol_diff/2;
					index+=vol_time;
					spkr.volcur=spkr.volwant;
				} else {
					/* Volume still not reached in this block */
					value += spkr.volcur * vol_len;
					const float speed_by_len = spkr_speed * vol_len;
					const float speed_by_len_sq = speed_by_len *
					                              vol_len / 2;
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
		*stream++=(Bit16s)(value/sample_add);
	}
	PlayOrFadeout(pos, len, reinterpret_cast<int16_t *>(MixTemp));
}
class PCSPEAKER:public Module_base {
private:
	MixerObject MixerChan = {};
public:
	PCSPEAKER(Section *configuration) : Module_base(configuration)
	{
		Section_prop * section=static_cast<Section_prop *>(configuration);
		if (!section->Get_bool("pcspeaker"))
			return;
		spkr.rate = std::max(section->Get_int("pcrate"), 8000);

		spkr.dc_silencer.Configure(static_cast<uint32_t>(spkr.rate),
		                           DC_SILENCER_WAVES, DC_SILENCER_WAVE_HZ);

		spkr.min_tr = (PIT_TICK_RATE + spkr.rate / 2 - 1) / (spkr.rate / 2);
		/* Register the sound channel */
		spkr.chan = MixerChan.Install(&PCSPEAKER_CallBack, spkr.rate, "SPKR");
		spkr.chan->SetPeakAmplitude(
		        static_cast<uint32_t>(AMPLITUDE_POSITIVE));
	}
	~PCSPEAKER(){
		Section_prop * section=static_cast<Section_prop *>(m_configuration);
		if(!section->Get_bool("pcspeaker")) return;
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
