/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
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
#include <map>
#include <memory>
#include <set>
#include <string>

#include "../src/hardware/compressor.h"
#include "audio_frame.h"
#include "envelope.h"

#include <Iir.h>

typedef void (*MIXER_MixHandler)(uint8_t* sampdate, uint32_t len);

// The mixer callback can accept a static function or a member function
// using a std::bind. The callback typically requests enough frames to
// fill one millisecond with of audio. For an audio channel running at
// 48000 Hz, that's 48 frames.
using MIXER_Handler = std::function<void(uint16_t frames)>;

enum class MixerState { Uninitialized, NoSound, On, Muted };

using work_index_t = uint16_t;

static constexpr work_index_t MixerBufferLength = {16 * 1024};
static constexpr work_index_t MixerBufferMask   = {MixerBufferLength - 1};

extern uint8_t MixTemp[MixerBufferLength];
extern int16_t lut_u8to16[UINT8_MAX + 1];

constexpr auto Max16BitSampleValue = INT16_MAX;
constexpr auto Min16BitSampleValue = INT16_MIN;

static constexpr auto max_filter_order = 16;

static constexpr auto millis_in_second   = 1000.0;
static constexpr auto millis_in_second_f = 1000.0f;

static constexpr uint8_t use_mixer_rate = 0;

