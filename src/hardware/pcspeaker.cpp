/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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

#include <math.h>
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "pic.h"


#ifndef PI
#define PI 3.14159265358979323846
#endif

#define SPKR_ENTRIES 1024
#define SPKR_VOLUME 5000
#define SPKR_SHIFT 8
#define SPKR_SPEED ((SPKR_VOLUME*2)/70)

enum SPKR_MODES {
	SPKR_OFF,SPKR_ON,SPKR_PIT_OFF,SPKR_PIT_ON
};

struct DelayEntry {
	Bitu index;
	Bits vol;
};

static struct {
	MixerChannel * chan;
	SPKR_MODES mode;
	Bitu pit_mode;
	Bitu rate;

	Bits pit_last;
	Bitu pit_new_max,pit_new_half;
	Bitu pit_max,pit_half;
	Bitu pit_index;
	Bits volwant,volcur;
	Bitu last_ticks;
	Bitu last_index;
	DelayEntry entries[SPKR_ENTRIES];
	Bitu used;
} spkr;

static void AddDelayEntry(Bitu index,Bits vol) {
	if (spkr.used==SPKR_ENTRIES) {
		return;
	}
	spkr.entries[spkr.used].index=index;
	spkr.entries[spkr.used].vol=vol;
	spkr.used++;
}


static void ForwardPIT(Bitu newindex) {
	Bitu micro=(newindex-spkr.last_index) << SPKR_SHIFT;
	Bitu delay_base=spkr.last_index << SPKR_SHIFT;
	spkr.last_index=newindex;
	switch (spkr.pit_mode) {
	case 0:
		return;
	case 2:
		while (micro) {
			/* passed the initial low cycle? */
			if (spkr.pit_index>=spkr.pit_half) {
				/* Start a new low cycle */
				if ((spkr.pit_index+micro)>=spkr.pit_max) {
					Bitu delay=spkr.pit_max-spkr.pit_index;
					delay_base+=delay;micro-=delay;
					spkr.pit_last=-SPKR_VOLUME;
					if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);
					spkr.pit_index=0;
				} else {
					spkr.pit_index+=micro;
					return;
				}
			} else {
				if ((spkr.pit_index+micro)>=spkr.pit_half) {
					Bitu delay=spkr.pit_half-spkr.pit_index;
					delay_base+=delay;micro-=delay;
					spkr.pit_last=SPKR_VOLUME;
					if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);
					spkr.pit_index=spkr.pit_half;
				} else {
					/* Didn't reach end, forward index */
					spkr.pit_index+=micro;
					return;
				}
			}
		}
		break;
		//END CASE 2
	case 3:
		while (micro) {
			/* Determine where in the wave we're located */
			if (spkr.pit_index>=spkr.pit_half) {
				if ((spkr.pit_index+micro)>=spkr.pit_max) {
					Bitu delay=spkr.pit_max-spkr.pit_index;
					delay_base+=delay;micro-=delay;
					spkr.pit_last=SPKR_VOLUME;
					if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);
					spkr.pit_index=0;
					/* Load the new count */
					spkr.pit_half=spkr.pit_new_half;
					spkr.pit_max=spkr.pit_new_max;
				} else {
					/* Didn't reach end, forward index */
					spkr.pit_index+=micro;
					return;
				}
			} else {
				if ((spkr.pit_index+micro)>=spkr.pit_half) {
					Bitu delay=spkr.pit_half-spkr.pit_index;
					delay_base+=delay;micro-=delay;
					spkr.pit_last=-SPKR_VOLUME;
					if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);
					spkr.pit_index=spkr.pit_half;
				} else {
					/* Didn't reach end, forward index */
					spkr.pit_index+=micro;
					return;
				}
			}
		}
		break;
		//END CASE 3
	case 4:
		if (spkr.pit_index<spkr.pit_max) {
			/* Check if we're gonna pass the end this block */
			if (spkr.pit_index+micro>=spkr.pit_max) {
				Bitu delay=spkr.pit_max-spkr.pit_index;
				delay_base+=delay;micro-=delay;
				spkr.pit_last=-SPKR_VOLUME;
				if (spkr.mode==SPKR_PIT_ON) AddDelayEntry(delay_base,spkr.pit_last);				//No new events unless reprogrammed
				spkr.pit_index=spkr.pit_max;
			} else spkr.pit_index+=micro;
		}
		break;
		//END CASE 4
	}
}

