/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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

// #define DEBUG 1

#include "mixer.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <sys/types.h>

#if defined (WIN32)
//Midi listing
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#include <SDL.h>

#include "mem.h"
#include "pic.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "cross.h"
#include "string_utils.h"
#include "mapper.h"
#include "hardware.h"
#include "programs.h"
#include "midi.h"

#define MIXER_SSIZE 4

//#define MIXER_SHIFT 14
//#define MIXER_REMAIN ((1<<MIXER_SHIFT)-1)

#define FREQ_SHIFT 14
#define FREQ_NEXT ( 1 << FREQ_SHIFT)
#define FREQ_MASK ( FREQ_NEXT -1 )

#define TICK_SHIFT 24
#define TICK_NEXT ( 1 << TICK_SHIFT)
#define TICK_MASK (TICK_NEXT -1)

// Over how many milliseconds will we permit a signal to grow from
// zero up to peak amplitude? (recommended 10 to 20ms)
#define ENVELOPE_MAX_EXPANSION_OVER_MS 15u

// Regardless if the signal needed to be eveloped or not, how long
// should the envelope monitor the initial signal? (recommended > 5s)
#define ENVELOPE_EXPIRES_AFTER_S 10u

template <class T, size_t ROWS, size_t COLS>
using matrix = std::array<std::array<T, COLS>, ROWS>;
using work_index_t = uint16_t;

static constexpr int16_t MIXER_CLIP(const int SAMP)
{
	if (SAMP <= MIN_AUDIO) return MIN_AUDIO;
	if (SAMP >= MAX_AUDIO) return MAX_AUDIO;
	
	return static_cast<int16_t>(SAMP);
}

struct mixer_t {
	// complex types
	matrix<float, MIXER_BUFSIZE, 2> work = {};
	std::array<float, 2> mastervol = {1.0f, 1.0f};
	std::map<std::string, mixer_channel_t> channels = {};
	std::mutex channel_mutex = {}; // use whenever accessing channels

	// Counters accessed by multiple threads
	std::atomic<work_index_t> pos = 0;
	std::atomic<int> done = 0;
	std::atomic<int> needed = 0;
	std::atomic<int> min_needed = 0;
	std::atomic<int> max_needed = 0;
	std::atomic<int> tick_add = 0; // samples needed per millisecond tick

	int tick_counter = 0;
	int freq = 0;           // sample rate negotiated with SDL
	uint16_t blocksize = 0; // matches SDL AudioSpec.samples type

	SDL_AudioDeviceID sdldevice = 0;
	bool nosound = false;
};

static struct mixer_t mixer = {};

uint8_t MixTemp[MIXER_BUFSIZE] = {};

MixerChannel::MixerChannel(MIXER_Handler _handler, const char *_name) : envelope(_name), handler(_handler)
{}

bool MixerChannel::StereoLine::operator==(const StereoLine &other) const
{
	return left == other.left && right == other.right;
}

mixer_channel_t MIXER_AddChannel(MIXER_Handler handler, const int freq, const char *name)
{
	auto chan = std::make_shared<MixerChannel>(handler, name);
	chan->SetFreq(freq); // also enables 'interpolate' if needed
	chan->SetScale(1.0);
	chan->SetVolume(1, 1);
	chan->ChangeChannelMap(LEFT, RIGHT);
	chan->Enable(false);

	const auto mix_rate = mixer.freq;
	const auto chan_rate = chan->GetSampleRate();
	if (chan_rate == mix_rate)
		LOG_MSG("MIXER: %s channel operating at %u Hz without resampling",
		        name, chan_rate);
	else
		LOG_MSG("MIXER: %s channel operating at %u Hz and %s to the output rate", name,
		        chan_rate, chan_rate > mix_rate ? "downsampling" : "upsampling");
	std::lock_guard lock(mixer.channel_mutex);
	mixer.channels[name] = chan; // replace the old, if it exists
	return chan;
}

mixer_channel_t MIXER_FindChannel(const char *name)
{
	std::lock_guard lock(mixer.channel_mutex);
	auto it = mixer.channels.find(name);
	return (it != mixer.channels.end()) ? it->second : nullptr;
}

static void MIXER_LockAudioDevice()
{
	SDL_LockAudioDevice(mixer.sdldevice);
}

static void MIXER_UnlockAudioDevice()
{
	SDL_UnlockAudioDevice(mixer.sdldevice);
}

void MixerChannel::RegisterLevelCallBack(apply_level_callback_f cb)
{
	apply_level = cb;
	const AudioFrame level{volmain[0], volmain[1]};
	apply_level(level);
}