// Get a DOS-formatted silent-sample when there's a chance it will
// be processed using AddSamples_nonnative()
template <typename T>
constexpr T Mixer_GetSilentDOSSample()
{
	// All signed samples are silent at true zero
	if (std::is_signed<T>::value) {
		return static_cast<T>(0);
	}

	// unsigned 8-bit samples: silence is always 128
	constexpr bool is_multibyte = sizeof(T) > 1;
	if (!is_multibyte) {
		return static_cast<T>(128);
	}

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
enum LineIndex : uint8_t {
	Left  = 0,
	Right = 1,
	// DOS games didn't support surround sound, but if surround sound
	// becomes standard at the host-level, then additional line indexes
	// would go here.
};

struct StereoLine {
	LineIndex left  = Left;
	LineIndex right = Right;

	bool operator==(const StereoLine other) const;
};

static constexpr StereoLine Stereo  = {Left, Right};
static constexpr StereoLine Reverse = {Right, Left};

enum class ChannelFeature {
	ChorusSend,
	DigitalAudio,
	ReverbSend,
	Sleep,
	Stereo,
	Synthesizer,
};
using channel_features_t = std::set<ChannelFeature>;

enum class FilterState { Off, On, ForcedOn };

enum class ResampleMethod {
	// Use simple linear interpolation to resample from the channel sample
	// rate to the mixer rate. This is the legacy behaviour, and it acts as a
	// sort of a low-pass filter.
	LinearInterpolation,

	// Upsample from the channel sample rate to the zero-order-hold target
	// frequency first (this is basically the "nearest-neighbour" equivalent
	// in audio), then resample to the mixer rate with Speex. This method
	// faithfully emulates the metallic, crunchy sound of old DACs.
	ZeroOrderHoldAndResample,

	// Resample from the channel sample rate to the mixer rate with Speex.
	// This is mathematically correct, high-quality resampling that cuts all
	// frequencies below the Nyquist frequency using a brickwall filter
	// (everything below half the channel's sample rate is cut).
	Resample
};

enum class ReverbPreset { None, Tiny, Small, Medium, Large, Huge };

constexpr auto DefaultReverbPreset = ReverbPreset::Medium;

enum class ChorusPreset { None, Light, Normal, Strong };

constexpr auto DefaultChorusPreset = ChorusPreset::Normal;

// forward declarations
struct SpeexResamplerState_;
typedef SpeexResamplerState_ SpeexResamplerState;

class MixerChannel {
public:
	MixerChannel(MIXER_Handler _handler, const char* name,
	             const std::set<ChannelFeature>& features);
	~MixerChannel();

	bool HasFeature(ChannelFeature feature) const;
	const std::string& GetName() const;
	uint16_t GetSampleRate() const;

	void Set0dbScalar(const float f);
	void RecalcCombinedVolume();

	const AudioFrame& GetUserVolume() const;
	void SetUserVolume(const float left, const float right);

	const AudioFrame& GetAppVolume() const;
	void SetAppVolume(const float f);
	void SetAppVolume(const float left, const float right);

	void SetChannelMap(const StereoLine map);
	void SetLineoutMap(const StereoLine map);
	std::string DescribeLineout() const;
	void SetSampleRate(const uint16_t _freq);
	void SetPeakAmplitude(const int peak);
	void Mix(const uint16_t frames_requested);

	// Fill up until needed
	void AddSilence();

	void SetHighPassFilter(const FilterState state);
	void SetLowPassFilter(const FilterState state);
	void ConfigureHighPassFilter(const uint8_t order, const uint16_t cutoff_freq);
	void ConfigureLowPassFilter(const uint8_t order, const uint16_t cutoff_freq);
	bool TryParseAndSetCustomFilter(const std::string_view filter_prefs);

	void SetResampleMethod(const ResampleMethod method);
	void SetZeroOrderHoldUpsamplerTargetFreq(const uint16_t target_freq);

	void SetCrossfeedStrength(const float strength);
	float GetCrossfeedStrength();

	void SetReverbLevel(const float level);
	float GetReverbLevel();

	void SetChorusLevel(const float level);
	float GetChorusLevel();

	template <class Type, bool stereo, bool signeddata, bool nativeorder>
	void AddSamples(const uint16_t len, const Type* data);

	void AddSamples_m8(const uint16_t len, const uint8_t* data);
	void AddSamples_s8(const uint16_t len, const uint8_t* data);
	void AddSamples_m8s(const uint16_t len, const int8_t* data);
	void AddSamples_s8s(const uint16_t len, const int8_t* data);
	void AddSamples_m16(const uint16_t len, const int16_t* data);
	void AddSamples_s16(const uint16_t len, const int16_t* data);
	void AddSamples_m16u(const uint16_t len, const uint16_t* data);
	void AddSamples_s16u(const uint16_t len, const uint16_t* data);
	void AddSamples_m32(const uint16_t len, const int32_t* data);
	void AddSamples_s32(const uint16_t len, const int32_t* data);
	void AddSamples_mfloat(const uint16_t len, const float* data);
	void AddSamples_sfloat(const uint16_t len, const float* data);
	void AddSamples_m16_nonnative(const uint16_t len, const int16_t* data);
	void AddSamples_s16_nonnative(const uint16_t len, const int16_t* data);
	void AddSamples_m16u_nonnative(const uint16_t len, const uint16_t* data);
	void AddSamples_s16u_nonnative(const uint16_t len, const uint16_t* data);
	void AddSamples_m32_nonnative(const uint16_t len, const int32_t* data);
	void AddSamples_s32_nonnative(const uint16_t len, const int32_t* data);

	void AddStretched(const uint16_t len, int16_t* data);

	void FillUp();
	void Enable(const bool should_enable);

	// Pass-through to the sleeper
	bool WakeUp();

	// Timing on how many sample frames have been done by the mixer
	std::atomic<int> frames_done = 0;

	bool is_enabled = false;

private:
	// prevent default construction, copying, and assignment
	MixerChannel()                    = delete;
	MixerChannel(const MixerChannel&) = delete;

	MixerChannel& operator=(const MixerChannel&) = delete;

	template <class Type, bool stereo, bool signeddata, bool nativeorder>
	AudioFrame ConvertNextFrame(const Type* data, const work_index_t pos);

	template <class Type, bool stereo, bool signeddata, bool nativeorder>
	void ConvertSamples(const Type* data, const uint16_t frames,
	                    std::vector<float>& out);

	void ConfigureResampler();
	void ClearResampler();
	void InitZohUpsamplerState();
	void InitLerpUpsamplerState();

	AudioFrame ApplyCrossfeed(const AudioFrame frame) const;

	std::string name = {};
	Envelope envelope;
	MIXER_Handler handler = nullptr;

	std::set<ChannelFeature> features = {};

	// This gets added the frequency counter each mixer step
	int freq_add = 0u;

	// When this flows over a new sample needs to be read from the device
	int freq_counter = 0u;

	// Timing on how many samples were needed by the mixer
	int frames_needed = 0u;

	// Previous and next sample fames
	AudioFrame prev_frame = {};
	AudioFrame next_frame = {};

	int sample_rate = 0u;

	// Volume scalars
	// ~~~~~~~~~~~~~~
	// The user sets this via MIXER.COM, which lets them magnify or diminish
	// the channel's volume relative to other adjustments, such as any
	// adjustments done by the application at runtime.
	AudioFrame user_volume_scalar = {1.0f, 1.0f};

	// The application (might) adjust a channel's volume programmatically at
	// runtime via the Sound Blaster or ReelMagic control interfaces.
	AudioFrame app_volume_scalar = {1.0f, 1.0f};

	// The 0 dB volume scalar is used to bring a channel to 0 dB in the
	// signed 16-bit [-32k, +32k] range.
	//
	// Two examples:
	//
	//  1. The ReelMagic's MP2 samples are decoded to as floats between
	//     [-1.0f, +1.0f], so for we we set this to 32767.0f.
	//
	//  2. The GUS's simultaneous voices can accumulate to ~100%+RMS
	//     above 0 dB, so for that channel we set this to RMS (sqrt of half).
	//
	float db0_volume_scalar = 1.0f;

	// All three of these are multiplied together to form the combined
	// volume scalar. This means we can apply one float-multiply per sample
	// and perform all three adjustments at once.
	//
	AudioFrame combined_volume_scalar = {1.0f, 1.0f};

	// Defines the peak sample amplitude we can expect in this channel.
	// Default to signed 16bit max, however channel's that know their own
	// peak, like the PCSpeaker, should update it with: SetPeakAmplitude()
	//
	int peak_amplitude = Max16BitSampleValue;

	// User-configurable that defines how the channel's Stereo line maps
	// into the mixer.
	StereoLine output_map = Stereo;

	// DOS application-configurable that maps the channels own "left" or
	// "right" as themselves or vice-versa.
	StereoLine channel_map = Stereo;

	bool last_samples_were_stereo  = false;
	bool last_samples_were_silence = true;

	ResampleMethod resample_method = {};
	bool do_resample               = false;
	bool do_zoh_upsample           = false;

	struct {
		float pos             = 0.0f;
		float step            = 0.0f;
		AudioFrame last_frame = {};
	} lerp_upsampler = {};

	struct {
		uint16_t target_freq = 0;
		float pos            = 0.0f;
		float step           = 0.0f;
	} zoh_upsampler = {};

	struct {
		SpeexResamplerState* state = nullptr;
	} speex_resampler = {};

	struct {
		struct {
			std::array<Iir::Butterworth::HighPass<max_filter_order>, 2> hpf = {};
			uint8_t order        = 0;
			uint16_t cutoff_freq = 0;
		} highpass = {};

		struct {
			std::array<Iir::Butterworth::LowPass<max_filter_order>, 2> lpf = {};
			uint8_t order        = 0;
			uint16_t cutoff_freq = 0;
		} lowpass = {};
	} filters               = {};
	bool do_highpass_filter = false;
	bool do_lowpass_filter  = false;

	struct {
		float strength  = 0.0f;
		float pan_left  = 0.0f;
		float pan_right = 0.0f;
	} crossfeed       = {};
	bool do_crossfeed = false;

	struct {
		float level     = 0.0f;
		float send_gain = 0.0f;
	} reverb            = {};
	bool do_reverb_send = false;

	struct {
		float level     = 0.0f;
		float send_gain = 0.0f;
	} chorus            = {};
	bool do_chorus_send = false;

	class Sleeper {
	public:
		Sleeper() = delete;
		Sleeper(MixerChannel& c);
		void Listen(const AudioFrame frame);
		void MaybeSleep();
		bool WakeUp();

	private:
		MixerChannel& channel;
		int64_t woken_at_ms = 0;
		bool had_noise      = false;
	};
	Sleeper sleeper;
	const bool do_sleep = false;
};

using mixer_channel_t = std::shared_ptr<MixerChannel>;

mixer_channel_t MIXER_AddChannel(MIXER_Handler handler, const uint16_t freq,
                                 const char* name,
                                 const std::set<ChannelFeature>& features);

mixer_channel_t MIXER_FindChannel(const char* name);
std::map<std::string, mixer_channel_t>& MIXER_GetChannels();

void MIXER_DeregisterChannel(mixer_channel_t& channel);

// Mixer configuration and initialization
void MIXER_AddConfigSection(const config_ptr_t& conf);
uint16_t MIXER_GetSampleRate();
uint16_t MIXER_GetPreBufferMs();

const AudioFrame MIXER_GetMasterVolume();
void MIXER_SetMasterVolume(const AudioFrame volume);

void MIXER_Mute();
void MIXER_Unmute();

void MIXER_UpdateAllChannelVolumes();

void MIXER_LockAudioDevice();
void MIXER_UnlockAudioDevice();

// Return true if the mixer was explicitly muted by the user (as opposed to
// auto-muted when `mute_when_inactive` is enabled)
bool MIXER_IsManuallyMuted();

ReverbPreset MIXER_GetReverbPreset();
void MIXER_SetReverbPreset(const ReverbPreset new_preset);

ChorusPreset MIXER_GetChorusPreset();
void MIXER_SetChorusPreset(const ChorusPreset new_preset);

#endif
