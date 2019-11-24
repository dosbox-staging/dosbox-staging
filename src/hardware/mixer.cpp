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


/*
	Remove the sdl code from here and have it handeld in the sdlmain.
	That should call the mixer start from there or something.
*/

// #define DEBUG 1

#include <string.h>
#include <sys/types.h>
#include <math.h>
#include <algorithm>

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
#include "midi.h"

#define MIXER_SSIZE 4

//#define MIXER_SHIFT 14
//#define MIXER_REMAIN ((1<<MIXER_SHIFT)-1)

#define MIXER_VOLSHIFT 13

#define FREQ_SHIFT 14
#define FREQ_NEXT ( 1 << FREQ_SHIFT)
#define FREQ_MASK ( FREQ_NEXT -1 )

#define TICK_SHIFT 14
#define TICK_NEXT ( 1 << TICK_SHIFT)
#define TICK_MASK (TICK_NEXT -1)


static INLINE Bit16s MIXER_CLIP(Bits SAMP) {
	if (SAMP < MAX_AUDIO) {
		if (SAMP > MIN_AUDIO)
			return SAMP;
		else return MIN_AUDIO;
	} else return MAX_AUDIO;
}

static struct {
	Bit32s work[MIXER_BUFSIZE][2];
	//Write/Read pointers for the buffer
	Bitu pos,done;
	Bitu needed, min_needed, max_needed;
	//For every millisecond tick how many samples need to be generated
	Bit32u tick_add;
	Bit32u tick_counter;
	float mastervol[2];
	MixerChannel * channels;
	bool nosound;
	Bit32u freq;
	Bit32u blocksize;
} mixer;

Bit8u MixTemp[MIXER_BUFSIZE];

MixerChannel::MixerChannel(MIXER_Handler _handler, Bitu _freq, const char * _name) :
		// Public member initialization
		volmain      {0, 0},
		next         (nullptr),
		name         (_name),
		done         (0),
		enabled      (false),

		// Private member initialization
		handler      (_handler),
		freq_add     (0),
		freq_counter (0),
		needed       (0),
		prev_sample  {0, 0},
		next_sample  {0, 0},
		volmul       {0, 0},
		scale        {0, 0},
		channel_map  {0, 0},
		interpolate  (false) {
}

MixerChannel * MIXER_AddChannel(MIXER_Handler handler, Bitu freq, const char * name) {
	MixerChannel * chan=new MixerChannel(handler, freq, name);
	chan->next=mixer.channels;
	chan->SetFreq(freq); // also enables 'interpolate' if needed
	chan->SetScale(1.0);
	chan->SetVolume(1, 1);
	chan->MapChannels(0, 1);
	chan->Enable(false);
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
	volmul[0]=(Bits)((1 << MIXER_VOLSHIFT)*scale[0]*volmain[0]*mixer.mastervol[0]);
	volmul[1]=(Bits)((1 << MIXER_VOLSHIFT)*scale[1]*volmain[1]*mixer.mastervol[1]);
}

void MixerChannel::SetVolume(float _left,float _right) {
	// Allow unconstrained user-defined values
	volmain[0] = _left;
	volmain[1] = _right;
	UpdateVolume();
}

void MixerChannel::SetScale(float f) {
	SetScale(f, f);
}

void MixerChannel::SetScale(float _left, float _right) {
	// Constrain application-defined volume between 0% and 100%
	const float min_volume(0.0);
	const float max_volume(1.0);
	_left  = clamp(_left,  min_volume, max_volume);
	_right = clamp(_right, min_volume, max_volume);
	if (scale[0] != _left || scale[1] != _right) {
		scale[0] = _left;
		scale[1] = _right;
		UpdateVolume();
#ifdef DEBUG
		LOG_MSG("MIXER %-7s channel: application changed left and right volumes to %3.0f%% and %3.0f%%, respectively",
		        name, scale[0] * 100, scale[1] * 100);
#endif
	}
}