void MixerChannel::UpdateVolume()
{
	// Don't scale by volmain[] if the level is being managed by the source
	const float level_l = apply_level ? 1 : volmain[0];
	const float level_r = apply_level ? 1 : volmain[1];
	volmul[0] = scale[0] * level_l * mixer.mastervol[0];
	volmul[1] = scale[1] * level_r * mixer.mastervol[1];
}

void MixerChannel::SetVolume(float _left,float _right) {
	// Allow unconstrained user-defined values
	volmain[0] = _left;
	volmain[1] = _right;

	if (apply_level) {
		const AudioFrame level{_left, _right};
		apply_level(level);
	}
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

void MixerChannel::ChangeChannelMap(const LINE_INDEX mapped_as_left,
                                    const LINE_INDEX mapped_as_right)
{
	assert(mapped_as_left == LEFT || mapped_as_left == RIGHT);
	assert(mapped_as_right == LEFT || mapped_as_right == RIGHT);
	channel_map = {mapped_as_left, mapped_as_right};

#ifdef DEBUG
	LOG_MSG("MIXER %-7s channel: application changed audio-channel mapping to left=>%s and right=>%s",
	        name, channel_map.left == LEFT ? "left" : "right",
	        channel_map.right == LEFT ? "left" : "right");
#endif
}

void MixerChannel::Enable(const bool should_enable)
{
	// Is the channel already in the desired state?
	if (is_enabled == should_enable)
		return;

	// Lock the channel before changing states
	MIXER_LockAudioDevice();

	// Prepare the channel to accept samples
	if (should_enable) {
		freq_counter = 0u;
		// Don't start with a deficit
		if (done < mixer.done)
			done = mixer.done.load();

		// Prepare the channel to go dormant
	} else {
		// Clear the current counters and sample values to
		// start clean if/when this channel is re-enabled.
		// Samples can be buffered into disable channels, so
		// we don't perform this zero'ing in the enable phase.
		done = 0u;
		needed = 0u;
		prev_sample[0] = 0;
		prev_sample[1] = 0;
		next_sample[0] = 0;
		next_sample[1] = 0;
	}
	is_enabled = should_enable;
	MIXER_UnlockAudioDevice();
}

void MixerChannel::SetFreq(int freq)
{
	if (!freq) {
		// If the channel rate is zero, then avoid resampling by running
		// the channel at the same rate as the mixer
		assert(mixer.freq > 0);
		freq = mixer.freq;
	}
	freq_add = (freq << FREQ_SHIFT) / mixer.freq;
	interpolate = (freq != mixer.freq);
	sample_rate = freq;
	envelope.Update(sample_rate, peak_amplitude,
	                ENVELOPE_MAX_EXPANSION_OVER_MS, ENVELOPE_EXPIRES_AFTER_S);
}

bool MixerChannel::IsInterpolated() const
{
	return interpolate;
}

int MixerChannel::GetSampleRate() const
{
	return sample_rate;
}

void MixerChannel::SetPeakAmplitude(const int peak)
{
	peak_amplitude = peak;
	envelope.Update(sample_rate, peak_amplitude,
	                ENVELOPE_MAX_EXPANSION_OVER_MS, ENVELOPE_EXPIRES_AFTER_S);
}

void MixerChannel::Mix(const int _needed)
{
	needed = _needed;
	while (is_enabled && needed > done) {
		auto left = needed - done;
		left *= freq_add;
		left  = (left >> FREQ_SHIFT) + ((left & FREQ_MASK)!=0);
		if (left <= 0) // avoid underflow
			break;
		left = std::min(left, MIXER_BUFSIZE); // avoid overflow
		handler(check_cast<uint16_t>(left));
	}
}

void MixerChannel::AddSilence()
{
	MIXER_LockAudioDevice();

	if (done < needed) {
		if(prev_sample[0] == 0 && prev_sample[1] == 0) {
			done = needed;
			//Make sure the next samples are zero when they get switched to prev
			next_sample[0] = 0;
			next_sample[1] = 0;
			//This should trigger an instant request for new samples
			freq_counter = FREQ_NEXT;
		} else {
			bool stereo = last_samples_were_stereo;

			const auto mapped_output_left = output_map.left;
			const auto mapped_output_right = output_map.right;

			// Position where to write the data
			auto mixpos = check_cast<work_index_t>(mixer.pos + done);
			while (done < needed) {
				// Maybe depend on sample rate. (the 4)
				if (prev_sample[0] > 4)       next_sample[0] = prev_sample[0] - 4;
				else if (prev_sample[0] < -4) next_sample[0] = prev_sample[0] + 4;
				else next_sample[0] = 0;
				if (prev_sample[1] > 4)       next_sample[1] = prev_sample[1] - 4;
				else if (prev_sample[1] < -4) next_sample[1] = prev_sample[1] + 4;
				else next_sample[1] = 0;

				mixpos &= MIXER_BUFMASK;

				mixer.work[mixpos][mapped_output_left] +=
				        prev_sample[0] * volmul[0];

				mixer.work[mixpos][mapped_output_right] +=
				        (stereo ? prev_sample[1] : prev_sample[0]) *
				        volmul[1];

				prev_sample[0] = next_sample[0];
				prev_sample[1] = next_sample[1];
				mixpos++;
				done++;
				freq_counter = FREQ_NEXT;
			} 
		}
	}
	last_samples_were_silence = true;
	offset[0] = offset[1] = 0;

	MIXER_UnlockAudioDevice();
}

// Floating-point conversion from unsigned 8-bit to signed 16-bit.
// This is only used to populate a lookup table that's 20-fold faster.
constexpr int16_t u8to16(const int u_val)
{
	assert(u_val >= 0 && u_val <= UINT8_MAX);
	const auto s_val = u_val - 128;
	if (s_val > 0) {
		constexpr auto scalar = INT16_MAX / 127.0;
		return static_cast<int16_t>(round(s_val * scalar));
	}
	return static_cast<int16_t>(s_val * 256);
}

// 8-bit to 16-bit lookup tables
static int16_t lut_u8to16[UINT8_MAX + 1] = {};
constexpr int16_t *lut_s8to16 = lut_u8to16 + 128;

constexpr void fill_8to16_lut()
{
	for (int i = 0; i <= UINT8_MAX; ++i)
		lut_u8to16[i] = u8to16(i);
}

template <class Type, bool stereo, bool signeddata, bool nativeorder>
void MixerChannel::AddSamples(uint16_t len, const Type *data)
{
	MIXER_LockAudioDevice();

	last_samples_were_stereo = stereo;

	// Position where to write the data
	auto mixpos = check_cast<work_index_t>(mixer.pos + done);
	//Position in the incoming data
	work_index_t pos = 0;

	// read-only aliases to avoid repeated dereferencing and to inform the compiler their values
	// don't change
	const auto mapped_output_left = output_map.left;
	const auto mapped_output_right = output_map.right;

	const auto mapped_channel_left = channel_map.left;
	const auto mapped_channel_right = channel_map.right;

	// Mix data for the full length
	while (1) {
		//Does new data need to get read?
		while (freq_counter >= FREQ_NEXT) {
			//Would this overflow the source data, then it's time to leave
			if (pos >= len) {
				last_samples_were_silence = false;
				MIXER_UnlockAudioDevice();
				return;
			}
			freq_counter -= FREQ_NEXT;

			prev_sample[0] = next_sample[0];
			if (stereo) {
				prev_sample[1] = next_sample[1];
			}

			if ( sizeof( Type) == 1) {
				// unsigned 8-bit
				if (!signeddata) {
					if (stereo) {
						next_sample[0] = lut_u8to16[data[pos * 2 + 0]];
						next_sample[1] = lut_u8to16[data[pos * 2 + 1]];
					} else {
						next_sample[0] = lut_u8to16[data[pos]];
					}
				}
				// signed 8-bit
				else {
					if (stereo) {
						next_sample[0] = lut_s8to16[data[pos * 2 + 0]];
						next_sample[1] = lut_s8to16[data[pos * 2 + 1]];
					} else {
						next_sample[0] = lut_s8to16[data[pos]];
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
								next_sample[0] = (int16_t)host_readw((HostPt)&data[pos * 2 + 0]);
								next_sample[1] = (int16_t)host_readw((HostPt)&data[pos * 2 + 1]);
							} else {
								next_sample[0]=(int32_t)host_readd((HostPt)&data[pos*2+0]);
								next_sample[1]=(int32_t)host_readd((HostPt)&data[pos*2+1]);
							}
						}
					} else {
						if (nativeorder) {
							next_sample[0] = data[pos];
						} else {
							if ( sizeof( Type) == 2) {
								next_sample[0] = (int16_t)host_readw((HostPt)&data[pos]);
							} else {
								next_sample[0]=(int32_t)host_readd((HostPt)&data[pos]);
							}
						}
					}
				} else {
					if (stereo) {
						if (nativeorder) {
							next_sample[0] = static_cast<int>(data[pos * 2 + 0]) - 32768;
							next_sample[1] = static_cast<int>(data[pos * 2 + 1]) - 32768;
						} else {
							if ( sizeof( Type) == 2) {
								next_sample[0] = static_cast<int>(host_readw((HostPt)&data[pos * 2 + 0])) - 32768;
								next_sample[1] = static_cast<int>(host_readw((HostPt)&data[pos * 2 + 1])) - 32768;
							} else {
								next_sample[0] = static_cast<int>(host_readd((HostPt)&data[pos * 2 + 0])) - 32768;
								next_sample[1] = static_cast<int>(host_readd((HostPt)&data[pos * 2 + 1])) - 32768;
							}
						}
					} else {
						if (nativeorder) {
							next_sample[0] = static_cast<int>(data[pos]) - 32768;
						} else {
							if ( sizeof( Type) == 2) {
								next_sample[0] = static_cast<int>(host_readw((HostPt)&data[pos])) - 32768;
							} else {
								next_sample[0] = static_cast<int>(host_readd((HostPt)&data[pos])) - 32768;
							}
						}
					}
				}
			}
			//This sample has been handled now, increase position
			pos++;
		}

		// Process initial samples through an expanding envelope to
		// prevent severe clicks and pops. Becomes a no-op when done.
		envelope.Process(stereo, interpolate, prev_sample, next_sample);

		//Where to write
		mixpos &= MIXER_BUFMASK;

		mixer.work[mixpos][mapped_output_left] +=
				prev_sample[mapped_channel_left] * volmul[0];

		mixer.work[mixpos][mapped_output_right] +=
				(stereo ? prev_sample[mapped_channel_right]
						: prev_sample[mapped_channel_left]) *
				volmul[1];

		//Prepare for next sample
		freq_counter += freq_add;
		mixpos++;
		done++;
	}
	
	MIXER_UnlockAudioDevice();
}