void PCSPEAKER_SetCounter(Bitu cntr,Bitu mode) {
	if (!spkr.last_ticks) {
		spkr.chan->Enable(true);
		spkr.last_index=0;
	}
	spkr.last_ticks=PIC_Ticks;
	Bitu newindex=PIC_Index();
	ForwardPIT(newindex);
	switch (mode) {
	case 0:		/* Mode 0 one shot, used with realsound */
		if (spkr.mode!=SPKR_PIT_ON) return;
		if (cntr>80) { 
			cntr=80;
		}
		spkr.pit_last=(cntr-40)*SPKR_SPEED;
		AddDelayEntry(newindex << SPKR_SHIFT,spkr.pit_last);
		spkr.pit_index=0;
		break;
	case 2:			/* Single cycle low, rest low high generator */
		spkr.pit_index=0;
		spkr.pit_last=-SPKR_VOLUME;
		AddDelayEntry(newindex << SPKR_SHIFT,spkr.pit_last);
		spkr.pit_half=(Bitu)(((double)(1000000 << SPKR_SHIFT)/PIT_TICK_RATE)*1);
		spkr.pit_max=(Bitu)(((double)(1000000 << SPKR_SHIFT)/PIT_TICK_RATE)*cntr);
		break;
	case 3:		/* Square wave generator */
		if (cntr<=40) {
			/* Makes DIGGER sound better */
			spkr.pit_last=0;
			spkr.pit_mode=0;
			return;
		}
		spkr.pit_new_max=(Bitu)(((double)(1000000 << SPKR_SHIFT)/PIT_TICK_RATE)*cntr);
		spkr.pit_new_half=spkr.pit_new_max/2;
		break;
	case 4:		/* Software triggered strobe */
		spkr.pit_last=SPKR_VOLUME;
		AddDelayEntry(newindex << SPKR_SHIFT,spkr.pit_last);
		spkr.pit_index=0;
		spkr.pit_max=(Bitu)(((double)(1000000 << SPKR_SHIFT)/PIT_TICK_RATE)*cntr);
		break;
	default:
#if C_DEBUG
		LOG_MSG("Unhandled speaker mode %d",mode);
#endif
		return;
	}
	spkr.pit_mode=mode;
}

void PCSPEAKER_SetType(Bitu mode) {
	if (!spkr.last_ticks) {
		spkr.chan->Enable(true);
		spkr.last_index=0;
	}
	spkr.last_ticks=PIC_Ticks;
	Bitu newindex=PIC_Index();
	ForwardPIT(newindex);
	switch (mode) {
	case 0:
    	spkr.mode=SPKR_OFF;
		AddDelayEntry(newindex << SPKR_SHIFT,-SPKR_VOLUME);
		break;
	case 1:
		spkr.mode=SPKR_PIT_OFF;
		AddDelayEntry(newindex << SPKR_SHIFT,-SPKR_VOLUME);
		break;
	case 2:
		spkr.mode=SPKR_ON;
		AddDelayEntry(newindex << SPKR_SHIFT,SPKR_VOLUME);
		break;
	case 3:
		if (spkr.mode!=SPKR_PIT_ON) {
			AddDelayEntry(newindex << SPKR_SHIFT,spkr.pit_last);
		}
		spkr.mode=SPKR_PIT_ON;
		break;
	};
}

static void PCSPEAKER_CallBack(Bitu len) {
	Bit16s * stream=(Bit16s*)MixTemp;
	ForwardPIT(1000);
	spkr.last_index=0;
	Bitu count=len;
	Bitu micro=0;Bitu pos=0;
	Bits micro_add=(1001 << SPKR_SHIFT)/len;
	while (count--) {
		Bitu index=micro;
		micro+=micro_add;
		Bitu end=micro;
		Bits value=0;
		while(index<end) {
			/* Check if there is an upcoming event */
			if (spkr.used && spkr.entries[pos].index<=index) {
				spkr.volwant=spkr.entries[pos].vol;
				pos++;spkr.used--;
				continue;
			}
			Bitu vol_end;
			if (spkr.used && spkr.entries[pos].index<end) {
				vol_end=spkr.entries[pos].index;
			} else vol_end=end;
			Bits vol_len=vol_end-index;
            /* Check if we have to slide the volume */
			Bits vol_diff=spkr.volwant-spkr.volcur;
			if (!vol_diff) {
				value+=spkr.volcur*vol_len;
				index+=vol_len;
			} else {
				/* Check how many microseonds it will take to goto new level */
				Bits vol_micro=(abs(vol_diff) << SPKR_SHIFT)/SPKR_SPEED;
				if (vol_micro<=vol_len) {
					/* Volume reaches endpoint in this block, calc until that point */
					value+=vol_micro*spkr.volcur;
					value+=vol_micro*vol_diff/2;
					index+=vol_micro;
					spkr.volcur=spkr.volwant;
				} else {
					/* Volume still not reached in this block */
					value+=spkr.volcur*vol_len;
					if (vol_diff<0) {
						value-=(((SPKR_SPEED*vol_len) >> SPKR_SHIFT)*vol_len)/2;
						spkr.volcur-=(SPKR_SPEED*vol_len) >> SPKR_SHIFT;
					} else {
						value+=(((SPKR_SPEED*vol_len) >> SPKR_SHIFT)*vol_len)/2;
						spkr.volcur+=(SPKR_SPEED*vol_len) >> SPKR_SHIFT;
					}
					index+=vol_len;
				}
			}
		}
		*stream++=value/micro_add;
	}
	spkr.chan->AddSamples_m16(len,(Bit16s*)MixTemp);
	if ((spkr.last_ticks+10000)<PIC_Ticks) {
		spkr.last_ticks=0;
		spkr.chan->Enable(false);
	}
}

void PCSPEAKER_Init(Section* sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	if(!section->Get_bool("pcspeaker")) return;
	spkr.mode=SPKR_OFF;
	spkr.last_ticks=0;
	spkr.last_index=0;
	spkr.rate=section->Get_int("pcrate");
	spkr.pit_max=(Bitu)(((double)(1000000 << SPKR_SHIFT)/PIT_TICK_RATE)*65535);
	spkr.pit_half=spkr.pit_max/2;
	spkr.pit_new_max=spkr.pit_max;
	spkr.pit_new_half=spkr.pit_half;
	spkr.pit_index=0;
	spkr.used=0;
	/* Register the sound channel */
	spkr.chan=MIXER_AddChannel(&PCSPEAKER_CallBack,spkr.rate,"SPKR");
}
