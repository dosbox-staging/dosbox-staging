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

#ifndef DOSBOX_MIXER_H
#define DOSBOX_MIXER_H

#include "dosbox.h"

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <set>

#include "envelope.h"
#include "mixer.h"

#include "../src/libs/iir1/Iir.h"

typedef void (*MIXER_MixHandler)(uint8_t *sampdate, uint32_t len);

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
	float left  = 0;
	float right = 0;

	float &operator[](std::size_t i)
	{
		assert(i < 2);
		return i == 0 ? left : right;
	}
};

#define MIXER_BUFSIZE (16 * 1024)
#define MIXER_BUFMASK (MIXER_BUFSIZE - 1)
extern uint8_t MixTemp[MIXER_BUFSIZE];

#define MAX_AUDIO ((1<<(16-1))-1)
#define MIN_AUDIO -(1<<(16-1))

static constexpr auto max_filter_order = 16;

// Get a DOS-formatted silent-sample when there's a chance it will
// be processed using AddSamples_nonnative()
template <typename T>
constexpr T Mixer_GetSilentDOSSample()
{
	// All signed samples are silent at true zero
	if (std::is_signed<T>::value)
		return static_cast<T>(0);

	// unsigned 8-bit samples: silence is always 128
	constexpr bool is_multibyte = sizeof(T) > 1;
	if (!is_multibyte)
		return static_cast<T>(128);

	// unsigned 16-bit samples: silence is always 32768 (little-endian)
	if (sizeof(T) == 2u)
#if defined(WORDS_BIGENDIAN)
		return static_cast<T>(0x0080); // 32768 (little-endian)
#else
		return static_cast<T>(0x8000); // 32768
#endif
	static_assert(sizeof(T) <= 2, "DOS only produced 8 and 16-bit samples");
}

// A simple enum to describe the array index associated with a given audio line
enum LINE_INDEX : uint8_t {
	LEFT = 0,
	RIGHT = 1,
	// DOS games didn't support surround sound, but if surround sound becomes
	// standard at the host-level, then additional line indexes would go here.
};

enum class ChannelFeature { Stereo, ReverbSend, ChorusSend };

enum class FilterState { Off, On, ForcedOn };

using work_index_t = uint16_t;

// forward declarations
struct SpeexResamplerState_;
typedef SpeexResamplerState_ SpeexResamplerState;

class MixerChannel {
public:
	MixerChannel(MIXER_Handler _handler, const char *name,
	             const std::set<ChannelFeature> &features);
	~MixerChannel();

	bool HasFeature(ChannelFeature feature);
	int GetSampleRate() const;
	using apply_level_callback_f = std::function<void(const AudioFrame &level)>;
	void RegisterLevelCallBack(apply_level_callback_f cb);
	void SetVolume(float _left, float _right);
	void SetScale(float f);
	void SetScale(float _left, float _right);
	void ChangeChannelMap(const LINE_INDEX left, const LINE_INDEX right);
	bool ChangeLineoutMap(std::string choice);
	std::string DescribeLineout() const;
	void UpdateVolume();
	void SetSampleRate(int _freq);
	void SetPeakAmplitude(int peak);
	void Mix(int _needed);
	void AddSilence(); // Fill up until needed

	void SetLowPassFilter(const FilterState state);
	void ConfigureLowPassFilter(const uint8_t order, const uint16_t cutoff_freq);

	void EnableZeroOrderHoldUpsampler(const bool enabled = true);
	void ConfigureZeroOrderHoldUpsampler(const uint16_t target_freq);

	void SetCrossfeedStrength(const float strength);
	float GetCrossfeedStrength();

	template <class Type, bool stereo, bool signeddata, bool nativeorder>
	void AddSamples(uint16_t len, const Type *data);

	void AddSamples_m8(uint16_t len, const uint8_t *data);
	void AddSamples_s8(uint16_t len, const uint8_t *data);
	void AddSamples_m8s(uint16_t len, const int8_t *data);
	void AddSamples_s8s(uint16_t len, const int8_t *data);
	void AddSamples_m16(uint16_t len, const int16_t *data);
	void AddSamples_s16(uint16_t len, const int16_t *data);
	void AddSamples_m16u(uint16_t len, const uint16_t *data);
	void AddSamples_s16u(uint16_t len, const uint16_t *data);
	void AddSamples_m32(uint16_t len, const int32_t *data);
	void AddSamples_s32(uint16_t len, const int32_t *data);
	void AddSamples_mfloat(uint16_t len, const float *data);
	void AddSamples_sfloat(uint16_t len, const float *data);
	void AddSamples_m16_nonnative(uint16_t len, const int16_t *data);
	void AddSamples_s16_nonnative(uint16_t len, const int16_t *data);
	void AddSamples_m16u_nonnative(uint16_t len, const uint16_t *data);
	void AddSamples_s16u_nonnative(uint16_t len, const uint16_t *data);
	void AddSamples_m32_nonnative(uint16_t len, const int32_t *data);
	void AddSamples_s32_nonnative(uint16_t len, const int32_t *data);