void MixerChannel::AddStretched(uint16_t len, int16_t *data)
{
	MIXER_LockAudioDevice();

	if (done >= needed) {
		LOG_MSG("Can't add, buffer full");
		MIXER_UnlockAudioDevice();
		return;
	}
	//Target samples this inputs gets stretched into
	auto outlen = needed - done;
	auto index = 0;
	auto index_add = (len << FREQ_SHIFT) / outlen;
	auto mixpos = check_cast<work_index_t>(mixer.pos + done);
	auto pos = 0;

	// read-only aliases to avoid dereferencing and inform compiler their
	// values don't change
	const auto mapped_output_left = output_map.left;
	const auto mapped_output_right = output_map.right;

	while (outlen--) {
		const auto new_pos = index >> FREQ_SHIFT;
		if (pos != new_pos) {
			pos = new_pos;
			//Forward the previous sample
			prev_sample[0] = data[0];
			data++;
		}
		const auto diff = data[0] - prev_sample[0];
		const auto diff_mul = index & FREQ_MASK;
		index += index_add;
		mixpos &= MIXER_BUFMASK;
		const auto sample = prev_sample[0] + ((diff * diff_mul) >> FREQ_SHIFT);
		mixer.work[mixpos][mapped_output_left] += sample * volmul[0];
		mixer.work[mixpos][mapped_output_right] += sample * volmul[1];
		mixpos++;
	}

	done = needed;
	
	MIXER_UnlockAudioDevice();
}

