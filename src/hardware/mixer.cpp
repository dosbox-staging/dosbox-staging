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
#include <sys/types.h>
#include <dirent.h>

#include "SDL.h"
#include "mem.h"
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "cross.h"
#include "support.h"
#include "keyboard.h"

#define MIXER_MAXCHAN 8
#define MIXER_BUFSIZE (16*1024)
#define MIXER_SSIZE 4
#define MIXER_SHIFT 16
#define MIXER_REMAIN ((1<<MIXER_SHIFT)-1)
#define MIXER_WAVESIZE MIXER_BUFSIZE

#define MIXER_CLIP(SAMP) (SAMP>MAX_AUDIO) ? (Bit16s)MAX_AUDIO : (SAMP<MIN_AUDIO) ? (Bit16s)MIN_AUDIO : ((Bit16s)SAMP)

#define MAKE_m8(CHAN) ((Bit8s)(mixer.temp.m8[pos][CHAN]^0x80) << 8)
#define MAKE_s8(CHAN) ((Bit8s)(mixer.temp.s8[pos][CHAN]^0x80) << 8)
#define MAKE_m16(CHAN) mixer.temp.m16[pos][CHAN]
#define MAKE_s16(CHAN) mixer.temp.s16[pos][CHAN]

#define MIX_NORMAL(TYPE, LCHAN,RCHAN) {													\
	(chan->handler)(((Bit8u*)&mixer.temp)+sizeof(mixer.temp.TYPE[0]),sample_toread);		\
	Bitu sample_index=(1 << MIXER_SHIFT) - chan->sample_left;							\
	Bit32s newsample;																	\
	for (Bitu mix=0;mix<samples;mix++) {												\
		Bitu pos=sample_index >> MIXER_SHIFT;sample_index+=chan->sample_add;			\
		newsample=mixer.work[mix][0]+MAKE_##TYPE( LCHAN );								\
		mixer.work[mix][0]=MIXER_CLIP(newsample);										\
		newsample=mixer.work[mix][1]+MAKE_##TYPE( RCHAN );								\
		mixer.work[mix][1]=MIXER_CLIP(newsample);										\
	}																					\
	chan->remain=*(Bitu *)&mixer.temp.TYPE[sample_index>>MIXER_SHIFT];						\
	chan->sample_left=sample_total-sample_index;										\
	break;																				\
}
 
struct MIXER_Channel {
	Bit8u volume;
	Bit8u mode;
	Bitu freq;
	char * name;
	MIXER_MixHandler handler;
	Bitu sample_add;
	Bitu sample_left;
	Bitu remain;
	bool playing;
	MIXER_Channel * next;
};

static Bit8u wavheader[]={
	'R','I','F','F',	0x0,0x0,0x0,0x0,		/* Bit32u Riff Chunk ID /  Bit32u riff size */
	'W','A','V','E',	'f','m','t',' ',		/* Bit32u Riff Format  / Bit32u fmt chunk id */
	0x10,0x0,0x0,0x0,	0x1,0x0,0x2,0x0,		/* Bit32u fmt size / Bit16u encoding/ Bit16u channels */
	0x0,0x0,0x0,0x0,	0x0,0x0,0x0,0x0,		/* Bit32u freq / Bit32u byterate */
	0x4,0x0,0x10,0x0,	'd','a','t','a',		/* Bit16u byte-block / Bit16u bits / Bit16u data chunk id */
	0x0,0x0,0x0,0x0,							/* Bit32u data size */
};

static struct {
	struct {
		Bit16s data[MIXER_BUFSIZE][2];
		Bitu read,write;
	} out;
	Bit16s work[MIXER_BUFSIZE][2];
	union {
		Bit16s	m16[MIXER_BUFSIZE][1];
		Bit16s	s16[MIXER_BUFSIZE][2];
		Bit8u	m8[MIXER_BUFSIZE][1];
		Bit8u	s8[MIXER_BUFSIZE][2];
	} temp;
	MIXER_Channel * channels;
	bool nosound;
	Bitu freq;
	Bitu blocksize;
	Bitu tick_add,tick_remain;
	struct {
		FILE * handle;
		const char * dir;
		Bit8u buf[MIXER_WAVESIZE];
		Bitu used;
		Bit32u length;
	} wave; 
} mixer;

