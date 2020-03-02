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


#ifndef DOSBOX_MIXER_H
#define DOSBOX_MIXER_H

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif

typedef void (*MIXER_MixHandler)(Bit8u * sampdate,Bit32u len);
typedef void (*MIXER_Handler)(Bitu len);

enum BlahModes {
	MIXER_8MONO,MIXER_8STEREO,
	MIXER_16MONO,MIXER_16STEREO
};

enum MixerModes {
	M_8M,M_8S,
	M_16M,M_16S
};

#define MIXER_BUFSIZE (16*1024)
#define MIXER_BUFMASK (MIXER_BUFSIZE-1)
extern Bit8u MixTemp[MIXER_BUFSIZE];

#define MAX_AUDIO ((1<<(16-1))-1)
#define MIN_AUDIO -(1<<(16-1))

class MixerChannel {
public:
	MixerChannel(MIXER_Handler _handler, Bitu _freq, const char * _name);
	void SetVolume(float _left,float _right);
	void SetScale(float f);
	void SetScale(float _left, float _right);
	void MapChannels(Bit8u _left, Bit8u _right);
	void UpdateVolume(void);
	void SetFreq(Bitu _freq);
	void Mix(Bitu _needed);
	void AddSilence(void);			//Fill up until needed

	template<class Type,bool stereo,bool signeddata,bool nativeorder>
	void AddSamples(Bitu len, const Type* data);

	void AddSamples_m8(Bitu len, const Bit8u * data);
	void AddSamples_s8(Bitu len, const Bit8u * data);
	void AddSamples_m8s(Bitu len, const Bit8s * data);
	void AddSamples_s8s(Bitu len, const Bit8s * data);
	void AddSamples_m16(Bitu len, const Bit16s * data);
	void AddSamples_s16(Bitu len, const Bit16s * data);
	void AddSamples_m16u(Bitu len, const Bit16u * data);
	void AddSamples_s16u(Bitu len, const Bit16u * data);
	void AddSamples_m32(Bitu len, const Bit32s * data);
	void AddSamples_s32(Bitu len, const Bit32s * data);
	void AddSamples_m16_nonnative(Bitu len, const Bit16s * data);
	void AddSamples_s16_nonnative(Bitu len, const Bit16s * data);
	void AddSamples_m16u_nonnative(Bitu len, const Bit16u * data);
	void AddSamples_s16u_nonnative(Bitu len, const Bit16u * data);
	void AddSamples_m32_nonnative(Bitu len, const Bit32s * data);
	void AddSamples_s32_nonnative(Bitu len, const Bit32s * data);
	
	void AddStretched(Bitu len,Bit16s * data);		//Strech block up into needed data

	void FillUp(void);
	void Enable(bool _yesno);

	float          volmain[2];
	MixerChannel*  next;
	const char*    name;
	Bitu           done;           //Timing on how many samples have been done by the mixer
	bool           enabled;

private:
	MixerChannel();
	MIXER_Handler  handler;
	Bitu           freq_add;       //This gets added the frequency counter each mixer step
	Bitu           freq_counter;   //When this flows over a new sample needs to be read from the device
	Bitu           needed; 	       //Timing on how many samples were needed by the mixer
	Bits           prev_sample[2]; //Previous and next samples
	Bits           next_sample[2];
	Bit32s         volmul[2];
	float          scale[2];
	Bit8u          channel_map[2]; //Output channel mapping
	bool           interpolate;
};

MixerChannel * MIXER_AddChannel(MIXER_Handler handler,Bitu freq,const char * name);
MixerChannel * MIXER_FindChannel(const char * name);
/* Find the device you want to delete with findchannel "delchan gets deleted" */
void MIXER_DelChannel(MixerChannel* delchan); 

/* Object to maintain a mixerchannel; As all objects it registers itself with create
 * and removes itself when destroyed. */
class MixerObject{
private:
	bool installed;
	char m_name[32];
public:
	MixerObject():installed(false){};
	MixerChannel* Install(MIXER_Handler handler,Bitu freq,const char * name);
	~MixerObject();
};


/* PC Speakers functions, tightly related to the timer functions */
void PCSPEAKER_SetCounter(Bitu cntr,Bitu mode);
void PCSPEAKER_SetType(Bitu mode);

#endif