void MixerChannel::AddSamples_m8(uint16_t len, const uint8_t *data)
{
	AddSamples<uint8_t, false, false, true>(len, data);
}
void MixerChannel::AddSamples_s8(uint16_t len, const uint8_t *data)
{
	AddSamples<uint8_t, true, false, true>(len, data);
}
void MixerChannel::AddSamples_m8s(uint16_t len, const int8_t *data)
{
	AddSamples<int8_t, false, true, true>(len, data);
}
void MixerChannel::AddSamples_s8s(uint16_t len, const int8_t *data)
{
	AddSamples<int8_t, true, true, true>(len, data);
}
void MixerChannel::AddSamples_m16(uint16_t len, const int16_t *data)
{
	AddSamples<int16_t, false, true, true>(len, data);
}
void MixerChannel::AddSamples_s16(uint16_t len, const int16_t *data)
{
	AddSamples<int16_t, true, true, true>(len, data);
}
void MixerChannel::AddSamples_m16u(uint16_t len, const uint16_t *data)
{
	AddSamples<uint16_t, false, false, true>(len, data);
}
void MixerChannel::AddSamples_s16u(uint16_t len, const uint16_t *data)
{
	AddSamples<uint16_t, true, false, true>(len, data);
}
void MixerChannel::AddSamples_m32(uint16_t len, const int32_t *data)
{
	AddSamples<int32_t, false, true, true>(len, data);
}
void MixerChannel::AddSamples_s32(uint16_t len, const int32_t *data)
{
	AddSamples<int32_t, true, true, true>(len, data);
}
void MixerChannel::AddSamples_m16_nonnative(uint16_t len, const int16_t *data)
{
	AddSamples<int16_t, false, true, false>(len, data);
}
void MixerChannel::AddSamples_s16_nonnative(uint16_t len, const int16_t *data)
{
	AddSamples<int16_t, true, true, false>(len, data);
}
void MixerChannel::AddSamples_m16u_nonnative(uint16_t len, const uint16_t *data)
{
	AddSamples<uint16_t, false, false, false>(len, data);
}
void MixerChannel::AddSamples_s16u_nonnative(uint16_t len, const uint16_t *data)
{
	AddSamples<uint16_t, true, false, false>(len, data);
}
void MixerChannel::AddSamples_m32_nonnative(uint16_t len, const int32_t *data)
{
	AddSamples<int32_t, false, true, false>(len, data);
}
void MixerChannel::AddSamples_s32_nonnative(uint16_t len, const int32_t *data)
{
	AddSamples<int32_t, true, true, false>(len, data);
}

