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

/* 
	Remove the sdl code from here and have it handeld in the sdlmain.
	That should call the mixer start from there or something.
*/

#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <math.h>

#include "SDL.h"
#include "mem.h"
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "cross.h"
#include "support.h"
#include "mapper.h"
#include "hardware.h"
#include "programs.h"

#define MIXER_MAXCHAN 8
#define MIXER_BUFSIZE (16*1024)
#define MIXER_SSIZE 4
#define MIXER_SHIFT 16
#define MIXER_REMAIN ((1<<MIXER_SHIFT)-1)
#define MIXER_WAVESIZE MIXER_BUFSIZE
#define MIXER_VOLSHIFT 14

#define MIXER_CLIP(SAMP) (SAMP>MAX_AUDIO) ? (Bit16s)MAX_AUDIO : (SAMP<MIN_AUDIO) ? (Bit16s)MIN_AUDIO : ((Bit16s)SAMP)

#define MAKE_m8(CHAN) ((Bit8s)(mixer.temp.m8[pos][CHAN]^0x80) << 8)
#define MAKE_s8(CHAN) ((Bit8s)(mixer.temp.s8[pos][CHAN]^0x80) << 8)
#define MAKE_m16(CHAN) mixer.temp.m16[pos][CHAN]
#define MAKE_s16(CHAN) mixer.temp.s16[pos][CHAN]

#define MIX_NORMAL(TYPE, LCHAN,RCHAN) {													\
	(chan->handler)(((Bit8u*)&mixer.temp)+sizeof(mixer.temp.TYPE[0]),sample_toread);	\
	Bitu sample_index=(1 << MIXER_SHIFT) - chan->sample_left;							\
	for (Bitu mix=0;mix<samples;mix++) {												\
		Bitu pos=sample_index >> MIXER_SHIFT;sample_index+=chan->sample_add;			\
		mixer.work[mix][0]+=chan->vol_mul[LCHAN]*MAKE_##TYPE( LCHAN );					\
		mixer.work[mix][1]+=chan->vol_mul[RCHAN]*MAKE_##TYPE( RCHAN );					\
	}																					\
	chan->remain=*(Bitu *)&mixer.temp.TYPE[sample_index>>MIXER_SHIFT];					\
	chan->sample_left=sample_total-sample_index;										\
	break;																				\
}
 
struct MIXER_Channel {
	double vol_main[2];
	Bits vol_mul[2];
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
	Bit32s work[MIXER_BUFSIZE][2];
	union {
		Bit16s	m16[MIXER_BUFSIZE][1];
		Bit16s	s16[MIXER_BUFSIZE][2];
		Bit8u	m8[MIXER_BUFSIZE][1];
		Bit8u	s8[MIXER_BUFSIZE][2];
	} temp;
	double mastervol[2];
	MIXER_Channel * channels;
	bool nosound;
	Bitu freq;
	Bitu blocksize;
	Bitu tick_add,tick_remain;
	struct {
		FILE * handle;
		Bit16s buf[MIXER_WAVESIZE][2];
		Bitu used;
		Bit32u length;
	} wave; 
} mixer;

MIXER_Channel * MIXER_AddChannel(MIXER_MixHandler handler,Bitu freq,char * name) {
//TODO Find a free channel
	MIXER_Channel * chan=new MIXER_Channel;
	if (!chan) return 0;
	chan->playing=false;
	chan->mode=MIXER_16STEREO;
	chan->handler=handler;
	chan->name=name;
	chan->sample_add=(freq<<MIXER_SHIFT)/mixer.freq;
	chan->sample_left=0;
	chan->next=mixer.channels;
	mixer.channels=chan;
	MIXER_SetVolume(chan,1,1);
	return chan;
}

MIXER_Channel * MIXER_FindChannel(const char * name) {
	MIXER_Channel * chan=mixer.channels;
	while (chan) {
		if (!strcasecmp(chan->name,name)) break;
		chan=chan->next;
	}
	return chan;
}

