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

// #define DEBUG 1

#include "mixer.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <set>
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
#include <speex/speex_resampler.h>

#include "../capture/capture.h"
#include "ansi_code_markup.h"
#include "control.h"
#include "cross.h"
#include "hardware.h"
#include "mapper.h"
#include "math_utils.h"
#include "mem.h"
#include "midi.h"
#include "mixer.h"
#include "pic.h"
#include "programs.h"
#include "setup.h"
#include "string_utils.h"
#include "timer.h"
#include "tracy.h"

#include "mverb/MVerb.h"
#include "tal-chorus/ChorusEngine.h"

constexpr auto mixer_frame_size = 4;

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

constexpr auto max_prebuffer_ms = 100;

template <class T, size_t ROWS, size_t COLS>
using matrix = std::array<std::array<T, COLS>, ROWS>;

static constexpr int16_t MIXER_CLIP(const int sample)
{
	if (sample <= MIN_AUDIO) return MIN_AUDIO;
	if (sample >= MAX_AUDIO) return MAX_AUDIO;
	
	return static_cast<int16_t>(sample);
}

using highpass_filter_t = std::array<Iir::Butterworth::HighPass<2>, 2>;

using EmVerb = MVerb<float>;

enum class ReverbPreset { None, Tiny, Small, Medium, Large, Huge };

struct reverb_settings_t {
	EmVerb mverb = {};

	// MVerb does not have an integrated high-pass filter to shape
	// the low-end response like other reverbs. So we're adding one
	// here. This helps take control over low-frequency build-up,
	// resulting in a more pleasant sound.
	highpass_filter_t highpass_filter = {};

	ReverbPreset preset = ReverbPreset::None;
	float synthesizer_send_level   = 0.0f;
	float digital_audio_send_level = 0.0f;
	float highpass_cutoff_freq     = 1.0f;

	void Setup(const float predelay, const float early_mix, const float size,
	           const float density, const float bandwidth_freq,
	           const float decay, const float dampening_freq,
	           const float synth_level, const float digital_level,
	           const float highpass_freq, const int sample_rate)
	{
		synthesizer_send_level   = synth_level;
		digital_audio_send_level = digital_level;
		highpass_cutoff_freq     = highpass_freq;

		mverb.setParameter(EmVerb::PREDELAY, predelay);
		mverb.setParameter(EmVerb::EARLYMIX, early_mix);
		mverb.setParameter(EmVerb::SIZE, size);
		mverb.setParameter(EmVerb::DENSITY, density);
		mverb.setParameter(EmVerb::BANDWIDTHFREQ, bandwidth_freq);
		mverb.setParameter(EmVerb::DECAY, decay);
		mverb.setParameter(EmVerb::DAMPINGFREQ, dampening_freq);
		mverb.setParameter(EmVerb::GAIN, 1.0f); // Always max gain (no attenuation)
		mverb.setParameter(EmVerb::MIX, 1.0f); // Always 100% wet signal

		mverb.setSampleRate(static_cast<float>(sample_rate));

		for (auto &f : highpass_filter)
			f.setup(sample_rate, highpass_freq);
	}
};

enum class ChorusPreset { None, Light, Normal, Strong };

struct chorus_settings_t {
	ChorusEngine chorus_engine = ChorusEngine(48000);

	ChorusPreset preset = ChorusPreset::None;
	float synthesizer_send_level   = 0.0f;
	float digital_audio_send_level = 0.0f;

	void Setup(const float synth_level, const float digital_level,
	           const int sample_rate)
	{
		synthesizer_send_level   = synth_level;
		digital_audio_send_level = digital_level;

		chorus_engine.setSampleRate(static_cast<float>(sample_rate));

		constexpr auto chorus1Enabled  = true;
		constexpr auto chorus2Disabled = false;
		chorus_engine.setEnablesChorus(chorus1Enabled, chorus2Disabled);
	}
};

struct mixer_t {
	// complex types
	matrix<float, MIXER_BUFSIZE, 2> work = {};
	matrix<float, MIXER_BUFSIZE, 2> aux_reverb = {};
	matrix<float, MIXER_BUFSIZE, 2> aux_chorus = {};

	std::vector<float> resample_temp = {};
	std::vector<float> resample_out = {};

	AudioFrame master_volume = {1.0f, 1.0f};
	std::map<std::string, mixer_channel_t> channels = {};

	// Counters accessed by multiple threads
	std::atomic<work_index_t> pos = 0;
	std::atomic<int> frames_done = 0;
	std::atomic<int> frames_needed = 0;
	std::atomic<int> min_frames_needed = 0;
	std::atomic<int> max_frames_needed = 0;
	std::atomic<int> tick_add = 0; // samples needed per millisecond tick

	int tick_counter = 0;
	std::atomic<int> sample_rate = 0; // sample rate negotiated with SDL
	uint16_t blocksize = 0; // matches SDL AudioSpec.samples type

	uint16_t prebuffer_ms = 25; // user-adjustable in conf

	SDL_AudioDeviceID sdldevice = 0;

	MixerState state = MixerState::Uninitialized; // use set_mixer_state() to change

	highpass_filter_t highpass_filter = {};
	Compressor compressor = {};
	bool do_compressor = false;

	reverb_settings_t reverb = {};
	bool do_reverb = false;

	chorus_settings_t chorus = {};
	bool do_chorus = false;

	bool is_manually_muted = false;
};

static struct mixer_t mixer = {};

alignas(sizeof(float)) uint8_t MixTemp[MIXER_BUFSIZE] = {};

uint16_t MIXER_GetPreBufferMs()
{
	assert(mixer.prebuffer_ms > 0);
	assert(mixer.prebuffer_ms <= max_prebuffer_ms);

	return mixer.prebuffer_ms;
}

int MIXER_GetSampleRate()
{
	const auto sample_rate_hz = mixer.sample_rate.load();
	assert(sample_rate_hz > 0);
	return sample_rate_hz;
}

static void MIXER_LockAudioDevice()
{
	SDL_LockAudioDevice(mixer.sdldevice);
}

static void MIXER_UnlockAudioDevice()
{
	SDL_UnlockAudioDevice(mixer.sdldevice);
}

MixerChannel::MixerChannel(MIXER_Handler _handler, const char *_name,
                           const std::set<ChannelFeature> &_features)
        : name(_name),
          envelope(_name),
          handler(_handler),
          features(_features),
          sleeper(*this),
          do_sleep(HasFeature(ChannelFeature::Sleep))
{}

bool MixerChannel::HasFeature(const ChannelFeature feature) const
{
	return features.find(feature) != features.end();
}

bool MixerChannel::StereoLine::operator==(const StereoLine &other) const
{
	return left == other.left && right == other.right;
}

static void set_global_crossfeed(mixer_channel_t channel)
{
	const auto sect = static_cast<Section_prop *>(control->GetSection("mixer"));
	assert(sect);

	// Global crossfeed
	auto crossfeed = 0.0f;

	const std::string_view crossfeed_pref = sect->Get_string("crossfeed");
	const auto crossfeed_pref_has_bool = parse_bool_setting(crossfeed_pref);

	if (crossfeed_pref_has_bool) {
		if (*crossfeed_pref_has_bool == true) {
			constexpr auto default_crossfeed_strength = 0.40f;
			crossfeed = default_crossfeed_strength;
		} else { // false
			crossfeed = 0.0f;
		}
	}
	// Otherwise a custom value was provided
	else if (const auto p = parse_percentage(crossfeed_pref); p) {
		crossfeed = percentage_to_gain(*p);
	} else {
		LOG_WARNING("MIXER: Invalid 'crossfeed' value: '%s', using 'off'",
		            crossfeed_pref.data());
	}
	channel->SetCrossfeedStrength(crossfeed);
}

// Apply the global reverb settings to the given channel
static void set_global_reverb(const mixer_channel_t &channel)
{
	assert(channel);
	if (!mixer.do_reverb || !channel->HasFeature(ChannelFeature::ReverbSend))
		channel->SetReverbLevel(0.0f);
	else if (channel->HasFeature(ChannelFeature::Synthesizer))
		channel->SetReverbLevel(mixer.reverb.synthesizer_send_level);
	else if (channel->HasFeature(ChannelFeature::DigitalAudio))
		channel->SetReverbLevel(mixer.reverb.digital_audio_send_level);
}

// Apply the global chorus settings to the given channel
static void set_global_chorus(const mixer_channel_t &channel)
{
	assert(channel);
	if (!mixer.do_chorus || !channel->HasFeature(ChannelFeature::ChorusSend))
		channel->SetChorusLevel(0.0f);
	else if (channel->HasFeature(ChannelFeature::Synthesizer))
		channel->SetChorusLevel(mixer.chorus.synthesizer_send_level);
	else if (channel->HasFeature(ChannelFeature::DigitalAudio))
		channel->SetChorusLevel(mixer.chorus.digital_audio_send_level);
}

static ReverbPreset reverb_pref_to_preset(const std::string_view pref)
{
	const auto pref_has_bool = parse_bool_setting(pref);
	if (pref_has_bool) {
		return *pref_has_bool ? ReverbPreset::Medium : ReverbPreset::None;
	}
	if (pref == "tiny") {
		return ReverbPreset::Tiny;
	}
	if (pref == "small")
		return ReverbPreset::Small;
	if (pref == "medium") {
		return ReverbPreset::Medium;
	}
	if (pref == "large")
		return ReverbPreset::Large;
	if (pref == "huge")
		return ReverbPreset::Huge;

	// the conf system programmatically guarantees only the above prefs are used
	LOG_WARNING("MIXER: Received an unknown reverb preset type: '%s'", pref.data());
	return ReverbPreset::None;
}

static void configure_reverb(const std::string_view reverb_pref)
{
	auto &r = mixer.reverb; // short-hand reference

	// Unchanged?
	const auto new_preset = reverb_pref_to_preset(reverb_pref);
	if (new_preset == r.preset) {
		return;
	}
	// Set new
	assert(r.preset != new_preset);
	r.preset = new_preset;

	// Pre-computed (negative) decibel scalars
	constexpr auto __5_2dB = 0.87f;
	constexpr auto __6_0dB = 0.85f;
	constexpr auto _12_0dB = 0.70f;
	constexpr auto _18_4dB = 0.54f;
	constexpr auto _24_0dB = 0.40f;
	constexpr auto _36_8dB = 0.08f;
	constexpr auto _37_2dB = 0.07f;
	constexpr auto _38_0dB = 0.05f;

	const auto rate_hz = mixer.sample_rate.load();

	// clang-format off
	switch (r.preset) { //             PDELAY EARLY   SIZE DNSITY BWFREQ  DECAY DAMPLV   -SYNLV   -DIGLV HIPASSHZ RATE_HZ
	case ReverbPreset::None:   break;
	case ReverbPreset::Tiny:   r.Setup(0.00f, 1.00f, 0.05f, 0.50f, 0.50f, 0.00f, 1.00f, __5_2dB, __5_2dB, 200.0f, rate_hz); break;
	case ReverbPreset::Small:  r.Setup(0.00f, 1.00f, 0.17f, 0.42f, 0.50f, 0.50f, 0.70f, _24_0dB, _36_8dB, 200.0f, rate_hz); break;
	case ReverbPreset::Medium: r.Setup(0.00f, 0.75f, 0.50f, 0.50f, 0.95f, 0.42f, 0.21f, _18_4dB, _37_2dB, 170.0f, rate_hz); break;
	case ReverbPreset::Large:  r.Setup(0.00f, 0.75f, 0.75f, 0.50f, 0.95f, 0.52f, 0.21f, _12_0dB, _38_0dB, 140.0f, rate_hz); break;
	case ReverbPreset::Huge:   r.Setup(0.00f, 0.75f, 0.75f, 0.50f, 0.95f, 0.52f, 0.21f, __6_0dB, _38_0dB, 140.0f, rate_hz); break;
	}
	// clang-format on

	// Configure the channels
	mixer.do_reverb = (r.preset != ReverbPreset::None);
	for (auto &it : mixer.channels)
		set_global_reverb(it.second);

	if (mixer.do_reverb) {
		LOG_MSG("MIXER: Reverb enabled ('%s' preset)", reverb_pref.data());
	} else {
		LOG_MSG("MIXER: Reverb disabled");
	}
}