void MixerChannel::FillUp()
{
	if (!is_enabled || done < mixer.done)
		return;
	const auto index = PIC_TickIndex();
	MIXER_LockAudioDevice();
	Mix(check_cast<uint16_t>(static_cast<int64_t>(index * mixer.needed)));
	MIXER_UnlockAudioDevice();
}

std::string MixerChannel::DescribeLineout() const
{
	if (output_map == STEREO)
		return "Stereo";
	if (output_map == REVERSE)
		return "Reverse";
	if (output_map == LEFT_MONO)
		return "LeftMono";
	if (output_map == RIGHT_MONO)
		return "RightMono";

	// Output_map is programmtically set (not directly assigned from user
	// data), so we can assert.
	assertm(false, "Unknown lineout mode");
	return "unknown";
}

bool MixerChannel::ChangeLineoutMap(std::string choice)
{
	lowcase(choice);

	if (choice == "stereo")
		output_map = STEREO;
	else if (choice == "reverse")
		output_map = REVERSE;
	else if (choice == "leftmono")
		output_map = LEFT_MONO;
	else if (choice == "rightmono")
		output_map = RIGHT_MONO;
	else
		return false;

	return true;
}

extern bool ticksLocked;
static inline bool Mixer_irq_important()
{
	/* In some states correct timing of the irqs is more important than
	 * non stuttering audo */
	return (ticksLocked || (CaptureState & (CAPTURE_WAVE|CAPTURE_VIDEO)));
}

static constexpr int calc_tickadd(const int freq)
{
#if TICK_SHIFT > 16
	const auto freq64 = static_cast<int64_t>(freq);
	return check_cast<int>((freq64 << TICK_SHIFT) / 1000);
#else
	return (freq<<TICK_SHIFT)/1000;
#endif
}

/* Mix a certain amount of new samples */
static void MIXER_MixData(int needed)
{
	std::unique_lock lock(mixer.channel_mutex);
	for (auto &it : mixer.channels)
		it.second->Mix(needed);
	lock.unlock();

	if (CaptureState & (CAPTURE_WAVE | CAPTURE_VIDEO)) {
		int16_t convert[1024][2];
		const auto added = check_cast<work_index_t>(std::min(needed - mixer.done, 1024));
		auto readpos = check_cast<work_index_t>((mixer.pos + mixer.done) & MIXER_BUFMASK);
		for (work_index_t i = 0; i < added; i++) {
			const auto sample_1 = mixer.work[readpos][0];
			const auto sample_2 = mixer.work[readpos][1];
			const auto s1 = static_cast<uint16_t>(MIXER_CLIP(sample_1));
			const auto s2 = static_cast<uint16_t>(MIXER_CLIP(sample_2));
			convert[i][0] = static_cast<int16_t>(host_to_le16(s1));
			convert[i][1] = static_cast<int16_t>(host_to_le16(s2));
			readpos = (readpos + 1) & MIXER_BUFMASK;
		}
		CAPTURE_AddWave(mixer.freq, added, reinterpret_cast<int16_t*>(convert));
	}
	//Reset the the tick_add for constant speed
	if( Mixer_irq_important() )
		mixer.tick_add = calc_tickadd(mixer.freq);
	mixer.done = needed;
}

static void MIXER_Mix()
{
	MIXER_LockAudioDevice();
	MIXER_MixData(mixer.needed);
	mixer.tick_counter += mixer.tick_add;
	mixer.needed+=(mixer.tick_counter >> TICK_SHIFT);
	mixer.tick_counter &= TICK_MASK;
	MIXER_UnlockAudioDevice();
}

static void MIXER_ReduceChannelsDoneCounts(const int at_most)
{
	std::lock_guard lock(mixer.channel_mutex);
	for (auto &it : mixer.channels)
		it.second->done -= std::min(it.second->done.load(), at_most);
}

