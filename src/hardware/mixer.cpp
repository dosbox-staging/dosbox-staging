/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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

/* $Id: mixer.cpp,v 1.35 2005-12-05 12:04:40 qbix79 Exp $ */

/* 
	Remove the sdl code from here and have it handeld in the sdlmain.
	That should call the mixer start from there or something.
*/

#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <math.h>

#if defined (WIN32)
//Midi listing
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#include "SDL.h"
#include "mem.h"
#include "pic.h"
#include "dosbox.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "cross.h"
#include "support.h"
#include "mapper.h"
#include "hardware.h"
#include "programs.h"

#define MIXER_SSIZE 4
#define MIXER_SHIFT 14
#define MIXER_REMAIN ((1<<MIXER_SHIFT)-1)
#define MIXER_WAVESIZE MIXER_BUFSIZE
#define MIXER_VOLSHIFT 13

static inline Bit16s MIXER_CLIP(Bits SAMP) {
	if (SAMP < MAX_AUDIO) {
		if (SAMP > MIN_AUDIO)
			return SAMP;
		else return MIN_AUDIO;
	} else return MAX_AUDIO;
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
	Bit32s work[MIXER_BUFSIZE][2];
	Bitu pos,done;
	Bitu needed,min_needed;
	float mastervol[2];
	MixerChannel * channels;
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

Bit8u MixTemp[MIXER_BUFSIZE];

MixerChannel * MIXER_AddChannel(MIXER_Handler handler,Bitu freq,char * name) {
	MixerChannel * chan=new MixerChannel();
	chan->handler=handler;
	chan->name=name;
	chan->SetFreq(freq);
	chan->next=mixer.channels;
	chan->SetVolume(1,1);
	chan->enabled=false;
	mixer.channels=chan;
	return chan;
}

MixerChannel * MIXER_FindChannel(const char * name) {
	MixerChannel * chan=mixer.channels;
	while (chan) {
		if (!strcasecmp(chan->name,name)) break;
		chan=chan->next;
	}
	return chan;
}

void MIXER_DelChannel(MixerChannel* delchan) {
	MixerChannel * chan=mixer.channels;
	MixerChannel * * where=&mixer.channels;
	while (chan) {
		if (chan==delchan) {
			*where=chan->next;
			delete delchan;
			return;
		}
		where=&chan->next;
		chan=chan->next;
	}
}

void MixerChannel::UpdateVolume(void) {
	volmul[0]=(Bits)((1 << MIXER_VOLSHIFT)*volmain[0]*mixer.mastervol[0]);
	volmul[1]=(Bits)((1 << MIXER_VOLSHIFT)*volmain[1]*mixer.mastervol[1]);
}

void MixerChannel::SetVolume(float _left,float _right) {
	volmain[0]=_left;
	volmain[1]=_right;
	UpdateVolume();
}

void MixerChannel::Enable(bool _yesno) {
	if (_yesno==enabled) return;
	enabled=_yesno;
	if (enabled) {
		freq_index=MIXER_REMAIN;
		SDL_LockAudio();
		if (done<mixer.done) done=mixer.done;
		SDL_UnlockAudio();
	}
}

void MixerChannel::SetFreq(Bitu _freq) {
	freq_add=(_freq<<MIXER_SHIFT)/mixer.freq;
}

void MixerChannel::Mix(Bitu _needed) {
	needed=_needed;
	while (enabled && needed>done) {
		Bitu todo=needed-done;
		todo*=freq_add;
		if (todo & MIXER_REMAIN) {
			todo=(todo >> MIXER_SHIFT) + 1;
		} else {
			todo=(todo >> MIXER_SHIFT);
		}
		handler(todo);
	}
}

void MixerChannel::AddSilence(void) {
	if (done<needed) {
		done=needed;
		last[0]=last[1]=0;
		freq_index=MIXER_REMAIN;
	}
}

template<bool _8bits,bool stereo>
INLINE void MixerChannel::AddSamples(Bitu len,void * data) {
	Bits diff[2];
	Bit8u * data8=(Bit8u*)data;
	Bit16s * data16=(Bit16s*)data;
	Bitu mixpos=mixer.pos+done;
	freq_index&=MIXER_REMAIN;
	Bitu pos=0;Bitu new_pos;

	goto thestart;
	while (1) {
		new_pos=freq_index >> MIXER_SHIFT;
		if (pos<new_pos) {
			last[0]+=diff[0];
			if (stereo) last[1]+=diff[1];
			pos=new_pos;
thestart:
			if (pos>=len) return;
			if (_8bits) {
				if (stereo) {
					diff[0]=(((Bit8s)(data8[pos*2+0] ^ 0x80)) << 8)-last[0];
					diff[1]=(((Bit8s)(data8[pos*2+1] ^ 0x80)) << 8)-last[1];
				} else {
					diff[0]=(((Bit8s)(data8[pos] ^ 0x80)) << 8)-last[0];
				}
			} else {
				if (stereo) {
					diff[0]=data16[pos*2+0]-last[0];
					diff[1]=data16[pos*2+1]-last[1];
				} else {
					diff[0]=data16[pos]-last[0];
				}
			}
		}
		Bits diff_mul=freq_index & MIXER_REMAIN;
		freq_index+=freq_add;
		mixpos&=MIXER_BUFMASK;
		Bits sample=last[0]+((diff[0]*diff_mul) >> MIXER_SHIFT);
		mixer.work[mixpos][0]+=sample*volmul[0];
		if (stereo) sample=last[1]+((diff[1]*diff_mul) >> MIXER_SHIFT);
		mixer.work[mixpos][1]+=sample*volmul[1];
		mixpos++;done++;
	}
}

void MixerChannel::AddStretched(Bitu len,Bit16s * data) {
	if (done>=needed) {
		LOG_MSG("Can't add, buffer full");	
		return;
	}
	Bitu outlen=needed-done;Bits diff;
	freq_index=0;
	Bitu temp_add=(len << MIXER_SHIFT)/outlen;
	Bitu mixpos=mixer.pos+done;done=needed;
	Bitu pos=0;
	diff=data[0]-last[0];
	while (outlen--) {
		Bitu new_pos=freq_index >> MIXER_SHIFT;
		if (pos<new_pos) {
			pos=new_pos;
			last[0]+=diff;
			diff=data[pos]-last[0];
		}
		Bits diff_mul=freq_index & MIXER_REMAIN;
		freq_index+=temp_add;
		mixpos&=MIXER_BUFMASK;
		Bits sample=last[0]+((diff*diff_mul) >> MIXER_SHIFT);
		mixer.work[mixpos][0]+=sample*volmul[0];
		mixer.work[mixpos][1]+=sample*volmul[1];
		mixpos++;
	}
};

void MixerChannel::AddSamples_m8(Bitu len,Bit8u * data) {
	AddSamples<true,false>(len,data);
}
void MixerChannel::AddSamples_s8(Bitu len,Bit8u * data) {
	AddSamples<true,true>(len,data);
}
void MixerChannel::AddSamples_m16(Bitu len,Bit16s * data) {
	AddSamples<false,false>(len,data);
}
void MixerChannel::AddSamples_s16(Bitu len,Bit16s * data) {
	AddSamples<false,true>(len,data);
}

void MixerChannel::FillUp(void) {
	SDL_LockAudio();
	if (!enabled || done<mixer.done) {
		SDL_UnlockAudio();
		return;
	}
	float index=PIC_TickIndex();
	Mix((Bitu)(index*mixer.needed));
	SDL_UnlockAudio();
}

/* Mix a certain amount of new samples */
static void MIXER_MixData(Bitu needed) {

	MixerChannel * chan=mixer.channels;
	while (chan) {
		chan->Mix(needed);
		chan=chan->next;
	}
	if (mixer.wave.handle) {
		Bitu added=needed-mixer.done;
		Bitu readpos=(mixer.pos+mixer.done)&MIXER_BUFMASK;

		Bits sample;
		while (added--) {
			sample=mixer.work[readpos][0] >> MIXER_VOLSHIFT;
			mixer.wave.buf[mixer.wave.used][0]=MIXER_CLIP(sample);
			sample=mixer.work[readpos][1] >> MIXER_VOLSHIFT;
			mixer.wave.buf[mixer.wave.used][1]=MIXER_CLIP(sample);
			readpos=(readpos+1)&MIXER_BUFMASK;
			if (++mixer.wave.used==MIXER_WAVESIZE) {
				mixer.wave.length+=MIXER_WAVESIZE*MIXER_SSIZE;
				mixer.wave.used=0;
				fwrite(mixer.wave.buf,MIXER_WAVESIZE*MIXER_SSIZE,1,mixer.wave.handle);
			}
		}
	}
	mixer.done=needed;
}

static void MIXER_Mix(void) {
	SDL_LockAudio();
	MIXER_MixData(mixer.needed);
	mixer.tick_remain+=mixer.tick_add;
	mixer.needed+=(mixer.tick_remain>>MIXER_SHIFT);
	mixer.tick_remain&=MIXER_REMAIN;
	SDL_UnlockAudio();
}

static void MIXER_Mix_NoSound(void) {
	MIXER_MixData(mixer.needed);
	/* Clear piece we've just generated */
	for (Bitu i=0;i<mixer.needed;i++) {
		mixer.work[mixer.pos][0]=0;
		mixer.work[mixer.pos][1]=0;
		mixer.pos=(mixer.pos+1)&MIXER_BUFMASK;
	}
	/* Reduce count in channels */
	for (MixerChannel * chan=mixer.channels;chan;chan=chan->next) {
		if (chan->done>mixer.needed) chan->done-=mixer.needed;
		else chan->done=0;
	}
	/* Set values for next tick */
	mixer.tick_remain+=mixer.tick_add;
	mixer.needed=mixer.tick_remain>>MIXER_SHIFT;
	mixer.tick_remain&=MIXER_REMAIN;
	mixer.done=0;
}

static void MIXER_CallBack(void * userdata, Uint8 *stream, int len) {
	Bitu need=(Bitu)len/MIXER_SSIZE;
	if (need>mixer.done) {
		//LOG_MSG("%d Buffer underrun %d done  and %d needed",count++,mixer.done,need);
		return;
	}//else 		LOG_MSG("%d Buffer regular run %d done  and %d needed",count++,mixer.done,need);
	/* Reduce done count in all channels */
	for (MixerChannel * chan=mixer.channels;chan;chan=chan->next) {
		if (chan->done>need) chan->done-=need;
		else chan->done=0;
	}
	mixer.done-=need;
	mixer.needed-=need;
	if (mixer.done > mixer.min_needed) {
		Bitu diff=mixer.done-mixer.min_needed;
		mixer.tick_add = ((mixer.freq-(diff/5)) << MIXER_SHIFT)/1000;
	} else {
		Bitu diff = ((mixer.min_needed>mixer.needed)?mixer.min_needed:mixer.needed) - mixer.done;
		mixer.tick_add = ((mixer.freq+(diff*3)) << MIXER_SHIFT)/1000;
	}
	Bit16s * output=(Bit16s *)stream;
	Bits sample;
	while (need--) {
		sample=mixer.work[mixer.pos][0]>>MIXER_VOLSHIFT;
		*output++=MIXER_CLIP(sample);
		mixer.work[mixer.pos][0]=0;
		sample=mixer.work[mixer.pos][1]>>MIXER_VOLSHIFT;
		*output++=MIXER_CLIP(sample);
		mixer.work[mixer.pos][1]=0;
		mixer.pos=(mixer.pos+1)&MIXER_BUFMASK;
	}
}

static void MIXER_WaveEvent(void) {
	/* Check for previously opened wave file */
	if (mixer.wave.handle) {
		LOG_MSG("Stopped capturing wave output.");
		/* Write last piece of audio in buffer */
		fwrite(mixer.wave.buf,1,mixer.wave.used*MIXER_SSIZE,mixer.wave.handle);
		mixer.wave.length+=mixer.wave.used*MIXER_SSIZE;
		/* Fill in the header with useful information */
		host_writed(&wavheader[0x04],mixer.wave.length+sizeof(wavheader)-8);
		host_writed(&wavheader[0x18],mixer.freq);
		host_writed(&wavheader[0x1C],mixer.freq*4);
		host_writed(&wavheader[0x28],mixer.wave.length);
		
		fseek(mixer.wave.handle,0,0);
		fwrite(wavheader,1,sizeof(wavheader),mixer.wave.handle);
		fclose(mixer.wave.handle);
		mixer.wave.handle=0;
	} else {
		mixer.wave.handle=OpenCaptureFile("Wave Output",".wav");
		if (!mixer.wave.handle) return;
		mixer.wave.length=0;
		mixer.wave.used=0;
		fwrite(wavheader,1,sizeof(wavheader),mixer.wave.handle);
	}
}

static void MIXER_Stop(Section* sec) {
	if (mixer.wave.handle) MIXER_WaveEvent();
}

class MIXER : public Program {
public:
	void MakeVolume(char * scan,float & vol0,float & vol1) {
		Bitu w=0;
		bool db=(toupper(*scan)=='D');
		if (db) scan++;
		while (*scan) {
			if (*scan==':') {
				++scan;w=1;
			}
			char * before=scan;
			float val=(float)strtod(scan,&scan);
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

	void Run(void) {
		if(cmd->FindExist("/LISTMIDI")) {
			ListMidi();
			return;
		}
		if (cmd->FindString("MASTER",temp_line,false)) {
			MakeVolume((char *)temp_line.c_str(),mixer.mastervol[0],mixer.mastervol[1]);
		}
		MixerChannel * chan=mixer.channels;
		while (chan) {
			if (cmd->FindString(chan->name,temp_line,false)) {
				MakeVolume((char *)temp_line.c_str(),chan->volmain[0],chan->volmain[1]);
			}
			chan->UpdateVolume();
			chan=chan->next;
		}
		if (cmd->FindExist("/NOSHOW")) return;
		chan=mixer.channels;
		WriteOut("Channel  Main    Main(dB)\n");
		ShowVolume("MASTER",mixer.mastervol[0],mixer.mastervol[1]);
		for (chan=mixer.channels;chan;chan=chan->next) 
			ShowVolume(chan->name,chan->volmain[0],chan->volmain[1]);
	}
private:
	void ShowVolume(char * name,float vol0,float vol1) {
		WriteOut("%-8s %3.0f:%-3.0f  %+3.2f:%-+3.2f \n",name,
			vol0*100,vol1*100,
			20*log(vol0)/log(10.0f),20*log(vol1)/log(10.0f)
		);
	}

	void ListMidi(){
#if defined (WIN32)
		unsigned int total = midiOutGetNumDevs();	
		for(unsigned int i=0;i<total;i++) {
			MIDIOUTCAPS mididev;
			midiOutGetDevCaps(i, &mididev, sizeof(MIDIOUTCAPS));
			WriteOut("%2d\t \"%s\"\n",i,mididev.szPname);
		}
#endif
	return;
	};

};

static void MIXER_ProgramStart(Program * * make) {
	*make=new MIXER;
}

MixerChannel* MixerObject::Install(MIXER_Handler handler,Bitu freq,char * name){
	if(!installed) {
		if(strlen(name) > 31) E_Exit("Too long mixer channel name");
		safe_strncpy(m_name,name,32);
		installed = true;
		return MIXER_AddChannel(handler,freq,name);
	} else {
		E_Exit("allready added mixer channel.");
		return 0; //Compiler happy
	}
}

MixerObject::~MixerObject(){
	if(!installed) return;
	MIXER_DelChannel(MIXER_FindChannel(m_name));
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
	mixer.pos=0;
	mixer.done=0;
	memset(mixer.work,0,sizeof(mixer.work));
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
		mixer.tick_add=(mixer.freq << MIXER_SHIFT)/1000;
		TIMER_AddTickHandler(MIXER_Mix);
		SDL_PauseAudio(0);
	}
	mixer.min_needed=section->Get_int("prebuffer");
	if (mixer.min_needed>100) mixer.min_needed=100;
	mixer.min_needed=(mixer.freq*mixer.min_needed)/1000;
	mixer.needed=mixer.min_needed+1;
	MAPPER_AddHandler(MIXER_WaveEvent,MK_f6,MMOD1,"recwave","Rec Wave");
	PROGRAMS_MakeFile("MIXER.COM",MIXER_ProgramStart);
}