static ChorusPreset chorus_pref_to_preset(const std::string_view pref)
{
	const auto pref_has_bool = parse_bool_setting(pref);
	if (pref_has_bool) {
		return *pref_has_bool ? ChorusPreset::Normal : ChorusPreset::None;
	}
	if (pref == "light") {
		return ChorusPreset::Light;
	}
	if (pref == "normal") {
		return ChorusPreset::Normal;
	}
	if (pref == "strong")
		return ChorusPreset::Strong;

	// the conf system programmatically guarantees only the above prefs are used
	LOG_WARNING("MIXER: Received an unknown chorus preset type: '%s'", pref.data());
	return ChorusPreset::None;
}

static void configure_chorus(const std::string_view chorus_pref)
{
	auto &c = mixer.chorus; // short-hand reference

	// Unchanged?
	const auto new_preset = chorus_pref_to_preset(chorus_pref);
	if (new_preset == c.preset) {
		return;
	}
	// Set new
	assert(c.preset != new_preset);
	c.preset = new_preset;

	// Pre-computed (negative) decibel scalars
	constexpr auto __6dB = 0.75f;
	constexpr auto _11dB = 0.54f;
	constexpr auto _16dB = 0.33f;

	const auto rate_hz = mixer.sample_rate.load();

	// clang-format off
	switch (c.preset) { //            -SYNLV -DIGLV  RATE_HZ
	case ChorusPreset::None:   break;
	case ChorusPreset::Light:  c.Setup(_16dB, 0.00f, rate_hz); break;
	case ChorusPreset::Normal: c.Setup(_11dB, 0.00f, rate_hz); break;
	case ChorusPreset::Strong: c.Setup(__6dB, 0.00f, rate_hz); break;
	}
	// clang-format on

	// Configure the channels
	mixer.do_chorus = (c.preset != ChorusPreset::None);
	for (auto &it : mixer.channels)
		set_global_chorus(it.second);

	if (mixer.do_chorus) {
		LOG_MSG("MIXER: Chorus enabled ('%s' preset)", chorus_pref.data());
	} else {
		LOG_MSG("MIXER: Chorus disabled");
	}
}

static void configure_compressor(const bool compressor_enabled)
{
	mixer.do_compressor = compressor_enabled;
	if (!mixer.do_compressor) {
		LOG_MSG("MIXER: Master compressor disabled");
		return;
	}

	const auto _0dbfs_sample_value = INT16_MAX;
	const auto threshold_db        = -6.0f;
	const auto ratio               = 3.0f;
	const auto attack_time_ms      = 0.01f;
	const auto release_time_ms     = 5000.0f;
	const auto rms_window_ms       = 10.0;

	mixer.compressor.Configure(mixer.sample_rate,
	                           _0dbfs_sample_value,
	                           threshold_db,
	                           ratio,
	                           attack_time_ms,
	                           release_time_ms,
	                           rms_window_ms);

	LOG_MSG("MIXER: Master compressor enabled");
}

// Remove a channel by name from the mixer's map of channels.
void MIXER_DeregisterChannel(const std::string& name_to_remove)
{
	MIXER_LockAudioDevice();
	auto it = mixer.channels.find(name_to_remove);
	if (it != mixer.channels.end()) {
		mixer.channels.erase(it);
	}
	MIXER_UnlockAudioDevice();
}
// Remove a channel using the shared pointer variable.
void MIXER_DeregisterChannel(mixer_channel_t& channel_to_remove)
{
	if (!channel_to_remove) {
		return;
	}

	MIXER_LockAudioDevice();
	auto it = mixer.channels.begin();
	while (it != mixer.channels.end()) {
		if (it->second.get() == channel_to_remove.get()) {
			it = mixer.channels.erase(it);
			break;
		}
		++it;
	}
	MIXER_UnlockAudioDevice();
}

mixer_channel_t MIXER_AddChannel(MIXER_Handler handler, const int freq,
                                 const char *name,
                                 const std::set<ChannelFeature> &features)
{
	auto chan = std::make_shared<MixerChannel>(handler, name, features);
	chan->SetSampleRate(freq);
	chan->SetAppVolume(1.0f);
	chan->SetUserVolume(1.0f, 1.0f);
	chan->ChangeChannelMap(LEFT, RIGHT);
	chan->Enable(false);

	set_global_crossfeed(chan);
	set_global_reverb(chan);
	set_global_chorus(chan);

	const auto chan_rate = chan->GetSampleRate();
	if (chan_rate == mixer.sample_rate)
		LOG_MSG("%s: Operating at %u Hz without resampling", name, chan_rate);
	else
		LOG_MSG("%s: Operating at %u Hz and %s to the output rate",
		        name,
		        chan_rate,
		        chan_rate > mixer.sample_rate ? "downsampling" : "upsampling");

	MIXER_LockAudioDevice();
	mixer.channels[name] = chan; // replace the old, if it exists
	MIXER_UnlockAudioDevice();
	return chan;
}

mixer_channel_t MIXER_FindChannel(const char *name)
{
	MIXER_LockAudioDevice();
	auto it = mixer.channels.find(name);

	if (it == mixer.channels.end()) {
		// Deprecated alias SPKR to PCSPEAKER
		if (std::string_view(name) == "SPKR") {
			LOG_WARNING("MIXER: 'SPKR' is deprecated due to inconsistent "
						"naming, please use 'PCSPEAKER' instead");
			it = mixer.channels.find("PCSPEAKER");

		// Deprecated alias FM to OPL
		} else if (std::string_view(name) == "FM") {
			LOG_WARNING("MIXER: 'FM' is deprecated due to inconsistent "
						"naming, please use 'OPL' instead");
			it = mixer.channels.find("OPL");
		}
	}

	const auto chan = (it != mixer.channels.end()) ? it->second : nullptr;
	MIXER_UnlockAudioDevice();

	return chan;
}

void MixerChannel::RecalcCombinedVolume()
{
	combined_volume_scalar.left = user_volume_scalar.left *
	                              app_volume_scalar.left *
	                              mixer.master_volume.left * db0_volume_scalar;

	combined_volume_scalar.right = user_volume_scalar.right *
	                               app_volume_scalar.right *
	                               mixer.master_volume.right * db0_volume_scalar;
}

void MixerChannel::SetUserVolume(const float left, const float right)
{
	// Allow unconstrained user-defined values
	user_volume_scalar = {left, right};
	RecalcCombinedVolume();
}

void MixerChannel::SetAppVolume(const float v)
{
	SetAppVolume(v, v);
}

void MixerChannel::SetAppVolume(const float left, const float right)
{
	// Constrain application-defined volume between 0% and 100%
	auto clamp_to_unity = [](const float vol) {
		constexpr auto min_unity_volume = 0.0f;
		constexpr auto max_unity_volume = 1.0f;
		return clamp(vol, min_unity_volume, max_unity_volume);
	};
	app_volume_scalar = {clamp_to_unity(left), clamp_to_unity(right)};
	RecalcCombinedVolume();

#ifdef DEBUG
	LOG_MSG("MIXER: %-7s channel: application requested volume "
	        "{%3.0f%%, %3.0f%%}, and was set to {%3.0f%%, %3.0f%%}",
	        name,
	        static_cast<double>(left),
	        static_cast<double>(right),
	        static_cast<double>(app_volume_scalar.left * 100.0f),
	        static_cast<double>(app_volume_scalar.right * 100.0f));
#endif
}

void MixerChannel::Set0dbScalar(const float scalar)
{
	// Realistically we expect some channels might need a fixed boost
	// to get to 0dB, but others might need a range mapping, like from
	// a unity float [-1.0f, +1.0f] to  16-bit int [-32k,+32k] range.
	assert(scalar >= 0.0f && scalar <= static_cast<int16_t>(INT16_MAX));

	db0_volume_scalar = scalar;

	RecalcCombinedVolume();
}

const AudioFrame& MixerChannel::GetUserVolume() const
{
	return user_volume_scalar;
}

const AudioFrame& MixerChannel::GetAppVolume() const
{
	return app_volume_scalar;
}

static void MIXER_UpdateAllChannelVolumes()
{
	MIXER_LockAudioDevice();

	for (auto &it : mixer.channels)
		it.second->RecalcCombinedVolume();

	MIXER_UnlockAudioDevice();
}

void MixerChannel::ChangeChannelMap(const LINE_INDEX mapped_as_left,
                                    const LINE_INDEX mapped_as_right)
{
	assert(mapped_as_left == LEFT || mapped_as_left == RIGHT);
	assert(mapped_as_right == LEFT || mapped_as_right == RIGHT);
	channel_map = {mapped_as_left, mapped_as_right};

#ifdef DEBUG
	LOG_MSG("MIXER: %-7s channel: application changed audio-channel mapping to left=>%s and right=>%s",
	        name,
	        channel_map.left == LEFT ? "left" : "right",
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
		if (frames_done < mixer.frames_done)
			frames_done = mixer.frames_done.load();

		// Prepare the channel to go dormant
	} else {
		// Clear the current counters and sample values to
		// start clean if/when this channel is re-enabled.
		// Samples can be buffered into disable channels, so
		// we don't perform this zero'ing in the enable phase.
		frames_done = 0u;
		frames_needed = 0u;

		prev_frame = {0.0f, 0.0f};
		next_frame = {0.0f, 0.0f};

		if (do_resample || do_zoh_upsample) {
			ClearResampler();
		}
	}
	is_enabled = should_enable;

	MIXER_UnlockAudioDevice();
}

// Depending on the resampling method and the channel, mixer and ZoH upsampler
// rates, the following scenarios are possible:
//
// LinearInterpolation
// -------------------
//   - Linear interpolation resampling only if channel_rate != mixer_rate
//
// ZeroOrderHoldAndResample
// ------------------------
//   - neither ZoH upsampling nor Speex resampling if:
//     channel_rate >= zoh_target_rate AND channel_rate == mixer_rate
//
//   - ZoH upsampling only if:
//     channel_rate < zoh_target_freq AND zoh_target_rate == mixer_rate
//
//   - Speex resampling only if:
//     channel_rate >= zoh_target_freq AND channel_rate != mixer_rate
//
//   - both ZoH upsampling and Speex resampling if:
//     channel_rate < zoh_target_rate AND zoh_target_rate != mixer_rate
//
// Resample
// --------
//   - Speex resampling only if channel_rate != mixer_rate
//
void MixerChannel::ConfigureResampler()
{
	const auto channel_rate = sample_rate;
	const auto mixer_rate   = mixer.sample_rate.load();

	do_resample = false;
	do_zoh_upsample = false;

	switch (resample_method) {
	case ResampleMethod::LinearInterpolation:
		do_resample = (channel_rate != mixer_rate);
		if (do_resample) {
			InitLerpUpsamplerState();
			// DEBUG_LOG_MSG("%s: Linear interpolation resampler is on",
			//               name.c_str());
		}
		break;

	case ResampleMethod::ZeroOrderHoldAndResample:
		do_zoh_upsample = (channel_rate < zoh_upsampler.target_freq);
		if (do_zoh_upsample) {
			InitZohUpsamplerState();
			// DEBUG_LOG_MSG("%s: Zero-order-hold upsampler is on, target rate: %d Hz ",
			//               name.c_str(),
			//               zoh_upsampler.target_freq);
		}
		[[fallthrough]];

	case ResampleMethod::Resample:
		const auto in_rate = do_zoh_upsample ? zoh_upsampler.target_freq
		                                     : channel_rate;
		const auto out_rate = mixer_rate;

		do_resample = (in_rate != out_rate);
		if (!do_resample)
			return;

		if (!speex_resampler.state) {
			constexpr auto num_channels = 2; // always stereo
			constexpr auto quality      = 5;

			speex_resampler.state = speex_resampler_init(
			        num_channels, in_rate, out_rate, quality, nullptr);
		}
		speex_resampler_set_rate(speex_resampler.state, in_rate, out_rate);

		// DEBUG_LOG_MSG("%s: Speex resampler is on, input rate: %d Hz, output rate: %d Hz)",
		//               name.c_str(),
		//               in_rate,
		//               out_rate);
		break;
	}
}