static void MIXER_Mix_NoSound()
{
	MIXER_MixData(mixer.needed);
	/* Clear piece we've just generated */
	for (auto i = 0; i < mixer.needed; ++i) {
		mixer.work[mixer.pos][0]=0;
		mixer.work[mixer.pos][1]=0;
		mixer.pos=(mixer.pos+1)&MIXER_BUFMASK;
	}
	MIXER_ReduceChannelsDoneCounts(mixer.needed);

	/* Set values for next tick */
	mixer.tick_counter += mixer.tick_add;
	mixer.needed = (mixer.tick_counter >> TICK_SHIFT);
	mixer.tick_counter &= TICK_MASK;
	mixer.done=0;
}

#define INDEX_SHIFT_LOCAL 14

static void SDLCALL MIXER_CallBack([[maybe_unused]] void *userdata, Uint8 *stream, int len)
{
	memset(stream, 0, len);
	auto need = len / MIXER_SSIZE;
	auto output = reinterpret_cast<int16_t *>(stream);
	auto reduce = 0;
	work_index_t pos = 0;
	// Local resampling counter to manipulate the data when sending it off
	// to the callback
	auto index_add = (1 << INDEX_SHIFT_LOCAL);
	auto index = (index_add % need) ? need : 0;

	auto sample = 0;
	/* Enough room in the buffer ? */
	if (mixer.done < need) {
		//LOG_WARNING("Full underrun need %d, have %d, min %d", need, mixer.done, mixer.min_needed);
		if ((need - mixer.done) > (need >> 7)) // Max 1 percent stretch.
			return;
		reduce = mixer.done;
		index_add = (reduce << INDEX_SHIFT_LOCAL) / need;
		mixer.tick_add = calc_tickadd(mixer.freq + mixer.min_needed);
	} else if (mixer.done < mixer.max_needed) {
		auto left = mixer.done - need;
		if (left < mixer.min_needed) {
			if (!Mixer_irq_important()) {
				auto needed = mixer.needed - need;
				auto diff = (mixer.min_needed > needed ? mixer.min_needed.load()
				                                       : needed) - left;
				mixer.tick_add = calc_tickadd(mixer.freq +
				                              (diff * 3));
				left = 0; // No stretching as we compensate with
				          // the tick_add value
			} else {
				left = (mixer.min_needed - left);
				left = 1 + (2 * left) / mixer.min_needed; // left=1,2,3
			}
			//LOG_WARNING("needed underrun need %d, have %d, min %d, left %d", need, mixer.done, mixer.min_needed, left);
			reduce = need - left;
			index_add = (reduce << INDEX_SHIFT_LOCAL) / need;
		} else {
			reduce = need;
			index_add = (1 << INDEX_SHIFT_LOCAL);
			//			LOG_MSG("regular run need %d, have
			//%d, min %d, left %d", need, mixer.done,
			//mixer.min_needed, left);

			/* Mixer tick value being updated:
			 * 3 cases:
			 * 1) A lot too high. >division by 5. but maxed by 2*
			 * min to prevent too fast drops. 2) A little too high >
			 * division by 8 3) A little to nothing above the
			 * min_needed buffer > go to default value
			 */
			int diff = left - mixer.min_needed;
			if (diff > (mixer.min_needed << 1))
				diff = mixer.min_needed << 1;
			if (diff > (mixer.min_needed >> 1))
				mixer.tick_add = calc_tickadd(mixer.freq -
				                              (diff / 5));
			else if (diff > (mixer.min_needed >> 2))
				mixer.tick_add = calc_tickadd(mixer.freq -
				                              (diff >> 3));
			else
				mixer.tick_add = calc_tickadd(mixer.freq);
		}
	} else {
		/* There is way too much data in the buffer */
		LOG_WARNING("overflow run need %u, have %u, min %u", need, mixer.done.load(), mixer.min_needed.load());
		if (mixer.done > MIXER_BUFSIZE)
			index_add = MIXER_BUFSIZE - 2 * mixer.min_needed;
		else
			index_add = mixer.done - 2 * mixer.min_needed;
		index_add = (index_add << INDEX_SHIFT_LOCAL) / need;
		reduce = mixer.done - 2 * mixer.min_needed;
		mixer.tick_add = calc_tickadd(mixer.freq - (mixer.min_needed / 5));
	}
	MIXER_ReduceChannelsDoneCounts(reduce);

	// Reset mixer.tick_add when irqs are important
	if( Mixer_irq_important() )
		mixer.tick_add = calc_tickadd(mixer.freq);

	mixer.done -= reduce;
	mixer.needed -= reduce;
	pos = mixer.pos;
	mixer.pos = (mixer.pos + reduce) & MIXER_BUFMASK;
	if (need != reduce) {
		while (need--) {
			const auto i = check_cast<work_index_t>((pos + (index >> INDEX_SHIFT_LOCAL)) & MIXER_BUFMASK);
			index += index_add;
			sample = mixer.work[i][0];
			*output++ = MIXER_CLIP(sample);
			sample = mixer.work[i][1];
			*output++ = MIXER_CLIP(sample);
		}
		/* Clean the used buffer */
		while (reduce--) {
			pos &= MIXER_BUFMASK;
			mixer.work[pos][0] = 0;
			mixer.work[pos][1] = 0;
			pos++;
		}
	} else {
		while (reduce--) {
			pos &= MIXER_BUFMASK;
			sample = mixer.work[pos][0];
			*output++ = MIXER_CLIP(sample);
			sample = mixer.work[pos][1];
			*output++ = MIXER_CLIP(sample);
			mixer.work[pos][0] = 0;
			mixer.work[pos][1] = 0;
			pos++;
		}
	}
}

