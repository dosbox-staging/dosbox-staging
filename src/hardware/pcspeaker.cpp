/*
 *  Copyright (C) 2002  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
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


#ifndef PI
#define PI 3.14159265358979323846
#endif

#define SPKR_BUF 4096
#define SPKR_RATE 22050
#define SPKR_VOLUME 5000

#define SPKR_SHIFT 16

#define SIN_ENT 1024
#define SIN_MAX (SIN_ENT << SPKR_SHIFT)

#define FREQ_MAX (2 << SPKR_SHIFT)
#define FREQ_HALF (FREQ_MAX >> 1)

struct Speaker {
	Bitu freq_add;
	Bitu freq_pos;
	Bit16s volume;
	MIXER_Channel * chan;
	bool enabled;
	bool realsound;
	bool sinewave;
	Bitu mode;
	Bit16u buffer[SPKR_BUF];
	Bit16s table[SIN_ENT];
	Bitu buf_pos;
};


static Speaker spkr;


void PCSPEAKER_SetCounter(Bitu cntr,Bitu mode) {
	spkr.mode=mode;
	switch (mode) {
	case 0:
		if (cntr>72) cntr=72;
		spkr.realsound=true;
		if (spkr.buf_pos<SPKR_BUF) spkr.buffer[spkr.buf_pos++]=(cntr-36)*600;
		break;
	case 3:
		if (spkr.sinewave) {
			spkr.freq_add=(Bitu)(SIN_MAX/((float)SPKR_RATE/(PIT_TICK_RATE/(float)cntr)));
		} else {
			spkr.freq_add=(Bitu)(FREQ_MAX/((float)SPKR_RATE/(PIT_TICK_RATE/(float)cntr)));
		}
		break;
	case 4:
		spkr.freq_pos=(Bitu)(SPKR_RATE/(PIT_TICK_RATE/(float)cntr));
	}
}

void PCSPEAKER_Enable(bool enable) {
	spkr.enabled=enable;
	MIXER_Enable(spkr.chan,enable);
}

static void PCSPEAKER_CallBack(Bit8u * stream,Bit32u len) {
	switch (spkr.mode) {
	case 0:
		/* Generate the "RealSound" */
		{
			Bitu buf_add=(spkr.buf_pos<<16)/len;
			Bitu buf_pos=0;
			spkr.buf_pos=0;spkr.realsound=0;
			while (len-->0) {
				*(Bit16s*)(stream)=spkr.buffer[buf_pos >> 16];
				buf_pos+=buf_add;
				stream+=2;
			}
			break;
		}
	case 3:
		if (spkr.sinewave) while (len-->0) {
			spkr.freq_pos+=spkr.freq_add;
			spkr.freq_pos&=(SIN_MAX-1);
			*(Bit16s*)(stream)=spkr.table[spkr.freq_pos>>SPKR_SHIFT];
			stream+=2;
		} else while (len-->0) {
			spkr.freq_pos+=spkr.freq_add;
			if (spkr.freq_pos>=FREQ_MAX) spkr.freq_pos-=FREQ_MAX;
			if (spkr.freq_pos>=FREQ_HALF) {
				*(Bit16s*)(stream)=spkr.volume;
			} else {
				*(Bit16s*)(stream)=-spkr.volume;
			}
			stream+=2;
		}
		break;
	case 4:
		while (len-->0) {
			if (spkr.freq_pos) {
				*(Bit16s*)(stream)=spkr.volume;
				spkr.freq_pos--;
			} else {
				*(Bit16s*)(stream)=-spkr.volume;
			}
			stream+=2;
		}
		break;
	}
}

void PCSPEAKER_Init(Section* sec) {
    MSG_Add("SPEAKER_CONFIGFILE_HELP","pcspeaker related options.\n");
	Section_prop * section=static_cast<Section_prop *>(sec);
	if(!section->Get_bool("enabled")) return;
	spkr.sinewave=section->Get_bool("sinewave");
	spkr.chan=MIXER_AddChannel(&PCSPEAKER_CallBack,SPKR_RATE,"PC-SPEAKER");
	MIXER_Enable(spkr.chan,false);
	MIXER_SetMode(spkr.chan,MIXER_16MONO);
	spkr.volume=SPKR_VOLUME;
	spkr.enabled=false;
	spkr.realsound=false;
	spkr.buf_pos=0;
	/* Generate the sine wave */
	for (Bitu i=0;i<SIN_ENT;i++) {
		spkr.table[i]=(Bit16s)(sin(2*PI/SIN_ENT*i)*SPKR_VOLUME*1.5);
	}
}