// Clear the resampler and prime its input queue with zeros
void MixerChannel::ClearResampler()
{
	switch (resample_method) {
	case ResampleMethod::LinearInterpolation:
		if (do_resample) {
			InitLerpUpsamplerState();
		}
		break;

	case ResampleMethod::ZeroOrderHoldAndResample:
		if (do_zoh_upsample) {
			InitZohUpsamplerState();
		}
		[[fallthrough]];

	case ResampleMethod::Resample:
		if (do_resample) {
			assert(speex_resampler.state);
			speex_resampler_reset_mem(speex_resampler.state);
			speex_resampler_skip_zeros(speex_resampler.state);

			// DEBUG_LOG_MSG("%s: Speex resampler cleared and primed %d-frame input queue",
			//               name.c_str(),
			//               speex_resampler_get_input_latency(
			//                       speex_resampler.state));
		}
		break;
	}
}

void MixerChannel::SetSampleRate(const int rate)
{
	// If the requested rate is zero, then avoid resampling by running the
	// channel at the mixer's rate
	const auto target_rate = rate ? rate : mixer.sample_rate.load();
	assert(target_rate > 0);

	// Nothing to do: the channel is already running at the requested rate
	if (target_rate == sample_rate)
		return;

	// DEBUG_LOG_MSG("%s: Changing rate from %d to %d Hz",
	//              name.c_str(),
	//              sample_rate,
	//              target_rate);
	sample_rate = target_rate;

	freq_add = (sample_rate << FREQ_SHIFT) / mixer.sample_rate;

	envelope.Update(sample_rate,
	                peak_amplitude,
	                ENVELOPE_MAX_EXPANSION_OVER_MS,
	                ENVELOPE_EXPIRES_AFTER_S);

	ConfigureResampler();
}

const std::string& MixerChannel::GetName() const
{
	return name;
}

int MixerChannel::GetSampleRate() const
{
	return sample_rate;
}

void MixerChannel::ReactivateEnvelope()
{
	envelope.Reactivate();
}

void MixerChannel::SetPeakAmplitude(const int peak)
{
	peak_amplitude = peak;
	envelope.Update(sample_rate, peak_amplitude,
	                ENVELOPE_MAX_EXPANSION_OVER_MS, ENVELOPE_EXPIRES_AFTER_S);
}

void MixerChannel::Mix(const int frames_requested)
{
	if (!is_enabled)
		return;

	frames_needed = frames_requested;
	while (frames_needed > frames_done) {
		auto frames_remaining = frames_needed - frames_done;
		frames_remaining *= freq_add;
		frames_remaining = (frames_remaining >> FREQ_SHIFT) +
		                   ((frames_remaining & FREQ_MASK) != 0);
		if (frames_remaining <= 0) // avoid underflow
			break;
		frames_remaining = std::min(frames_remaining, MIXER_BUFSIZE); // avoid overflow
		handler(check_cast<uint16_t>(frames_remaining));
	}
	if (do_sleep)
		sleeper.MaybeSleep();
}

void MixerChannel::AddSilence()
{
	MIXER_LockAudioDevice();

	if (frames_done < frames_needed) {
		if (prev_frame[0] == 0.0f && prev_frame[1] == 0.0f) {
			frames_done = frames_needed;
			// Make sure the next samples are zero when they get
			// switched to prev
			next_frame = {0.0f, 0.0f};
			// This should trigger an instant request for new samples
			freq_counter = FREQ_NEXT;
		} else {
			bool stereo = last_samples_were_stereo;

			const auto mapped_output_left  = output_map.left;
			const auto mapped_output_right = output_map.right;

			// Position where to write the data
			auto mixpos = check_cast<work_index_t>(mixer.pos + frames_done);
			while (frames_done < frames_needed) {
				// Fade gradually to silence to avoid clicks.
				// Maybe the fade factor f depends on the sample
				// rate.
				constexpr auto f = 4.0f;

				for (auto ch = 0; ch < 2; ++ch) {
					if (prev_frame[ch] > f)
						next_frame[ch] = prev_frame[ch] - f;
					else if (prev_frame[ch] < -f)
						next_frame[ch] = prev_frame[0] + f;
					else
						next_frame[ch] = 0.0f;
				}

				mixpos &= MIXER_BUFMASK;

				mixer.work[mixpos][mapped_output_left] +=
				        prev_frame.left * combined_volume_scalar.left;

				mixer.work[mixpos][mapped_output_right] +=
				        (stereo ? prev_frame.right : prev_frame.left) *
				        combined_volume_scalar.right;

				prev_frame = next_frame;
				mixpos++;
				frames_done++;
				freq_counter = FREQ_NEXT;
			}
		}
	}
	last_samples_were_silence = true;

	MIXER_UnlockAudioDevice();
}

static void log_filter_settings(const std::string &channel_name,
                                const std::string_view filter_name,
                                const FilterState state, const uint8_t order,
                                const uint16_t cutoff_freq)
{
	// we programmatically expect only 'on' and 'forced-on' states:
	assert(state != FilterState::Off);
	assert(state == FilterState::On || state == FilterState::ForcedOn);

	constexpr auto db_per_order = 6;

	LOG_MSG("%s: %s filter enabled%s (%d dB/oct at %u Hz)",
	        channel_name.c_str(),
	        filter_name.data(),
	        state == FilterState::ForcedOn ? " (forced)" : "",
	        order * db_per_order,
	        cutoff_freq);
}

void MixerChannel::SetHighPassFilter(const FilterState state)
{
	do_highpass_filter = state != FilterState::Off;

	if (do_highpass_filter)
		log_filter_settings(name,
		                    "Highpass",
		                    state,
		                    filters.highpass.order,
		                    filters.highpass.cutoff_freq);
}

void MixerChannel::SetLowPassFilter(const FilterState state)
{
	do_lowpass_filter = state != FilterState::Off;

	if (do_lowpass_filter)
		log_filter_settings(name,
		                    "Lowpass",
		                    state,
		                    filters.lowpass.order,
		                    filters.lowpass.cutoff_freq);
}

void MixerChannel::ConfigureHighPassFilter(const uint8_t order,
                                           const uint16_t cutoff_freq)
{
	assert(order > 0 && order <= max_filter_order);
	for (auto &f : filters.highpass.hpf)
		f.setup(order, mixer.sample_rate, cutoff_freq);

	filters.highpass.order = order;
	filters.highpass.cutoff_freq = cutoff_freq;
}

void MixerChannel::ConfigureLowPassFilter(const uint8_t order,
                                          const uint16_t cutoff_freq)
{
	assert(order > 0 && order <= max_filter_order);
	for (auto &f : filters.lowpass.lpf)
		f.setup(order, mixer.sample_rate, cutoff_freq);

	filters.lowpass.order = order;
	filters.lowpass.cutoff_freq = cutoff_freq;
}

// Tries to set custom filter settings from the passed in filter preferences.
// Returns true if the custom filters could be successfully set, false
// otherwise and disables all filters for the channel.
bool MixerChannel::TryParseAndSetCustomFilter(const std::string_view filter_prefs)
{
	SetLowPassFilter(FilterState::Off);
	SetHighPassFilter(FilterState::Off);

	if (!(starts_with(filter_prefs, "lpf") || starts_with(filter_prefs, "hpf"))) {
		return false;
	}

	const auto parts = split(filter_prefs, ' ');

	const auto single_filter = (parts.size() == 3);
	const auto dual_filter   = (parts.size() == 6);

	if (!(single_filter || dual_filter)) {
		LOG_WARNING("%s: Invalid custom filter definition: '%s'. Must be "
		            "specified in \"lfp|hpf ORDER CUTOFF_FREQUENCY\" format",
		            name.c_str(),
		            filter_prefs.data());
		return false;
	}

	auto set_filter = [&](const std::string &type_pref,
	                      const std::string &order_pref,
	                      const std::string &cutoff_freq_pref) {
		int order;
		if (!sscanf(order_pref.c_str(), "%d", &order) || order < 1 ||
		    order > max_filter_order) {
			LOG_WARNING("%s: Invalid custom filter order: '%s'. Must be an integer between 1 and %d.",
			            name.c_str(),
			            order_pref.c_str(),
			            max_filter_order);
			return false;
		}

		int cutoff_freq_hz;
		if (!sscanf(cutoff_freq_pref.c_str(), "%d", &cutoff_freq_hz) ||
		    cutoff_freq_hz <= 0) {
			LOG_WARNING("%s: Invalid custom filter cutoff frequency: '%s'. Must be a positive number.",
			            name.c_str(),
			            cutoff_freq_pref.c_str());
			return false;
		}

		const auto max_cutoff_freq_hz = (do_zoh_upsample ? zoh_upsampler.target_freq
		                                                 : sample_rate) / 2 - 1;

		if (cutoff_freq_hz > max_cutoff_freq_hz) {
			LOG_WARNING("%s: Invalid custom filter cutoff frequency: '%s'. "
			            "Must be lower than half the sample rate; clamping to %d Hz.",
			            name.c_str(),
			            cutoff_freq_pref.c_str(),
			            max_cutoff_freq_hz);

			cutoff_freq_hz = max_cutoff_freq_hz;
		}

		if (type_pref == "lpf") {
			ConfigureLowPassFilter(order, cutoff_freq_hz);
			SetLowPassFilter(FilterState::On);
		} else if (type_pref == "hpf") {
			ConfigureHighPassFilter(order, cutoff_freq_hz);
			SetHighPassFilter(FilterState::On);
		} else {
			LOG_WARNING("%s: Invalid custom filter type: '%s'. Must be either 'hpf' or 'lpf'.",
			            name.c_str(),
			            type_pref.c_str());
			return false;
		}
		return true;
	};

	if (single_filter) {
		auto i = 0;

		const auto filter_type        = parts[i++];
		const auto filter_order       = parts[i++];
		const auto filter_cutoff_freq = parts[i++];

		return set_filter(filter_type, filter_order, filter_cutoff_freq);
	} else {
		auto i = 0;

		const auto filter1_type        = parts[i++];
		const auto filter1_order       = parts[i++];
		const auto filter1_cutoff_freq = parts[i++];

		const auto filter2_type        = parts[i++];
		const auto filter2_order       = parts[i++];
		const auto filter2_cutoff_freq = parts[i++];

		if (filter1_type == filter2_type) {
			LOG_WARNING("%s: Invalid custom filter definition: '%s'. "
			            "The two filters must be of different types.",
			            name.c_str(),
			            filter_prefs.data());
			return false;
		}

		if (!set_filter(filter1_type, filter1_order, filter1_cutoff_freq))
			return false;

		return set_filter(filter2_type, filter2_order, filter2_cutoff_freq);
	}
}

void MixerChannel::SetZeroOrderHoldUpsamplerTargetFreq(const uint16_t target_freq)
{
	// TODO make sure that the ZOH target frequency cannot be set after the
	// filter have been configured
	zoh_upsampler.target_freq = target_freq;

	ConfigureResampler();
}

void MixerChannel::InitZohUpsamplerState()
{
	zoh_upsampler.step = std::min(static_cast<float>(sample_rate) /
	                                      zoh_upsampler.target_freq,
	                              1.0f);
	zoh_upsampler.pos  = 0.0f;
}

void MixerChannel::InitLerpUpsamplerState()
{
	lerp_upsampler.step       = std::min(static_cast<float>(sample_rate) /
                                               mixer.sample_rate.load(),
                                       1.0f);
	lerp_upsampler.pos        = 0.0f;
	lerp_upsampler.last_frame = {0.0f, 0.0f};
}

void MixerChannel::SetResampleMethod(const ResampleMethod method)
{
	resample_method = method;

	ConfigureResampler();
}

void MixerChannel::SetCrossfeedStrength(const float strength)
{
	assert(strength >= 0.0f);
	assert(strength <= 1.0f);

	do_crossfeed = (HasFeature(ChannelFeature::Stereo) && strength > 0.0f);

	if (!do_crossfeed) {
		// DEBUG_LOG_MSG("%s: Crossfeed is off", name.c_str());
		crossfeed.strength = 0.0f;
		return;
	}

	crossfeed.strength = strength;

	// map [0, 1] range to [0.5, 0]
	auto p = (1.0f - strength) / 2.0f;
	constexpr auto center = 0.5f;
	crossfeed.pan_left = center - p;
	crossfeed.pan_right = center + p;

	// DEBUG_LOG_MSG("%s: Crossfeed is on (strength: %.3f)",
	//               name.c_str(),
	//               static_cast<double>(crossfeed.strength));
}

float MixerChannel::GetCrossfeedStrength()
{
	return crossfeed.strength;
}