#undef INDEX_SHIFT_LOCAL

static void MIXER_Stop([[maybe_unused]] Section *sec)
{}

class MIXER final : public Program {
public:
	MIXER() { AddMessages(); }

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
			else
				val = powf(10.0f, val / 20.0f);
			if (val<0) val=1.0f;
			if (!w) {
				vol0=val;
			} else {
				vol1=val;
			}
		}
		if (!w) vol1=vol0;
	}

	void Run()
	{
		if (cmd->FindExist("/?", false) || cmd->FindExist("-?", false) ||
		    cmd->FindExist("-h", false) || cmd->FindExist("--help", false)) {
			WriteOut(MSG_Get("SHELL_CMD_MIXER_HELP_LONG"));
			return;
		}
		if(cmd->FindExist("/LISTMIDI")) {
			ListMidi();
			return;
		}
		if (cmd->FindString("MASTER",temp_line,false)) {
			MakeVolume((char *)temp_line.c_str(),mixer.mastervol[0],mixer.mastervol[1]);
		}

		std::unique_lock lock(mixer.channel_mutex);
		for (auto &[name, channel] : mixer.channels) {
			if (cmd->FindString(name.c_str(), temp_line, false)) {
				if (channel->ChangeLineoutMap(temp_line)) {
					continue;
				}
				float left_vol = 0;
				float right_vol = 0;
				MakeVolume(&temp_line[0], left_vol, right_vol);
				channel->SetVolume(left_vol, right_vol);
			}
			channel->UpdateVolume();
		}
		lock.unlock();

		if (cmd->FindExist("/NOSHOW"))
			return;
		WriteOut("Channel      Main       Main(dB)     Rate(Hz)   Lineout mode\n");
		ShowSettings("MASTER", mixer.mastervol[0], mixer.mastervol[1],
		             mixer.freq, "Stereo (always)");

		lock.lock();
		for (auto &[name, channel] : mixer.channels)
			ShowSettings(name.c_str(), channel->volmain[0],
			             channel->volmain[1], channel->GetSampleRate(),
			             channel->DescribeLineout().c_str());
		lock.unlock();
	}

private:
	void ShowSettings(const char *name,
	                  const float vol0,
	                  const float vol1,
	                  const int rate,
	                  const char *lineout_mode)
	{
		WriteOut("%-11s %4.0f:%-4.0f  %+6.2f:%-+6.2f %8d   %s\n", name,
		         static_cast<double>(vol0 * 100),
		         static_cast<double>(vol1 * 100),
		         static_cast<double>(20 * log(vol0) / log(10.0f)),
		         static_cast<double>(20 * log(vol1) / log(10.0f)), rate,
		         lineout_mode);
	}

	void ListMidi() { MIDI_ListAll(this); }

	void AddMessages() 
	{
		MSG_Add("SHELL_CMD_MIXER_HELP_LONG",
		        "Displays or changes the current sound mixer settings.\n"
		        "\n"
		        "Usage:\n"
		        "  [color=green]mixer[reset] [color=cyan]CHANNEL[reset] [color=white]VOLUME[reset] [/noshow]\n"
		        "  [color=green]mixer[reset] [color=cyan]CHANNEL[reset] [color=white]LINEOUT[reset] [/noshow]\n"
		        "  [color=green]mixer[reset] [/listmidi]\n"
		        "\n"
		        "Where:\n"
		        "  [color=cyan]CHANNEL[reset] is the sound channel you want to change the volume.\n"
		        "  [color=white]VOLUME[reset]  is an integer between 0 and 100 representing the volume.\n"
		        "  [color=white]LINEOUT[reset] sets the channel's line out mode to one of the following:\n"
		        "          [color=white]stereo, reverse, leftmono, rightmono[reset]"
		        "\n"
		        "Notes:\n"
		        "  Running [color=green]mixer[reset] without an argument show the channels' settings.\n"
		        "  You can view available MIDI devices and user options with /listmidi option.\n"
		        "  You may change the settings for more than one sound channel in one command.\n"
		        "  The /noshow option applies the change without showing status.\n"
		        "\n"
		        "Examples:\n"
		        "  [color=green]mixer[reset]\n"
		        "  [color=green]mixer[reset] [color=cyan]cdda[reset] [color=white]50[reset] [color=cyan]sb[reset] [color=white]reverse[reset] /noshow\n"
		        "  [color=green]mixer[reset] /listmidi");
	}
};

