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
#include <SDL/SDL.h>
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


struct MIXER_Channel {
	Bit8u volume;
	Bit8u mode;
	Bit32u freq;
	char * name;
	MIXER_MixHandler handler;
	Bit32u sample_add;
	Bit32u sample_remain;
	Bit16s sample_data[2];
	bool playing;
	MIXER_Channel * next;
};

static MIXER_Channel * first_channel;
static union {
	Bit16s temp_m16[MIXER_BUFSIZE][1];
	Bit16s temp_s16[MIXER_BUFSIZE][2];
	Bit8u temp_m8[MIXER_BUFSIZE][1];
	Bit8u temp_s8[MIXER_BUFSIZE][2];
};
static Bit16s mix_bufout[MIXER_BUFSIZE][2];
static Bit16s mix_buftemp[MIXER_BUFSIZE][2];
static Bit32s mix_bufextra;
static Bit32u mix_writepos;
static Bit32u mix_readpos;
static Bit32u mix_ticks;
static Bit32u mix_add;
static Bit32u mix_remain;

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
			Bit32u chan_samples=samples*chan->sample_add;
			Bit32u real_samples=chan_samples>>MIXER_SHIFT;
			if (chan_samples & MIXER_REMAIN) real_samples++;
			(chan->handler)((Bit8u*)&temp_m8,real_samples);
			switch (chan->mode) {
			case MIXER_8MONO:
				/* Mix a 8 bit mono stream into the final 16 bit stereo output stream */
				{
					/* Mix the data with output buffer */
					Bit32s newsample;Bit32u sample_read=0;Bit32u sample_add=chan->sample_add;
					for (Bit32u mix=0;mix<samples;mix++) {
						Bit32u pos=sample_read >> MIXER_SHIFT;
						sample_read+=sample_add;
						newsample=mix_buftemp[mix][0]+((Bit8s)(temp_m8[pos][0]^0x80) << 8);
						if (newsample>MAX_AUDIO) mix_buftemp[mix][0]=MAX_AUDIO;
						else if (newsample<MIN_AUDIO) mix_buftemp[mix][0]=MIN_AUDIO;
						else mix_buftemp[mix][0]=(Bit16s)newsample;
						newsample=mix_buftemp[mix][1]+((Bit8s)(temp_m8[pos][0]^0x80) << 8);
						if (newsample>MAX_AUDIO) mix_buftemp[mix][1]=MAX_AUDIO;
						else if (newsample<MIN_AUDIO) mix_buftemp[mix][1]=MIN_AUDIO;
						else mix_buftemp[mix][1]=(Bit16s)newsample;
					}
					break;
				}
			case MIXER_16MONO:
				/* Mix a 16 bit mono stream into the final 16 bit stereo output stream */
				{
					Bit32s newsample;Bit32u sample_read=0;Bit32u sample_add=chan->sample_add;
					for (Bit32u mix=0;mix<samples;mix++) {
						Bit32u pos=sample_read >> MIXER_SHIFT;
						sample_read+=sample_add;
						newsample=mix_buftemp[mix][0]+temp_m16[pos][0];
						if (newsample>MAX_AUDIO) mix_buftemp[mix][0]=MAX_AUDIO;
						else if (newsample<MIN_AUDIO) mix_buftemp[mix][0]=MIN_AUDIO;
						else mix_buftemp[mix][0]=(Bit16s)newsample;
						newsample=mix_buftemp[mix][1]+temp_m16[pos][0];
						if (newsample>MAX_AUDIO) mix_buftemp[mix][1]=MAX_AUDIO;
						else if (newsample<MIN_AUDIO) mix_buftemp[mix][1]=MIN_AUDIO;
						else mix_buftemp[mix][1]=(Bit16s)newsample;
					}
					break;
				}
			case MIXER_16STEREO:
				/* Mix a 16 bit stereo stream into the final 16 bit stereo output stream */
				{
					Bit32s newsample;Bit32u sample_read=0;Bit32u sample_add=chan->sample_add;
					for (Bit32u mix=0;mix<samples;mix++) {
						Bit32u pos=sample_read >> MIXER_SHIFT;
						sample_read+=sample_add;
						newsample=mix_buftemp[mix][0]+temp_s16[pos][0];
						if (newsample>MAX_AUDIO) mix_buftemp[mix][0]=MAX_AUDIO;
						else if (newsample<MIN_AUDIO) mix_buftemp[mix][0]=MIN_AUDIO;
						else mix_buftemp[mix][0]=(Bit16s)newsample;
						newsample=mix_buftemp[mix][1]+temp_s16[pos][1];
						if (newsample>MAX_AUDIO) mix_buftemp[mix][1]=MAX_AUDIO;
						else if (newsample<MIN_AUDIO) mix_buftemp[mix][1]=MIN_AUDIO;
						else mix_buftemp[mix][1]=(Bit16s)newsample;
					}
					break;

				}
			default:
				E_Exit("MIXER:Illegal sound mode %2X",chan->mode);
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
	if (remain>=len/MIXER_SSIZE) {
		memcpy((void *)stream,(void *)&mix_bufout[mix_readpos][0],len);
	} else {
		memcpy((void *)stream,(void *)&mix_bufout[mix_readpos][0],remain*MIXER_SSIZE);
		stream+=remain*MIXER_SSIZE;
		memcpy((void *)stream,(void *)&mix_bufout[0][0],(len)-remain*MIXER_SSIZE);
	}
	mix_readpos+=(len/MIXER_SSIZE);
	if (mix_readpos>=MIXER_BUFSIZE) mix_readpos-=MIXER_BUFSIZE;
}



void MIXER_Init(void) {
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