void MixerChannel::MapChannels(Bit8u _left, Bit8u _right) {
	// Constrain mapping to the 0 (left) and 1 (right) channel indexes
	const Bit8u min_channel(0);
	const Bit8u max_channel(1);
	_left =  clamp(_left,  min_channel, max_channel);
	_right = clamp(_right, min_channel, max_channel);
	if (channel_map[0] != _left || channel_map[1] != _right) {
		channel_map[0] = _left;
		channel_map[1] = _right;
#ifdef DEBUG
		LOG_MSG("MIXER %-7s channel: application changed audio-channel mapping to left=>%s and right=>%s",
		        name,
		        channel_map[0] == 0 ? "left" : "right",
		        channel_map[1] == 0 ? "left" : "right");
#endif
	}
}

void MixerChannel::Enable(bool _yesno) {
	if (_yesno==enabled) return;
	enabled=_yesno;
	if (enabled) {
		freq_counter = 0;
		SDL_LockAudio();
		if (done<mixer.done) done=mixer.done;
		SDL_UnlockAudio();
	}
}

void MixerChannel::SetFreq(Bitu freq) {
	freq_add=(freq<<FREQ_SHIFT)/mixer.freq;

	if (freq != mixer.freq) {
		interpolate = true;
	}
	else {
		interpolate = false;
	}
}

void MixerChannel::Mix(Bitu _needed) {
	needed=_needed;
	while (enabled && needed>done) {
		Bitu left = (needed - done);
		left *= freq_add;
		left  = (left >> FREQ_SHIFT) + ((left & FREQ_MASK)!=0);
		handler(left);
	}
}

void MixerChannel::AddSilence(void) {
	if (done<needed) {
		done=needed;
		//Make sure the next samples are zero when they get switched to prev
		next_sample[0] = 0;
		next_sample[1] = 0;
		//This should trigger an instant request for new samples
		freq_counter = FREQ_NEXT;
	}
}