void MIXER_SetFreq(MIXER_Channel * chan,Bitu freq) {
	if (!chan) return;
	chan->freq=freq;
	chan->sample_add=(freq<<MIXER_SHIFT)/mixer.freq;
}	

void MIXER_SetMode(MIXER_Channel * chan,Bit8u mode) {
	if (chan) chan->mode=mode;
}

void MIXER_UpdateVolume(MIXER_Channel * chan) {
	chan->vol_mul[0]=(Bits)((1 << MIXER_VOLSHIFT)*chan->vol_main[0]*mixer.mastervol[0]);
	chan->vol_mul[1]=(Bits)((1 << MIXER_VOLSHIFT)*chan->vol_main[1]*mixer.mastervol[1]);
}

void MIXER_SetVolume(MIXER_Channel * chan,float left,float right) {
	if (!chan) return;
	if (left>=0) chan->vol_main[0]=left;
	if (right>=0) chan->vol_main[1]=right;
	MIXER_UpdateVolume(chan);
}

void MIXER_Enable(MIXER_Channel * chan,bool enable) {
	if (chan) chan->playing=enable;
}

/* Mix a certain amount of new samples */
static void MIXER_MixData(Bitu samples) {
	if (!samples) return;
	if (samples>MIXER_BUFSIZE) samples=MIXER_BUFSIZE;
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
	Bitu index=mixer.out.write;
	for (Bitu read=0;read<samples;read++) {
		Bits temp=mixer.work[read][0] >> MIXER_VOLSHIFT;
		mixer.out.data[index][0]=MIXER_CLIP(temp);
		mixer.work[read][0]=0;
		temp=mixer.work[read][1] >> MIXER_VOLSHIFT;
		mixer.out.data[index][1]=MIXER_CLIP(temp);
		mixer.work[read][1]=0;
		index=(index+1)&(MIXER_BUFSIZE-1);
	}
	mixer.out.write=index;
	if (mixer.wave.handle) {
		index=(MIXER_BUFSIZE+mixer.out.write-samples);
		while (samples--) {
			index=index&(MIXER_BUFSIZE-1);
			mixer.wave.buf[mixer.wave.used][0]=mixer.out.data[index][0];
			mixer.wave.buf[mixer.wave.used][1]=mixer.out.data[index][1];
			index++;mixer.wave.used++;
		}
		if (mixer.wave.used>(MIXER_WAVESIZE-1024)){
			fwrite(mixer.wave.buf,1,mixer.wave.used*MIXER_SSIZE,mixer.wave.handle);
			mixer.wave.used=0;
		}
	}
}

static void MIXER_Mix(void) {
	mixer.tick_remain+=mixer.tick_add;
	Bitu count=mixer.tick_remain>>MIXER_SHIFT;
	mixer.tick_remain&=((1<<MIXER_SHIFT)-1);
	Bitu size=MIXER_BUFSIZE+mixer.out.write-mixer.out.read;
	if (size>=MIXER_BUFSIZE) size-=MIXER_BUFSIZE;
	if (size>mixer.blocksize*2) return;
	MIXER_MixData(count);		
}

static void MIXER_Mix_NoSound(void) {
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
	/* Check for previously opened wave file */
	if (mixer.wave.handle) {
		LOG_MSG("Stopped recording");
		/* Write last piece of audio in buffer */
		fwrite(mixer.wave.buf,1,mixer.wave.used*MIXER_SSIZE,mixer.wave.handle);
		/* Fill in the header with useful information */
		host_writed(&wavheader[4],mixer.wave.length+sizeof(wavheader)-8);
		host_writed(&wavheader[0x18],mixer.freq);
		host_writed(&wavheader[0x1C],mixer.freq*4);
		host_writed(&wavheader[0x28],mixer.wave.length);
		
		fseek(mixer.wave.handle,0,0);
		fwrite(wavheader,1,sizeof(wavheader),mixer.wave.handle);
		fclose(mixer.wave.handle);
		mixer.wave.handle=0;
		return;
	}
	mixer.wave.handle=OpenCaptureFile("Wave Outut",".wav");
	if (!mixer.wave.handle) return;
	mixer.wave.length=0;
	mixer.wave.used=0;
	fwrite(wavheader,1,sizeof(wavheader),mixer.wave.handle);
}