void MixerChannel::SetReverbLevel(const float level)
{
	constexpr auto level_min    = 0.0f;
	constexpr auto level_max    = 1.0f;
	constexpr auto level_min_db = -40.0f;
	constexpr auto level_max_db = 0.0f;

	assert(level >= level_min);
	assert(level <= level_max);

	do_reverb_send = (HasFeature(ChannelFeature::ReverbSend) && level > level_min);

	if (!do_reverb_send) {
		// DEBUG_LOG_MSG("%s: Reverb send is off", name.c_str());
		reverb.level = level_min;
		reverb.send_gain = level_min_db;
		return;
	}

	reverb.level = level;

	const auto level_db = remap(level_min, level_max, level_min_db, level_max_db, level);

	reverb.send_gain = static_cast<float>(decibel_to_gain(level_db));

	// DEBUG_LOG_MSG("%s: SetReverbLevel: level: %4.2f, level_db: %6.2f, gain: %4.2f",
	//               name.c_str(),
	//               level,
	//               level_db,
	//               reverb.send_gain);
}

float MixerChannel::GetReverbLevel()
{
	return reverb.level;
}

void MixerChannel::SetChorusLevel(const float level)
{
	constexpr auto level_min    = 0.0f;
	constexpr auto level_max    = 1.0f;
	constexpr auto level_min_db = -24.0f;
	constexpr auto level_max_db = 0.0f;

	assert(level >= level_min);
	assert(level <= level_max);

	do_chorus_send = (HasFeature(ChannelFeature::ChorusSend) && level > level_min);

	if (!do_chorus_send) {
		// DEBUG_LOG_MSG("%s: Chorus send is off", name.c_str());
		chorus.level = level_min;
		chorus.send_gain = level_min_db;
		return;
	}

	chorus.level = level;

	const auto level_db = remap(level_min, level_max, level_min_db, level_max_db, level);

	chorus.send_gain = static_cast<float>(decibel_to_gain(level_db));

	//DEBUG_LOG_MSG("%s: SetChorusLevel: level: %4.2f, level_db: %6.2f, gain: %4.2f",
	//              name.c_str(),
	//              level,
	//              level_db,
	//              chorus.send_gain);
}