template<class Type,bool stereo,bool signeddata,bool nativeorder>
inline void MixerChannel::AddSamples(Bitu len, const Type* data) {
	//Position where to write the data
	Bitu mixpos = mixer.pos + done;
	//Position in the incoming data
	Bitu pos = 0;
	//Mix and data for the full length
	while (1) {
		//Does new data need to get read?
		while (freq_counter >= FREQ_NEXT) {
			//Would this overflow the source data, then it's time to leave
			if (pos >= len)
				return;
			freq_counter -= FREQ_NEXT;

			prev_sample[0] = next_sample[0];
			if (stereo) {
				prev_sample[1] = next_sample[1];
			}

			if ( sizeof( Type) == 1) {
				if (!signeddata) {
					if (stereo) {
						next_sample[0]=(((Bit8s)(data[pos*2+0] ^ 0x80)) << 8);
						next_sample[1]=(((Bit8s)(data[pos*2+1] ^ 0x80)) << 8);
					} else {
						next_sample[0]=(((Bit8s)(data[pos] ^ 0x80)) << 8);
					}
				} else {
					if (stereo) {
						next_sample[0]=(data[pos*2+0] << 8);
						next_sample[1]=(data[pos*2+1] << 8);
					} else {
						next_sample[0]=(data[pos] << 8);
					}
				}
			//16bit and 32bit both contain 16bit data internally
			} else  {
				if (signeddata) {
					if (stereo) {
						if (nativeorder) {
							next_sample[0]=data[pos*2+0];
							next_sample[1]=data[pos*2+1];
						} else {
							if ( sizeof( Type) == 2) {
								next_sample[0]=(Bit16s)host_readw((HostPt)&data[pos*2+0]);
								next_sample[1]=(Bit16s)host_readw((HostPt)&data[pos*2+1]);
							} else {
								next_sample[0]=(Bit32s)host_readd((HostPt)&data[pos*2+0]);
								next_sample[1]=(Bit32s)host_readd((HostPt)&data[pos*2+1]);
							}
						}
					} else {
						if (nativeorder) {
							next_sample[0] = data[pos];
						} else {
							if ( sizeof( Type) == 2) {
								next_sample[0]=(Bit16s)host_readw((HostPt)&data[pos]);
							} else {
								next_sample[0]=(Bit32s)host_readd((HostPt)&data[pos]);
							}
						}
					}
				} else {
					if (stereo) {
						if (nativeorder) {
							next_sample[0]=(Bits)data[pos*2+0]-32768;
							next_sample[1]=(Bits)data[pos*2+1]-32768;
						} else {
							if ( sizeof( Type) == 2) {
								next_sample[0]=(Bits)host_readw((HostPt)&data[pos*2+0])-32768;
								next_sample[1]=(Bits)host_readw((HostPt)&data[pos*2+1])-32768;
							} else {
								next_sample[0]=(Bits)host_readd((HostPt)&data[pos*2+0])-32768;
								next_sample[1]=(Bits)host_readd((HostPt)&data[pos*2+1])-32768;
							}
						}
					} else {
						if (nativeorder) {
							next_sample[0]=(Bits)data[pos]-32768;
						} else {
							if ( sizeof( Type) == 2) {
								next_sample[0]=(Bits)host_readw((HostPt)&data[pos])-32768;
							} else {
								next_sample[0]=(Bits)host_readd((HostPt)&data[pos])-32768;
							}
						}
					}
				}
			}
			//This sample has been handled now, increase position
			pos++;
		}

		//Apply the left and right channel mappers only on write[..]
		//assignments.  This ensures the channels are mapped only once
		//(avoiding double-swapping) and also minimizes the places where
		//we use our mapping variables as array indexes.
		//Note that volumes are independent of the channels mapping.
		const Bit8u left_map(channel_map[0]);
		const Bit8u right_map(channel_map[1]);

		//Where to write
		mixpos &= MIXER_BUFMASK;
		Bit32s* write = mixer.work[mixpos];
		if (!interpolate) {
			write[0] += prev_sample[left_map] * volmul[0];
			write[1] += (stereo ? prev_sample[right_map] : prev_sample[left_map]) * volmul[1];
		}
		else {
			Bits diff_mul = freq_counter & FREQ_MASK;
			Bits sample = prev_sample[left_map] + (((next_sample[left_map] - prev_sample[left_map]) * diff_mul) >> FREQ_SHIFT);
			write[0] += sample*volmul[0];
			if (stereo) {
				sample = prev_sample[right_map] + (((next_sample[right_map] - prev_sample[right_map]) * diff_mul) >> FREQ_SHIFT);
			}
			write[1] += sample*volmul[1];
		}
		//Prepare for next sample
		freq_counter += freq_add;
		mixpos++;
		done++;
	}
}

void MixerChannel::AddStretched(Bitu len,Bit16s * data) {
	if (done >= needed) {
		LOG_MSG("Can't add, buffer full");
		return;
	}
	//Target samples this inputs gets stretched into
	Bitu outlen = needed - done;
	Bitu index = 0;
	Bitu index_add = (len << FREQ_SHIFT)/outlen;
	Bitu mixpos = mixer.pos + done;
	done = needed;
	Bitu pos = 0;

	while (outlen--) {
		Bitu new_pos = index >> FREQ_SHIFT;
		if (pos != new_pos) {
			pos = new_pos;
			//Forward the previous sample
			prev_sample[0] = data[0];
			data++;
		}
		Bits diff = data[0] - prev_sample[0];
		Bits diff_mul = index & FREQ_MASK;
		index += index_add;
		mixpos &= MIXER_BUFMASK;
		Bits sample = prev_sample[0] + ((diff * diff_mul) >> FREQ_SHIFT);
		mixer.work[mixpos][0] += sample * volmul[0];
		mixer.work[mixpos][1] += sample * volmul[1];
		mixpos++;
	}
}

