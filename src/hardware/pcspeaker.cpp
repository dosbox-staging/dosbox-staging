/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
//#define SPKR_DEBUGGING
#include <math.h>
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "pic.h"


#ifdef SPKR_DEBUGGING
FILE* PCSpeakerLog = NULL;
FILE* PCSpeakerState = NULL;
FILE* PCSpeakerOutput = NULL;
FILE* PCSpeakerOutputLevelLog = NULL;

typedef enum {
	LABEL_CONTROL,
	LABEL_COUNTER,
	LABEL_OUTPUT
} state_label_e;

typedef struct {
	bool pit_output_enabled;
	bool pit_clock_gate_enabled;
} output_state_t;

typedef union {
	unsigned char pit_mode;
	Bit32u counter;
	output_state_t output_state;
} state_specific_data_u;

typedef struct {
	double timestamp;
	state_label_e state_label;
	state_specific_data_u state_specific_data;
} speaker_state_change_t;

#endif

#define SPKR_ENTRIES 1024
#define SPKR_VOLUME 10000
//#define SPKR_SHIFT 8
#define SPKR_SPEED (float)((SPKR_VOLUME*2)/0.050f) // TODO: replace with runtime value

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
	speaker_state_change_t temp;
	memset(&temp, 0, sizeof(speaker_state_change_t));
	temp.timestamp = PIC_FullIndex();
	temp.state_label = LABEL_CONTROL;
	temp.state_specific_data.pit_mode = mode;
	if (fwrite(&temp, sizeof(temp), 1, PCSpeakerState) != 1)
		LOG_MSG(__FILE__ ": unable to write to pc speaker log");
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
	speaker_state_change_t temp;
	memset(&temp, 0, sizeof(speaker_state_change_t));
	temp.timestamp = PIC_FullIndex();
	temp.state_label = LABEL_COUNTER;
	temp.state_specific_data.counter = cntr;
	if (fwrite(&temp, sizeof(temp), 1, PCSpeakerState) != 1)
		LOG_MSG(__FILE__ ": unable to write to pc speaker log");
#endif
	if (!spkr.last_ticks) {
		if(spkr.chan) spkr.chan->Enable(true);
		spkr.last_index=0;
	}
	spkr.last_ticks=PIC_Ticks;
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
			cntr = spkr.minimum_counter;
			//spkr.pit_output_level = 1; // avoid breaking digger music
			//spkr.pit_mode = 6; // dummy mode with constant output
			//AddPITOutput(newindex);
			//return;
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
	speaker_state_change_t temp;
	memset(&temp, 0, sizeof(speaker_state_change_t));
	temp.timestamp = PIC_FullIndex();
	temp.state_label = LABEL_OUTPUT;
	temp.state_specific_data.output_state.pit_clock_gate_enabled = pit_clock_gate_enabled;
	temp.state_specific_data.output_state.pit_output_enabled     = pit_output_enabled;
	if (fwrite(&temp, sizeof(temp), 1, PCSpeakerState) != 1)
		LOG_MSG(__FILE__ ": unable to write to pc speaker log");
#endif
	if (!spkr.last_ticks) {
		if(spkr.chan) spkr.chan->Enable(true);
		spkr.last_index=0;
	}
	spkr.last_ticks=PIC_Ticks;
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

static void PCSPEAKER_CallBack(Bitu len) {
	Bit16s * stream=(Bit16s*)MixTemp;
	ForwardPIT(1);
	spkr.last_index=0;
	Bitu count=len;
	Bitu pos=0;
	float sample_base=0;
	float sample_add=(1.0001f)/len;
	while (count--) {
		float index=sample_base;
		sample_base+=sample_add;
		float end=sample_base;
		double value=0;
		while(index<end) {
			/* Check if there is an upcoming event */
			if (spkr.used && spkr.entries[pos].index<=index) {
				spkr.volwant=SPKR_VOLUME*(float)spkr.entries[pos].output_level;
#ifdef SPKR_DEBUGGING
				fprintf(
						PCSpeakerOutputLevelLog,
						"%f %u\n",
						PIC_Ticks + spkr.entries[pos].index,
						spkr.entries[pos].output_level);
				double tempIndex = PIC_Ticks + spkr.entries[pos].index;
				unsigned char tempOutputLevel = spkr.entries[pos].output_level;
				fwrite(&tempIndex, sizeof(double), 1, PCSpeakerOutput);
				fwrite(&tempOutputLevel, sizeof(unsigned char), 1, PCSpeakerOutput);
#endif
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
				/* Check how long it will take to goto new level */
				float vol_time=fabs(vol_diff)/SPKR_SPEED;
				if (vol_time<=vol_len) {
					/* Volume reaches endpoint in this block, calc until that point */
					value+=vol_time*spkr.volcur;
					value+=vol_time*vol_diff/2;
					index+=vol_time;
					spkr.volcur=spkr.volwant;
				} else {
					/* Volume still not reached in this block */
					value+=spkr.volcur*vol_len;
					if (vol_diff<0) {
						value-=(SPKR_SPEED*vol_len*vol_len)/2;
						spkr.volcur-=SPKR_SPEED*vol_len;
					} else {
						value+=(SPKR_SPEED*vol_len*vol_len)/2;
						spkr.volcur+=SPKR_SPEED*vol_len;
					}
					index+=vol_len;
				}
			}
		}
		*stream++=(Bit16s)(value/sample_add);
	}
	if(spkr.chan) spkr.chan->AddSamples_m16(len,(Bit16s*)MixTemp);

	//Turn off speaker after 10 seconds of idle or one second idle when in off mode
	bool turnoff = false;
	Bitu test_ticks = PIC_Ticks;
	if ((spkr.last_ticks + 10000) < test_ticks) turnoff = true;
	if((!spkr.pit_output_enabled) && ((spkr.last_ticks + 1000) < test_ticks)) turnoff = true;

	if(turnoff){
		if(spkr.volwant == 0) { 
			spkr.last_ticks = 0;
			if(spkr.chan) spkr.chan->Enable(false);
		} else {
			if(spkr.volwant > 0) spkr.volwant--; else spkr.volwant++;
		
		}
	}
#ifdef SPKR_DEBUGGING
	if (spkr.used) {
		LOG_MSG("PCSPEAKER_CallBack: DelayEntries not emptied (%u) at %f", spkr.used, PIC_FullIndex());
	}
#endif

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
		spkr.last_ticks=0;
		spkr.last_index=0;
		spkr.rate=section->Get_int("pcrate");

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
#ifdef SPKR_DEBUGGING
		PCSpeakerLog = fopen("PCSpeakerLog.txt", "w");
		if (PCSpeakerLog == NULL) {
			E_Exit(__FILE__ ": Unable to create a PC speaker log for debugging.");
		}
		PCSpeakerState = fopen("PCSpeakerState.dat", "wb");
		if (PCSpeakerState == NULL) {
			E_Exit(__FILE__ ": Unable to create a PC speaker state file for debugging.");
		}
		PCSpeakerOutput = fopen("PCSpeakerOutput.dat", "wb");
		if (PCSpeakerOutput == NULL) {
			E_Exit(__FILE__ ": Unable to create a PC speaker output file for debugging.");
		}
		PCSpeakerOutputLevelLog = fopen("PCSpeakerOutputLevelLog.txt", "w");
		if (PCSpeakerOutputLevelLog == NULL) {
			E_Exit(__FILE__ ": Unable to create a PC speaker output level log for debugging.");
		}
#endif
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