MIXER_Channel * MIXER_AddChannel(MIXER_MixHandler handler,Bit32u freq,char * name) {
//TODO Find a free channel
	MIXER_Channel * chan=new MIXER_Channel;
	if (!chan) return 0;
	chan->playing=false;
	chan->volume=255;
	chan->mode=MIXER_16STEREO;
	chan->handler=handler;
	chan->name=name;
	chan->sample_add=(freq<<MIXER_SHIFT)/mixer.freq;
	chan->sample_left=0;
	chan->next=mixer.channels;
	mixer.channels=chan;
	return chan;
};

void MIXER_SetFreq(MIXER_Channel * chan,Bit32u freq) {
	if (chan) {
		chan->freq=freq;
		/* Calculate the new addition value */
		chan->sample_add=(freq<<MIXER_SHIFT)/mixer.freq;
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
	/* Clear work buffer */
	memset(mixer.work,0,samples*MIXER_SSIZE);
	MIXER_Channel * chan=mixer.channels;
	while (chan) {
		if (chan->playing) {
			/* This should always allocate 1 extra sample */
 			Bitu sample_total=(samples*chan->sample_add)-chan->sample_left;
			Bitu sample_toread=sample_total >> MIXER_SHIFT;
			if (sample_total & MIXER_REMAIN) sample_toread++;
			sample_total=(sample_toread+1)<<MIXER_SHIFT;
			*(Bitu *)&mixer.temp=chan->remain;
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
	Bitu buf_remain=MIXER_BUFSIZE-mixer.out.write;
	/* Fill the samples size buffer with 0's */
	if (buf_remain>samples) {
		memcpy(&mixer.out.data[mixer.out.write][0],&mixer.work[0][0],samples*MIXER_SSIZE);
		mixer.out.write+=samples;
	} else {
		memcpy(&mixer.out.data[mixer.out.write][0],&mixer.work[0][0],buf_remain*MIXER_SSIZE);
		memcpy(&mixer.out.data[0][0],&mixer.work[buf_remain][0],(samples-buf_remain)*MIXER_SSIZE);
		mixer.out.write=(mixer.out.write+samples)-MIXER_BUFSIZE;
	}
	if (mixer.wave.handle) {
		memcpy(&mixer.wave.buf[mixer.wave.used],&mixer.work[0][0],samples*MIXER_SSIZE);
		mixer.wave.length+=samples*MIXER_SSIZE;
		mixer.wave.used+=samples*MIXER_SSIZE;
		if (mixer.wave.used>(MIXER_WAVESIZE-1024)){
			fwrite(mixer.wave.buf,1,mixer.wave.used,mixer.wave.handle);
			mixer.wave.used=0;
		}
	}
}

static void MIXER_Mix(Bitu ticks) {
	mixer.tick_remain+=mixer.tick_add;
	Bitu count=mixer.tick_remain>>MIXER_SHIFT;
	mixer.tick_remain&=((1<<MIXER_SHIFT)-1);
	Bitu size=MIXER_BUFSIZE+mixer.out.write-mixer.out.read;
	if (size>=MIXER_BUFSIZE) size-=MIXER_BUFSIZE;
	if (size>mixer.blocksize*2) return;
	MIXER_MixData(count);		
}

static void MIXER_Mix_NoSound(Bitu ticks) {
	mixer.tick_remain+=mixer.tick_add;
	Bitu count=mixer.tick_remain>>MIXER_SHIFT;
	mixer.tick_remain&=((1<<MIXER_SHIFT)-1);
	MIXER_MixData(count);		
}


static void MIXER_CallBack(void * userdata, Uint8 *stream, int len) {
	/* Copy data from out buffer to the stream */
	Bitu size=MIXER_BUFSIZE+mixer.out.write-mixer.out.read;
	if (size>=MIXER_BUFSIZE) size-=MIXER_BUFSIZE;
	if (size*MIXER_SSIZE<(Bitu)len) {
//		LOG(0,"Buffer underrun");
		/* When there's a buffer underrun, keep the data so there will be more next time */
		return;
	}
	Bitu remain=MIXER_BUFSIZE-mixer.out.read;
	if (remain>=(Bitu)len/MIXER_SSIZE) {
		memcpy((void *)stream,(void *)&mixer.out.data[mixer.out.read][0],len);
	} else {
		memcpy((void *)stream,(void *)&mixer.out.data[mixer.out.read][0],remain*MIXER_SSIZE);
		stream+=remain*MIXER_SSIZE;
		memcpy((void *)stream,(void *)&mixer.out.data[0][0],(len)-remain*MIXER_SSIZE);
	}
	mixer.out.read+=(len/MIXER_SSIZE);
	if (mixer.out.read>=MIXER_BUFSIZE) mixer.out.read-=MIXER_BUFSIZE;
}

static void MIXER_WaveEvent(void) {
	Bitu last=0;char file_name[CROSS_LEN];
	DIR * dir;struct dirent * dir_ent;

	/* Check for previously opened wave file */
	if (mixer.wave.handle) {
		LOG_MSG("Stopped recording");
		/* Write last piece of audio in buffer */
		fwrite(mixer.wave.buf,1,mixer.wave.used,mixer.wave.handle);
		/* Fill in the header with useful information */
		writed(&wavheader[4],mixer.wave.length+sizeof(wavheader)-8);
		writed(&wavheader[0x18],mixer.freq);
		writed(&wavheader[0x1C],mixer.freq*4);
		writed(&wavheader[0x28],mixer.wave.length);
		
		fseek(mixer.wave.handle,0,0);
		fwrite(wavheader,1,sizeof(wavheader),mixer.wave.handle);
		fclose(mixer.wave.handle);
		mixer.wave.handle=0;
		return;
	}
	/* Find a filename to open */
	dir=opendir(mixer.wave.dir);
	if (!dir) {
		LOG_MSG("Can't open waveout dir %s",mixer.wave.dir);
		return;
	}
	while ((dir_ent=readdir(dir))) {
		char tempname[CROSS_LEN];
		strcpy(tempname,dir_ent->d_name);
		char * test=strstr(tempname,".wav");
		if (!test) continue;
		*test=0;
		if (strlen(tempname)<5) continue;
		if (strncmp(tempname,"wave",4)!=0) continue;
		Bitu num=atoi(&tempname[4]);
		if (num>=last) last=num+1;
	}
	closedir(dir);
	sprintf(file_name,"%s%cwave%04d.wav",mixer.wave.dir,CROSS_FILESPLIT,last);
	/* Open the actual file */
	mixer.wave.handle=fopen(file_name,"wb");
	if (!mixer.wave.handle) {
		LOG_MSG("Can't open file %s for waveout",file_name);
		return;
	}
	mixer.wave.length=0;
	LOG_MSG("Started recording to file %s",file_name);
	fwrite(wavheader,1,sizeof(wavheader),mixer.wave.handle);
}

static void MIXER_Stop(Section* sec) {
	if (mixer.wave.handle) MIXER_WaveEvent();
}

void MIXER_Init(Section* sec) {
	sec->AddDestroyFunction(&MIXER_Stop);
	Section_prop * section=static_cast<Section_prop *>(sec);
	/* Read out config section */
	mixer.freq=section->Get_int("rate");
	mixer.nosound=section->Get_bool("nosound");
	mixer.blocksize=section->Get_int("blocksize");
	mixer.wave.dir=section->Get_string("wavedir");

	/* Initialize the internal stuff */
	mixer.channels=0;
	mixer.out.write=0;
	mixer.out.read=0;
	memset(mixer.out.data,0,sizeof(mixer.out.data));
	mixer.wave.handle=0;
	mixer.wave.used=0;

	/* Start the Mixer using SDL Sound at 22 khz */
	SDL_AudioSpec spec;
	SDL_AudioSpec obtained;

	spec.freq=mixer.freq;
	spec.format=AUDIO_S16SYS;
	spec.channels=2;
	spec.callback=MIXER_CallBack;
	spec.userdata=NULL;
	spec.samples=mixer.blocksize;

	mixer.tick_remain=0;
	if (mixer.nosound) {
		LOG_MSG("MIXER:No Sound Mode Selected.");
		mixer.tick_add=((mixer.freq) << MIXER_SHIFT)/1000;
		TIMER_RegisterTickHandler(MIXER_Mix_NoSound);
	} else if (SDL_OpenAudio(&spec, &obtained) <0 ) {
		LOG_MSG("MIXER:Can't open audio: %s , running in nosound mode.",SDL_GetError());
		mixer.tick_add=((mixer.freq) << MIXER_SHIFT)/1000;
		TIMER_RegisterTickHandler(MIXER_Mix_NoSound);
	} else {
		mixer.freq=obtained.freq;
		mixer.blocksize=obtained.samples;
		mixer.tick_add=((mixer.freq+100) << MIXER_SHIFT)/1000;
		TIMER_RegisterTickHandler(MIXER_Mix);
		SDL_PauseAudio(0);
	}
	KEYBOARD_AddEvent(KBD_f6,KBD_MOD_CTRL,MIXER_WaveEvent);
}
