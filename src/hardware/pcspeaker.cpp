/*
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
 
// NOTE: a lot of this code assumes that the callback is called every emulated millisecond

//#define SPKR_DEBUGGING
#define LOOKUP
#include <math.h>
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "pic.h"

#define NASSERT(X, Y) do{if (X) E_Exit(__FILE__ ": assertion violation: " Y);}while(0)
#define ASSERT(X, Y)  do{if (!(X)) E_Exit(__FILE__ ": assertion violation: " Y);}while(0)

#ifndef PI
#define PI 3.14159265358979323846
#endif

#define SPKR_ENTRIES 1024
#define SPKR_VOLUME 40000

#define SPKR_FILTER_QUALITY 100
#define SPKR_OVERSAMPLING 32
#define SPKR_CUTOFF_MARGIN 0.2 // must be greater than 0.0
#define SPKR_FILTER_WIDTH (SPKR_FILTER_QUALITY*SPKR_OVERSAMPLING)
#define SPKR_HIGHPASS 0.999 // FIXME: should be selected based on sampling rate

struct DelayEntry {
	float index;
	bool output_level;
};

static struct {
	MixerChannel * chan;
	Bitu pit_mode;
	Bitu rate;

	bool  pit_output_enabled;
	bool  pit_clock_gate_enabled;
	bool  pit_output_level;
	float pit_new_max,pit_new_half;
	float pit_max,pit_half;
	float pit_index;
	bool  pit_mode1_waiting_for_counter;
	bool  pit_mode1_waiting_for_trigger;
	float pit_mode1_pending_max;

	bool  pit_mode3_counting;
	float volwant,volcur;
	Bitu last_ticks;
	float last_index;
	Bitu minimum_counter;
	DelayEntry entries[SPKR_ENTRIES];
	Bitu used;
} spkr;

inline static void AddDelayEntry(float index, bool new_output_level) {
#ifdef SPKR_DEBUGGING
	if (index < 0 || index > 1) {
		LOG_MSG("AddDelayEntry: index out of range %f at %f", index, PIC_FullIndex());
	}
#endif
	static bool previous_output_level = 0;
	if (new_output_level == previous_output_level) {
		return;
	}
	previous_output_level = new_output_level;
	if (spkr.used == SPKR_ENTRIES) {
		return;
	}
	spkr.entries[spkr.used].index=index;
	spkr.entries[spkr.used].output_level=new_output_level;
	spkr.used++;
}

inline static void AddPITOutput(float index) {
	if (spkr.pit_output_enabled) {
		AddDelayEntry(index, spkr.pit_output_level);
	}
}

static void ForwardPIT(float newindex) {
#ifdef SPKR_DEBUGGING
	if (newindex < 0 || newindex > 1) {
		LOG_MSG("ForwardPIT: index out of range %f at %f", newindex, PIC_FullIndex());
	}
#endif
	float passed=(newindex-spkr.last_index);
	float delay_base=spkr.last_index;
	spkr.last_index=newindex;
	switch (spkr.pit_mode) {
	case 6: // dummy
		return;
	case 0:
		if (spkr.pit_index >= spkr.pit_max) {
			return; // counter reached zero before previous call so do nothing
		}
		spkr.pit_index += passed;
		if (spkr.pit_index >= spkr.pit_max) {
			// counter reached zero between previous and this call
			float delay = delay_base;
			delay += spkr.pit_max - spkr.pit_index + passed;
			spkr.pit_output_level = 1;
			AddPITOutput(delay);
		}
		return;
	case 1:
		if (spkr.pit_mode1_waiting_for_counter) {
			// assert output level is high
			return; // counter not written yet
		}
		if (spkr.pit_mode1_waiting_for_trigger) {
			// assert output level is high
			return; // no pulse yet
		}
		if (spkr.pit_index >= spkr.pit_max) {
			return; // counter reached zero before previous call so do nothing
		}
		spkr.pit_index += passed;
		if (spkr.pit_index >= spkr.pit_max) {
			// counter reached zero between previous and this call
			float delay = delay_base;
			delay += spkr.pit_max - spkr.pit_index + passed;
			spkr.pit_output_level = 1;
			AddPITOutput(delay);
			// finished with this pulse
			spkr.pit_mode1_waiting_for_trigger = 1;
		}
		return;
	case 2:
		while (passed>0) {
			/* passed the initial low cycle? */
			if (spkr.pit_index>=spkr.pit_half) {
				/* Start a new low cycle */
				if ((spkr.pit_index+passed)>=spkr.pit_max) {
					float delay=spkr.pit_max-spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_output_level = 0;
					AddPITOutput(delay_base);
					spkr.pit_index=0;
				} else {
					spkr.pit_index+=passed;
					return;
				}
			} else {
				if ((spkr.pit_index+passed)>=spkr.pit_half) {
					float delay=spkr.pit_half-spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_output_level = 1;
					AddPITOutput(delay_base);
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
		if (!spkr.pit_mode3_counting) break;
		while (passed>0) {
			/* Determine where in the wave we're located */
			if (spkr.pit_index>=spkr.pit_half) {
				if ((spkr.pit_index+passed)>=spkr.pit_max) {
					float delay=spkr.pit_max-spkr.pit_index;
					delay_base+=delay;passed-=delay;
					spkr.pit_output_level = 1;
					AddPITOutput(delay_base);
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
					spkr.pit_output_level = 0;
					AddPITOutput(delay_base);
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
				spkr.pit_output_level = 0;
				AddPITOutput(delay_base); //No new events unless reprogrammed
				spkr.pit_index=spkr.pit_max;
			} else spkr.pit_index+=passed;
		}
		break;
		//END CASE 4
	}
}

void PCSPEAKER_SetPITControl(Bitu mode) {
	float newindex = PIC_TickIndex();
	ForwardPIT(newindex);
#ifdef SPKR_DEBUGGING
	fprintf(PCSpeakerLog, "%f pit command: %u\n", PIC_FullIndex(), mode);
#endif
	// TODO: implement all modes
	switch(mode) {
	case 1:
		spkr.pit_mode = 1;
		spkr.pit_mode1_waiting_for_counter = 1;
		spkr.pit_mode1_waiting_for_trigger = 0;
		spkr.pit_output_level = 1;
		break;
	case 3:
		spkr.pit_mode = 3;
		spkr.pit_mode3_counting = 0;
		spkr.pit_output_level = 1;
		break;
	default:
		return;
	}
	AddPITOutput(newindex);
}

void PCSPEAKER_SetCounter(Bitu cntr, Bitu mode) {
#ifdef SPKR_DEBUGGING
	fprintf(PCSpeakerLog, "%f counter: %u, mode: %u\n", PIC_FullIndex(), cntr, mode);
#endif
	//if (!spkr.last_ticks) {
	//	if(spkr.chan) spkr.chan->Enable(true);
	//	spkr.last_index=0;
	//}
	//spkr.last_ticks=PIC_Ticks;
	float newindex=PIC_TickIndex();
	ForwardPIT(newindex);
	switch (mode) {
	case 0:		/* Mode 0 one shot, used with "realsound" (PWM) */
		//if (cntr>80) {
		//	cntr=80;
		//}
		//spkr.pit_output_level=((float)cntr-40)*(SPKR_VOLUME/40.0f);
		spkr.pit_output_level = 0;
		spkr.pit_index = 0;
		spkr.pit_max = (1000.0f / PIT_TICK_RATE) * cntr;
		AddPITOutput(newindex);
		break;
	case 1: // retriggerable one-shot, used by Star Control 1
		spkr.pit_mode1_pending_max = (1000.0f / PIT_TICK_RATE) * cntr;
		if (spkr.pit_mode1_waiting_for_counter) {
			// assert output level is high
			spkr.pit_mode1_waiting_for_counter = 0;
			spkr.pit_mode1_waiting_for_trigger = 1;
		}
		break;
	case 2:			/* Single cycle low, rest low high generator */
		spkr.pit_index=0;
		spkr.pit_output_level = 0;
		AddPITOutput(newindex);
		spkr.pit_half=(1000.0f/PIT_TICK_RATE)*1;
		spkr.pit_max=(1000.0f/PIT_TICK_RATE)*cntr;
		break;
	case 3:		/* Square wave generator */
		if (cntr < spkr.minimum_counter) {
//#ifdef SPKR_DEBUGGING
//			LOG_MSG(
//				"SetCounter: too high frequency %u (cntr %u) at %f",
//				PIT_TICK_RATE/cntr,
//				cntr,
//				PIC_FullIndex());
//#endif
			// hack to save CPU cycles
			//cntr = spkr.minimum_counter;
			spkr.pit_output_level = 1; // avoid breaking digger music
			spkr.pit_mode = 6; // dummy mode with constant high output
			AddPITOutput(newindex);			return;
		}
		spkr.pit_new_max = (1000.0f/PIT_TICK_RATE)*cntr;
		spkr.pit_new_half=spkr.pit_new_max/2;
		if (!spkr.pit_mode3_counting) {
			spkr.pit_index = 0;
			spkr.pit_max = spkr.pit_new_max;
			spkr.pit_half = spkr.pit_new_half;
			if (spkr.pit_clock_gate_enabled) {
				spkr.pit_mode3_counting = 1;
				spkr.pit_output_level = 1; // probably not necessary
				AddPITOutput(newindex);
			}
		}
		break;
	case 4:		/* Software triggered strobe */
		spkr.pit_output_level = 1;
		AddPITOutput(newindex);
		spkr.pit_index=0;
		spkr.pit_max=(1000.0f/PIT_TICK_RATE)*cntr;
		break;
	default:
#ifdef SPKR_DEBUGGING
		LOG_MSG("Unhandled speaker mode %d at %f", mode, PIC_FullIndex());
#endif
		return;
	}
	spkr.pit_mode = mode;
}

void PCSPEAKER_SetType(bool pit_clock_gate_enabled, bool pit_output_enabled) {
#ifdef SPKR_DEBUGGING
	fprintf(
			PCSpeakerLog,
			"%f output: %s, clock gate %s\n",
			PIC_FullIndex(),
			pit_output_enabled ? "pit" : "forced low",
			pit_clock_gate_enabled ? "on" : "off"
			);
#endif
	//if (!spkr.last_ticks) {
	//	if(spkr.chan) spkr.chan->Enable(true);
	//	spkr.last_index=0;
	//}
	//spkr.last_ticks=PIC_Ticks;
	float newindex=PIC_TickIndex();
	ForwardPIT(newindex);
	// pit clock gate enable rising edge is a trigger
	bool pit_trigger = pit_clock_gate_enabled && !spkr.pit_clock_gate_enabled;
	spkr.pit_clock_gate_enabled = pit_clock_gate_enabled;
	spkr.pit_output_enabled     = pit_output_enabled;
	if (pit_trigger) {
		switch (spkr.pit_mode) {
	case 1:
			if (spkr.pit_mode1_waiting_for_counter) {
				// assert output level is high
		break;
			}
			spkr.pit_output_level = 0;
			spkr.pit_index = 0;
			spkr.pit_max = spkr.pit_mode1_pending_max;
			spkr.pit_mode1_waiting_for_trigger = 0;
		break;
	case 3:
			spkr.pit_mode3_counting = 1;
			spkr.pit_new_max = spkr.pit_new_max;
			spkr.pit_new_half=spkr.pit_new_max/2;
			spkr.pit_index = 0;
			spkr.pit_max = spkr.pit_new_max;
			spkr.pit_half = spkr.pit_new_half;
			spkr.pit_output_level = 1;
			break;
		default:
			// TODO: implement other modes
			break;
		}
	} else if (!pit_clock_gate_enabled) {
		switch (spkr.pit_mode) {
		case 1:
			// gate level does not affect mode1
		break;
		case 3:
			// low gate forces pit output high
			spkr.pit_output_level   = 1;
			spkr.pit_mode3_counting = 0;
			break;
		default:
			// TODO: implement other modes
			break;
		}
	}
	if (pit_output_enabled) {
		AddDelayEntry(newindex, spkr.pit_output_level);
	} else {
		AddDelayEntry(newindex, 0);
	}
}

static unsigned output_buffer_length = 0;
static double*  output_buffer = NULL;
static double*  sampled_impulse = NULL;

// TODO: check if this is accurate
static inline double sinc(double t) {
#define SINC_ACCURACY 20
	double result = 1;
	int k;
	for (k = 1; k < SINC_ACCURACY; k++) {
		result *= cos(t/pow(2.0, k));
	}
	return result;
}

static inline double impulse(double t) {
	// raised-cosine-windowed sinc function
	double fs = spkr.rate;
	double fc = fs / (2 + SPKR_CUTOFF_MARGIN);
	double  q = SPKR_FILTER_QUALITY;
	if ((0 < t) && (t*fs < q)) {
		double window = 1.0 + cos(2*fs*PI*(q/(2*fs)-t)/q);
		return window*(sinc(2*fc*PI*(t-q/(2*fs))))/2.0;
	} else return 0.0;
}

static inline void add_impulse(double index, double amplitude) {
#ifndef REFERENCE
	unsigned offset = (unsigned)(index*spkr.rate/1000.0);
	unsigned phase  =
			(unsigned)(index*spkr.rate*SPKR_OVERSAMPLING/1000.0) %
			SPKR_OVERSAMPLING;
	if (phase != 0) {
		offset++;
		phase = SPKR_OVERSAMPLING - phase;
	}
	for (unsigned i = 0; i < SPKR_FILTER_QUALITY; i++) {
		ASSERT(offset + i < output_buffer_length, "index into output_buffer too high");
		ASSERT(phase + SPKR_OVERSAMPLING*i < SPKR_FILTER_WIDTH, "index into sampled_impulse too high");
		output_buffer[offset + i] += amplitude*sampled_impulse[phase + i*SPKR_OVERSAMPLING];
	}
}
#else
	for (unsigned i = 0; i < output_buffer_length; i++) {
		output_buffer[i] += amplitude*impulse((double)i/spkr.rate - index/1000.0);
	}
}
#endif