std::unique_ptr<Program> MIXER_ProgramCreate() {
	return ProgramCreate<MIXER>();
}

void MIXER_Init(Section* sec) {
	sec->AddDestroyFunction(&MIXER_Stop);

	Section_prop * section=static_cast<Section_prop *>(sec);
	/* Read out config section */

	mixer.nosound=section->Get_bool("nosound");
	mixer.freq = section->Get_int("rate");
	mixer.blocksize = static_cast<uint16_t>(section->Get_int("blocksize"));
	const auto negotiate = section->Get_bool("negotiate");

	/* Start the Mixer using SDL Sound at 22 khz */
	SDL_AudioSpec spec;
	SDL_AudioSpec obtained;

	spec.freq = static_cast<int>(mixer.freq);
	spec.format = AUDIO_S16SYS;
	spec.channels = 2;
	spec.callback = MIXER_CallBack;
	spec.userdata = nullptr;
	spec.samples = mixer.blocksize;

	int sdl_allow_flags = 0;

	if (negotiate) {
		sdl_allow_flags |= SDL_AUDIO_ALLOW_FREQUENCY_CHANGE;
#if defined(SDL_AUDIO_ALLOW_SAMPLES_CHANGE)
		sdl_allow_flags |= SDL_AUDIO_ALLOW_SAMPLES_CHANGE;
#endif
	}

	mixer.tick_counter=0;
	if (mixer.nosound) {
		LOG_MSG("MIXER: No Sound Mode Selected.");
		mixer.tick_add=calc_tickadd(mixer.freq);
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else if ((mixer.sdldevice = SDL_OpenAudioDevice(NULL, 0, &spec, &obtained, sdl_allow_flags)) ==0 ) {
		mixer.nosound = true;
		LOG_WARNING("MIXER: Can't open audio: %s , running in nosound mode.",SDL_GetError());
		mixer.tick_add=calc_tickadd(mixer.freq);
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else {
		// Does SDL want something other than stereo output?
		if (obtained.channels != spec.channels)
			E_Exit("SDL gave us %u-channel output but we require %u channels",
			       obtained.channels, spec.channels);

		// Does SDL want a different playback rate?
		if (obtained.freq != mixer.freq) {
			LOG_WARNING("MIXER: SDL changed the playback rate from %d to %d Hz", mixer.freq, obtained.freq);
			mixer.freq = obtained.freq;
		}

		// Does SDL want a different blocksize?
		const auto obtained_blocksize = obtained.samples;
		if (obtained_blocksize != mixer.blocksize) {
			LOG_WARNING("MIXER: SDL changed the blocksize from %u to %u frames",
			        mixer.blocksize, obtained_blocksize);
			mixer.blocksize = obtained_blocksize;
		}
		mixer.tick_add = calc_tickadd(mixer.freq);
		TIMER_AddTickHandler(MIXER_Mix);
		SDL_PauseAudioDevice(mixer.sdldevice, 0);

		LOG_MSG("MIXER: Negotiated %u-channel %u-Hz audio in %u-frame blocks",
		        obtained.channels, mixer.freq, mixer.blocksize);
	}

	//1000 = 8 *125
	mixer.tick_counter = (mixer.freq%125)?TICK_NEXT:0;
	const auto requested_prebuffer = section->Get_int("prebuffer");
	mixer.min_needed = static_cast<uint16_t>(clamp(requested_prebuffer, 0, 100));
	mixer.min_needed = (mixer.freq * mixer.min_needed) / 1000;
	mixer.max_needed = mixer.blocksize * 2 + 2 * mixer.min_needed;
	mixer.needed = mixer.min_needed + 1;

	// Initialize the 8-bit to 16-bit lookup table
	fill_8to16_lut();
}

void MIXER_CloseAudioDevice()
{
	std::lock_guard lock(mixer.channel_mutex);
	for (auto &it : mixer.channels)
		it.second->Enable(false);

	if (!mixer.nosound) {
		if (mixer.sdldevice != 0) {
			SDL_CloseAudioDevice(mixer.sdldevice);
			mixer.sdldevice = 0;
		}
	}
}