float MixerChannel::GetChorusLevel()
{
	return chorus.level;
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
int16_t lut_u8to16[UINT8_MAX + 1] = {};
constexpr int16_t *lut_s8to16 = lut_u8to16 + 128;

constexpr void fill_8to16_lut()
{
	for (int i = 0; i <= UINT8_MAX; ++i)
		lut_u8to16[i] = u8to16(i);
}

template <class Type, bool stereo, bool signeddata, bool nativeorder>
AudioFrame MixerChannel::ConvertNextFrame(const Type *data, const work_index_t pos)
{
	AudioFrame frame = {};

	if (sizeof(Type) == 1) {
		// Integer types
		// unsigned 8-bit
		if (!signeddata) {
			if (stereo) {
				frame[0] = lut_u8to16[static_cast<uint8_t>(data[pos * 2 + 0])];
				frame[1] = lut_u8to16[static_cast<uint8_t>(data[pos * 2 + 1])];
			} else {
				frame[0] = lut_u8to16[static_cast<uint8_t>(data[pos])];
			}
		}
		// signed 8-bit
		else {
			if (stereo) {
				frame[0] = lut_s8to16[static_cast<int8_t>(data[pos * 2 + 0])];
				frame[1] = lut_s8to16[static_cast<int8_t>(data[pos * 2 + 1])];
			} else {
				frame[0] = lut_s8to16[static_cast<int8_t>(data[pos])];
			}
		}
	} else {
		// 16-bit and 32-bit both contain 16-bit data internally
		if (signeddata) {
			if (stereo) {
				if (nativeorder) {
					frame[0] = static_cast<float>(data[pos * 2 + 0]);
					frame[1] = static_cast<float>(data[pos * 2 + 1]);
				} else {
					if (sizeof(Type) == 2) {
						frame[0] = (int16_t)host_readw(
						        (const HostPt)&data[pos * 2 + 0]);
						frame[1] = (int16_t)host_readw(
						        (const HostPt)&data[pos * 2 + 1]);
					} else {
						frame[0] = static_cast<float>((int32_t)host_readd(
						        (const HostPt)&data[pos * 2 + 0]));
						frame[1] = static_cast<float>((int32_t)host_readd(
						        (const HostPt)&data[pos * 2 + 1]));
					}
				}
			} else { // mono
				if (nativeorder) {
					frame[0] = static_cast<float>(data[pos]);
				} else {
					if (sizeof(Type) == 2) {
						frame[0] = (int16_t)host_readw(
						        (const HostPt)&data[pos]);
					} else {
						frame[0] = static_cast<float>(
						        (int32_t)host_readd(
						                (const HostPt)&data[pos]));
					}
				}
			}
		} else { // unsigned
			const auto offs = 32768;
			if (stereo) {
				if (nativeorder) {
					frame[0] = static_cast<float>(
					        static_cast<int>(data[pos * 2 + 0]) -
					        offs);
					frame[1] = static_cast<float>(
					        static_cast<int>(data[pos * 2 + 1]) -
					        offs);
				} else {
					if (sizeof(Type) == 2) {
						frame[0] = static_cast<float>(
						        static_cast<int>(host_readw(
						                (const HostPt)&data[pos * 2 + 0])) -
						        offs);
						frame[1] = static_cast<float>(
						        static_cast<int>(host_readw(
						                (const HostPt)&data[pos * 2 + 1])) -
						        offs);
					} else {
						frame[0] = static_cast<float>(
						        static_cast<int>(host_readd(
						                (const HostPt)&data[pos * 2 + 0])) -
						        offs);
						frame[1] = static_cast<float>(
						        static_cast<int>(host_readd(
						                (const HostPt)&data[pos * 2 + 1])) -
						        offs);
					}
				}
			} else { // mono
				if (nativeorder) {
					frame[0] = static_cast<float>(
					        static_cast<int>(data[pos]) - offs);
				} else {
					if (sizeof(Type) == 2) {
						frame[0] = static_cast<float>(
						        static_cast<int>(host_readw(
						                (const HostPt)&data[pos])) -
						        offs);
					} else {
						frame[0] = static_cast<float>(
						        static_cast<int>(host_readd(
						                (const HostPt)&data[pos])) -
						        offs);
					}
				}
			}
		}
	}
	return frame;
}

// Converts sample stream to floats, performs output channel mappings, removes
// clicks, and optionally performs zero-order-hold-upsampling.
template <class Type, bool stereo, bool signeddata, bool nativeorder>
void MixerChannel::ConvertSamples(const Type *data, const uint16_t frames,
                                  std::vector<float> &out)
{
	// read-only aliases to avoid repeated dereferencing and to inform the
	// compiler their values don't change
	const auto mapped_output_left  = output_map.left;
	const auto mapped_output_right = output_map.right;

	const auto mapped_channel_left  = channel_map.left;
	const auto mapped_channel_right = channel_map.right;

	work_index_t pos = 0;
	std::array<float, 2> out_frame;

	out.resize(0);

	while (pos < frames) {
		prev_frame = next_frame;

		if (std::is_same<Type, float>::value) {
			if (stereo) {
				next_frame.left  = static_cast<float>(data[pos * 2 + 0]);
				next_frame.right = static_cast<float>(data[pos * 2 + 1]);
			} else {
				next_frame.left  = static_cast<float>(data[pos]);
			}
		} else {
			next_frame = ConvertNextFrame<Type, stereo, signeddata, nativeorder>(
			        data, pos);
		}

		AudioFrame frame_with_gain = {
		        prev_frame[mapped_channel_left] * combined_volume_scalar.left,
		        (stereo ? prev_frame[mapped_channel_right]
		                : prev_frame[mapped_channel_left]) *
		                combined_volume_scalar.right};

		// Process initial samples through an expanding envelope to
		// prevent severe clicks and pops. Becomes a no-op when done.
		envelope.Process(stereo, frame_with_gain);

		out_frame = {0.0f, 0.0f};
		out_frame[mapped_output_left] += frame_with_gain.left;
		out_frame[mapped_output_right] += frame_with_gain.right;

		out.emplace_back(out_frame[0]);
		out.emplace_back(out_frame[1]);

		if (do_zoh_upsample) {
			zoh_upsampler.pos += zoh_upsampler.step;
			if (zoh_upsampler.pos > 1.0f) {
				zoh_upsampler.pos -= 1.0f;
				++pos;
			}
		} else {
			++pos;
		}
	}
}

spx_uint32_t estimate_max_out_frames(SpeexResamplerState *resampler_state,
                                     const spx_uint32_t in_frames)
{
	assert(resampler_state);
	assert(in_frames);

	spx_uint32_t ratio_num = 0;
	spx_uint32_t ratio_den = 0;
	speex_resampler_get_ratio(resampler_state, &ratio_num, &ratio_den);
	assert(ratio_num && ratio_den);

	return ceil_udivide(in_frames * ratio_den, ratio_num);
}

AudioFrame MixerChannel::ApplyCrossfeed(const AudioFrame &frame) const
{
	// Pan mono sample using -6dB linear pan law in the stereo field
	// pan: 0.0 = left, 0.5 = center, 1.0 = right
	auto pan = [](const float sample, const float pan) -> AudioFrame {
		return {(1.0f - pan) * sample, pan * sample};
	};
	const auto a = pan(frame.left, crossfeed.pan_left);
	const auto b = pan(frame.right, crossfeed.pan_right);
	return {a.left + b.left, a.right + b.right};
}

MixerChannel::Sleeper::Sleeper(MixerChannel &c) : channel(c) {}

// Records if samples had a magnitude great than 1. This is a one-way street;
// once "had_noise" is tripped, it can only be reset after waking up.
void MixerChannel::Sleeper::Listen(const AudioFrame &frame)
{
	if (had_noise)
		return;

	constexpr auto silent_threshold = 1.0f;

	had_noise = fabsf(frame.left) > silent_threshold ||
	            fabsf(frame.right) > silent_threshold;
}

void MixerChannel::Sleeper::MaybeSleep()
{
	constexpr auto consider_sleeping_after_ms = 250;

	// Not enough time has passed.. try again later
	if (GetTicksSince(woken_at_ms) < consider_sleeping_after_ms)
		return;

	// Stay awake if it's been noisy, otherwise we can sleep
	if (had_noise) {
		WakeUp();
	} else {
		channel.Enable(false);
		// LOG_INFO("MIXER: %s fell asleep", channel.name.c_str());
	}
}

// Returns true when actually awoken otherwise false if already awake.
bool MixerChannel::Sleeper::WakeUp()
{
	// Always reset for another round of awakeness
	woken_at_ms = GetTicks();
	had_noise   = false;

	const auto was_sleeping = !channel.is_enabled;
	if (was_sleeping) {
		channel.Enable(true);
		// LOG_INFO("MIXER: %s woke up", channel.name.c_str());
	}
	return was_sleeping;
}

// Audio devices that use the sleep feature need to wake up the channel whenever
// they might prepare new samples for it. Typically this is on IO port
// writes into the card.
bool MixerChannel::WakeUp()
{
	assert(do_sleep);
	return sleeper.WakeUp();
}

template <class Type, bool stereo, bool signeddata, bool nativeorder>
void MixerChannel::AddSamples(const uint16_t frames, const Type *data)
{
	assert(frames > 0);

	last_samples_were_stereo = stereo;

	auto &convert_out = do_resample ? mixer.resample_temp : mixer.resample_out;
	ConvertSamples<Type, stereo, signeddata, nativeorder>(data, frames, convert_out);

	if (do_resample) {
		switch (resample_method) {
		case ResampleMethod::LinearInterpolation: {
			auto &s = lerp_upsampler;

			auto in_pos = mixer.resample_temp.begin();
			auto &out   = mixer.resample_out;
			out.resize(0);

			while (in_pos != mixer.resample_temp.end()) {
				AudioFrame curr_frame = {*in_pos, *(in_pos + 1)};

				const auto out_left = lerp(s.last_frame.left,
				                           curr_frame.left,
				                           s.pos);

				const auto out_right = lerp(s.last_frame.right,
				                            curr_frame.right,
				                            s.pos);

				out.emplace_back(out_left);
				out.emplace_back(out_right);

				s.pos += s.step;

				//DEBUG_LOG_MSG("%s: AddSamples last %.1f:%.1f curr %.1f:%.1f"
				//              " -> out %.1f:%.1f, pos=%.2f, step=%.2f",
				//              name.c_str(),
				//              s.last_frame.left,
				//              s.last_frame.right,
				//              curr_frame.left,
				//              curr_frame.right,
				//              out_left,
				//              out_right,
				//              s.pos,
				//              s.step);

				if (s.pos > 1.0f) {
					s.pos -= 1.0f;
					s.last_frame = curr_frame;

					// Move to the next input frame
					in_pos += 2;
				}
			}
		} break;

		case ResampleMethod::ZeroOrderHoldAndResample:
			[[fallthrough]];
			// Zero-order-hold upsampling is performed in
			// ConvertSamples for efficiency reasons to reduce the
			// number of temporary buffers

		case ResampleMethod::Resample: {
			auto in_frames = check_cast<uint32_t>(
			                         mixer.resample_temp.size()) /
			                 2u;

			auto out_frames = estimate_max_out_frames(
			        speex_resampler.state, in_frames);

			mixer.resample_out.resize(out_frames * 2);

			speex_resampler_process_interleaved_float(
			        speex_resampler.state,
			        mixer.resample_temp.data(),
			        &in_frames,
			        mixer.resample_out.data(),
			        &out_frames);

			// out_frames now contains the actual number of
			// resampled frames, ensure the number of output frames
			// is within the logical size.
			assert(out_frames <= mixer.resample_out.size() / 2);
			mixer.resample_out.resize(out_frames * 2); // only shrinks
		} break;
		}
	}

	MIXER_LockAudioDevice();

	// Optionally filter, apply crossfeed, then mix the results to the
	// master output
	const auto out_frames = static_cast<int>(mixer.resample_out.size()) / 2;

	auto pos    = mixer.resample_out.begin();
	auto mixpos = check_cast<work_index_t>(mixer.pos + frames_done);

	while (pos != mixer.resample_out.end()) {
		mixpos &= MIXER_BUFMASK;

		AudioFrame frame = {*pos++, *pos++};

		if (do_highpass_filter) {
			frame.left = filters.highpass.hpf[0].filter(frame.left);
			frame.right = filters.highpass.hpf[1].filter(frame.right);
		}
		if (do_lowpass_filter) {
			frame.left  = filters.lowpass.lpf[0].filter(frame.left);
			frame.right = filters.lowpass.lpf[1].filter(frame.right);
		}
		if (do_crossfeed)
			frame = ApplyCrossfeed(frame);

		if (do_reverb_send) {
			// Mix samples to the reverb aux buffer, scaled by the
			// reverb send volume
			mixer.aux_reverb[mixpos][0] += frame.left * reverb.send_gain;
			mixer.aux_reverb[mixpos][1] += frame.right * reverb.send_gain;
		}
		if (do_chorus_send) {
			// Mix samples to the chorus aux buffer, scaled by the
			// chorus send volume
			mixer.aux_chorus[mixpos][0] += frame.left * chorus.send_gain;
			mixer.aux_chorus[mixpos][1] += frame.right * chorus.send_gain;
		}

		if (do_sleep)
			sleeper.Listen(frame);

		// Mix samples to the master output
		mixer.work[mixpos][0] += frame.left;
		mixer.work[mixpos][1] += frame.right;

		++mixpos;
	}
	frames_done += out_frames;

	MIXER_UnlockAudioDevice();
}

void MixerChannel::AddStretched(const uint16_t len, int16_t *data)
{
	MIXER_LockAudioDevice();

	if (frames_done >= frames_needed) {
		LOG_MSG("Can't add, buffer full");
		MIXER_UnlockAudioDevice();
		return;
	}
	// Target samples this inputs gets stretched into
	auto frames_remaining = frames_needed - frames_done;
	auto index            = 0;
	auto index_add        = (len << FREQ_SHIFT) / frames_remaining;
	auto mixpos = check_cast<work_index_t>(mixer.pos + frames_done);
	auto pos    = 0;

	// read-only aliases to avoid dereferencing and inform compiler their
	// values don't change
	const auto mapped_output_left  = output_map.left;
	const auto mapped_output_right = output_map.right;

	while (frames_remaining--) {
		const auto new_pos = index >> FREQ_SHIFT;
		if (pos != new_pos) {
			pos = new_pos;
			// Forward the previous sample
			prev_frame.left = data[0];
			data++;
		}
		assert(prev_frame.left <= INT16_MAX);
		assert(prev_frame.left >= INT16_MIN);
		const auto diff = data[0] - static_cast<int16_t>(prev_frame.left);

		const auto diff_mul = index & FREQ_MASK;
		index += index_add;
		mixpos &= MIXER_BUFMASK;

		const auto sample = prev_frame.left +
		                    ((diff * diff_mul) >> FREQ_SHIFT);

		const AudioFrame frame_with_gain = {
		        sample * combined_volume_scalar.left,
		        sample * combined_volume_scalar.right};
		if (do_sleep) {
			sleeper.Listen(frame_with_gain);
		}

		mixer.work[mixpos][mapped_output_left] += frame_with_gain.left;
		mixer.work[mixpos][mapped_output_right] += frame_with_gain.right;

		mixpos++;
	}

	frames_done = frames_needed;

	MIXER_UnlockAudioDevice();
}

void MixerChannel::AddSamples_m8(const uint16_t len, const uint8_t *data)
{
	AddSamples<uint8_t, false, false, true>(len, data);
}
void MixerChannel::AddSamples_s8(const uint16_t len, const uint8_t *data)
{
	AddSamples<uint8_t, true, false, true>(len, data);
}
void MixerChannel::AddSamples_m8s(const uint16_t len, const int8_t *data)
{
	AddSamples<int8_t, false, true, true>(len, data);
}
void MixerChannel::AddSamples_s8s(const uint16_t len, const int8_t *data)
{
	AddSamples<int8_t, true, true, true>(len, data);
}
void MixerChannel::AddSamples_m16(const uint16_t len, const int16_t *data)
{
	AddSamples<int16_t, false, true, true>(len, data);
}
void MixerChannel::AddSamples_s16(const uint16_t len, const int16_t *data)
{
	AddSamples<int16_t, true, true, true>(len, data);
}
void MixerChannel::AddSamples_m16u(const uint16_t len, const uint16_t *data)
{
	AddSamples<uint16_t, false, false, true>(len, data);
}
void MixerChannel::AddSamples_s16u(const uint16_t len, const uint16_t *data)
{
	AddSamples<uint16_t, true, false, true>(len, data);
}
void MixerChannel::AddSamples_m32(const uint16_t len, const int32_t *data)
{
	AddSamples<int32_t, false, true, true>(len, data);
}
void MixerChannel::AddSamples_s32(const uint16_t len, const int32_t *data)
{
	AddSamples<int32_t, true, true, true>(len, data);
}
void MixerChannel::AddSamples_mfloat(const uint16_t len, const float *data)
{
	AddSamples<float, false, true, true>(len, data);
}
void MixerChannel::AddSamples_sfloat(const uint16_t len, const float *data)
{
	AddSamples<float, true, true, true>(len, data);
}
void MixerChannel::AddSamples_m16_nonnative(const uint16_t len, const int16_t *data)
{
	AddSamples<int16_t, false, true, false>(len, data);
}
void MixerChannel::AddSamples_s16_nonnative(const uint16_t len, const int16_t *data)
{
	AddSamples<int16_t, true, true, false>(len, data);
}
void MixerChannel::AddSamples_m16u_nonnative(const uint16_t len, const uint16_t *data)
{
	AddSamples<uint16_t, false, false, false>(len, data);
}
void MixerChannel::AddSamples_s16u_nonnative(const uint16_t len, const uint16_t *data)
{
	AddSamples<uint16_t, true, false, false>(len, data);
}
void MixerChannel::AddSamples_m32_nonnative(const uint16_t len, const int32_t *data)
{
	AddSamples<int32_t, false, true, false>(len, data);
}
void MixerChannel::AddSamples_s32_nonnative(const uint16_t len, const int32_t *data)
{
	AddSamples<int32_t, true, true, false>(len, data);
}

void MixerChannel::FillUp()
{
	if (!is_enabled || frames_done < mixer.frames_done)
		return;
	const auto index = PIC_TickIndex();
	MIXER_LockAudioDevice();
	Mix(check_cast<uint16_t>(static_cast<int64_t>(index * mixer.frames_needed)));
	MIXER_UnlockAudioDevice();
}

std::string MixerChannel::DescribeLineout() const
{
	if (!HasFeature(ChannelFeature::Stereo))
		return MSG_Get("SHELL_CMD_MIXER_CHANNEL_MONO");
	if (output_map == STEREO)
		return MSG_Get("SHELL_CMD_MIXER_CHANNEL_STEREO");
	if (output_map == REVERSE)
		return MSG_Get("SHELL_CMD_MIXER_CHANNEL_REVERSE");

	// Output_map is programmtically set (not directly assigned from user
	// data), so we can assert.
	assertm(false, "Unknown lineout mode");
	return "unknown";
}

bool MixerChannel::ChangeLineoutMap(std::string choice)
{
	if (!HasFeature(ChannelFeature::Stereo))
		return false;

	lowcase(choice);

	if (choice == "stereo")
		output_map = STEREO;
	else if (choice == "reverse")
		output_map = REVERSE;
	else
		return false;

	return true;
}

MixerChannel::~MixerChannel()
{
	if (speex_resampler.state) {
		speex_resampler_destroy(speex_resampler.state);
		speex_resampler.state = nullptr;
	}
}

extern bool ticksLocked;
static inline bool Mixer_irq_important()
{
	/* In some states correct timing of the irqs is more important than
	 * non stuttering audo */
	return (ticksLocked ||
	        CAPTURE_IsCapturingAudio() || CAPTURE_IsCapturingVideo());
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

// Mix a certain amount of new sample frames
static void MIXER_MixData(const int frames_requested)
{
	constexpr auto capture_buf_frames = 1024;

	const auto frames_added = check_cast<work_index_t>(
	        std::min(frames_requested - mixer.frames_done, capture_buf_frames));

	const auto start_pos = check_cast<work_index_t>(
	        (mixer.pos + mixer.frames_done) & MIXER_BUFMASK);

	// Render all channels and accumulate results in the master mixbuffer
	for (auto &it : mixer.channels)
		it.second->Mix(frames_requested);

	if (mixer.do_reverb) {
		// Apply reverb effect to the reverb aux buffer, then mix the
		// results to the master output
		auto pos = start_pos;

		for (work_index_t i = 0; i < frames_added; ++i) {
			AudioFrame frame = {mixer.aux_reverb[pos][0],
			                    mixer.aux_reverb[pos][1]};

			// High-pass filter the reverb input
			for (auto ch = 0; ch < 2; ++ch)
				frame[ch] = mixer.reverb.highpass_filter[ch].filter(
				        frame[ch]);

			// MVerb operates on two non-interleaved sample streams
			static float in_left[1]     = {};
			static float in_right[1]    = {};
			static float *reverb_buf[2] = {in_left, in_right};

			in_left[0]  = frame.left;
			in_right[0] = frame.right;

			const auto in         = reverb_buf;
			auto out              = reverb_buf;
			constexpr auto frames = 1;
			mixer.reverb.mverb.process(in, out, frames);

			mixer.work[pos][0] += reverb_buf[0][0];
			mixer.work[pos][1] += reverb_buf[1][0];

			pos = (pos + 1) & MIXER_BUFMASK;
		}
	}

	if (mixer.do_chorus) {
		// Apply chorus effect to the chorus aux buffer, then mix the
		// results to the master output
		auto pos = start_pos;

		for (work_index_t i = 0; i < frames_added; ++i) {
			AudioFrame frame = {mixer.aux_chorus[pos][0],
			                    mixer.aux_chorus[pos][1]};

			mixer.chorus.chorus_engine.process(&frame.left, &frame.right);

			mixer.work[pos][0] += frame.left;
			mixer.work[pos][1] += frame.right;

			pos = (pos + 1) & MIXER_BUFMASK;
		}
	}

	// Apply high-pass filter to the master output
	auto pos = start_pos;

	for (work_index_t i = 0; i < frames_added; ++i) {
		for (auto ch = 0; ch < 2; ++ch) {
			mixer.work[pos][ch] = mixer.highpass_filter[ch].filter(
			        mixer.work[pos][ch]);
		}
		pos = (pos + 1) & MIXER_BUFMASK;
	}

	if (mixer.do_compressor) {
		// Apply compressor to the master output as the very last step
		pos = start_pos;

		for (work_index_t i = 0; i < frames_added; ++i) {
			AudioFrame frame = {mixer.work[pos][0], mixer.work[pos][1]};

			frame = mixer.compressor.Process(frame);

			mixer.work[pos][0] = frame.left;
			mixer.work[pos][1] = frame.right;

			pos = (pos + 1) & MIXER_BUFMASK;
		}
	}

	// Capture audio output if requested
	if (CAPTURE_IsCapturingAudio() || CAPTURE_IsCapturingVideo()) {
		int16_t out[capture_buf_frames][2];
		auto pos = start_pos;

		for (work_index_t i = 0; i < frames_added; i++) {
			const auto left = static_cast<uint16_t>(
			        MIXER_CLIP(static_cast<int>(mixer.work[pos][0])));

			const auto right = static_cast<uint16_t>(
			        MIXER_CLIP(static_cast<int>(mixer.work[pos][1])));

			out[i][0] = static_cast<int16_t>(host_to_le16(left));
			out[i][1] = static_cast<int16_t>(host_to_le16(right));

			pos = (pos + 1) & MIXER_BUFMASK;
		}

		CAPTURE_AddAudioData(mixer.sample_rate,
		                     frames_added,
		                     reinterpret_cast<int16_t*>(out));
	}

	// Reset the the tick_add for constant speed
	if (Mixer_irq_important())
		mixer.tick_add = calc_tickadd(mixer.sample_rate);

	mixer.frames_done = frames_requested;
}

static void MIXER_Mix()
{
	MIXER_LockAudioDevice();
	MIXER_MixData(mixer.frames_needed);
	mixer.tick_counter += mixer.tick_add;
	mixer.frames_needed += mixer.tick_counter >> TICK_SHIFT;
	mixer.tick_counter &= TICK_MASK;
	MIXER_UnlockAudioDevice();
}

static void MIXER_ReduceChannelsDoneCounts(const int at_most)
{
	for (auto &it : mixer.channels)
		it.second->frames_done -= std::min(it.second->frames_done.load(),
		                                   at_most);
}

static void MIXER_Mix_NoSound()
{
	MIXER_LockAudioDevice();
	MIXER_MixData(mixer.frames_needed);

	/* Clear piece we've just generated */
	for (auto i = 0; i < mixer.frames_needed; ++i) {
		mixer.work[mixer.pos][0] = 0;
		mixer.work[mixer.pos][1] = 0;

		mixer.pos = (mixer.pos + 1) & MIXER_BUFMASK;
	}

	MIXER_ReduceChannelsDoneCounts(mixer.frames_needed);

	/* Set values for next tick */
	mixer.tick_counter += mixer.tick_add;
	mixer.frames_needed = (mixer.tick_counter >> TICK_SHIFT);
	mixer.tick_counter &= TICK_MASK;
	mixer.frames_done = 0;
	MIXER_UnlockAudioDevice();
}

#define INDEX_SHIFT_LOCAL 14

static void SDLCALL MIXER_CallBack([[maybe_unused]] void *userdata,
                                   Uint8 *stream, int len)
{
	ZoneScoped;
	memset(stream, 0, len);

	auto frames_requested = len / mixer_frame_size;
	auto output           = reinterpret_cast<int16_t *>(stream);
	auto reduce_frames    = 0;
	work_index_t pos      = 0;

	// Local resampling counter to manipulate the data when sending it off
	// to the callback
	auto index_add = (1 << INDEX_SHIFT_LOCAL);
	auto index     = (index_add % frames_requested) ? frames_requested : 0;

	/* Enough room in the buffer ? */
	if (mixer.frames_done < frames_requested) {
		//		LOG_WARNING("Full underrun requested %d, have
		//%d, min %d", frames_requested, mixer.frames_done.load(),
		// mixer.min_frames_needed.load());
		if ((frames_requested - mixer.frames_done) >
		    (frames_requested >> 7)) // Max 1 percent
		                             // stretch.
			return;
		reduce_frames = mixer.frames_done;
		index_add = (reduce_frames << INDEX_SHIFT_LOCAL) / frames_requested;
		mixer.tick_add = calc_tickadd(mixer.sample_rate +
		                              mixer.min_frames_needed);

	} else if (mixer.frames_done < mixer.max_frames_needed) {
		auto frames_remaining = mixer.frames_done - frames_requested;

		if (frames_remaining < mixer.min_frames_needed) {
			if (!Mixer_irq_important()) {
				auto frames_needed = mixer.frames_needed -
				                     frames_requested;
				auto diff = (mixer.min_frames_needed > frames_needed
				                     ? mixer.min_frames_needed.load()
				                     : frames_needed) -
				            frames_remaining;

				mixer.tick_add = calc_tickadd(mixer.sample_rate +
				                              (diff * 3));
				frames_remaining = 0; // No stretching as we
				                      // compensate with the
				                      // tick_add value
			} else {
				frames_remaining = (mixer.min_frames_needed -
				                    frames_remaining);
				frames_remaining = 1 + (2 * frames_remaining) /
				                               mixer.min_frames_needed; // frames_remaining=1,2,3
			}
			//			LOG_WARNING("needed underrun
			// requested %d, have %d, min %d, frames_remaining %d",
			// frames_requested, mixer.frames_done.load(),
			// mixer.min_frames_needed.load(), frames_remaining);
			reduce_frames = frames_requested - frames_remaining;
			index_add     = (reduce_frames << INDEX_SHIFT_LOCAL) /
			            frames_requested;
		} else {
			reduce_frames = frames_requested;
			index_add     = (1 << INDEX_SHIFT_LOCAL);
			//			LOG_MSG("regular run requested
			//%d, have %d, min %d, frames_remaining %d",
			// frames_requested, mixer.frames_done.load(),
			// mixer.min_frames_needed.load(), frames_remaining);

			/* Mixer tick value being updated:
			 * 3 cases:
			 * 1) A lot too high. >division by 5. but maxed by 2*
			 * min to prevent too fast drops. 2) A little too high >
			 * division by 8 3) A little to nothing above the
			 * min_needed buffer > go to default value
			 */
			int diff = frames_remaining - mixer.min_frames_needed;

			if (diff > (mixer.min_frames_needed << 1))
				diff = mixer.min_frames_needed << 1;

			if (diff > (mixer.min_frames_needed >> 1))
				mixer.tick_add = calc_tickadd(mixer.sample_rate -
				                              (diff / 5));

			else if (diff > (mixer.min_frames_needed >> 2))
				mixer.tick_add = calc_tickadd(mixer.sample_rate -
				                              (diff >> 3));
			else
				mixer.tick_add = calc_tickadd(mixer.sample_rate);
		}
	} else {
		/* There is way too much data in the buffer */
		//		LOG_WARNING("overflow run requested %u, have %u,
		// min %u", frames_requested, mixer.frames_done.load(),
		// mixer.min_frames_needed.load());
		if (mixer.frames_done > MIXER_BUFSIZE)
			index_add = MIXER_BUFSIZE - 2 * mixer.min_frames_needed;
		else
			index_add = mixer.frames_done - 2 * mixer.min_frames_needed;

		index_add = (index_add << INDEX_SHIFT_LOCAL) / frames_requested;
		reduce_frames = mixer.frames_done - 2 * mixer.min_frames_needed;

		mixer.tick_add = calc_tickadd(mixer.sample_rate -
		                              (mixer.min_frames_needed / 5));
	}

	MIXER_ReduceChannelsDoneCounts(reduce_frames);

	// Reset mixer.tick_add when irqs are important
	if (Mixer_irq_important())
		mixer.tick_add = calc_tickadd(mixer.sample_rate);

	mixer.frames_done -= reduce_frames;
	mixer.frames_needed -= reduce_frames;

	pos       = mixer.pos;
	mixer.pos = (mixer.pos + reduce_frames) & MIXER_BUFMASK;

	if (frames_requested != reduce_frames) {
		while (frames_requested--) {
			const auto i = check_cast<work_index_t>(
			        (pos + (index >> INDEX_SHIFT_LOCAL)) & MIXER_BUFMASK);
			index += index_add;

			*output++ = MIXER_CLIP(static_cast<int>(mixer.work[i][0]));
			*output++ = MIXER_CLIP(static_cast<int>(mixer.work[i][1]));
		}
		// Clean the used buffers
		while (reduce_frames--) {
			pos &= MIXER_BUFMASK;

			mixer.work[pos][0] = 0.0f;
			mixer.work[pos][1] = 0.0f;

			mixer.aux_reverb[pos][0] = 0.0f;
			mixer.aux_reverb[pos][1] = 0.0f;

			mixer.aux_chorus[pos][0] = 0.0f;
			mixer.aux_chorus[pos][1] = 0.0f;

			++pos;
		}
	} else {
		while (reduce_frames--) {
			pos &= MIXER_BUFMASK;

			*output++ = MIXER_CLIP(static_cast<int>(mixer.work[pos][0]));
			*output++ = MIXER_CLIP(static_cast<int>(mixer.work[pos][1]));

			mixer.work[pos][0] = 0.0f;
			mixer.work[pos][1] = 0.0f;

			mixer.aux_reverb[pos][0] = 0.0f;
			mixer.aux_reverb[pos][1] = 0.0f;

			mixer.aux_chorus[pos][0] = 0.0f;
			mixer.aux_chorus[pos][1] = 0.0f;

			++pos;
		}
	}
}

#undef INDEX_SHIFT_LOCAL

static void MIXER_Stop([[maybe_unused]] Section *sec)
{}

using channels_set_t = std::set<mixer_channel_t>;
static channels_set_t set_of_channels()
{
	channels_set_t channels = {};
	for (const auto &it : mixer.channels)
		channels.emplace(it.second);
	return channels;
}

// Parse the volume in string form, either in stereo or mono format,
// and possibly in decibel format, which is prefixed with a 'd'.
static std::optional<AudioFrame> parse_volume(const std::string &s)
{
	auto to_volume = [](const std::string &s) -> std::optional<float> {
		// try parsing the volume from a percent value
		constexpr auto min_percent = 0.0f;
		constexpr auto max_percent = 9999.0f;
		if (const auto p = parse_value(s, min_percent, max_percent); p)
			return percentage_to_gain(*p);

		// try parsing the volume from a decibel value
		constexpr auto min_db = -40.00f;
		constexpr auto max_db = 39.999f;
		constexpr auto decibel_prefix = 'd';
		if (const auto d = parse_prefixed_value(decibel_prefix, s, min_db, max_db); d)
			return decibel_to_gain(*d);

		return {};
	};
	// single volume value
	auto parts = split(s, ':');
	if (parts.size() == 1) {
		if (const auto v = to_volume(parts[0]); v) {
			return AudioFrame(*v, *v);
		}
	}
	// stereo volume value
	else if (parts.size() == 2) {
		const auto l = to_volume(parts[0]);
		const auto r = to_volume(parts[1]);
		if (l && r) {
			return AudioFrame(*l, *r);
		}
	}
	return {};
}

class MIXER final : public Program {
public:
	MIXER()
	{
		AddMessages();
		help_detail = {HELP_Filter::Common,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "MIXER"};
	}

	void Run() override
	{
		if (HelpRequested()) {
			WriteOut(MSG_Get("SHELL_CMD_MIXER_HELP_LONG"));
			return;
		}
		if (cmd->FindExist("/LISTMIDI")) {
			MIDI_ListAll(this);
			return;
		}
		auto showStatus = !cmd->FindExist("/NOSHOW", true);

		std::vector<std::string> args = {};
		cmd->FillVector(args);

		auto set_reverb_level = [&](const float level,
		                            const channels_set_t &selected_channels) {
			const auto should_zero_other_channels = !mixer.do_reverb;

			// Do we need to start the reverb engine?
			if (!mixer.do_reverb)
				configure_reverb("on");
			for ([[maybe_unused]] const auto &[_, channel] : mixer.channels)
				if (selected_channels.find(channel) != selected_channels.end())
					channel->SetReverbLevel(level);
				else if (should_zero_other_channels)
					channel->SetReverbLevel(0);
		};

		auto set_chorus_level = [&](const float level,
		                            const channels_set_t &selected_channels) {
			const auto should_zero_other_channels = !mixer.do_chorus;

			// Do we need to start the chorus engine?
			if (!mixer.do_chorus)
				configure_chorus("on");
			for ([[maybe_unused]] const auto &[_, channel] : mixer.channels)
				if (selected_channels.find(channel) != selected_channels.end())
					channel->SetChorusLevel(level);
				else if (should_zero_other_channels)
					channel->SetChorusLevel(0);
		};

		auto is_master = false;
		mixer_channel_t channel = {};

		MIXER_LockAudioDevice();
		for (auto &arg : args) {
			// Does this argument set the target channel of
			// subsequent commands?
			upcase(arg);
			if (arg == "MASTER") {
				channel   = nullptr;
				is_master = true;
				continue;
			} else {
				auto chan = MIXER_FindChannel(arg.c_str());
				if (chan) {
					channel   = chan;
					is_master = false;
					continue;
				}
			}
			const auto global_command = !is_master && !channel;

			constexpr auto crossfeed_command = 'X';
			constexpr auto reverb_command    = 'R';
			constexpr auto chorus_command    = 'C';

			if (global_command) {
				// Global commands apply to all non-master channels
				if (auto p = parse_prefixed_percentage(crossfeed_command, arg); p) {
					for (auto &it : mixer.channels) {
						const auto strength = percentage_to_gain(*p);
						it.second->SetCrossfeedStrength(strength);
					}
					continue;
				} else if (p = parse_prefixed_percentage(reverb_command, arg); p) {
					const auto level = percentage_to_gain(*p);
					set_reverb_level(level, set_of_channels());
					continue;
				} else if (p = parse_prefixed_percentage(chorus_command, arg); p) {
					const auto level = percentage_to_gain(*p);
					set_chorus_level(level, set_of_channels());
					continue;
				}

			} else if (is_master) {
				// Only setting the volume is allowed for the
				// master channel
				if (const auto v = parse_volume(arg); v) {
					mixer.master_volume = *v;
				}

			} else if (channel) {
				// Adjust settings of a regular non-master channel
				if (auto p = parse_prefixed_percentage(crossfeed_command, arg); p) {
					const auto strength = percentage_to_gain(*p);
					channel->SetCrossfeedStrength(strength);
					continue;
				} else if (p = parse_prefixed_percentage(reverb_command, arg); p) {
					const auto level = percentage_to_gain(*p);
					set_reverb_level(level, {channel});
					continue;
				} else if (p = parse_prefixed_percentage(chorus_command, arg); p) {
					const auto level = percentage_to_gain(*p);
					set_chorus_level(level, {channel});
					continue;
				}

				if (channel->ChangeLineoutMap(arg))
					continue;

				if (const auto v = parse_volume(arg); v) {
					channel->SetUserVolume(v->left, v->right);
				}
			}
		}
		MIXER_UnlockAudioDevice();

		MIXER_UpdateAllChannelVolumes();

		if (showStatus)
			ShowMixerStatus();
	}

private:
	void AddMessages()
	{
		MSG_Add("SHELL_CMD_MIXER_HELP_LONG",
		        "Displays or changes the sound mixer settings.\n"
		        "\n"
		        "Usage:\n"
		        "  [color=green]mixer[reset] [color=cyan][CHANNEL][reset] [color=white]COMMANDS[reset] [/noshow]\n"
		        "  [color=green]mixer[reset] [/listmidi]\n"
		        "\n"
		        "Where:\n"
		        "  [color=cyan]CHANNEL[reset]  is the sound channel to change the settings of.\n"
		        "  [color=white]COMMANDS[reset] is one or more of the following commands:\n"
		        "    Volume:    [color=white]0[reset] to [color=white]100[reset], or decibel value prefixed with [color=white]d[reset] (e.g. [color=white]d-7.5[reset])\n"
		        "               use [color=white]L:R[reset] to set the left and right side separately (e.g. [color=white]10:20[reset])\n"
		        "    Lineout:   [color=white]stereo[reset], [color=white]reverse[reset] (for stereo channels only)\n"
		        "    Crossfeed: [color=white]x0[reset] to [color=white]x100[reset]    Reverb: [color=white]r0[reset] to [color=white]r100[reset]    Chorus: [color=white]c0[reset] to [color=white]c100[reset]\n"
		        "Notes:\n"
		        "  Running [color=green]mixer[reset] without an argument shows the current mixer settings.\n"
		        "  You may change the settings of more than one channel in a single command.\n"
		        "  If channel is unspecified, you can set crossfeed, reverb or chorus globally.\n"
		        "  You can view the list of available MIDI devices with /listmidi.\n"
		        "  The /noshow option applies the changes without showing the mixer settings.\n"
		        "\n"
		        "Examples:\n"
		        "  [color=green]mixer[reset] [color=cyan]cdda[reset] [color=white]50[reset] [color=cyan]sb[reset] [color=white]reverse[reset] /noshow\n"
		        "  [color=green]mixer[reset] [color=white]x30[reset] [color=cyan]opl[reset] [color=white]150 r50 c30[reset] [color=cyan]sb[reset] [color=white]x10[reset]");

		MSG_Add("SHELL_CMD_MIXER_HEADER_LAYOUT",
		        "%-22s %4.0f:%-4.0f %+6.2f:%-+6.2f  %-8s %5s %7s %7s");

		MSG_Add("SHELL_CMD_MIXER_HEADER_LABELS",
		        "[color=white]Channel      Volume    Volume (dB)   Mode     Xfeed  Reverb  Chorus[reset]");

		MSG_Add("SHELL_CMD_MIXER_CHANNEL_OFF", "off");

		MSG_Add("SHELL_CMD_MIXER_CHANNEL_STEREO", "Stereo");

		MSG_Add("SHELL_CMD_MIXER_CHANNEL_REVERSE", "Reverse");

		MSG_Add("SHELL_CMD_MIXER_CHANNEL_MONO", "Mono");
	}

	void ShowMixerStatus()
	{
		std::string column_layout = MSG_Get("SHELL_CMD_MIXER_HEADER_LAYOUT");
		column_layout.append({'\n'});

		auto show_channel = [&](const std::string &name,
		                        const AudioFrame &volume,
		                        const std::string &mode,
		                        const std::string &xfeed,
		                        const std::string &reverb,
		                        const std::string &chorus) {
			WriteOut(column_layout.c_str(),
			         name.c_str(),
			         static_cast<double>(volume.left * 100.0f),
			         static_cast<double>(volume.right * 100.0f),
			         static_cast<double>(gain_to_decibel(volume.left)),
			         static_cast<double>(gain_to_decibel(volume.right)),
			         mode.c_str(),
			         xfeed.c_str(),
			         reverb.c_str(),
			         chorus.c_str());
		};

		WriteOut("%s\n", MSG_Get("SHELL_CMD_MIXER_HEADER_LABELS"));

		const auto off_value = MSG_Get("SHELL_CMD_MIXER_CHANNEL_OFF");
		constexpr auto none_value = "-";

		MIXER_LockAudioDevice();

		constexpr auto master_channel_string = "[color=cyan]MASTER[reset]";

		show_channel(convert_ansi_markup(master_channel_string),
		             mixer.master_volume,
		             MSG_Get("SHELL_CMD_MIXER_CHANNEL_STEREO"),
		             none_value,
		             none_value,
		             none_value);

		for (auto &[name, chan] : mixer.channels) {
			std::string xfeed = none_value;
			if (chan->HasFeature(ChannelFeature::Stereo)) {
				if (chan->GetCrossfeedStrength() > 0.0f) {
					xfeed = std::to_string(static_cast<uint8_t>(
					        round(chan->GetCrossfeedStrength() *
					              100.0f)));
				} else {
					xfeed = off_value;
				}
			}

			std::string reverb = none_value;
			if (chan->HasFeature(ChannelFeature::ReverbSend)) {
				if (chan->GetReverbLevel() > 0.0f) {
					reverb = std::to_string(static_cast<uint8_t>(round(
					        chan->GetReverbLevel() * 100.0f)));
				} else {
					reverb = off_value;
				}
			}

			std::string chorus = none_value;
			if (chan->HasFeature(ChannelFeature::ChorusSend)) {
				if (chan->GetChorusLevel() > 0.0f) {
					chorus = std::to_string(static_cast<uint8_t>(round(
					        chan->GetChorusLevel() * 100.0f)));
				} else {
					chorus = off_value;
				}
			}

			auto channel_name = std::string("[color=cyan]") + name +
			                    std::string("[reset]");

			auto mode = chan->DescribeLineout();

			show_channel(convert_ansi_markup(channel_name),
			             chan->GetUserVolume(),
			             mode,
			             xfeed,
			             reverb,
			             chorus);
		}

		MIXER_UnlockAudioDevice();
	}
};

std::unique_ptr<Program> MIXER_ProgramCreate() {
	return ProgramCreate<MIXER>();
}

[[maybe_unused]] static const char * MixerStateToString(const MixerState s)
{
	switch (s) {
	case MixerState::Uninitialized: return "uninitialized";
	case MixerState::NoSound: return "no sound";
	case MixerState::On: return "on";
	case MixerState::Muted: return "mute";
	}
	return "unknown!";
}

static void set_mixer_state(const MixerState requested)
{
	// The mixer starts uninitialized but should never be programmatically
	// asked to become uninitialized
	assert(requested != MixerState::Uninitialized);

	// When sdldevice is zero then the SDL audio device doesn't exist, which
	// is a valid configuration. In these cases the only logical end-state
	// is NoSound. So switch the request and let the rest of the function
	// handle it.
	auto new_state = requested;
	if (mixer.sdldevice == 0) {
		new_state = MixerState::NoSound;
	}

	if (mixer.state == new_state) {
		// Nothing to do.
		// LOG_MSG("MIXER: Is already %s, skipping",
		// MixerStateToString(mixer.state));
	} else if (mixer.state == MixerState::On && new_state != MixerState::On) {
		TIMER_DelTickHandler(MIXER_Mix);
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
		// LOG_MSG("MIXER: Changed from on to %s",
		// MixerStateToString(new_state));
	} else if (mixer.state != MixerState::On && new_state == MixerState::On) {
		TIMER_DelTickHandler(MIXER_Mix_NoSound);
		TIMER_AddTickHandler(MIXER_Mix);
		// LOG_MSG("MIXER: Changed from %s to on",
		// MixerStateToString(mixer.state));
	} else {
		// Nothing to do.
		// LOG_MSG("MIXER: Skipping unecessary change from %s to %s",
		// MixerStateToString(mixer.state), MixerStateToString(new_state));
	}
	mixer.state = new_state;

	// Finally, we start the audio device either paused or unpaused:
	if (mixer.sdldevice) {
		SDL_PauseAudioDevice(mixer.sdldevice, mixer.state != MixerState::On);
	}
	//
	// When unpaused, the device pulls frames queued by the MIXER_Mix
	// function, which it fetches from each channel's callback (every
	// millisecond tick), and mixes into a stereo frame buffer.

	// When paused, the audio device stops reading frames from our buffer,
	// so it's imporant that the we no longer queue them, which is why we
	// use MIXER_Mix_NoSound (to throw away frames instead of queuing).
}

using channel_state_t = std::pair<std::string, bool>;

static std::vector<channel_state_t> save_channel_states()
{
	std::vector<channel_state_t> states = {};
	for (const auto &[name, channel] : mixer.channels)
		states.emplace_back(name, channel->is_enabled);
	return states;
}

static void restore_channel_states(const std::vector<channel_state_t> &states)
{
	for (const auto &[name, was_enabled] : states)
		if (auto channel = MIXER_FindChannel(name.c_str()); channel)
			channel->Enable(was_enabled);
}

void MIXER_CloseAudioDevice()
{
	// Stop either mixing method
	TIMER_DelTickHandler(MIXER_Mix);
	TIMER_DelTickHandler(MIXER_Mix_NoSound);

	MIXER_LockAudioDevice();
	for (auto &it : mixer.channels)
		it.second->Enable(false);
	MIXER_UnlockAudioDevice();

	if (mixer.sdldevice) {
		SDL_CloseAudioDevice(mixer.sdldevice);
		mixer.sdldevice = 0;
	}
	mixer.state = MixerState::Uninitialized;
}

void MIXER_Init(Section *sec)
{
	const auto channel_states = save_channel_states();

	MIXER_CloseAudioDevice();

	sec->AddDestroyFunction(&MIXER_Stop);

	Section_prop *section = static_cast<Section_prop *>(sec);
	/* Read out config section */

	mixer.sample_rate = section->Get_int("rate");
	mixer.blocksize = static_cast<uint16_t>(section->Get_int("blocksize"));
	const auto negotiate = section->Get_bool("negotiate");

	/* Start the Mixer using SDL Sound at 22 khz */
	SDL_AudioSpec spec;
	SDL_AudioSpec obtained;

	spec.freq     = static_cast<int>(mixer.sample_rate);
	spec.format   = AUDIO_S16SYS;
	spec.channels = 2;
	spec.callback = MIXER_CallBack;
	spec.userdata = nullptr;
	spec.samples  = mixer.blocksize;

	int sdl_allow_flags = 0;

	if (negotiate) {
		// We allow chunk size changes (if supported), but don't allow
		// frequency changes because we've only seen them go beyond
		// SDL's desired 48 Khz limit
#if defined(SDL_AUDIO_ALLOW_SAMPLES_CHANGE)
		sdl_allow_flags |= SDL_AUDIO_ALLOW_SAMPLES_CHANGE;
#endif
	}

	const auto configured_state = section->Get_bool("nosound")
	                                    ? MixerState::NoSound
	                                    : MixerState::On;

	if (configured_state == MixerState::NoSound) {
		LOG_MSG("MIXER: No Sound Mode Selected.");
		mixer.tick_add = calc_tickadd(mixer.sample_rate);
		set_mixer_state(MixerState::NoSound);

	} else if ((mixer.sdldevice = SDL_OpenAudioDevice(
	                    nullptr, 0, &spec, &obtained, sdl_allow_flags)) == 0) {
		LOG_WARNING("MIXER: Can't open audio: %s , running in nosound mode.",
		            SDL_GetError());
		mixer.tick_add = calc_tickadd(mixer.sample_rate);
		set_mixer_state(MixerState::NoSound);

	} else {
		// Does SDL want something other than stereo output?
		if (obtained.channels != spec.channels)
			E_Exit("SDL gave us %u-channel output but we require %u channels",
			       obtained.channels,
			       spec.channels);

		// Does SDL want a different playback rate?
		if (obtained.freq != mixer.sample_rate) {
			LOG_WARNING("MIXER: SDL changed the playback rate from %d to %d Hz",
			            mixer.sample_rate.load(),
			            obtained.freq);
			mixer.sample_rate = obtained.freq;
		}

		// Does SDL want a different blocksize?
		const auto obtained_blocksize = obtained.samples;

		if (obtained_blocksize != mixer.blocksize) {
			LOG_WARNING("MIXER: SDL changed the blocksize from %u to %u frames",
			            mixer.blocksize,
			            obtained_blocksize);
			mixer.blocksize = obtained_blocksize;
		}

		mixer.tick_add = calc_tickadd(mixer.sample_rate);
		set_mixer_state(MixerState::On);

		LOG_MSG("MIXER: Negotiated %u-channel %u Hz audio in %u-frame blocks",
		        obtained.channels,
		        mixer.sample_rate.load(),
		        mixer.blocksize);
	}

	// 1000 = 8 *125
	mixer.tick_counter = (mixer.sample_rate % 125) ? TICK_NEXT : 0;

	const auto requested_prebuffer_ms = section->Get_int("prebuffer");

	mixer.prebuffer_ms = check_cast<uint16_t>(
	        clamp(requested_prebuffer_ms, 1, max_prebuffer_ms));

	const auto prebuffer_frames = (mixer.sample_rate * mixer.prebuffer_ms) / 1000;

	mixer.pos               = 0;
	mixer.frames_done       = 0;
	mixer.frames_needed     = mixer.min_frames_needed + 1;
	mixer.min_frames_needed = 0;
	mixer.max_frames_needed = mixer.blocksize * 2 + 2 * prebuffer_frames;

	// Initialize the 8-bit to 16-bit lookup table
	fill_8to16_lut();

	// Initialise master high-pass filter
	// ----------------------------------
	// The purpose of this filter is two-fold:
	//
	// - Remove any DC offset from the summed master output (any high-pass
	//   filter can achieve this, even a 6dB/oct HPF at 1Hz). Virtually all
	//   synth modules (CMS, OPL, etc.) can introduce DC offset; this
	//   usually isn't a problem on real hardware as most audio interfaces
	//   include a DC-blocking or high-pass filter in the analog output
	//   stages.
	//
	// - Get rid of (or more precisely, attenuate) unnecessary rumble below
	//   20Hz that serves no musical purpose and only eats up headroom.
	//   Issues like this could have gone unnoticed in the 80s/90s due to
	//   much lower quality consumer audio equipment available, plus
	//   most sound cards had weak bass response (on some models the bass
	//   rolloff starts from as high as 100-120Hz), so the presence of
	//   unnecessary ultra low-frequency content never became an issue back
	//   then.
	//
	// Thanks to the float mixbuffer, it is sufficient to peform the
	// high-pass filtering only once at the very end of the processing
	// chain, instead of doing it on every single mixer channel.

	constexpr auto highpass_cutoff_freq = 20.0;
	for (auto &f : mixer.highpass_filter)
		f.setup(mixer.sample_rate, highpass_cutoff_freq);

	// Initialise send effects
	configure_reverb(section->Get_string("reverb"));
	configure_chorus(section->Get_string("chorus"));

	// Initialise compressor
	configure_compressor(section->Get_bool("compressor"));

	restore_channel_states(channel_states);
}

void MIXER_Mute()
{
	if (mixer.state == MixerState::On) {
		set_mixer_state(MixerState::Muted);
		MIDI_Mute();
		LOG_MSG("MIXER: Muted");
	}
}

void MIXER_Unmute()
{
	if (mixer.state == MixerState::Muted) {
		set_mixer_state(MixerState::On);
		MIDI_Unmute();
		LOG_MSG("MIXER: Unmuted");
	}
}

bool MIXER_IsManuallyMuted()
{
	return mixer.is_manually_muted;
}

// Toggle the mixer on/off when a 'true' bool is passed in.
static void ToggleMute(const bool was_pressed)
{
	// The "pressed" bool argument is used by the Mapper API, which sends a
	// true-bool for key down events and a false-bool for key up events.
	if (!was_pressed) {
		return;
	}

	switch (mixer.state) {
	case MixerState::NoSound:
		LOG_WARNING("MIXER: Mute requested, but sound is off (nosound mode)");
		break;
	case MixerState::Muted:
		MIXER_Unmute();
		mixer.is_manually_muted = false;
		break;
	case MixerState::On:
		MIXER_Mute();
		mixer.is_manually_muted = true;
		break;
	default: break;
	};
}

void init_mixer_dosbox_settings(Section_prop &sec_prop)
{
	constexpr int default_rate = 48000;
#if defined(WIN32)
	// Longstanding known-good defaults for Windows
	constexpr int default_blocksize = 1024;
	constexpr int default_prebuffer_ms = 25;
	constexpr bool default_allow_negotiate = false;

#else
	// Non-Windows platforms tolerate slightly lower latency
	constexpr int default_blocksize = 512;
	constexpr int16_t default_prebuffer_ms = 20;
	constexpr bool default_allow_negotiate = true;
#endif

	constexpr auto always        = Property::Changeable::Always;
	constexpr auto when_idle     = Property::Changeable::WhenIdle;
	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;

	auto bool_prop = sec_prop.Add_bool("nosound", always, false);
	assert(bool_prop);
	bool_prop->Set_help(
	        "Enable silent mode (disabled by default).\n"
	        "Sound is still fully emulated in silent mode, but DOSBox outputs silence.");

	auto int_prop = sec_prop.Add_int("rate", only_at_start, default_rate);
	assert(int_prop);
	const char *rates[] = {
	        "8000", "11025", "16000", "22050", "32000", "44100", "48000", nullptr};
	int_prop->Set_values(rates);
	int_prop->Set_help("Mixer sample rate (48000 by default).");

	const char *blocksizes[] = {"128", "256", "512", "1024", "2048", "4096", "8192", nullptr};

	int_prop = sec_prop.Add_int("blocksize", only_at_start, default_blocksize);
	int_prop->Set_values(blocksizes);
	int_prop->Set_help("Mixer block size; larger values might help with sound stuttering but sound will\n"
	                   "also be more lagged.");

	int_prop = sec_prop.Add_int("prebuffer", only_at_start, default_prebuffer_ms);
	int_prop->SetMinMax(0, max_prebuffer_ms);
	int_prop->Set_help(
	        "How many milliseconds of sound to render on top of the blocksize; larger values\n"
	        "might help with sound stuttering but sound will also be more lagged.");

	bool_prop = sec_prop.Add_bool("negotiate", only_at_start, default_allow_negotiate);
	bool_prop->Set_help("Let the system audio driver negotiate (possibly) better rate and blocksize\n"
	                    "settings.");

	const auto default_on = true;
	bool_prop = sec_prop.Add_bool("compressor", when_idle, default_on);
	bool_prop->Set_help("Enable the auto-leveling compressor on the master channel to prevent clipping\n"
	                    "of the audio output:\n"
	                    "  off:  Disable compressor.\n"
	                    "  on:   Enable compressor (default).");

	auto string_prop = sec_prop.Add_string("crossfeed", when_idle, "off");
	string_prop->Set_help(
	        "Set crossfeed globally on all stereo channels for headphone listening:\n"
	        "  off:         No crossfeed (default).\n"
	        "  on:          Enable crossfeed (at strength 40).\n"
	        "  <strength>:  Set crossfeed strength from 0 to 100, where 0 means no crossfeed\n"
	        "               (off) and 100 full crossfeed (effectively turning stereo content\n"
	        "               into mono).\n"
	        "Notes: You can set per-channel crossfeed via mixer commands.");

	const char *reverb_presets[] = {"off", "on", "tiny", "small", "medium", "large", "huge", nullptr};
	string_prop = sec_prop.Add_string("reverb", when_idle, reverb_presets[0]);
	string_prop->Set_help(
	        "Enable reverb globally to add a sense of space to the sound:\n"
	        "  off:     No reverb (default).\n"
	        "  on:      Enable reverb (medium preset).\n"
	        "  tiny:    Simulates the sound of a small integrated speaker in a room;\n"
	        "           specifically designed for small-speaker audio systems\n"
	        "           (PC Speaker, Tandy, PS/1 Audio, and LPT DAC devices).\n"
	        "  small:   Adds a subtle sense of space; good for games that use a single\n"
	        "           synth channel (typically OPL) for both music and sound effects.\n"
	        "  medium:  Medium room preset that works well with a wide variety of games.\n"
	        "  large:   Large hall preset recommended for games that use separate\n"
	        "           channels for music and digital audio.\n"
	        "  huge:    A stronger variant of the large hall preset; works really well\n"
	        "           in some games with more atmospheric soundtracks.\n"
	        "Notes: You can fine-tune per-channel reverb levels via mixer commands.");
	string_prop->Set_values(reverb_presets);

	const char *chorus_presets[] = {"off", "on", "light", "normal", "strong", nullptr};
	string_prop = sec_prop.Add_string("chorus", when_idle, "off");
	string_prop->Set_help(
	        "Enable chorus globally to add a sense of stereo movement to the sound:\n"
	        "  off:     No chorus (default).\n"
	        "  on:      Enable chorus (normal preset).\n"
	        "  light:   A light chorus effect (especially suited for synth music that\n"
	        "           features lots of white noise.)\n"
	        "  normal:  Normal chorus that works well with a wide variety of games.\n"
	        "  strong:  An obvious and upfront chorus effect.\n"
	        "Notes: You can fine-tune per-channel chorus levels via mixer commands.");
	string_prop->Set_values(chorus_presets);

	MAPPER_AddHandler(ToggleMute, SDL_SCANCODE_F8, PRIMARY_MOD, "mute", "Mute");
}

void MIXER_AddConfigSection(const config_ptr_t& conf)
{
	assert(conf);

	constexpr auto changeable_at_runtime = true;

	Section_prop* sec = conf->AddSection_prop("mixer",
	                                          &MIXER_Init,
	                                          changeable_at_runtime);
	assert(sec);
	init_mixer_dosbox_settings(*sec);
}