static void PCSPEAKER_CallBack(Bitu len) {
	Bit16s * stream=(Bit16s*)MixTemp;
	ForwardPIT(1);
	spkr.last_index=0;
	for (unsigned i = 0; i < spkr.used; i++) {
		float index        = spkr.entries[i].index;
		float output_level = spkr.entries[i].output_level;
		if (index < 0.0) { // this happens in digger sometimes
			index = 0.0;
		}
		if (index > 1.0) { // probably not necessary
			index = 1.0f;
		}
		add_impulse(
			index,
			output_level*(double)SPKR_VOLUME-SPKR_VOLUME/2.0);
	}
	spkr.used = 0;
	if (len > output_buffer_length) {
		// massive HACK, insert zeros if callback wants too many samples
		LOG_MSG("mixer callback wants too many samples from pc speaker emulator: %u", len);
		for (unsigned i = len - output_buffer_length; i; i--) {
			*stream++= 0; // output
		}
		len = output_buffer_length;
	}
	// "consume" output buffer
	static double current_output_level = 0.0;
	for (unsigned i = 0; i < len; i++) {
		current_output_level += output_buffer[i];
		*stream++=(Bit16s)current_output_level; // output sample
		current_output_level *= SPKR_HIGHPASS;
	}
	// shift out consumed samples
	// TODO: use ring buffer or something to avoid shifting
	for (unsigned i = len; i < output_buffer_length; i++) {
		NASSERT(i - len < 0, "index into output_buffer too low");
		ASSERT(i - len < output_buffer_length, "index into output_buffer too high");
		ASSERT(i < output_buffer_length, "index into output_buffer too high");
		output_buffer[i - len] = output_buffer[i];
	}
	// zero the rest of the samples
	for (unsigned i = output_buffer_length - len; i < output_buffer_length; i++) {
		output_buffer[i] = 0.0;
	}
	if(spkr.chan) spkr.chan->AddSamples_m16(len,(Bit16s*)MixTemp);

	//Turn off speaker after 10 seconds of idle or one second idle when in off mode
	//bool turnoff = false;
	//Bitu test_ticks = PIC_Ticks;
	//if ((spkr.last_ticks + 10000) < test_ticks) turnoff = true;
	//if((!spkr.pit_output_enabled) && ((spkr.last_ticks + 1000) < test_ticks)) turnoff = true;

	//if(turnoff){
	//	if(spkr.volwant == 0) {
	//		spkr.last_ticks = 0;
	//		if(spkr.chan) spkr.chan->Enable(false);
	//	} else {
	//		if(spkr.volwant > 0) spkr.volwant--; else spkr.volwant++;
	//
	//	}
	//}
#ifdef SPKR_DEBUGGING
	if (spkr.used) {
		LOG_MSG("PCSPEAKER_CallBack: DelayEntries not emptied (%u) at %f", spkr.used, PIC_FullIndex());
	} 
#endif
}

