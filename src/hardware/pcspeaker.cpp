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
#include "pic.h"


#ifndef PI
#define PI 3.14159265358979323846
#endif

#define SPKR_BUF 4096
#define SPKR_RATE 22050
#define SPKR_VOLUME 5000

#define SPKR_SHIFT 10

enum SPKR_MODE {
	MODE_NONE,MODE_WAVE,MODE_DELAY,MODE_ONOFF,MODE_REAL,
};

/* TODO 
  Maybe interpolate at the moment we switch between on/off 
  Keep track of postion of speaker conus in ONOFF mode 
*/

static struct {
	Bit16s volume;
	MIXER_Channel * chan;
	Bit16u buffer[SPKR_BUF];
	SPKR_MODE mode;
	SPKR_MODE pit_mode;
	struct {
		Bit16s buf[SPKR_BUF];
		Bitu used;
	} real;
	struct {
		Bitu index;
		struct {
			Bitu max,half;
		} count,new_count;
	} wave;
	struct {
		Bit16s buf[SPKR_BUF];
		Bitu pos;
	} out;
	struct {
		Bitu index;
		Bitu max;
	} delay;
	struct {
		Bitu vol;
		Bitu rate;
		Bitu rate_conv;
		Bitu tick_add;
	} hw;
	bool onoff,enabled;
	Bitu buf_pos;
} spkr;

static void GenerateSound(Bitu size) {
	while (spkr.out.pos<size) {
		Bitu samples=size-spkr.out.pos;
		Bit16s * stream=&spkr.out.buf[spkr.out.pos];
		switch (spkr.mode) {
		case MODE_NONE:
			memset(stream,0,samples*2);
			spkr.out.pos+=samples;
			break;
		case MODE_WAVE:
			{
				for (Bitu i=0;i<samples;i++) {
					spkr.wave.index+=spkr.hw.tick_add;
					if (spkr.wave.index>=spkr.wave.count.max) {
						*stream++=+spkr.volume;					
						spkr.wave.index-=spkr.wave.count.max;
						spkr.wave.count.max=spkr.wave.new_count.max;
						spkr.wave.count.half=spkr.wave.new_count.half;
					} else if (spkr.wave.index>spkr.wave.count.half) {
						*stream++=-spkr.volume;					
					} else {
						*stream++=+spkr.volume;					
					}
				}
				spkr.out.pos+=samples;
			}
			break;
		case MODE_ONOFF:
			{
				Bit16s val=spkr.onoff ? spkr.volume : -spkr.volume;
				for (Bitu i=0;i<samples;i++) *stream++=val;
				spkr.out.pos+=samples;
			}
			break;
		case MODE_REAL:
			{
				Bitu buf_add=(spkr.real.used<<16)/samples;
				Bitu buf_pos=0;
				spkr.real.used=0;spkr.mode=MODE_NONE;
				for (Bitu i=0;i<samples;i++) {
					*stream++=spkr.real.buf[buf_pos >> 16];
					buf_pos+=buf_add;
				}
				spkr.out.pos+=samples;
				break;
			}
		case MODE_DELAY:
			{
				for (Bitu i=0;i<samples;i++) {
					spkr.delay.index+=spkr.hw.tick_add;
					if (spkr.delay.index<=spkr.delay.max) {
						*stream++=+spkr.volume;
					} else {
						*stream++=-spkr.volume;
					}
				}
			}			
			spkr.out.pos+=samples;
			break;
		}
	}
}

void PCSPEAKER_SetCounter(Bitu cntr,Bitu mode) {
	Bitu index=PIC_Index();
	Bitu len=(spkr.hw.rate_conv*index)>>16;
	if (spkr.mode==MODE_NONE) MIXER_Enable(spkr.chan,true);
	switch (mode) {
	case 0:		/* Mode 0 one shot, used with realsound */
		if (!spkr.enabled) return;
		if (cntr>72) cntr=72;
		if (spkr.mode!=MODE_REAL) GenerateSound(len);
		spkr.pit_mode=MODE_REAL;
		if (spkr.real.used<SPKR_BUF) spkr.real.buf[spkr.real.used++]=(cntr-36)*600;
		break;
	case 3:		/* Square wave generator */
		GenerateSound(len);
		spkr.pit_mode=MODE_WAVE;
		spkr.wave.new_count.max=cntr << SPKR_SHIFT;
		spkr.wave.new_count.half=(cntr/2) << SPKR_SHIFT;
		break;
	case 4:		/* Keep high till end of count */
		GenerateSound(len);
		spkr.delay.max=cntr << SPKR_SHIFT;
		spkr.delay.index=0;
		spkr.pit_mode=MODE_DELAY;
		break;
	}
	if (spkr.enabled) spkr.mode=spkr.pit_mode;
}

void PCSPEAKER_SetType(Bitu mode) {
	Bitu index=PIC_Index();
	Bitu len=(spkr.hw.rate_conv*index)>>16;
	GenerateSound(len);
	if (spkr.mode==MODE_NONE) MIXER_Enable(spkr.chan,true);
	switch (mode) {
	case 0:
		if (spkr.mode==MODE_ONOFF && spkr.onoff) spkr.onoff=false;
		else spkr.mode=MODE_NONE;
		spkr.enabled=false;
		break;
	case 1:
		spkr.mode=MODE_ONOFF;
		spkr.enabled=false;
		spkr.onoff=false;
		break;
	case 2:
		spkr.enabled=false;
		spkr.onoff=true;
		spkr.mode=MODE_ONOFF;
		break;
	case 3:
		spkr.onoff=true;
		spkr.enabled=true;
		spkr.mode=spkr.pit_mode;		
		break;
	};
}

static void PCSPEAKER_CallBack(Bit8u * stream,Bit32u len) {
	if (spkr.out.pos<len) GenerateSound(len);
	memcpy(stream,spkr.out.buf,len*2);
	memcpy(spkr.out.buf,&spkr.out.buf[len],(spkr.out.pos-len)*2);
	spkr.out.pos-=len;
	if (spkr.mode==MODE_NONE) MIXER_Enable(spkr.chan,false);
}

void PCSPEAKER_Init(Section* sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	if(!section->Get_bool("pcspeaker")) return;
	spkr.volume=SPKR_VOLUME;
	spkr.mode=MODE_NONE;
	spkr.pit_mode=MODE_WAVE;
	spkr.real.used=0;
	spkr.out.pos=0;
	spkr.onoff=false;
//	spkr.hw.vol=section->Get_int("volume");
	spkr.hw.rate=section->Get_int("pcrate");
	spkr.hw.rate_conv=(spkr.hw.rate<<16)/1000000;
	spkr.hw.tick_add=(Bitu)((double)PIT_TICK_RATE*(double)(1 << SPKR_SHIFT)/(double)spkr.hw.rate);
	spkr.wave.index=0;
	spkr.wave.count.max=spkr.wave.new_count.max=0x10000 << SPKR_SHIFT;
	spkr.wave.count.half=spkr.wave.new_count.half=(0x10000 << SPKR_SHIFT)/2;

	/* Register the sound channel */
	spkr.chan=MIXER_AddChannel(&PCSPEAKER_CallBack,spkr.hw.rate,"PC-SPEAKER");
	MIXER_Enable(spkr.chan,false);
	MIXER_SetMode(spkr.chan,MIXER_16MONO);
}
