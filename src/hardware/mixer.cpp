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

/* 
	Remove the sdl code from here and have it handeld in the sdlmain.
	That should call the mixer start from there or something.
*/

#include <string.h>
#include <SDL.h>
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"

#define MIXER_MAXCHAN 8
#define MIXER_BLOCKSIZE 1024
#define MIXER_BUFSIZE MIXER_BLOCKSIZE*8
#define MIXER_SSIZE 4
#define MIXER_SHIFT 16
#define MIXER_REMAIN ((1<<MIXER_SHIFT)-1)
#define MIXER_FREQ 22050

#define MIXER_CLIP(SAMP) (SAMP>MAX_AUDIO) ? (Bit16s)MAX_AUDIO : (SAMP<MIN_AUDIO) ? (Bit16s)MIN_AUDIO : ((Bit16s)SAMP)

#define MAKE_m8(CHAN) ((Bit8s)(mix_temp.m8[pos][CHAN]^0x80) << 8)
#define MAKE_s8(CHAN) ((Bit8s)(mix_temp.s8[pos][CHAN]^0x80) << 8)
#define MAKE_m16(CHAN) mix_temp.m16[pos][CHAN]
#define MAKE_s16(CHAN) mix_temp.s16[pos][CHAN]

#define MIX_NORMAL(TYPE, LCHAN,RCHAN) {													\
	(chan->handler)(((Bit8u*)&mix_temp)+sizeof(mix_temp.TYPE[0]),sample_toread);		\
	Bitu sample_index=(1 << MIXER_SHIFT) - chan->sample_left;							\
	Bit32s newsample;																	\
	for (Bitu mix=0;mix<samples;mix++) {												\
		Bitu pos=sample_index >> MIXER_SHIFT;sample_index+=chan->sample_add;			\
		newsample=mix_buftemp[mix][0]+MAKE_##TYPE( LCHAN );								\
		mix_buftemp[mix][0]=MIXER_CLIP(newsample);										\
		newsample=mix_buftemp[mix][1]+MAKE_##TYPE( RCHAN );								\
		mix_buftemp[mix][1]=MIXER_CLIP(newsample);										\
	}																					\
	chan->remain.TYPE[LCHAN]=mix_temp.TYPE[sample_index>>MIXER_SHIFT][LCHAN];			\
	if (RCHAN) chan->remain.TYPE[RCHAN]=mix_temp.TYPE[sample_index>>MIXER_SHIFT][RCHAN];\
	chan->sample_left=sample_total-sample_index;										\
	break;																				\
}
 
union Sample {
	Bit16s m16[1];
	Bit16s s16[2];
	Bit8u m8[1];
	Bit8u s8[2];
	Bit32u full;
};

struct MIXER_Channel {
	Bit8u volume;
	Bit8u mode;
	Bitu freq;
	char * name;
	MIXER_MixHandler handler;
	Bitu sample_add;
	Bitu sample_left;
	Sample remain;
	bool playing;
	MIXER_Channel * next;
};


static MIXER_Channel * first_channel;
static union {
	Bit16s	m16[MIXER_BUFSIZE][1];
	Bit16s	s16[MIXER_BUFSIZE][2];
	Bit8u	m8[MIXER_BUFSIZE][1];
	Bit8u	s8[MIXER_BUFSIZE][2];
	Bit32u	full[MIXER_BUFSIZE];
} mix_temp;

static Bit16s mix_bufout[MIXER_BUFSIZE][2];
static Bit16s mix_buftemp[MIXER_BUFSIZE][2];
static Bit32s mix_bufextra;
static Bitu mix_writepos;
static Bitu mix_readpos;
static Bitu mix_ticks;
static Bitu mix_add;
static Bitu mix_remain;

MIXER_Channel * MIXER_AddChannel(MIXER_MixHandler handler,Bit32u freq,char * name) {
//TODO Find a free channel
	MIXER_Channel * chan=new MIXER_Channel;
	if (!chan) return 0;
	chan->playing=false;
	chan->volume=255;
	chan->mode=MIXER_16STEREO;
	chan->handler=handler;
	chan->name=name;
	chan->sample_add=(freq<<MIXER_SHIFT)/MIXER_FREQ;
	chan->sample_left=0;
	chan->next=first_channel;
	first_channel=chan;
	return chan;
};

void MIXER_SetFreq(MIXER_Channel * chan,Bit32u freq) {
	if (chan) {
		chan->freq=freq;
		/* Calculate the new addition value */
		chan->sample_add=(freq<<MIXER_SHIFT)/MIXER_FREQ;
	}	
}	

void MIXER_SetMode(MIXER_Channel * chan,Bit8u mode) {
	if (chan) chan->mode=mode;
};

void MIXER_SetVolume(MIXER_Channel * chan,Bit8u vol) {
	if (chan) chan->volume=vol;
}

