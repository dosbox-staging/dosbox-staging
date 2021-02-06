/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "dosbox.h"

#include <functional>

#include "envelope.h"

typedef void (*MIXER_MixHandler)(Bit8u *sampdate, Bit32u len);

// The mixer callback can accept a static function or a member function
// using a std::bind. The callback typically requests enough frames to
// fill one millisecond with of audio. For an audio channel running at
// 48000 Hz, that's 48 frames.
using MIXER_Handler = std::function<void(uint16_t frames)>;

enum BlahModes {
	MIXER_8MONO,MIXER_8STEREO,
	MIXER_16MONO,MIXER_16STEREO
};

enum MixerModes {
	M_8M,M_8S,
	M_16M,M_16S
};

// A simple stereo audio frame
struct AudioFrame {
	float left = 0;
	float right = 0;
};

#define MIXER_BUFSIZE (16 * 1024)
#define MIXER_BUFMASK (MIXER_BUFSIZE - 1)
extern Bit8u MixTemp[MIXER_BUFSIZE];

#define MAX_AUDIO ((1<<(16-1))-1)
#define MIN_AUDIO -(1<<(16-1))

class MixerChannel {
public:
	MixerChannel(MIXER_Handler _handler, Bitu _freq, const char * _name);
	uint32_t GetSampleRate() const;
	bool IsInterpolated() const;
	using apply_level_callback_f = std::function<void(const AudioFrame &level)>;
	void RegisterLevelCallBack(apply_level_callback_f cb);
	void SetVolume(float _left, float _right);
	void SetScale(float f);
	void SetScale(float _left, float _right);
	void MapChannels(Bit8u _left, Bit8u _right);
	void UpdateVolume();
	void SetFreq(Bitu _freq);
	void SetPeakAmplitude(uint32_t peak);
	void Mix(Bitu _needed);
	void AddSilence(); // Fill up until needed

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
	
	void AddStretched(Bitu len,Bit16s * data);		//Stretch block up into needed data

	void FillUp();
	void Enable(bool should_enable);
	void FlushSamples();

	float volmain[2] = {0.0f, 0.0f};
	MixerChannel *next = nullptr;
	const char *name = nullptr;
	Bitu done = 0u; // Timing on how many samples have been done by the mixer
	bool is_enabled = false;

private:
	// prevent default construction, copying, and assignment
	MixerChannel() = delete;
	MixerChannel(const MixerChannel &) = delete;
	MixerChannel &operator=(const MixerChannel &) = delete;

	Envelope envelope;
	MIXER_Handler handler = nullptr;
	Bitu freq_add = 0u; // This gets added the frequency counter each mixer
	                    // step
	Bitu freq_counter = 0u; // When this flows over a new sample needs to be
	                        // read from the device
	Bitu needed = 0u; // Timing on how many samples were needed by the mixer
	Bits prev_sample[2] = {0}; // Previous and next samples
	Bits next_sample[2] = {0};
	// Simple way to lower the impact of DC offset. if MIXER_UPRAMP_STEPS is >0.
	// Still work in progress and thus disabled for now.
	Bits offset[2] = {0};
	uint32_t sample_rate = 0u;
	int32_t volmul[2] = {0};
	float scale[2] = {0.0f, 0.0f};

	// Defines the peak sample amplitude we can expect in this channel.
	// Default to signed 16bit max, however channel's that know their own
	// peak, like the PCSpeaker, should update it with: SetPeakAmplitude()
	uint32_t peak_amplitude = MAX_AUDIO;

	uint8_t channel_map[2] = {0u, 0u}; // Output channel mapping

	// The RegisterLevelCallBack() assigns this callback that can be used by
	// the channel's source to manage the stream's level prior to mixing,
	// in-place of scaling by volmain[]
	apply_level_callback_f apply_level = nullptr;

	bool interpolate = false;
	bool last_samples_were_stereo = false;
	bool last_samples_were_silence = true;
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
	MixerObject() : installed(false) {}
	MixerChannel* Install(MIXER_Handler handler,Bitu freq,const char * name);
	~MixerObject();
};

void MIXER_AddConfigSection(Config *conf);

/* PC Speakers functions, tightly related to the timer functions */
void PCSPEAKER_SetCounter(Bitu cntr,Bitu mode);
void PCSPEAKER_SetType(Bitu mode);

#endif
