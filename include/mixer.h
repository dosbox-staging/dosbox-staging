/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
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
#include <utility>
#include <vector>

#include "../src/hardware/compressor.h"
#include "audio_frame.h"
#include "envelope.h"

#include <Iir.h>

typedef void (*MIXER_MixHandler)(uint8_t* sampdate, int len);

// The mixer callback can accept a static function or a member function
// using a std::bind. The callback typically requests enough frames to
// fill one millisecond with of audio. For an audio channel running at
// 48000 Hz, that's 48 frames.
using MIXER_Handler = std::function<void(int frames)>;

enum class MixerState { Uninitialized, NoSound, On, Muted };

static constexpr int MixerBufferLength = {16 * 1024};
static constexpr int MixerBufferMask   = {MixerBufferLength - 1};

// TODO This is hacky and should be removed. Only the PS1 Audio uses it.
extern uint8_t MixTemp[MixerBufferLength];

// TODO This seems like is a general-purpose lookup, consider moving it
extern int16_t lut_u8to16[UINT8_MAX + 1];

constexpr auto Max16BitSampleValue = INT16_MAX;
constexpr auto Min16BitSampleValue = INT16_MIN;

static constexpr auto MaxFilterOrder = 16;

static constexpr auto MillisInSecond   = 1000.0;
static constexpr auto MillisInSecondF = 1000.0f;

static constexpr int UseMixerRate = 0;

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
enum LineIndex {
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
	FadeOut,
	ReverbSend,
	Sleep,
	Stereo,
	Synthesizer,
};

enum class FilterState { Off, On, ForcedOn };

struct MixerChannelSettings {
	bool is_enabled          = {};
	AudioFrame user_volume   = {};
	StereoLine lineout_map   = {};
	float crossfeed_strength = {};
	float reverb_level       = {};
	float chorus_level       = {};
};

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

enum class CrossfeedPreset { None, Light, Normal, Strong };