static void init_interpolation() {
	sampled_impulse = new double[SPKR_FILTER_WIDTH];
	if (sampled_impulse == NULL) E_Exit("memory allocation failed");
	for (unsigned i = 0; i < SPKR_FILTER_WIDTH; i++) {
		sampled_impulse[i] = impulse((double)i/(spkr.rate*SPKR_OVERSAMPLING));
	}
	//FILE* asdf = fopen("dummkopf.raw", "wb");
	//fwrite(sampled_impulse, sizeof(double), SPKR_FILTER_WIDTH, asdf);
	//fclose(asdf);
	// +1 to compensate for rounding down of the division
	output_buffer_length  = SPKR_FILTER_QUALITY + spkr.rate/1000 + 1;
	output_buffer = new double[output_buffer_length];
	if (output_buffer == NULL) E_Exit("memory allocation failed");
	for (unsigned i = 0; i < output_buffer_length; i++) {
		output_buffer[i] = 0.0;
	}
	// DEBUG
	LOG_MSG("PC speaker output buffer length: %u", output_buffer_length);
}

class PCSPEAKER:public Module_base {
private:
	MixerObject MixerChan;
public:
	PCSPEAKER(Section* configuration):Module_base(configuration){
		spkr.chan=0;
		Section_prop * section=static_cast<Section_prop *>(configuration);
		if(!section->Get_bool("pcspeaker")) return;
		spkr.pit_output_enabled = 0;
		spkr.pit_clock_gate_enabled = 0;
		spkr.pit_mode1_waiting_for_trigger = 1;
		//spkr.last_ticks=0;
		spkr.last_index=0;
		spkr.rate=section->Get_int("pcrate");
		init_interpolation();

		// PIT initially in mode 3 at ~903 Hz
		spkr.pit_mode = 3;
		spkr.pit_mode3_counting = 0;
		spkr.pit_output_level = 1;
		spkr.pit_max=(1000.0f/PIT_TICK_RATE)*1320;
		spkr.pit_half=spkr.pit_max/2;
		spkr.pit_new_max=spkr.pit_max;
		spkr.pit_new_half=spkr.pit_half;
		spkr.pit_index=0;

		//spkr.minimum_counter = (PIT_TICK_RATE + spkr.rate/2-1)/(spkr.rate/2);
		spkr.minimum_counter = 2*PIT_TICK_RATE/spkr.rate;
		spkr.used=0;
		/* Register the sound channel */
		spkr.chan=MixerChan.Install(&PCSPEAKER_CallBack,spkr.rate,"SPKR");
		if (!spkr.chan) {
			E_Exit(__FILE__ ": Unable to register channel with mixer.");
		}
		spkr.chan->Enable(true);
	}
	~PCSPEAKER(){
		Section_prop * section=static_cast<Section_prop *>(m_configuration);
		if(!section->Get_bool("pcspeaker")) return;
	}
};
static PCSPEAKER* test;

void PCSPEAKER_ShutDown(Section* sec){
	delete test;
}

void PCSPEAKER_Init(Section* sec) {
	test = new PCSPEAKER(sec);
	sec->AddDestroyFunction(&PCSPEAKER_ShutDown,true);
}