	void AddStretched(uint16_t len, int16_t *data); // Stretch block up into
	                                                // needed data

	void FillUp();
	void Enable(bool should_enable);
	void FlushSamples();

	float volmain[2] = {1.0f, 1.0f};
	std::atomic<int> done = 0; // Timing on how many samples have been done
	                           // by the mixer
	bool is_enabled = false;

private:
	// prevent default construction, copying, and assignment
	MixerChannel() = delete;
	MixerChannel(const MixerChannel &) = delete;
	MixerChannel &operator=(const MixerChannel &) = delete;

	template <class Type, bool stereo, bool signeddata, bool nativeorder>
	AudioFrame ConvertNextFrame(const Type *data, const work_index_t pos);

	template <class Type, bool stereo, bool signeddata, bool nativeorder>
	void ConvertSamples(const Type *data, const uint16_t frames,
	                    std::vector<float> &out);

	void ConfigureResampler();
	void UpdateZOHUpsamplerState();

	AudioFrame ApplyCrossfeed(const AudioFrame &frame) const;

	std::string name = {};
	Envelope envelope;
	MIXER_Handler handler = nullptr;
	std::set<ChannelFeature> features = {};

	int freq_add = 0u;           // This gets added the frequency counter each mixer step
	int freq_counter = 0u;       // When this flows over a new sample needs to be read from the device
	int needed = 0u;             // Timing on how many samples were needed by the mixer

	AudioFrame prev_frame = {}; // Previous and next samples
	AudioFrame next_frame = {};

	// Simple way to lower the impact of DC offset. if MIXER_UPRAMP_STEPS is
	// >0. Still work in progress and thus disabled for now.
	int offset[2] = {0, 0};
	int sample_rate = 0u;
	float volmul[2] = {1.0f, 1.0f};
	float scale[2] = {1.0f, 1.0f};

	// Defines the peak sample amplitude we can expect in this channel.
	// Default to signed 16bit max, however channel's that know their own
	// peak, like the PCSpeaker, should update it with: SetPeakAmplitude()
	int peak_amplitude = MAX_AUDIO;

	struct StereoLine {
		LINE_INDEX left = LEFT;
		LINE_INDEX right = RIGHT;
		bool operator==(const StereoLine &other) const;
	};

	static constexpr StereoLine STEREO = {LEFT, RIGHT};
	static constexpr StereoLine REVERSE = {RIGHT, LEFT};

	// User-configurable that defines how the channel's stereo line maps
	// into the mixer.
	StereoLine output_map = STEREO;

	// DOS application-configurable that maps the channels own "left" or
	// "right" as themselves or vice-versa.
	StereoLine channel_map = STEREO;

	// The RegisterLevelCallBack() assigns this callback that can be used by
	// the channel's source to manage the stream's level prior to mixing,
	// in-place of scaling by volmain[]
	apply_level_callback_f apply_level = nullptr;

	bool last_samples_were_stereo = false;
	bool last_samples_were_silence = true;

	struct {
		bool enabled = false;
		SpeexResamplerState *state = nullptr;
	} resampler = {};

	struct {
		bool enabled = false;
		uint16_t target_freq = 0;
		float pos = 0.0f;
		float step = 0.0f;
	} zoh_upsampler = {};

	struct {
		FilterState state = FilterState::Off;
		std::array<Iir::Butterworth::LowPass<max_filter_order>, 2> lpf = {};
	} filter = {};

	struct {
		float strength = 0.0f;
		float pan_left = 0.0f;
		float pan_right = 0.0f;
	} crossfeed = {};
};
using mixer_channel_t = std::shared_ptr<MixerChannel>;

mixer_channel_t MIXER_AddChannel(MIXER_Handler handler, const int freq,
                                 const char *name,
                                 const std::set<ChannelFeature> &features);

mixer_channel_t MIXER_FindChannel(const char *name);

/* PC Speakers functions, tightly related to the timer functions */
void PCSPEAKER_SetCounter(int cntr, int mode);
void PCSPEAKER_SetType(int mode);

// Mixer configuration and initialization
void MIXER_AddConfigSection(const config_ptr_t &conf);

#endif