constexpr auto DefaultCrossfeedPreset = CrossfeedPreset::Normal;

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
	std::set<ChannelFeature> GetFeatures() const;
	const std::string& GetName() const;
	int GetSampleRate() const;

	void Set0dbScalar(const float f);
	void RecalcCombinedVolume();

	const AudioFrame GetUserVolume() const;
	void SetUserVolume(const AudioFrame volume);

	const AudioFrame GetAppVolume() const;
	void SetAppVolume(const AudioFrame volume);

	void SetChannelMap(const StereoLine map);

	void SetLineoutMap(const StereoLine map);
	StereoLine GetLineoutMap() const;

	std::string DescribeLineout() const;
	void SetSampleRate(const int sample_rate_hz);
	void SetPeakAmplitude(const int peak);
	void Mix(const int frames_requested);

	MixerChannelSettings GetSettings() const;
	void SetSettings(const MixerChannelSettings& s);

	// Fill up until needed
	void AddSilence();

	void SetHighPassFilter(const FilterState state);
	void SetLowPassFilter(const FilterState state);
	void ConfigureHighPassFilter(const int order, const int cutoff_freq_hz);
	void ConfigureLowPassFilter(const int order, const int cutoff_freq_hz);
	bool TryParseAndSetCustomFilter(const std::string& filter_prefs);

	bool ConfigureFadeOut(const std::string& fadeout_prefs);

	void SetResampleMethod(const ResampleMethod method);
	void SetZeroOrderHoldUpsamplerTargetRate(const int target_rate_hz);

	void SetCrossfeedStrength(const float strength);
	float GetCrossfeedStrength() const;

	void SetReverbLevel(const float level);
	float GetReverbLevel() const;

	void SetChorusLevel(const float level);
	float GetChorusLevel() const;

	template <class Type, bool stereo, bool signeddata, bool nativeorder>
	void AddSamples(const int len, const Type* data);

	void AddSamples_m8(const int len, const uint8_t* data);
	void AddSamples_s8(const int len, const uint8_t* data);
	void AddSamples_m8s(const int len, const int8_t* data);
	void AddSamples_s8s(const int len, const int8_t* data);
	void AddSamples_m16(const int len, const int16_t* data);
	void AddSamples_s16(const int len, const int16_t* data);
	void AddSamples_m16u(const int len, const uint16_t* data);
	void AddSamples_s16u(const int len, const uint16_t* data);
	void AddSamples_m32(const int len, const int32_t* data);
	void AddSamples_s32(const int len, const int32_t* data);
	void AddSamples_mfloat(const int len, const float* data);
	void AddSamples_sfloat(const int len, const float* data);
	void AddSamples_m16_nonnative(const int len, const int16_t* data);
	void AddSamples_s16_nonnative(const int len, const int16_t* data);
	void AddSamples_m16u_nonnative(const int len, const uint16_t* data);
	void AddSamples_s16u_nonnative(const int len, const uint16_t* data);
	void AddSamples_m32_nonnative(const int len, const int32_t* data);
	void AddSamples_s32_nonnative(const int len, const int32_t* data);

	void AddStretched(const int len, int16_t* data);

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

	template <class Type, bool stereo, bool signeddata, bool nativeorder>
	AudioFrame ConvertNextFrame(const Type* data, const int pos);

	template <class Type, bool stereo, bool signeddata, bool nativeorder>
	void ConvertSamples(const Type* data, const int frames,
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

	int sample_rate_hz = 0u;

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
		int target_rate_hz = 0;
		float pos          = 0.0f;
		float step         = 0.0f;
	} zoh_upsampler = {};

	struct {
		SpeexResamplerState* state = nullptr;
	} speex_resampler = {};

	struct {
		struct {
			std::array<Iir::Butterworth::HighPass<MaxFilterOrder>, 2> hpf = {};
			int order          = 0;
			int cutoff_freq_hz = 0;
		} highpass = {};

		struct {
			std::array<Iir::Butterworth::LowPass<MaxFilterOrder>, 2> lpf = {};
			int order          = 0;
			int cutoff_freq_hz = 0;
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
		Sleeper(MixerChannel& c, const int sleep_after_ms = DefaultWaitMs);
		bool ConfigureFadeOut(const std::string& prefs);
		AudioFrame MaybeFadeOrListen(const AudioFrame& frame);
		void MaybeSleep();
		bool WakeUp();

	private:
		void DecrementFadeLevel(const int64_t awake_for_ms);

		MixerChannel& channel;

		// The wait before fading or sleeping is bound between:
		static constexpr auto MinWaitMs     = 100;
		static constexpr auto DefaultWaitMs = 500;
		static constexpr auto MaxWaitMs     = 5000;

		int64_t woken_at_ms            = {};
		float fadeout_level            = {};
		float fadeout_decrement_per_ms = {};
		int fadeout_or_sleep_after_ms  = {};

		bool wants_fadeout = false;
		bool had_signal    = false;
	};
	Sleeper sleeper;
	const bool do_sleep = false;
};

using mixer_channel_t = std::shared_ptr<MixerChannel>;

mixer_channel_t MIXER_AddChannel(MIXER_Handler handler,
                                 const int sample_rate_hz, const char* name,
                                 const std::set<ChannelFeature>& features);

mixer_channel_t MIXER_FindChannel(const char* name);
std::map<std::string, mixer_channel_t>& MIXER_GetChannels();

void MIXER_DeregisterChannel(mixer_channel_t& channel);

// Mixer configuration and initialization
void MIXER_AddConfigSection(const config_ptr_t& conf);
int MIXER_GetSampleRate();
int MIXER_GetPreBufferMs();

const AudioFrame MIXER_GetMasterVolume();
void MIXER_SetMasterVolume(const AudioFrame volume);

void MIXER_Mute();
void MIXER_Unmute();

void MIXER_LockAudioDevice();
void MIXER_UnlockAudioDevice();

// Return true if the mixer was explicitly muted by the user (as opposed to
// auto-muted when `mute_when_inactive` is enabled)
bool MIXER_IsManuallyMuted();

CrossfeedPreset MIXER_GetCrossfeedPreset();
void MIXER_SetCrossfeedPreset(const CrossfeedPreset new_preset);

ReverbPreset MIXER_GetReverbPreset();
void MIXER_SetReverbPreset(const ReverbPreset new_preset);

ChorusPreset MIXER_GetChorusPreset();
void MIXER_SetChorusPreset(const ChorusPreset new_preset);

#endif