void MIXER_Enable(MIXER_Channel * chan,bool enable) {
	if (chan) chan->playing=enable;
}



/* Mix a certain amount of new samples */
static void MIXER_MixData(Bit32u samples) {
/* This Should mix the channels */
	if (!samples) return;
	if (samples>MIXER_BUFSIZE) samples=MIXER_BUFSIZE;
	/* 16-bit stereo is 4 bytes per sample */
	memset((void *)&mix_buftemp,0,samples*MIXER_SSIZE);
	MIXER_Channel * chan=first_channel;
	while (chan) {
		if (chan->playing) {
			/* This should always allocate 1 extra sample */
 			Bitu sample_total=(samples*chan->sample_add)-chan->sample_left;
			Bitu sample_toread=sample_total >> MIXER_SHIFT;
			if (sample_total & MIXER_REMAIN) sample_toread++;
			sample_total=(sample_toread+1)<<MIXER_SHIFT;
			mix_temp.full[0]=chan->remain.full;
			switch (chan->mode) {
			case MIXER_8MONO:
				MIX_NORMAL(m8,0,0);
				break;
			case MIXER_8STEREO:
				MIX_NORMAL(m8,0,1);
				break;
			case MIXER_16MONO:
				MIX_NORMAL(m16,0,0);
				break;
			case MIXER_16STEREO:
				MIX_NORMAL(s16,0,1);
				break;
			default:
				E_Exit("MIXER:Illegal sound mode %2X",chan->mode);
				break;
			}
		}
		chan=chan->next;
	}
	Bit32u buf_remain=MIXER_BUFSIZE-mix_writepos;
	/* Fill the samples size buffer with 0's */
	if (buf_remain>samples) {
		memcpy(&mix_bufout[mix_writepos][0],&mix_buftemp[0][0],samples*MIXER_SSIZE);
		mix_writepos+=samples;
	} else {
		memcpy(&mix_bufout[mix_writepos][0],&mix_buftemp[0][0],buf_remain*MIXER_SSIZE);
		memcpy(&mix_bufout[0][0],&mix_buftemp[buf_remain][0],(samples-buf_remain)*MIXER_SSIZE);
		mix_writepos=(mix_writepos+samples)-MIXER_BUFSIZE;
	}


}

void MIXER_Mix(Bitu ticks) {
/* Check for 1 ms of sound to mix */
	Bitu count=(ticks*mix_add)+mix_remain;
	mix_remain=count&((1<<10)-1);
	count>>=10;
	Bit32u size=MIXER_BUFSIZE+mix_writepos-mix_readpos;
	if (size>=MIXER_BUFSIZE) size-=MIXER_BUFSIZE;
	if (size>MIXER_BLOCKSIZE+2048) return;
	MIXER_MixData(count);		

}

static Bit32u last_pos;
static void MIXER_CallBack(void * userdata, Uint8 *stream, int len) {
	/* Copy data from buf_out to the stream */

	Bit32u remain=MIXER_BUFSIZE-mix_readpos;
	if (remain>=(Bit32u)len/MIXER_SSIZE) {
		memcpy((void *)stream,(void *)&mix_bufout[mix_readpos][0],len);
	} else {
		memcpy((void *)stream,(void *)&mix_bufout[mix_readpos][0],remain*MIXER_SSIZE);
		stream+=remain*MIXER_SSIZE;
		memcpy((void *)stream,(void *)&mix_bufout[0][0],(len)-remain*MIXER_SSIZE);
	}
	mix_readpos+=(len/MIXER_SSIZE);
	if (mix_readpos>=MIXER_BUFSIZE) mix_readpos-=MIXER_BUFSIZE;
}



void MIXER_Init(Section* sec) {
	/* Initialize the internal stuff */
	first_channel=0;
	mix_ticks=GetTicks();
	mix_bufextra=0;
	mix_writepos=0;
	mix_readpos=0;

	/* Start the Mixer using SDL Sound at 22 khz */
	SDL_AudioSpec spec;
	SDL_AudioSpec obtained;
	mix_add=((MIXER_FREQ) << 10)/1000;
	mix_remain=0;
	spec.freq=MIXER_FREQ;
	spec.format=AUDIO_S16SYS;
	spec.channels=2;
	spec.callback=MIXER_CallBack;
	spec.userdata=NULL;
	spec.samples=MIXER_BLOCKSIZE;
	
	TIMER_RegisterTickHandler(MIXER_Mix);

	if ( SDL_OpenAudio(&spec, &obtained) < 0 ) {
		LOG_MSG("No sound output device found, starting in no sound mode");
	} else {
		MIXER_MixData(MIXER_BLOCKSIZE/MIXER_SSIZE);
		SDL_PauseAudio(0);
	}
}