static void MIXER_Stop(Section* sec) {
	if (mixer.wave.handle) MIXER_WaveEvent();
}

class MIXER : public Program {
public:
	void MakeVolume(char * scan,double & vol0,double & vol1) {
		Bitu w=0;
		bool db=(toupper(*scan)=='D');
		if (db) scan++;
		while (*scan) {
			if (*scan==':') {
				++scan;w=1;
			}
			char * before=scan;
			double val=strtod(scan,&scan);
			if (before==scan) {
				++scan;continue;
			}
			if (!db) val/=100;
			else val=powf(10.0f,(float)val/20.0f);
			if (val<0) val=1.0f;
			if (!w) {
				vol0=val;
			} else {
				vol1=val;
			}
		}
		if (!w) vol1=vol0;
	}
	void ShowVolume(char * name,double vol0,double vol1) {
		WriteOut("%-8s %3.0f:%-3.0f  %+3.2f:%-+3.2f \n",name,
			vol0*100,vol1*100,
			20*log(vol0)/log(10.0f),20*log(vol1)/log(10.0f)
		);
	}
	void Run(void) {
		if (cmd->FindString("MASTER",temp_line,false)) {
				MakeVolume((char *)temp_line.c_str(),mixer.mastervol[0],mixer.mastervol[1]);
		}
		MIXER_Channel * chan=mixer.channels;
		while (chan) {
			if (cmd->FindString(chan->name,temp_line,false)) {
				MakeVolume((char *)temp_line.c_str(),chan->vol_main[0],chan->vol_main[1]);
			}
			MIXER_UpdateVolume(chan);
			chan=chan->next;
		}
		if (cmd->FindExist("/NOSHOW")) return;
		chan=mixer.channels;
		WriteOut("Channel  Main    Main(dB)\n");
		ShowVolume("MASTER",mixer.mastervol[0],mixer.mastervol[1]);
		for (chan=mixer.channels;chan;chan=chan->next) 
			ShowVolume(chan->name,chan->vol_main[0],chan->vol_main[1]);
	}
};

static void MIXER_ProgramStart(Program * * make) {
	*make=new MIXER;
}

void MIXER_Init(Section* sec) {
	sec->AddDestroyFunction(&MIXER_Stop);
	Section_prop * section=static_cast<Section_prop *>(sec);
	/* Read out config section */
	mixer.freq=section->Get_int("rate");
	mixer.nosound=section->Get_bool("nosound");
	mixer.blocksize=section->Get_int("blocksize");

	/* Initialize the internal stuff */
	mixer.channels=0;
	mixer.out.write=0;
	mixer.out.read=0;
	memset(mixer.out.data,0,sizeof(mixer.out.data));
	mixer.wave.handle=0;
	mixer.wave.used=0;
	mixer.mastervol[0]=1.0f;
	mixer.mastervol[1]=1.0f;

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
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else if (SDL_OpenAudio(&spec, &obtained) <0 ) {
		LOG_MSG("MIXER:Can't open audio: %s , running in nosound mode.",SDL_GetError());
		mixer.tick_add=((mixer.freq) << MIXER_SHIFT)/1000;
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else {
		mixer.freq=obtained.freq;
		mixer.blocksize=obtained.samples;
		mixer.tick_add=((mixer.freq+100) << MIXER_SHIFT)/1000;
		TIMER_AddTickHandler(MIXER_Mix);
		SDL_PauseAudio(0);
	}
	MAPPER_AddHandler(MIXER_WaveEvent,MK_f6,MMOD1,"recwave","Rec Wave");
	PROGRAMS_MakeFile("MIXER.COM",MIXER_ProgramStart);
}