void MixerChannel::AddSamples_m8(Bitu len, const Bit8u * data) {
	AddSamples<Bit8u,false,false,true>(len,data);
}
void MixerChannel::AddSamples_s8(Bitu len,const Bit8u * data) {
	AddSamples<Bit8u,true,false,true>(len,data);
}
void MixerChannel::AddSamples_m8s(Bitu len,const Bit8s * data) {
	AddSamples<Bit8s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s8s(Bitu len,const Bit8s * data) {
	AddSamples<Bit8s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s16(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16u(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,false,false,true>(len,data);
}
void MixerChannel::AddSamples_s16u(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,true,false,true>(len,data);
}
void MixerChannel::AddSamples_m32(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,false,true,true>(len,data);
}
void MixerChannel::AddSamples_s32(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,true,true,true>(len,data);
}
void MixerChannel::AddSamples_m16_nonnative(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,false,true,false>(len,data);
}
void MixerChannel::AddSamples_s16_nonnative(Bitu len,const Bit16s * data) {
	AddSamples<Bit16s,true,true,false>(len,data);
}
void MixerChannel::AddSamples_m16u_nonnative(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,false,false,false>(len,data);
}
void MixerChannel::AddSamples_s16u_nonnative(Bitu len,const Bit16u * data) {
	AddSamples<Bit16u,true,false,false>(len,data);
}
void MixerChannel::AddSamples_m32_nonnative(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,false,true,false>(len,data);
}
void MixerChannel::AddSamples_s32_nonnative(Bitu len,const Bit32s * data) {
	AddSamples<Bit32s,true,true,false>(len,data);
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

extern bool ticksLocked;
static inline bool Mixer_irq_important(void) {
	/* In some states correct timing of the irqs is more important then
	 * non stuttering audo */
	return (ticksLocked || (CaptureState & (CAPTURE_WAVE|CAPTURE_VIDEO)));
}

static Bit32u calc_tickadd(Bit32u freq) {
#if TICK_SHIFT >16
	Bit64u freq64 = static_cast<Bit64u>(freq);
	freq64 = (freq64<<TICK_SHIFT)/1000;
	Bit32u r = static_cast<Bit32u>(freq64);
	return r;
#else
	return (freq<<TICK_SHIFT)/1000;
#endif
}

/* Mix a certain amount of new samples */
static void MIXER_MixData(Bitu needed) {
	MixerChannel * chan=mixer.channels;
	while (chan) {
		chan->Mix(needed);
		chan=chan->next;
	}
	if (CaptureState & (CAPTURE_WAVE|CAPTURE_VIDEO)) {
		Bit16s convert[1024][2];
		Bitu added=needed-mixer.done;
		if (added>1024)
			added=1024;
		Bitu readpos=(mixer.pos+mixer.done)&MIXER_BUFMASK;
		for (Bitu i=0;i<added;i++) {
			Bits sample=mixer.work[readpos][0] >> MIXER_VOLSHIFT;
			convert[i][0]=MIXER_CLIP(sample);
			sample=mixer.work[readpos][1] >> MIXER_VOLSHIFT;
			convert[i][1]=MIXER_CLIP(sample);
			readpos=(readpos+1)&MIXER_BUFMASK;
		}
		CAPTURE_AddWave( mixer.freq, added, (Bit16s*)convert );
	}
	//Reset the the tick_add for constant speed
	if( Mixer_irq_important() )
		mixer.tick_add = calc_tickadd(mixer.freq);
	mixer.done = needed;
}

static void MIXER_Mix(void) {
	SDL_LockAudio();
	MIXER_MixData(mixer.needed);
	mixer.tick_counter += mixer.tick_add;
	mixer.needed+=(mixer.tick_counter >> TICK_SHIFT);
	mixer.tick_counter &= TICK_MASK;
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
	mixer.tick_counter += mixer.tick_add;
	mixer.needed = (mixer.tick_counter >> TICK_SHIFT);
	mixer.tick_counter &= TICK_MASK;
	mixer.done=0;
}

static void SDLCALL MIXER_CallBack(void * userdata, Uint8 *stream, int len) {
	Bitu need=(Bitu)len/MIXER_SSIZE;
	Bit16s * output=(Bit16s *)stream;
	Bitu reduce;
	Bitu pos;
	//Local resampling counter to manipulate the data when sending it off to the callback
	Bitu index, index_add;
	Bits sample;
	/* Enough room in the buffer ? */
	if (mixer.done < need) {
//		LOG_MSG("Full underrun need %d, have %d, min %d", need, mixer.done, mixer.min_needed);
		if((need - mixer.done) > (need >>7) ) //Max 1 procent stretch.
			return;
		reduce = mixer.done;
		index_add = (reduce << TICK_SHIFT) / need;
		mixer.tick_add = calc_tickadd(mixer.freq+mixer.min_needed);
	} else if (mixer.done < mixer.max_needed) {
		Bitu left = mixer.done - need;
		if (left < mixer.min_needed) {
			if( !Mixer_irq_important() ) {
				Bitu needed = mixer.needed - need;
				Bitu diff = (mixer.min_needed>needed?mixer.min_needed:needed) - left;
				mixer.tick_add = calc_tickadd(mixer.freq+(diff*3));
				left = 0; //No stretching as we compensate with the tick_add value
			} else {
				left = (mixer.min_needed - left);
				left = 1 + (2*left) / mixer.min_needed; //left=1,2,3
			}
//			LOG_MSG("needed underrun need %d, have %d, min %d, left %d", need, mixer.done, mixer.min_needed, left);
			reduce = need - left;
			index_add = (reduce << TICK_SHIFT) / need;
		} else {
			reduce = need;
			index_add = (1 << TICK_SHIFT);
//			LOG_MSG("regular run need %d, have %d, min %d, left %d", need, mixer.done, mixer.min_needed, left);

			/* Mixer tick value being updated:
			 * 3 cases:
			 * 1) A lot too high. >division by 5. but maxed by 2* min to prevent too fast drops.
			 * 2) A little too high > division by 8
			 * 3) A little to nothing above the min_needed buffer > go to default value
			 */
			Bitu diff = left - mixer.min_needed;
			if(diff > (mixer.min_needed<<1)) diff = mixer.min_needed<<1;
			if(diff > (mixer.min_needed>>1))
				mixer.tick_add = calc_tickadd(mixer.freq-(diff/5));
			else if (diff > (mixer.min_needed>>2))
				mixer.tick_add = calc_tickadd(mixer.freq-(diff>>3));
			else
				mixer.tick_add = calc_tickadd(mixer.freq);
		}
	} else {
		/* There is way too much data in the buffer */
//		LOG_MSG("overflow run need %d, have %d, min %d", need, mixer.done, mixer.min_needed);
		if (mixer.done > MIXER_BUFSIZE)
			index_add = MIXER_BUFSIZE - 2*mixer.min_needed;
		else
			index_add = mixer.done - 2*mixer.min_needed;
		index_add = (index_add << TICK_SHIFT) / need;
		reduce = mixer.done - 2* mixer.min_needed;
		mixer.tick_add = calc_tickadd(mixer.freq-(mixer.min_needed/5));
	}
	/* Reduce done count in all channels */
	for (MixerChannel * chan=mixer.channels;chan;chan=chan->next) {
		if (chan->done>reduce) chan->done-=reduce;
		else chan->done=0;
	}

	// Reset mixer.tick_add when irqs are important
	if( Mixer_irq_important() )
		mixer.tick_add = calc_tickadd(mixer.freq);

	mixer.done -= reduce;
	mixer.needed -= reduce;
	pos = mixer.pos;
	mixer.pos = (mixer.pos + reduce) & MIXER_BUFMASK;
	index = 0;
	if(need != reduce) {
		while (need--) {
			Bitu i = (pos + (index >> TICK_SHIFT )) & MIXER_BUFMASK;
			index += index_add;
			sample=mixer.work[i][0]>>MIXER_VOLSHIFT;
			*output++=MIXER_CLIP(sample);
			sample=mixer.work[i][1]>>MIXER_VOLSHIFT;
			*output++=MIXER_CLIP(sample);
		}
		/* Clean the used buffer */
		while (reduce--) {
			pos &= MIXER_BUFMASK;
			mixer.work[pos][0]=0;
			mixer.work[pos][1]=0;
			pos++;
		}
	} else {
		while (reduce--) {
			pos &= MIXER_BUFMASK;
			sample=mixer.work[pos][0]>>MIXER_VOLSHIFT;
			*output++=MIXER_CLIP(sample);
			sample=mixer.work[pos][1]>>MIXER_VOLSHIFT;
			*output++=MIXER_CLIP(sample);
			mixer.work[pos][0]=0;
			mixer.work[pos][1]=0;
			pos++;
		}
	}
}

static void MIXER_Stop(Section* sec) {
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
		MixerChannel * chan = mixer.channels;
		while (chan) {
			if (cmd->FindString(chan->name,temp_line,false)) {
				MakeVolume((char *)temp_line.c_str(),chan->volmain[0],chan->volmain[1]);
			}
			chan->UpdateVolume();
			chan = chan->next;
		}
		if (cmd->FindExist("/NOSHOW")) return;
		WriteOut("Channel  Main    Main(dB)\n");
		ShowVolume("MASTER",mixer.mastervol[0],mixer.mastervol[1]);
		for (chan = mixer.channels;chan;chan = chan->next)
			ShowVolume(chan->name,chan->volmain[0],chan->volmain[1]);
	}
private:
	void ShowVolume(const char * name,float vol0,float vol1) {
		WriteOut("%-8s %3.0f:%-3.0f  %+3.2f:%-+3.2f \n",name,
			vol0*100,vol1*100,
			20*log(vol0)/log(10.0f),20*log(vol1)/log(10.0f)
		);
	}

	void ListMidi(){
		if(midi.handler) midi.handler->ListAll(this);
	};

};

static void MIXER_ProgramStart(Program * * make) {
	*make=new MIXER;
}

MixerChannel* MixerObject::Install(MIXER_Handler handler,Bitu freq,const char * name){
	if(!installed) {
		if(strlen(name) > 31) E_Exit("Too long mixer channel name");
		safe_strncpy(m_name,name,32);
		installed = true;
		return MIXER_AddChannel(handler,freq,name);
	} else {
		E_Exit("already added mixer channel.");
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
	spec.samples=(Uint16)mixer.blocksize;

	mixer.tick_counter=0;
	if (mixer.nosound) {
		LOG_MSG("MIXER: No Sound Mode Selected.");
		mixer.tick_add=calc_tickadd(mixer.freq);
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else if (SDL_OpenAudio(&spec, &obtained) <0 ) {
		mixer.nosound = true;
		LOG_MSG("MIXER: Can't open audio: %s , running in nosound mode.",SDL_GetError());
		mixer.tick_add=calc_tickadd(mixer.freq);
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else {
		if((static_cast<Bit16s>(mixer.freq) != obtained.freq) || (mixer.blocksize != obtained.samples))
			LOG_MSG("MIXER: Got different values from SDL: freq %d, blocksize %d",obtained.freq,obtained.samples);
		mixer.freq=obtained.freq;
		mixer.blocksize=obtained.samples;
		mixer.tick_add=calc_tickadd(mixer.freq);
		TIMER_AddTickHandler(MIXER_Mix);
		SDL_PauseAudio(0);
	}
	mixer.min_needed=section->Get_int("prebuffer");
	if (mixer.min_needed>100) mixer.min_needed=100;
	mixer.min_needed=(mixer.freq*mixer.min_needed)/1000;
	mixer.max_needed=mixer.blocksize * 2 + 2*mixer.min_needed;
	mixer.needed=mixer.min_needed+1;
	PROGRAMS_MakeFile("MIXER.COM",MIXER_ProgramStart);
}
