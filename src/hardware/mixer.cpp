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

// #define DEBUG 1

#include "mixer.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <sys/types.h>

#include <SDL.h>
#include <speex/speex_resampler.h>

#include "../capture/capture.h"
#include "channel_names.h"
#include "checks.h"
#include "control.h"
#include "cross.h"
#include "hardware.h"
#include "mapper.h"
#include "math_utils.h"
#include "mem.h"
#include "midi.h"
#include "pic.h"
#include "setup.h"
#include "string_utils.h"
#include "timer.h"
#include "tracy.h"
#include "video.h"

#include "mverb/MVerb.h"
#include "tal-chorus/ChorusEngine.h"

CHECK_NARROWING();

//#define DEBUG_MIXER

constexpr auto MixerFrameSize = 4;

constexpr auto FreqShift = 14;
constexpr auto FreqNext  = (1 << FreqShift);
constexpr auto FreqMask  = (FreqNext - 1);

constexpr auto TickShift = 24;
constexpr auto TickNext  = (1 << TickShift);
constexpr auto TickMask  = (TickNext - 1);

constexpr auto IndexShiftLocal = 14;

// Over how many milliseconds will we permit a signal to grow from
// zero up to peak amplitude? (recommended 10 to 20ms)
constexpr auto EnvelopeMaxExpansionOverMs = 15u;

// Regardless if the signal needed to be eveloped or not, how long
// should the envelope monitor the initial signal? (recommended > 5s)
constexpr auto EnvelopeExpiresAfterSeconds = 10u;

constexpr auto MaxPrebufferMs = 100;

template <class T, size_t ROWS, size_t COLS>
using matrix = std::array<std::array<T, COLS>, ROWS>;

using HighpassFilter = std::array<Iir::Butterworth::HighPass<2>, 2>;

using EmVerb = MVerb<float>;

struct CrossfeedSettings {
	CrossfeedPreset preset = CrossfeedPreset::None;
	float global_strength  = 0.0f;
};

struct ReverbSettings {
	EmVerb mverb = {};

	// MVerb does not have an integrated high-pass filter to shape
	// the low-end response like other reverbs. So we're adding one
	// here. This helps take control over low-frequency build-up,
	// resulting in a more pleasant sound.
	HighpassFilter highpass_filter = {};

	ReverbPreset preset            = ReverbPreset::None;
	float synthesizer_send_level   = 0.0f;
	float digital_audio_send_level = 0.0f;
	float highpass_cutoff_freq_hz  = 1.0f;

	void Setup(const float predelay, const float early_mix, const float size,
	           const float density, const float bandwidth_freq_hz,
	           const float decay, const float dampening_freq_hz,
	           const float synth_level, const float digital_level,
	           const float highpass_freq_hz, const uint16_t sample_rate_hz)
	{
		synthesizer_send_level   = synth_level;
		digital_audio_send_level = digital_level;
		highpass_cutoff_freq_hz  = highpass_freq_hz;

		mverb.setParameter(EmVerb::PREDELAY, predelay);
		mverb.setParameter(EmVerb::EARLYMIX, early_mix);
		mverb.setParameter(EmVerb::SIZE, size);
		mverb.setParameter(EmVerb::DENSITY, density);
		mverb.setParameter(EmVerb::BANDWIDTHFREQ, bandwidth_freq_hz);
		mverb.setParameter(EmVerb::DECAY, decay);
		mverb.setParameter(EmVerb::DAMPINGFREQ, dampening_freq_hz);
		mverb.setParameter(EmVerb::GAIN, 1.0f); // Always max gain (no
		                                        // attenuation)
		mverb.setParameter(EmVerb::MIX, 1.0f); // Always 100% wet signal

		mverb.setSampleRate(static_cast<float>(sample_rate_hz));

		for (auto& f : highpass_filter) {
			f.setup(sample_rate_hz, highpass_freq_hz);
		}
	}
};

struct ChorusSettings {
	ChorusEngine chorus_engine = ChorusEngine(48000);

	ChorusPreset preset            = ChorusPreset::None;
	float synthesizer_send_level   = 0.0f;
	float digital_audio_send_level = 0.0f;

	void Setup(const float synth_level, const float digital_level,
	           const uint16_t sample_rate_hz)
	{
		synthesizer_send_level   = synth_level;
		digital_audio_send_level = digital_level;

		chorus_engine.setSampleRate(static_cast<float>(sample_rate_hz));

		constexpr auto chorus1_enabled  = true;
		constexpr auto chorus2_disabled = false;
		chorus_engine.setEnablesChorus(chorus1_enabled, chorus2_disabled);
	}
};

struct MixerSettings {
	// Complex types
	matrix<float, MixerBufferLength, 2> work       = {};
	matrix<float, MixerBufferLength, 2> aux_reverb = {};
	matrix<float, MixerBufferLength, 2> aux_chorus = {};

	std::vector<float> resample_temp = {};
	std::vector<float> resample_out  = {};

	AudioFrame master_volume = {1.0f, 1.0f};

	std::map<std::string, mixer_channel_t> channels = {};

	std::map<std::string, MixerChannelSettings> channel_settings_cache = {};

	// Counters accessed by multiple threads
	std::atomic<work_index_t> pos      = 0;
	std::atomic<int> frames_done       = 0;
	std::atomic<int> frames_needed     = 0;
	std::atomic<int> min_frames_needed = 0;
	std::atomic<int> max_frames_needed = 0;
	std::atomic<int> tick_add = 0; // samples needed per millisecond tick

	int tick_counter = 0;
	std::atomic<uint16_t> sample_rate_hz = 0; // sample rate negotiated with SDL
	uint16_t blocksize = 0; // matches SDL AudioSpec.samples type

	uint16_t prebuffer_ms = 25; // user-adjustable in conf

	SDL_AudioDeviceID sdldevice = 0;

	MixerState state = MixerState::Uninitialized; // use set_mixer_state()
	                                              // to change

	HighpassFilter highpass_filter = {};
	Compressor compressor          = {};
	bool do_compressor             = false;

	CrossfeedSettings crossfeed = {};
	bool do_crossfeed           = false;

	ReverbSettings reverb = {};
	bool do_reverb        = false;

	ChorusSettings chorus = {};
	bool do_chorus        = false;

	bool is_manually_muted = false;
};

static struct MixerSettings mixer = {};

alignas(sizeof(float)) uint8_t MixTemp[MixerBufferLength] = {};

void MixerChannel::SetLineoutMap(const StereoLine map)
{
	output_map = map;
}

StereoLine MixerChannel::GetLineoutMap() const
{
	return output_map;
}

static Section_prop* get_mixer_section()
{
	assert(control);

	auto section = static_cast<Section_prop*>(control->GetSection("mixer"));
	assert(section);

	return section;
}

uint16_t MIXER_GetPreBufferMs()
{
	assert(mixer.prebuffer_ms > 0);
	assert(mixer.prebuffer_ms <= MaxPrebufferMs);

	return mixer.prebuffer_ms;
}

uint16_t MIXER_GetSampleRate()
{
	const auto sample_rate_hz = mixer.sample_rate_hz.load();
	assert(sample_rate_hz > 0);
	return sample_rate_hz;
}

void MIXER_LockAudioDevice()
{
	SDL_LockAudioDevice(mixer.sdldevice);
}

void MIXER_UnlockAudioDevice()
{
	SDL_UnlockAudioDevice(mixer.sdldevice);
}

MixerChannel::MixerChannel(MIXER_Handler _handler, const char* _name,
                           const std::set<ChannelFeature>& _features)
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

std::set<ChannelFeature> MixerChannel::GetFeatures() const
{
	return features;
}

bool StereoLine::operator==(const StereoLine other) const
{
	return left == other.left && right == other.right;
}

static void set_global_crossfeed(const mixer_channel_t& channel)
{
	assert(channel);

	if (!mixer.do_crossfeed || !channel->HasFeature(ChannelFeature::Stereo)) {
		channel->SetCrossfeedStrength(0.0f);
	} else {
		channel->SetCrossfeedStrength(mixer.crossfeed.global_strength);
	}
}

static void set_global_reverb(const mixer_channel_t& channel)
{
	assert(channel);

	if (!mixer.do_reverb || !channel->HasFeature(ChannelFeature::ReverbSend)) {
		channel->SetReverbLevel(0.0f);

	} else if (channel->HasFeature(ChannelFeature::Synthesizer)) {
		channel->SetReverbLevel(mixer.reverb.synthesizer_send_level);

	} else if (channel->HasFeature(ChannelFeature::DigitalAudio)) {
		channel->SetReverbLevel(mixer.reverb.digital_audio_send_level);
	}
}

static void set_global_chorus(const mixer_channel_t& channel)
{
	assert(channel);

	if (!mixer.do_chorus || !channel->HasFeature(ChannelFeature::ChorusSend)) {
		channel->SetChorusLevel(0.0f);

	} else if (channel->HasFeature(ChannelFeature::Synthesizer)) {
		channel->SetChorusLevel(mixer.chorus.synthesizer_send_level);

	} else if (channel->HasFeature(ChannelFeature::DigitalAudio)) {
		channel->SetChorusLevel(mixer.chorus.digital_audio_send_level);
	}
}

static CrossfeedPreset crossfeed_pref_to_preset(const std::string& pref)
{
	const auto pref_has_bool = parse_bool_setting(pref);
	if (pref_has_bool) {
		return *pref_has_bool ? DefaultCrossfeedPreset : CrossfeedPreset::None;
	}
	if (pref == "light") {
		return CrossfeedPreset::Light;
	}
	if (pref == "normal") {
		return CrossfeedPreset::Normal;
	}
	if (pref == "strong") {
		return CrossfeedPreset::Strong;
	}

	// the conf system programmatically guarantees only the above prefs are
	// used
	LOG_WARNING("MIXER: Invalid 'crossfeed' setting: '%s', using 'off'",
	            pref.c_str());
	return CrossfeedPreset::None;
}

static const char* to_string(const CrossfeedPreset preset)
{
	switch (preset) {
	case CrossfeedPreset::None: return "off";
	case CrossfeedPreset::Light: return "light";
	case CrossfeedPreset::Normal: return "normal";
	case CrossfeedPreset::Strong: return "strong";
	default: assertm(false, "Invalid CrossfeedPreset"); return "";
	}
}

static void sync_crossfeed_setting(const CrossfeedPreset preset)
{
	const auto string_prop = get_mixer_section()->GetStringProp("crossfeed");
	string_prop->SetValue(to_string(preset));
}

CrossfeedPreset MIXER_GetCrossfeedPreset()
{
	return mixer.crossfeed.preset;
}

void MIXER_SetCrossfeedPreset(const CrossfeedPreset new_preset)
{
	auto& c = mixer.crossfeed; // short-hand reference

	// Unchanged?
	if (new_preset == c.preset) {
		return;
	}
	// Set new
	assert(c.preset != new_preset);
	c.preset = new_preset;

	switch (c.preset) {
	case CrossfeedPreset::None: break;
	case CrossfeedPreset::Light: c.global_strength = 0.20f; break;
	case CrossfeedPreset::Normal: c.global_strength = 0.40f; break;
	case CrossfeedPreset::Strong: c.global_strength = 0.60f; break;
	}

	// Configure the channels
	mixer.do_crossfeed = (c.preset != CrossfeedPreset::None);

	// Configure the channels
	for (const auto& [_, channel] : mixer.channels) {
		set_global_crossfeed(channel);
	}

	sync_crossfeed_setting(c.preset);

	if (mixer.do_crossfeed) {
		LOG_MSG("MIXER: Crossfeed enabled ('%s' preset)", to_string(c.preset));
	} else {
		LOG_MSG("MIXER: Crossfeed disabled");
	}
}

static ReverbPreset reverb_pref_to_preset(const std::string& pref)
{
	const auto pref_has_bool = parse_bool_setting(pref);
	if (pref_has_bool) {
		return *pref_has_bool ? DefaultReverbPreset : ReverbPreset::None;
	}
	if (pref == "tiny") {
		return ReverbPreset::Tiny;
	}
	if (pref == "small") {
		return ReverbPreset::Small;
	}
	if (pref == "medium") {
		return ReverbPreset::Medium;
	}
	if (pref == "large") {
		return ReverbPreset::Large;
	}
	if (pref == "huge") {
		return ReverbPreset::Huge;
	}

	// the conf system programmatically guarantees only the above prefs are
	// used
	LOG_WARNING("MIXER: Invalid 'reverb' setting: '%s', using 'off'",
	            pref.c_str());
	return ReverbPreset::None;
}

static const char* to_string(const ReverbPreset preset)
{
	switch (preset) {
	case ReverbPreset::None: return "off";
	case ReverbPreset::Tiny: return "tiny";
	case ReverbPreset::Small: return "small";
	case ReverbPreset::Medium: return "medium";
	case ReverbPreset::Large: return "large";
	case ReverbPreset::Huge: return "huge";
	default: assertm(false, "Invalid ReverbPreset"); return "";
	}
}

static void sync_reverb_setting(const ReverbPreset preset)
{
	const auto string_prop = get_mixer_section()->GetStringProp("reverb");
	string_prop->SetValue(to_string(preset));
}

ReverbPreset MIXER_GetReverbPreset()
{
	return mixer.reverb.preset;
}

void MIXER_SetReverbPreset(const ReverbPreset new_preset)
{
	auto& r = mixer.reverb; // short-hand reference

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

	const auto rate_hz = mixer.sample_rate_hz.load();

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

	for (const auto& [_, channel] : mixer.channels) {
		set_global_reverb(channel);
	}

	sync_reverb_setting(r.preset);

	if (mixer.do_reverb) {
		LOG_MSG("MIXER: Reverb enabled ('%s' preset)", to_string(r.preset));
	} else {
		LOG_MSG("MIXER: Reverb disabled");
	}
}

static ChorusPreset chorus_pref_to_preset(const std::string& pref)
{
	const auto pref_has_bool = parse_bool_setting(pref);
	if (pref_has_bool) {
		return *pref_has_bool ? DefaultChorusPreset : ChorusPreset::None;
	}
	if (pref == "light") {
		return ChorusPreset::Light;
	}
	if (pref == "normal") {
		return ChorusPreset::Normal;
	}
	if (pref == "strong") {
		return ChorusPreset::Strong;
	}

	// the conf system programmatically guarantees only the above prefs are
	// used
	LOG_WARNING("MIXER: Invalid 'chorus' setting: '%s', using ' off'",
	            pref.c_str());
	return ChorusPreset::None;
}

static const char* to_string(const ChorusPreset preset)
{
	switch (preset) {
	case ChorusPreset::None: return "off";
	case ChorusPreset::Light: return "light";
	case ChorusPreset::Normal: return "normal";
	case ChorusPreset::Strong: return "strong";
	default: assertm(false, "Invalid ChorusPreset"); return "";
	}
}

static void sync_chorus_setting(const ChorusPreset preset)
{
	const auto string_prop = get_mixer_section()->GetStringProp("chorus");
	string_prop->SetValue(to_string(preset));
}

ChorusPreset MIXER_GetChorusPreset()
{
	return mixer.chorus.preset;
}

void MIXER_SetChorusPreset(const ChorusPreset new_preset)
{
	auto& c = mixer.chorus; // short-hand reference

	// Unchanged?
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

	const auto rate_hz = mixer.sample_rate_hz.load();

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

	for (const auto& [_, channel] : mixer.channels) {
		set_global_chorus(channel);
	}

	sync_chorus_setting(c.preset);

	if (mixer.do_chorus) {
		LOG_MSG("MIXER: Chorus enabled ('%s' preset)", to_string(c.preset));
	} else {
		LOG_MSG("MIXER: Chorus disabled");
	}
}

static void init_compressor(const bool compressor_enabled)
{
	mixer.do_compressor = compressor_enabled;
	if (!mixer.do_compressor) {
		LOG_MSG("MIXER: Master compressor disabled");
		return;
	}

	const auto _0dbfs_sample_value = Max16BitSampleValue;
	const auto threshold_db        = -6.0f;
	const auto ratio               = 3.0f;
	const auto attack_time_ms      = 0.01f;
	const auto release_time_ms     = 5000.0f;
	const auto rms_window_ms       = 10.0;

	mixer.compressor.Configure(mixer.sample_rate_hz,
	                           _0dbfs_sample_value,
	                           threshold_db,
	                           ratio,
	                           attack_time_ms,
	                           release_time_ms,
	                           rms_window_ms);

	LOG_MSG("MIXER: Master compressor enabled");
}

void MIXER_DeregisterChannel(mixer_channel_t& channel_to_remove)
{
	if (!channel_to_remove) {
		return;
	}

	MIXER_LockAudioDevice();

	auto it = mixer.channels.begin();
	while (it != mixer.channels.end()) {
		const auto [name, channel] = *it;

		if (channel.get() == channel_to_remove.get()) {
			// Save channel settings to a cache so we can
			// restore them if the channel gets recreated later.
			// This is necessary to persist channel settings when
			// changing the `sbtype` , for example.
			mixer.channel_settings_cache[name] = channel->GetSettings();

			it = mixer.channels.erase(it);
			break;
		}
		++it;
	}

	MIXER_UnlockAudioDevice();
}

mixer_channel_t MIXER_AddChannel(MIXER_Handler handler,
                                 const uint16_t sample_rate_hz, const char* name,
                                 const std::set<ChannelFeature>& features)
{
	auto chan = std::make_shared<MixerChannel>(handler, name, features);
	chan->SetSampleRate(sample_rate_hz);
	chan->SetAppVolume({1.0f, 1.0f});

	const auto chan_rate_hz = chan->GetSampleRate();
	if (chan_rate_hz == mixer.sample_rate_hz) {
		LOG_MSG("%s: Operating at %u Hz without resampling", name, chan_rate_hz);
	} else {
		LOG_MSG("%s: Operating at %u Hz and %s to the output rate",
		        name,
		        chan_rate_hz,
		        chan_rate_hz > mixer.sample_rate_hz ? "downsampling"
		                                            : "upsampling");
	}

	// Try to restore saved channel settings from the cache first.
	if (mixer.channel_settings_cache.find(name) !=
	    mixer.channel_settings_cache.end()) {
		chan->SetSettings(mixer.channel_settings_cache[name]);
	} else {
		// If no saved settings exist, set the defaults.
		chan->Enable(false);

		chan->SetUserVolume({1.0f, 1.0f});

		// We're only dealing with stereo channels internally, so we
		// need to set the "stereo" line-out even for mono content.
		chan->SetChannelMap(Stereo);

		set_global_crossfeed(chan);
		set_global_reverb(chan);
		set_global_chorus(chan);
	}

	MIXER_LockAudioDevice();
	mixer.channels[name] = chan; // replace the old, if it exists
	MIXER_UnlockAudioDevice();

	return chan;
}

mixer_channel_t MIXER_FindChannel(const char* name)
{
	MIXER_LockAudioDevice();

	auto it = mixer.channels.find(name);

	if (it == mixer.channels.end()) {
		// Deprecated alias SPKR to PCSPEAKER
		if (std::string_view(name) == "SPKR") {
			LOG_WARNING("MIXER: 'SPKR' is deprecated due to inconsistent "
			            "naming, please use 'PCSPEAKER' instead");
			it = mixer.channels.find(ChannelName::PcSpeaker);

			// Deprecated alias FM to OPL
		} else if (std::string_view(name) == "FM") {
			LOG_WARNING("MIXER: 'FM' is deprecated due to inconsistent "
			            "naming, please use '%s' instead",
			            ChannelName::Opl);
			it = mixer.channels.find(ChannelName::Opl);
		}
	}

	const auto chan = (it != mixer.channels.end()) ? it->second : nullptr;

	MIXER_UnlockAudioDevice();

	return chan;
}

std::map<std::string, mixer_channel_t>& MIXER_GetChannels()
{
	return mixer.channels;
}

void MixerChannel::Set0dbScalar(const float scalar)
{
	// Realistically we expect some channels might need a fixed boost
	// to get to 0dB, but others might need a range mapping, like from
	// a unity float [-1.0f, +1.0f] to  16-bit int [-32k,+32k] range.
	assert(scalar >= 0.0f && scalar <= static_cast<int16_t>(Max16BitSampleValue));

	db0_volume_scalar = scalar;

	RecalcCombinedVolume();
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

const AudioFrame MixerChannel::GetUserVolume() const
{
	return user_volume_scalar;
}

void MixerChannel::SetUserVolume(const AudioFrame volume)
{
	// Allow unconstrained user-defined values
	user_volume_scalar = volume;
	RecalcCombinedVolume();
}

const AudioFrame MixerChannel::GetAppVolume() const
{
	return app_volume_scalar;
}

void MixerChannel::SetAppVolume(const AudioFrame volume)
{
	// Constrain application-defined volume between 0% and 100%
	auto clamp_to_unity = [](const float vol) {
		constexpr auto min_unity_volume = 0.0f;
		constexpr auto max_unity_volume = 1.0f;
		return clamp(vol, min_unity_volume, max_unity_volume);
	};
	app_volume_scalar = {clamp_to_unity(volume.left),
	                     clamp_to_unity(volume.right)};
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

const AudioFrame MIXER_GetMasterVolume()
{
	return mixer.master_volume;
}

void MIXER_SetMasterVolume(const AudioFrame volume)
{
	mixer.master_volume = volume;

	for (const auto& [_, channel] : mixer.channels) {
		channel->RecalcCombinedVolume();
	}
}

void MixerChannel::SetChannelMap(const StereoLine map)
{
	assert(map.left == Left || map.left == Right);
	assert(map.right == Left || map.right == Right);
	channel_map = map;

#ifdef DEBUG
	LOG_MSG("MIXER: %-7s channel: application changed audio channel mapping to left=>%s and right=>%s",
	        name,
	        channel_map.left == Left ? "left" : "right",
	        channel_map.right == Left ? "left" : "right");
#endif
}

void MixerChannel::Enable(const bool should_enable)
{
	// Is the channel already in the desired state?
	if (is_enabled == should_enable) {
		return;
	}

	// Lock the channel before changing states
	MIXER_LockAudioDevice();

	// Prepare the channel to accept samples
	if (should_enable) {
		freq_counter = 0u;
		// Don't start with a deficit
		if (frames_done < mixer.frames_done) {
			frames_done = mixer.frames_done.load();
		}

		// Prepare the channel to go dormant
	} else {
		// Clear the current counters and sample values to
		// start clean if/when this channel is re-enabled.
		// Samples can be buffered into disable channels, so
		// we don't perform this zero'ing in the enable phase.
		frames_done   = 0u;
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
//   - Linear interpolation resampling only if
//     channel_rate_hz != mixer_rate_hz
//
// ZeroOrderHoldAndResample
// ------------------------
//   - neither ZoH upsampling nor Speex resampling if:
//         channel_rate_hz >= zoh_target_rate_hz AND
//         channel_rate_hz _hz== mixer_rate_hz
//
//   - ZoH upsampling only if:
//         channel_rate_hz < zoh_target_freq_hz AND
//         zoh_target_rate_hz == mixer_rate_hz
//
//   - Speex resampling only if:
//         channel_rate_hz >= zoh_target_freq_hz AND
//         channel_rate_hz != mixer_rate_hz
//
//   - both ZoH upsampling and Speex resampling if:
//         channel_rate_hz < zoh_target_rate_hz AND
//         zoh_target_rate_hz != mixer_rate_hz
//
// Resample
// --------
//   - Speex resampling only if channel_rate != mixer_rate
//
void MixerChannel::ConfigureResampler()
{
	const auto channel_rate_hz = sample_rate_hz;
	const auto mixer_rate_hz   = mixer.sample_rate_hz.load();

	do_resample     = false;
	do_zoh_upsample = false;

	switch (resample_method) {
	case ResampleMethod::LinearInterpolation:
		do_resample = (channel_rate_hz != mixer_rate_hz);
		if (do_resample) {
			InitLerpUpsamplerState();
#ifdef DEBUG_MIXER
			LOG_DEBUG("%s: Linear interpolation resampler is on",
			          name.c_str());
#endif
		}
		break;

	case ResampleMethod::ZeroOrderHoldAndResample:
		do_zoh_upsample = (channel_rate_hz < zoh_upsampler.target_rate_hz);
		if (do_zoh_upsample) {
			InitZohUpsamplerState();
#ifdef DEBUG_MIXER
			LOG_DEBUG("%s: Zero-order-hold upsampler is on, target rate: %d Hz ",
			          name.c_str(),
			          zoh_upsampler.target_rate_hz);
#endif
		}
		[[fallthrough]];

	case ResampleMethod::Resample:
		const spx_uint32_t in_rate_hz  = do_zoh_upsample
		                                       ? zoh_upsampler.target_rate_hz
		                                       : check_cast<spx_uint32_t>(
                                                                channel_rate_hz);
		const spx_uint32_t out_rate_hz = mixer_rate_hz;

		do_resample = (in_rate_hz != out_rate_hz);
		if (!do_resample) {
			return;
		}

		if (!speex_resampler.state) {
			constexpr auto num_channels = 2; // always stereo
			constexpr auto quality      = 5;

			speex_resampler.state = speex_resampler_init(num_channels,
			                                             in_rate_hz,
			                                             out_rate_hz,
			                                             quality,
			                                             nullptr);
		}
		speex_resampler_set_rate(speex_resampler.state, in_rate_hz, out_rate_hz);

#ifdef DEBUG_MIXER
		LOG_DEBUG("%s: Speex resampler is on, input rate: %d Hz, output rate: %d Hz)",
		          name.c_str(),
		          in_rate_hz,
		          out_rate_hz);
#endif
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

#ifdef DEBUG_MIXER
			LOG_DEBUG("%s: Speex resampler cleared and primed %d-frame input queue",
			          name.c_str(),
			          speex_resampler_get_input_latency(
			                  speex_resampler.state));
#endif
		}
		break;
	}
}

void MixerChannel::SetSampleRate(const uint16_t new_sample_rate_hz)
{
	// If the requested rate is zero, then avoid resampling by running the
	// channel at the mixer's rate
	const uint16_t target_rate_hz = new_sample_rate_hz
	                                      ? new_sample_rate_hz
	                                      : mixer.sample_rate_hz.load();
	assert(target_rate_hz > 0);

	// Nothing to do: the channel is already running at the requested rate
	if (target_rate_hz == sample_rate_hz) {
		return;
	}

#ifdef DEBUG_MIXER
	LOG_DEBUG("%s: Changing rate from %d to %d Hz",
	          name.c_str(),
	          sample_rate_hz,
	          target_rate_hz);
#endif

	sample_rate_hz = target_rate_hz;

	freq_add = (sample_rate_hz << FreqShift) / mixer.sample_rate_hz;

	envelope.Update(sample_rate_hz,
	                peak_amplitude,
	                EnvelopeMaxExpansionOverMs,
	                EnvelopeExpiresAfterSeconds);

	ConfigureResampler();
}

const std::string& MixerChannel::GetName() const
{
	return name;
}

uint16_t MixerChannel::GetSampleRate() const
{
	return check_cast<uint16_t>(sample_rate_hz);
}

void MixerChannel::SetPeakAmplitude(const int peak)
{
	peak_amplitude = peak;
	envelope.Update(sample_rate_hz,
	                peak_amplitude,
	                EnvelopeMaxExpansionOverMs,
	                EnvelopeExpiresAfterSeconds);
}

void MixerChannel::Mix(const uint16_t frames_requested)
{
	if (!is_enabled) {
		return;
	}

	frames_needed = frames_requested;

	while (frames_needed > frames_done) {
		auto frames_remaining = frames_needed - frames_done;
		frames_remaining *= freq_add;
		frames_remaining = (frames_remaining >> FreqShift) +
		                   ((frames_remaining & FreqMask) != 0);

		// avoid underflow
		if (frames_remaining <= 0) {
			break;
		}
		// avoid overflow
		frames_remaining = std::min(clamp_to_uint16(frames_remaining),
		                            MixerBufferLength);

		handler(static_cast<work_index_t>(frames_remaining));
	}

	if (do_sleep) {
		sleeper.MaybeSleep();
	}
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
			freq_counter = FreqNext;
		} else {
			bool stereo = last_samples_were_stereo;

			const auto mapped_output_left  = output_map.left;
			const auto mapped_output_right = output_map.right;

			// Position where to write the data
			auto mixpos = check_cast<work_index_t>(
			        (mixer.pos + frames_done) & MixerBufferMask);

			while (frames_done < frames_needed) {
				// Fade gradually to silence to avoid clicks.
				// Maybe the fade factor f depends on the sample
				// rate.
				constexpr auto f = 4.0f;

				for (size_t ch = 0; ch < 2; ++ch) {
					if (prev_frame[ch] > f) {
						next_frame[ch] = prev_frame[ch] - f;
					} else if (prev_frame[ch] < -f) {
						next_frame[ch] = prev_frame[ch] + f;
					} else {
						next_frame[ch] = 0.0f;
					}
				}

				mixer.work[mixpos][mapped_output_left] +=
				        prev_frame.left * combined_volume_scalar.left;

				mixer.work[mixpos][mapped_output_right] +=
				        (stereo ? prev_frame.right : prev_frame.left) *
				        combined_volume_scalar.right;

				prev_frame = next_frame;

				mixpos = static_cast<work_index_t>(
				        (mixpos + 1) & MixerBufferMask);
				frames_done++;
				freq_counter = FreqNext;
			}
		}
	}
	last_samples_were_silence = true;

	MIXER_UnlockAudioDevice();
}

static void log_filter_settings(const std::string& channel_name,
                                const std::string& filter_name,
                                const FilterState state, const uint8_t order,
                                const uint16_t cutoff_freq_hz)
{
	// we programmatically expect only 'on' and 'forced-on' states:
	assert(state != FilterState::Off);
	assert(state == FilterState::On || state == FilterState::ForcedOn);

	constexpr auto db_per_order = 6;

	LOG_MSG("%s: %s filter enabled%s (%d dB/oct at %u Hz)",
	        channel_name.c_str(),
	        filter_name.c_str(),
	        state == FilterState::ForcedOn ? " (forced)" : "",
	        order * db_per_order,
	        cutoff_freq_hz);
}

void MixerChannel::SetHighPassFilter(const FilterState state)
{
	do_highpass_filter = state != FilterState::Off;

	if (do_highpass_filter) {
		log_filter_settings(name,
		                    "High-pass",
		                    state,
		                    filters.highpass.order,
		                    filters.highpass.cutoff_freq_hz);
	}
}

void MixerChannel::SetLowPassFilter(const FilterState state)
{
	do_lowpass_filter = state != FilterState::Off;

	if (do_lowpass_filter) {
		log_filter_settings(name,
		                    "Low-pass",
		                    state,
		                    filters.lowpass.order,
		                    filters.lowpass.cutoff_freq_hz);
	}
}

static uint16_t clamp_filter_cutoff_freq([[maybe_unused]] const std::string& channel_name,
                                         const uint16_t cutoff_freq_hz)
{
	const auto max_cutoff_freq_hz = check_cast<uint16_t>(
	        mixer.sample_rate_hz / 2 - 1);

	if (cutoff_freq_hz <= max_cutoff_freq_hz) {
		return cutoff_freq_hz;
	} else {
		LOG_DEBUG(
		        "%s: Filter cutoff frequency %d Hz is not below half of the "
		        "sample rate, clamping to %d Hz",
		        channel_name.c_str(),
		        cutoff_freq_hz,
		        max_cutoff_freq_hz);

		return max_cutoff_freq_hz;
	}
}

void MixerChannel::ConfigureHighPassFilter(const uint8_t order,
                                           const uint16_t _cutoff_freq_hz)
{
	const auto cutoff_freq_hz = clamp_filter_cutoff_freq(name, _cutoff_freq_hz);

	assert(order > 0 && order <= max_filter_order);
	for (auto& f : filters.highpass.hpf) {
		f.setup(order, mixer.sample_rate_hz, cutoff_freq_hz);
	}

	filters.highpass.order          = order;
	filters.highpass.cutoff_freq_hz = cutoff_freq_hz;
}

void MixerChannel::ConfigureLowPassFilter(const uint8_t order,
                                          const uint16_t _cutoff_freq_hz)
{
	const auto cutoff_freq_hz = clamp_filter_cutoff_freq(name, _cutoff_freq_hz);

	assert(order > 0 && order <= max_filter_order);
	for (auto& f : filters.lowpass.lpf) {
		f.setup(order, mixer.sample_rate_hz, cutoff_freq_hz);
	}

	filters.lowpass.order          = order;
	filters.lowpass.cutoff_freq_hz = cutoff_freq_hz;
}

// Tries to set custom filter settings from the passed in filter preferences.
// Returns true if the custom filters could be successfully set, false
// otherwise and disables all filters for the channel.
bool MixerChannel::TryParseAndSetCustomFilter(const std::string& filter_prefs)
{
	SetLowPassFilter(FilterState::Off);
	SetHighPassFilter(FilterState::Off);

	if (!(filter_prefs.starts_with("lpf") || filter_prefs.starts_with("hpf"))) {
		return false;
	}

	const auto parts = split_with_empties(filter_prefs, ' ');

	const auto single_filter = (parts.size() == 3);
	const auto dual_filter   = (parts.size() == 6);

	if (!(single_filter || dual_filter)) {
		LOG_WARNING("%s: Invalid custom filter definition: '%s'. Must be "
		            "specified in \"lfp|hpf ORDER CUTOFF_FREQUENCY\" format",
		            name.c_str(),
		            filter_prefs.c_str());
		return false;
	}

	auto set_filter = [&](const std::string& type_pref,
	                      const std::string& order_pref,
	                      const std::string& cutoff_freq_pref) {
		const auto filter_name = (type_pref == "lpf") ? "low-pass"
		                                              : "high-pass";

		int order;
		if (!sscanf(order_pref.c_str(), "%d", &order) || order < 1 ||
		    order > max_filter_order) {
			LOG_WARNING(
			        "%s: Invalid custom %s filter order: '%s'. "
			        "Must be an integer between 1 and %d.",
			        name.c_str(),
			        filter_name,
			        order_pref.c_str(),
			        max_filter_order);
			return false;
		}

		uint16_t cutoff_freq_hz;
		if (!sscanf(cutoff_freq_pref.c_str(), "%" SCNu16, &cutoff_freq_hz) ||
		    cutoff_freq_hz <= 0) {
			LOG_WARNING(
			        "%s: Invalid custom %s filter cutoff frequency: '%s'. "
			        "Must be a positive number.",
			        name.c_str(),
			        filter_name,
			        cutoff_freq_pref.c_str());
			return false;
		}

		if (type_pref == "lpf") {
			ConfigureLowPassFilter(check_cast<uint8_t>(order),
			                       cutoff_freq_hz);
			SetLowPassFilter(FilterState::On);

		} else if (type_pref == "hpf") {
			ConfigureHighPassFilter(check_cast<uint8_t>(order),
			                        cutoff_freq_hz);
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
		size_t i = 0;

		const auto filter_type           = parts[i++];
		const auto filter_order          = parts[i++];
		const auto filter_cutoff_freq_hz = parts[i++];

		return set_filter(filter_type, filter_order, filter_cutoff_freq_hz);
	} else {
		size_t i = 0;

		const auto filter1_type           = parts[i++];
		const auto filter1_order          = parts[i++];
		const auto filter1_cutoff_freq_hz = parts[i++];

		const auto filter2_type           = parts[i++];
		const auto filter2_order          = parts[i++];
		const auto filter2_cutoff_freq_hz = parts[i++];

		if (filter1_type == filter2_type) {
			LOG_WARNING("%s: Invalid custom filter definition: '%s'. "
			            "The two filters must be of different types.",
			            name.c_str(),
			            filter_prefs.c_str());
			return false;
		}

		if (!set_filter(filter1_type, filter1_order, filter1_cutoff_freq_hz)) {
			return false;
		}

		return set_filter(filter2_type, filter2_order, filter2_cutoff_freq_hz);
	}
}

void MixerChannel::SetZeroOrderHoldUpsamplerTargetRate(const uint16_t target_rate_hz)
{
	// TODO make sure that the ZOH target frequency cannot be set after the
	// filter has been configured
	zoh_upsampler.target_rate_hz = target_rate_hz;

	ConfigureResampler();
}

void MixerChannel::InitZohUpsamplerState()
{
	zoh_upsampler.step = std::min(static_cast<float>(sample_rate_hz) /
	                                      zoh_upsampler.target_rate_hz,
	                              1.0f);
	zoh_upsampler.pos  = 0.0f;
}

void MixerChannel::InitLerpUpsamplerState()
{
	lerp_upsampler.step = std::min(static_cast<float>(sample_rate_hz) /
	                                       mixer.sample_rate_hz.load(),
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
#ifdef DEBUG_MIXER
		LOG_DEBUG("%s: Crossfeed is off", name.c_str());
#endif
		crossfeed.strength = 0.0f;
		return;
	}

	crossfeed.strength = strength;

	// map [0, 1] range to [0.5, 0]
	auto p                = (1.0f - strength) / 2.0f;
	constexpr auto center = 0.5f;
	crossfeed.pan_left    = center - p;
	crossfeed.pan_right   = center + p;

#ifdef DEBUG_MIXER
	LOG_DEBUG("%s: Crossfeed strength: %.3f",
	          name.c_str(),
	          static_cast<double>(crossfeed.strength));
#endif
}

float MixerChannel::GetCrossfeedStrength() const
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
#ifdef DEBUG_MIXER
		LOG_DEBUG("%s: Reverb send is off", name.c_str());
#endif
		reverb.level     = level_min;
		reverb.send_gain = level_min_db;
		return;
	}

	reverb.level = level;

	const auto level_db = remap(level_min, level_max, level_min_db, level_max_db, level);

	reverb.send_gain = static_cast<float>(decibel_to_gain(level_db));

#ifdef DEBUG_MIXER
	LOG_DEBUG("%s: Reverb send is on: level: %4.2f, level_db: %6.2f, gain: %4.2f",
	          name.c_str(),
	          level,
	          level_db,
	          reverb.send_gain);
#endif
}

float MixerChannel::GetReverbLevel() const
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
#ifdef DEBUG_MIXER
		LOG_DEBUG("%s: Chorus send is off", name.c_str());
#endif
		chorus.level     = level_min;
		chorus.send_gain = level_min_db;
		return;
	}

	chorus.level = level;

	const auto level_db = remap(level_min, level_max, level_min_db, level_max_db, level);

	chorus.send_gain = static_cast<float>(decibel_to_gain(level_db));

#ifdef DEBUG_MIXER
	LOG_DEBUG("%s: Chorus send is on: level: %4.2f, level_db: %6.2f, gain: %4.2f",
	          name.c_str(),
	          level,
	          level_db,
	          chorus.send_gain);
#endif
}

float MixerChannel::GetChorusLevel() const
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
		constexpr auto scalar = Max16BitSampleValue / 127.0;
		return static_cast<int16_t>(round(s_val * scalar));
	}
	return static_cast<int16_t>(s_val * 256);
}

// 8-bit to 16-bit lookup tables
int16_t lut_u8to16[UINT8_MAX + 1] = {};
constexpr int16_t* lut_s8to16     = lut_u8to16 + 128;

constexpr void fill_8to16_lut()
{
	for (int i = 0; i <= UINT8_MAX; ++i) {
		lut_u8to16[i] = u8to16(i);
	}
}

template <class Type, bool stereo, bool signeddata, bool nativeorder>
AudioFrame MixerChannel::ConvertNextFrame(const Type* data, const work_index_t pos)
{
	AudioFrame frame = {};

	const auto left_pos  = static_cast<work_index_t>(pos * 2 + 0);
	const auto right_pos = static_cast<work_index_t>(pos * 2 + 1);

	if (sizeof(Type) == 1) {
		// Integer types
		// unsigned 8-bit
		if (!signeddata) {
			if (stereo) {
				frame[0] = lut_u8to16[static_cast<uint8_t>(data[left_pos])];
				frame[1] = lut_u8to16[static_cast<uint8_t>(data[right_pos])];
			} else {
				frame[0] = lut_u8to16[static_cast<uint8_t>(data[pos])];
			}
		}
		// signed 8-bit
		else {
			if (stereo) {
				frame[0] = lut_s8to16[static_cast<int8_t>(data[left_pos])];
				frame[1] = lut_s8to16[static_cast<int8_t>(data[right_pos])];
			} else {
				frame[0] = lut_s8to16[static_cast<int8_t>(data[pos])];
			}
		}
	} else {
		// 16-bit and 32-bit both contain 16-bit data internally
		if (signeddata) {
			if (stereo) {
				if (nativeorder) {
					frame[0] = static_cast<float>(data[left_pos]);
					frame[1] = static_cast<float>(data[right_pos]);
				} else {
					auto host_pt0 = reinterpret_cast<const uint8_t* const>(
					        data + left_pos);
					auto host_pt1 = reinterpret_cast<const uint8_t* const>(
					        data + right_pos);
					if (sizeof(Type) == 2) {
						frame[0] = (int16_t)host_readw(host_pt0);
						frame[1] = (int16_t)host_readw(host_pt1);
					} else {
						frame[0] = static_cast<float>(
						        (int32_t)host_readd(host_pt0));
						frame[1] = static_cast<float>(
						        (int32_t)host_readd(host_pt1));
					}
				}
			} else { // mono
				if (nativeorder) {
					frame[0] = static_cast<float>(data[pos]);
				} else {
					auto host_pt = reinterpret_cast<const uint8_t* const>(
					        data + pos);
					if (sizeof(Type) == 2) {
						frame[0] = (int16_t)host_readw(host_pt);
					} else {
						frame[0] = static_cast<float>(
						        (int32_t)host_readd(host_pt));
					}
				}
			}
		} else { // unsigned
			const auto offs = 32768;
			if (stereo) {
				if (nativeorder) {
					frame[0] = static_cast<float>(
					        static_cast<int>(data[left_pos]) -
					        offs);
					frame[1] = static_cast<float>(
					        static_cast<int>(data[right_pos]) -
					        offs);
				} else {
					auto host_pt0 = reinterpret_cast<const uint8_t* const>(
					        data + left_pos);
					auto host_pt1 = reinterpret_cast<const uint8_t* const>(
					        data + right_pos);
					if (sizeof(Type) == 2) {
						frame[0] = static_cast<float>(
						        static_cast<int>(host_readw(
						                host_pt0)) -
						        offs);
						frame[1] = static_cast<float>(
						        static_cast<int>(host_readw(
						                host_pt1)) -
						        offs);
					} else {
						frame[0] = static_cast<float>(
						        static_cast<int>(host_readd(
						                host_pt0)) -
						        offs);
						frame[1] = static_cast<float>(
						        static_cast<int>(host_readd(
						                host_pt1)) -
						        offs);
					}
				}
			} else { // mono
				if (nativeorder) {
					frame[0] = static_cast<float>(
					        static_cast<int>(data[pos]) - offs);
				} else {
					auto host_pt = reinterpret_cast<const uint8_t* const>(
					        data + pos);
					if (sizeof(Type) == 2) {
						frame[0] = static_cast<float>(
						        static_cast<int>(host_readw(
						                host_pt)) -
						        offs);
					} else {
						frame[0] = static_cast<float>(
						        static_cast<int>(host_readd(
						                host_pt)) -
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
void MixerChannel::ConvertSamples(const Type* data, const uint16_t frames,
                                  std::vector<float>& out)
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
				next_frame.left = static_cast<float>(
				        data[pos * 2 + 0]);
				next_frame.right = static_cast<float>(
				        data[pos * 2 + 1]);
			} else {
				next_frame.left = static_cast<float>(data[pos]);
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

spx_uint32_t estimate_max_out_frames(SpeexResamplerState* resampler_state,
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

AudioFrame MixerChannel::ApplyCrossfeed(const AudioFrame frame) const
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

// Returns true if configuration succeeded and false otherwise
bool MixerChannel::ConfigureFadeOut(const std::string& prefs)
{
	return sleeper.ConfigureFadeOut(prefs);
}

// Returns true if configuration succeeded and false otherwise
bool MixerChannel::Sleeper::ConfigureFadeOut(const std::string& prefs)
{
	auto set_wait_and_fade = [&](const int wait_ms, const int fade_ms) {
		fadeout_or_sleep_after_ms = check_cast<uint16_t>(wait_ms);

		fadeout_decrement_per_ms = 1.0f / static_cast<uint16_t>(fade_ms);

		LOG_MSG("%s: Fade-out enabled (wait %d ms then fade for %d ms)",
		        channel.GetName().c_str(),
		        wait_ms,
		        fade_ms);
	};
	// Disable fade-out (default)
	if (has_false(prefs)) {
		wants_fadeout = false;
		return true;
	}
	// Enable fade-out with defaults
	if (has_true(prefs)) {
		set_wait_and_fade(default_wait_ms, default_wait_ms);
		wants_fadeout = true;
		return true;
	}

	// Let the fade-out last between 10 ms and 3 seconds.
	constexpr auto min_fade_ms = 10;
	constexpr auto max_fade_ms = 3000;

	// Custom setting in 'WAIT FADE' syntax, where both are milliseconds.
	if (auto prefs_vec = split(prefs); prefs_vec.size() == 2) {
		const auto wait_ms = parse_int(prefs_vec[0]);
		const auto fade_ms = parse_int(prefs_vec[1]);
		if (wait_ms && fade_ms) {
			const auto wait_is_valid = (*wait_ms >= min_wait_ms &&
			                            *wait_ms <= max_wait_ms);

			const auto fade_is_valid = (*fade_ms >= min_fade_ms &&
			                            *fade_ms <= max_fade_ms);

			if (wait_is_valid && fade_is_valid) {
				set_wait_and_fade(*wait_ms, *fade_ms);
				wants_fadeout = true;
				return true;
			}
		}
	}
	// Otherwise inform the user and disable the fade
	LOG_WARNING("%s: Invalid custom fade-out definition: '%s'. Must be "
	            "specified in \"WAIT FADE\" format where WAIT is between "
	            "%d and %d (in milliseconds) and FADE is between %d and "
	            "%d (in milliseconds); using 'off'.",
	            channel.GetName().c_str(),
	            prefs.c_str(),
	            min_wait_ms,
	            max_wait_ms,
	            min_fade_ms,
	            max_fade_ms);

	wants_fadeout = false;
	return false;
}

void MixerChannel::Sleeper::DecrementFadeLevel(const int64_t awake_for_ms)
{
	assert(awake_for_ms >= fadeout_or_sleep_after_ms);
	const auto elapsed_fade_ms = static_cast<float>(
	        awake_for_ms - fadeout_or_sleep_after_ms);

	const auto decrement = fadeout_decrement_per_ms * elapsed_fade_ms;

	constexpr auto min_level = 0.0f;
	constexpr auto max_level = 1.0f;
	fadeout_level = std::clamp(max_level - decrement, min_level, max_level);
}

MixerChannel::Sleeper::Sleeper(MixerChannel& c, const uint16_t sleep_after_ms)
        : channel(c),
          fadeout_or_sleep_after_ms(sleep_after_ms)
{
	// The constructed sleep period is programmatically controlled (so assert)
	assert(fadeout_or_sleep_after_ms >= min_wait_ms);
	assert(fadeout_or_sleep_after_ms <= max_wait_ms);
}

// Either fades the frame or checks if the channel had any signal output.
AudioFrame MixerChannel::Sleeper::MaybeFadeOrListen(const AudioFrame& frame)
{
	if (wants_fadeout) {
		// When fading, we actively drive down the channel level
		return {frame.left * fadeout_level, frame.right * fadeout_level};
	}
	if (!had_signal) {
		// Otherwise, we simply passively listen for signal
		constexpr auto silence_threshold = 1.0f;
		had_signal = fabsf(frame.left) > silence_threshold ||
		             fabsf(frame.right) > silence_threshold;
	}
	return frame;
}

void MixerChannel::Sleeper::MaybeSleep()
{
	const auto awake_for_ms = GetTicksSince(woken_at_ms);
	// Not enough time has passed.. .. try to sleep later
	if (awake_for_ms < fadeout_or_sleep_after_ms) {
		return;
	}
	if (wants_fadeout) {
		// The channel is still fading out.. try to sleep later
		if (fadeout_level > 0.0f) {
			DecrementFadeLevel(awake_for_ms);
			return;
		}
	} else if (had_signal) {
		// The channel is still producing a signal.. so stay away
		WakeUp();
		return;
	}
	channel.Enable(false);
	// LOG_INFO("MIXER: %s fell asleep", channel.name.c_str());
}

// Returns true when actually awoken otherwise false if already awake.
bool MixerChannel::Sleeper::WakeUp()
{
	// Always reset for another round of awakeness
	woken_at_ms = GetTicks();
	fadeout_level = 1.0f;
	had_signal    = false;

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
void MixerChannel::AddSamples(const uint16_t frames, const Type* data)
{
	assert(frames > 0);

	last_samples_were_stereo = stereo;

	auto& convert_out = do_resample ? mixer.resample_temp : mixer.resample_out;
	ConvertSamples<Type, stereo, signeddata, nativeorder>(data, frames, convert_out);

	if (do_resample) {
		switch (resample_method) {
		case ResampleMethod::LinearInterpolation: {
			auto& s = lerp_upsampler;

			auto in_pos = mixer.resample_temp.begin();
			auto& out   = mixer.resample_out;
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

#ifdef DEBUG_MIXER
				LOG_DEBUG("%s: AddSamples last %.1f:%.1f curr %.1f:%.1f"
				          " -> out %.1f:%.1f, pos=%.2f, step=%.2f",
				          name.c_str(),
				          s.last_frame.left,
				          s.last_frame.right,
				          curr_frame.left,
				          curr_frame.right,
				          out_left,
				          out_right,
				          s.pos,
				          s.step);
#endif

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
	const uint16_t out_frames = static_cast<uint16_t>(mixer.resample_out.size()) /
	                            2;

	auto pos = mixer.resample_out.begin();

	auto mixpos = check_cast<work_index_t>((mixer.pos + frames_done) &
	                                       MixerBufferMask);

	while (pos != mixer.resample_out.end()) {
		AudioFrame frame = {*pos++, *pos++};

		if (do_highpass_filter) {
			frame.left = filters.highpass.hpf[0].filter(frame.left);
			frame.right = filters.highpass.hpf[1].filter(frame.right);
		}
		if (do_lowpass_filter) {
			frame.left = filters.lowpass.lpf[0].filter(frame.left);
			frame.right = filters.lowpass.lpf[1].filter(frame.right);
		}
		if (do_crossfeed) {
			frame = ApplyCrossfeed(frame);
		}

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

		if (do_sleep) {
			frame = sleeper.MaybeFadeOrListen(frame);
		}

		// Mix samples to the master output
		mixer.work[mixpos][0] += frame.left;
		mixer.work[mixpos][1] += frame.right;

		mixpos = static_cast<work_index_t>((mixpos + 1) & MixerBufferMask);
	}
	frames_done += out_frames;

	MIXER_UnlockAudioDevice();
}

void MixerChannel::AddStretched(const uint16_t len, int16_t* data)
{
	MIXER_LockAudioDevice();

	if (frames_done >= frames_needed) {
		LOG_MSG("Can't add, buffer full");
		MIXER_UnlockAudioDevice();
		return;
	}
	// Target samples this inputs gets stretched into
	auto frames_remaining = frames_needed - frames_done;

	auto index     = 0;
	auto index_add = (len << FreqShift) / frames_remaining;

	auto mixpos = check_cast<work_index_t>((mixer.pos + frames_done) &
	                                       MixerBufferMask);

	auto pos = 0;

	// read-only aliases to avoid dereferencing and inform compiler their
	// values don't change
	const auto mapped_output_left  = output_map.left;
	const auto mapped_output_right = output_map.right;

	while (frames_remaining--) {
		const auto new_pos = index >> FreqShift;
		if (pos != new_pos) {
			pos = new_pos;
			// Forward the previous sample
			prev_frame.left = data[0];
			data++;
		}

		assert(prev_frame.left <= Max16BitSampleValue);
		assert(prev_frame.left >= Min16BitSampleValue);
		const auto diff = data[0] - static_cast<int16_t>(prev_frame.left);

		const auto diff_mul = index & FreqMask;
		index += index_add;

		auto sample = prev_frame.left +
		              static_cast<float>((diff * diff_mul) >> FreqShift);

		AudioFrame frame_with_gain = {sample * combined_volume_scalar.left,
		                              sample * combined_volume_scalar.right};

		if (do_sleep) {
			frame_with_gain = sleeper.MaybeFadeOrListen(frame_with_gain);
		}

		mixer.work[mixpos][mapped_output_left] += frame_with_gain.left;
		mixer.work[mixpos][mapped_output_right] += frame_with_gain.right;

		mixpos = static_cast<work_index_t>((mixpos + 1) & MixerBufferMask);
	}

	frames_done = frames_needed;

	MIXER_UnlockAudioDevice();
}

void MixerChannel::AddSamples_m8(const uint16_t len, const uint8_t* data)
{
	AddSamples<uint8_t, false, false, true>(len, data);
}
void MixerChannel::AddSamples_s8(const uint16_t len, const uint8_t* data)
{
	AddSamples<uint8_t, true, false, true>(len, data);
}
void MixerChannel::AddSamples_m8s(const uint16_t len, const int8_t* data)
{
	AddSamples<int8_t, false, true, true>(len, data);
}
void MixerChannel::AddSamples_s8s(const uint16_t len, const int8_t* data)
{
	AddSamples<int8_t, true, true, true>(len, data);
}
void MixerChannel::AddSamples_m16(const uint16_t len, const int16_t* data)
{
	AddSamples<int16_t, false, true, true>(len, data);
}
void MixerChannel::AddSamples_s16(const uint16_t len, const int16_t* data)
{
	AddSamples<int16_t, true, true, true>(len, data);
}
void MixerChannel::AddSamples_m16u(const uint16_t len, const uint16_t* data)
{
	AddSamples<uint16_t, false, false, true>(len, data);
}
void MixerChannel::AddSamples_s16u(const uint16_t len, const uint16_t* data)
{
	AddSamples<uint16_t, true, false, true>(len, data);
}
void MixerChannel::AddSamples_m32(const uint16_t len, const int32_t* data)
{
	AddSamples<int32_t, false, true, true>(len, data);
}
void MixerChannel::AddSamples_s32(const uint16_t len, const int32_t* data)
{
	AddSamples<int32_t, true, true, true>(len, data);
}
void MixerChannel::AddSamples_mfloat(const uint16_t len, const float* data)
{
	AddSamples<float, false, true, true>(len, data);
}
void MixerChannel::AddSamples_sfloat(const uint16_t len, const float* data)
{
	AddSamples<float, true, true, true>(len, data);
}
void MixerChannel::AddSamples_m16_nonnative(const uint16_t len, const int16_t* data)
{
	AddSamples<int16_t, false, true, false>(len, data);
}
void MixerChannel::AddSamples_s16_nonnative(const uint16_t len, const int16_t* data)
{
	AddSamples<int16_t, true, true, false>(len, data);
}
void MixerChannel::AddSamples_m16u_nonnative(const uint16_t len, const uint16_t* data)
{
	AddSamples<uint16_t, false, false, false>(len, data);
}
void MixerChannel::AddSamples_s16u_nonnative(const uint16_t len, const uint16_t* data)
{
	AddSamples<uint16_t, true, false, false>(len, data);
}
void MixerChannel::AddSamples_m32_nonnative(const uint16_t len, const int32_t* data)
{
	AddSamples<int32_t, false, true, false>(len, data);
}
void MixerChannel::AddSamples_s32_nonnative(const uint16_t len, const int32_t* data)
{
	AddSamples<int32_t, true, true, false>(len, data);
}

void MixerChannel::FillUp()
{
	if (!is_enabled || frames_done < mixer.frames_done) {
		return;
	}
	const auto index = PIC_TickIndex();

	auto frames_remaining = static_cast<int>(index * mixer.frames_needed);
	while (frames_remaining > 0) {
		const auto frames_to_mix = std::clamp(
		        frames_remaining, 0, static_cast<int>(MixerBufferLength));

		MIXER_LockAudioDevice();
		Mix(check_cast<work_index_t>(frames_to_mix));
		MIXER_UnlockAudioDevice();
		// Let SDL fetch frames after each chunk

		frames_remaining = -frames_to_mix;
	}
}

std::string MixerChannel::DescribeLineout() const
{
	if (!HasFeature(ChannelFeature::Stereo)) {
		return MSG_Get("SHELL_CMD_MIXER_CHANNEL_MONO");
	}
	if (output_map == Stereo) {
		return MSG_Get("SHELL_CMD_MIXER_CHANNEL_STEREO");
	}
	if (output_map == Reverse) {
		return MSG_Get("SHELL_CMD_MIXER_CHANNEL_REVERSE");
	}

	// Output_map is programmtically set (not directly assigned from user
	// data), so we can assert.
	assertm(false, "Unknown lineout mode");
	return "unknown";
}

MixerChannelSettings MixerChannel::GetSettings() const
{
	MixerChannelSettings s = {};

	s.is_enabled         = is_enabled;
	s.user_volume        = GetUserVolume();
	s.lineout_map        = GetLineoutMap();
	s.crossfeed_strength = GetCrossfeedStrength();
	s.reverb_level       = GetReverbLevel();
	s.chorus_level       = GetChorusLevel();

	return s;
}

void MixerChannel::SetSettings(const MixerChannelSettings& s)
{
	is_enabled = s.is_enabled;

	SetUserVolume(s.user_volume);
	SetLineoutMap(s.lineout_map);

	if (mixer.do_crossfeed) {
		SetCrossfeedStrength(s.crossfeed_strength);
	}
	if (mixer.do_reverb) {
		SetReverbLevel(s.reverb_level);
	}
	if (mixer.do_chorus) {
		SetChorusLevel(s.chorus_level);
	}
}

MixerChannel::~MixerChannel()
{
	if (speex_resampler.state) {
		speex_resampler_destroy(speex_resampler.state);
		speex_resampler.state = nullptr;
	}
}

extern bool ticksLocked;
static inline bool is_mixer_irq_important()
{
	/* In some states correct timing of the irqs is more important than
	 * non stuttering audo */
	return (ticksLocked || CAPTURE_IsCapturingAudio() ||
	        CAPTURE_IsCapturingVideo());
}

static constexpr int calc_tickadd(const int freq)
{
	const auto freq64 = static_cast<int64_t>(freq);
	return check_cast<int>((freq64 << TickShift) / 1000);
}

// Mix a certain amount of new sample frames
static void mix_samples(const int frames_requested)
{
	constexpr auto capture_buf_frames = 1024;

	const auto frames_added = check_cast<work_index_t>(
	        std::min(frames_requested - mixer.frames_done, capture_buf_frames));

	const auto start_pos = check_cast<work_index_t>(
	        (mixer.pos + mixer.frames_done) & MixerBufferMask);

	// Render all channels and accumulate results in the master mixbuffer
	for (const auto& [_, channel] : mixer.channels) {
		channel->Mix(check_cast<work_index_t>(frames_requested));
	}

	if (mixer.do_reverb) {
		// Apply reverb effect to the reverb aux buffer, then mix the
		// results to the master output
		auto pos = start_pos;

		for (work_index_t i = 0; i < frames_added; ++i) {
			AudioFrame frame = {mixer.aux_reverb[pos][0],
			                    mixer.aux_reverb[pos][1]};

			// High-pass filter the reverb input
			for (size_t ch = 0; ch < 2; ++ch) {
				frame[ch] = mixer.reverb.highpass_filter[ch].filter(
				        frame[ch]);
			}

			// MVerb operates on two non-interleaved sample streams
			static float in_left[1]     = {};
			static float in_right[1]    = {};
			static float* reverb_buf[2] = {in_left, in_right};

			in_left[0]  = frame.left;
			in_right[0] = frame.right;

			const auto in         = reverb_buf;
			auto out              = reverb_buf;
			constexpr auto frames = 1;
			mixer.reverb.mverb.process(in, out, frames);

			mixer.work[pos][0] += reverb_buf[0][0];
			mixer.work[pos][1] += reverb_buf[1][0];

			pos = (pos + 1) & MixerBufferMask;
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

			pos = (pos + 1) & MixerBufferMask;
		}
	}

	// Apply high-pass filter to the master output
	{
		auto pos = start_pos;

		for (work_index_t i = 0; i < frames_added; ++i) {
			for (size_t ch = 0; ch < 2; ++ch) {
				mixer.work[pos][ch] = mixer.highpass_filter[ch].filter(
						mixer.work[pos][ch]);
			}
			pos = (pos + 1) & MixerBufferMask;
		}
	}

	if (mixer.do_compressor) {
		// Apply compressor to the master output as the very last step
		auto pos = start_pos;

		for (work_index_t i = 0; i < frames_added; ++i) {
			AudioFrame frame = {mixer.work[pos][0], mixer.work[pos][1]};

			frame = mixer.compressor.Process(frame);

			mixer.work[pos][0] = frame.left;
			mixer.work[pos][1] = frame.right;

			pos = (pos + 1) & MixerBufferMask;
		}
	}

	// Capture audio output if requested
	if (CAPTURE_IsCapturingAudio() || CAPTURE_IsCapturingVideo()) {
		int16_t out[capture_buf_frames][2];
		auto pos = start_pos;

		for (work_index_t i = 0; i < frames_added; i++) {
			const auto left = static_cast<uint16_t>(clamp_to_int16(
			        static_cast<int>(mixer.work[pos][0])));

			const auto right = static_cast<uint16_t>(clamp_to_int16(
			        static_cast<int>(mixer.work[pos][1])));

			out[i][0] = static_cast<int16_t>(host_to_le16(left));
			out[i][1] = static_cast<int16_t>(host_to_le16(right));

			pos = (pos + 1) & MixerBufferMask;
		}

		CAPTURE_AddAudioData(mixer.sample_rate_hz,
		                     frames_added,
		                     reinterpret_cast<int16_t*>(out));
	}

	// Reset the tick_add for constant speed
	if (is_mixer_irq_important()) {
		mixer.tick_add = calc_tickadd(mixer.sample_rate_hz);
	}

	mixer.frames_done = frames_requested;
}

static void handle_mix_samples()
{
	MIXER_LockAudioDevice();

	mix_samples(mixer.frames_needed);
	mixer.tick_counter += mixer.tick_add;

	const auto frames_needed = mixer.frames_needed +
	                           (mixer.tick_counter >> TickShift);
	mixer.frames_needed = frames_needed;

	mixer.tick_counter &= TickMask;

	MIXER_UnlockAudioDevice();
}

static void reduce_channels_done_counts(const int at_most)
{
	for (const auto& [_, channel] : mixer.channels) {
		channel->frames_done -= std::min(channel->frames_done.load(), at_most);
	}
}

static void handle_mix_no_sound()
{
	MIXER_LockAudioDevice();

	mix_samples(mixer.frames_needed);

	/* Clear piece we've just generated */
	for (auto i = 0; i < mixer.frames_needed; ++i) {
		mixer.work[mixer.pos][0] = 0;
		mixer.work[mixer.pos][1] = 0;

		mixer.pos = (mixer.pos + 1) & MixerBufferMask;
	}

	reduce_channels_done_counts(mixer.frames_needed);

	/* Set values for next tick */
	mixer.tick_counter += mixer.tick_add;
	mixer.frames_needed = mixer.tick_counter >> TickShift;
	mixer.tick_counter &= TickMask;
	mixer.frames_done = 0;

	MIXER_UnlockAudioDevice();
}

static void SDLCALL mixer_callback([[maybe_unused]] void* userdata,
                                   Uint8* stream, int len)
{
	ZoneScoped;
	memset(stream, 0, static_cast<size_t>(len));

	auto frames_requested = len / MixerFrameSize;
	auto output           = reinterpret_cast<int16_t*>(stream);
	auto reduce_frames    = 0;
	work_index_t pos      = 0;

	// Local resampling counter to manipulate the data when sending it off
	// to the callback
	auto index_add = (1 << IndexShiftLocal);
	auto index     = (index_add % frames_requested) ? frames_requested : 0;

	/* Enough room in the buffer ? */
	if (mixer.frames_done < frames_requested) {
		//		LOG_WARNING("Full underrun requested %d, have
		//%d, min %d", frames_requested, mixer.frames_done.load(),
		// mixer.min_frames_needed.load());
		if ((frames_requested - mixer.frames_done) >
		    (frames_requested >> 7)) { // Max 1 percent
			                       // stretch.
			return;
		}
		reduce_frames = mixer.frames_done;
		index_add = (reduce_frames << IndexShiftLocal) / frames_requested;

		mixer.tick_add = calc_tickadd(mixer.sample_rate_hz +
		                              mixer.min_frames_needed);

	} else if (mixer.frames_done < mixer.max_frames_needed) {
		auto frames_remaining = mixer.frames_done - frames_requested;

		if (frames_remaining < mixer.min_frames_needed) {
			if (!is_mixer_irq_important()) {
				auto frames_needed = mixer.frames_needed -
				                     frames_requested;

				auto diff = (mixer.min_frames_needed > frames_needed
				                     ? mixer.min_frames_needed.load()
				                     : frames_needed) -
				            frames_remaining;

				mixer.tick_add = calc_tickadd(mixer.sample_rate_hz +
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

			index_add = (reduce_frames << IndexShiftLocal) /
			            frames_requested;
		} else {
			reduce_frames = frames_requested;
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

			if (diff > (mixer.min_frames_needed << 1)) {
				diff = mixer.min_frames_needed << 1;
			}

			if (diff > (mixer.min_frames_needed >> 1)) {
				mixer.tick_add = calc_tickadd(mixer.sample_rate_hz -
				                              (diff / 5));
			}

			else if (diff > (mixer.min_frames_needed >> 2)) {
				mixer.tick_add = calc_tickadd(mixer.sample_rate_hz -
				                              (diff >> 3));
			} else {
				mixer.tick_add = calc_tickadd(mixer.sample_rate_hz);
			}
		}
	} else {
		/* There is way too much data in the buffer */
		//		LOG_WARNING("overflow run requested %u, have %u,
		// min %u", frames_requested, mixer.frames_done.load(),
		// mixer.min_frames_needed.load());
		if (mixer.frames_done > MixerBufferLength) {
			index_add = MixerBufferLength - 2 * mixer.min_frames_needed;
		} else {
			index_add = mixer.frames_done - 2 * mixer.min_frames_needed;
		}

		index_add = (index_add << IndexShiftLocal) / frames_requested;
		reduce_frames = mixer.frames_done - 2 * mixer.min_frames_needed;

		mixer.tick_add = calc_tickadd(mixer.sample_rate_hz -
		                              (mixer.min_frames_needed / 5));
	}

	reduce_channels_done_counts(reduce_frames);

	// Reset mixer.tick_add when irqs are important
	if (is_mixer_irq_important()) {
		mixer.tick_add = calc_tickadd(mixer.sample_rate_hz);
	}

	mixer.frames_done -= reduce_frames;

	const auto frames_needed = mixer.frames_needed - reduce_frames;
	mixer.frames_needed      = frames_needed;

	pos       = mixer.pos;
	mixer.pos = check_cast<work_index_t>((mixer.pos + reduce_frames) &
	                                     MixerBufferMask);

	if (frames_requested != reduce_frames) {
		while (frames_requested--) {
			const auto i = check_cast<work_index_t>(
			        (pos + (index >> IndexShiftLocal)) & MixerBufferMask);
			index += index_add;

			*output++ = clamp_to_int16(
			        static_cast<int>(mixer.work[i][0]));
			*output++ = clamp_to_int16(
			        static_cast<int>(mixer.work[i][1]));
		}
		// Clean the used buffers
		while (reduce_frames--) {
			pos &= MixerBufferMask;

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
			pos &= MixerBufferMask;

			*output++ = clamp_to_int16(
			        static_cast<int>(mixer.work[pos][0]));
			*output++ = clamp_to_int16(
			        static_cast<int>(mixer.work[pos][1]));

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

static void stop_mixer([[maybe_unused]] Section* sec) {}

[[maybe_unused]] static const char* MixerStateToString(const MixerState s)
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
		TIMER_DelTickHandler(handle_mix_samples);
		TIMER_AddTickHandler(handle_mix_no_sound);
		// LOG_MSG("MIXER: Changed from on to %s",
		// MixerStateToString(new_state));

	} else if (mixer.state != MixerState::On && new_state == MixerState::On) {
		TIMER_DelTickHandler(handle_mix_no_sound);
		TIMER_AddTickHandler(handle_mix_samples);
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
	// When unpaused, the device pulls frames queued by the
	// handle_mix_samples() function, which it fetches from each channel's
	// callback (every millisecond tick), and mixes into a stereo frame
	// buffer.
	//
	// When paused, the audio device stops reading frames from our buffer,
	// so it's imporant that the we no longer queue them, which is why we
	// use handle_mix_no_sound() (to throw away frames instead of queuing).
}

void MIXER_CloseAudioDevice()
{
	// Stop either mixing method
	TIMER_DelTickHandler(handle_mix_samples);
	TIMER_DelTickHandler(handle_mix_no_sound);

	MIXER_LockAudioDevice();
	for (const auto& [_, channel] : mixer.channels) {
		channel->Enable(false);
	}
	MIXER_UnlockAudioDevice();

	if (mixer.sdldevice) {
		SDL_CloseAudioDevice(mixer.sdldevice);
		mixer.sdldevice = 0;
	}
	mixer.state = MixerState::Uninitialized;
}

static bool init_sdl_sound(Section_prop* section)
{
	const auto negotiate = section->Get_bool("negotiate");

	// Start the mixer using SDL sound
	SDL_AudioSpec spec;
	SDL_AudioSpec obtained;

	spec.freq     = static_cast<int>(mixer.sample_rate_hz);
	spec.format   = AUDIO_S16SYS;
	spec.channels = 2;
	spec.callback = mixer_callback;
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

	constexpr auto sdl_error = 0;
	if ((mixer.sdldevice = SDL_OpenAudioDevice(
	             nullptr, 0, &spec, &obtained, sdl_allow_flags)) == sdl_error) {
		LOG_WARNING("MIXER: Can't open audio device: '%s'; sound output disabled",
		            SDL_GetError());

		return false;
	}

	// Does SDL want something other than stereo output?
	if (obtained.channels != spec.channels) {
		E_Exit("SDL gave us %u-channel output but we require %u channels",
		       obtained.channels,
		       spec.channels);
	}

	// Does SDL want a different playback rate?
	if (obtained.freq != mixer.sample_rate_hz) {
		LOG_WARNING("MIXER: SDL changed the requested sample rate of %d to %d Hz",
		            mixer.sample_rate_hz.load(),
		            obtained.freq);

		mixer.sample_rate_hz = check_cast<uint16_t>(obtained.freq);
		set_section_property_value("mixer",
		                           "rate",
		                           format_str("%d",
		                                      mixer.sample_rate_hz.load()));
	}

	// Does SDL want a different blocksize?
	const auto obtained_blocksize = obtained.samples;

	if (obtained_blocksize != mixer.blocksize) {
		LOG_WARNING("MIXER: SDL changed the requested blocksize of %u to %u frames",
		            mixer.blocksize,
		            obtained_blocksize);

		mixer.blocksize = obtained_blocksize;
		set_section_property_value("mixer",
		                           "blocksize",
		                           format_str("%d", mixer.blocksize));
	}

	LOG_MSG("MIXER: Negotiated %u-channel %u Hz audio of %u-frame blocks",
	        obtained.channels,
	        mixer.sample_rate_hz.load(),
	        mixer.blocksize);

	return true;
}

static void init_master_highpass_filter()
{
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
	//
	constexpr auto highpass_cutoff_freq_hz = 20.0;
	for (auto& f : mixer.highpass_filter) {
		f.setup(mixer.sample_rate_hz, highpass_cutoff_freq_hz);
	}
}

void MIXER_Init(Section* sec)
{
	MIXER_CloseAudioDevice();

	sec->AddDestroyFunction(&stop_mixer);

	Section_prop* section = static_cast<Section_prop*>(sec);
	assert(section);

	mixer.sample_rate_hz = check_cast<uint16_t>(section->Get_int("rate"));
	mixer.blocksize = static_cast<uint16_t>(section->Get_int("blocksize"));

	const auto configured_state = section->Get_bool("nosound")
	                                    ? MixerState::NoSound
	                                    : MixerState::On;

	if (configured_state == MixerState::NoSound) {
		LOG_MSG("MIXER: Sound output disabled ('nosound' mode)");
		set_mixer_state(MixerState::NoSound);

	} else {
		if (init_sdl_sound(section)) {
			set_mixer_state(MixerState::On);
		} else {
			set_mixer_state(MixerState::NoSound);
		}
	}

	mixer.tick_counter = (mixer.sample_rate_hz % (1000 / 8)) ? TickNext : 0;

	mixer.tick_add = calc_tickadd(mixer.sample_rate_hz);

	const auto requested_prebuffer_ms = section->Get_int("prebuffer");

	mixer.prebuffer_ms = check_cast<uint16_t>(
	        clamp(requested_prebuffer_ms, 1, MaxPrebufferMs));

	const auto prebuffer_frames = (mixer.sample_rate_hz * mixer.prebuffer_ms) /
	                              1000;

	mixer.pos           = 0;
	mixer.frames_done   = 0;
	mixer.frames_needed = mixer.min_frames_needed + 1;
	mixer.min_frames_needed = 0;
	mixer.max_frames_needed = mixer.blocksize * 2 + 2 * prebuffer_frames;

	// Initialize the 8-bit to 16-bit lookup table
	fill_8to16_lut();

	init_master_highpass_filter();
	init_compressor(section->Get_bool("compressor"));

	// Initialise send effects
	const auto new_crossfeed_preset = crossfeed_pref_to_preset(
	        section->Get_string("crossfeed"));
	if (mixer.crossfeed.preset != new_crossfeed_preset) {
		MIXER_SetCrossfeedPreset(new_crossfeed_preset);
	}

	const auto new_reverb_preset = reverb_pref_to_preset(
	        section->Get_string("reverb"));
	if (mixer.reverb.preset != new_reverb_preset) {
		MIXER_SetReverbPreset(new_reverb_preset);
	}

	const auto new_chorus_preset = chorus_pref_to_preset(
	        section->Get_string("chorus"));
	if (mixer.chorus.preset != new_chorus_preset) {
		MIXER_SetChorusPreset(new_chorus_preset);
	}
}

void MIXER_Mute()
{
	if (mixer.state == MixerState::On) {
		set_mixer_state(MixerState::Muted);
		MIDI_Mute();
		GFX_NotifyAudioMutedStatus(true);
		LOG_MSG("MIXER: Muted audio output");
	}
}

void MIXER_Unmute()
{
	if (mixer.state == MixerState::Muted) {
		set_mixer_state(MixerState::On);
		MIDI_Unmute();
		GFX_NotifyAudioMutedStatus(false);
		LOG_MSG("MIXER: Unmuted audio output");
	}
}

bool MIXER_IsManuallyMuted()
{
	return mixer.is_manually_muted;
}

// Toggle the mixer on/off when a 'true' bool is passed in.
static void handle_toggle_mute(const bool was_pressed)
{
	// The "pressed" bool argument is used by the Mapper API, which sends a
	// true-bool for key down events and a false-bool for key up events.
	if (!was_pressed) {
		return;
	}

	switch (mixer.state) {
	case MixerState::NoSound:
		LOG_WARNING("MIXER: Mute requested, but sound is disabled ('nosound' mode)");
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

void init_mixer_dosbox_settings(Section_prop& sec_prop)
{
	constexpr uint16_t default_sample_rate_hz = 48000;
#if defined(WIN32)
	// Longstanding known-good defaults for Windows
	constexpr uint16_t default_blocksize    = 1024;
	constexpr uint16_t default_prebuffer_ms = 25;
	constexpr bool default_allow_negotiate  = false;

#else
	// Non-Windows platforms tolerate slightly lower latency
	constexpr uint16_t default_blocksize    = 512;
	constexpr uint16_t default_prebuffer_ms = 20;
	constexpr bool default_allow_negotiate  = true;
#endif

	constexpr auto always        = Property::Changeable::Always;
	constexpr auto when_idle     = Property::Changeable::WhenIdle;
	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;

	auto bool_prop = sec_prop.Add_bool("nosound", always, false);
	assert(bool_prop);
	bool_prop->Set_help(
	        "Enable silent mode (disabled by default).\n"
	        "Sound is still fully emulated in silent mode, but DOSBox outputs silence.");

	auto int_prop = sec_prop.Add_int("rate", only_at_start, default_sample_rate_hz);
	assert(int_prop);
	int_prop->Set_values(
	        {"8000", "11025", "16000", "22050", "32000", "44100", "48000"});
	int_prop->Set_help("Mixer sample rate in Hz (%s by default).");

	int_prop = sec_prop.Add_int("blocksize", only_at_start, default_blocksize);
	int_prop->Set_values(
	        {"128", "256", "512", "1024", "2048", "4096", "8192"});
	int_prop->Set_help(
	        "Mixer block size in sample frames (%s by default). Larger values might help\n"
	        "with sound stuttering but the sound will also be more lagged.");

	int_prop = sec_prop.Add_int("prebuffer", only_at_start, default_prebuffer_ms);
	int_prop->SetMinMax(0, MaxPrebufferMs);
	int_prop->Set_help(
	        "How many milliseconds of sound to render on top of the blocksize\n"
	        "(%s by default). Larger values might help with sound stuttering but the sound\n"
	        "will also be more lagged.");

	bool_prop = sec_prop.Add_bool("negotiate", only_at_start, default_allow_negotiate);
	bool_prop->Set_help(
	        "Let the system audio driver negotiate possibly better sample rate and blocksize\n"
	        "settings (%s by default).");

	const auto default_on = true;
	bool_prop = sec_prop.Add_bool("compressor", when_idle, default_on);
	bool_prop->Set_help("Enable the auto-leveling compressor on the master channel to prevent clipping\n"
	                    "of the audio output:\n"
	                    "  off:  Disable compressor.\n"
	                    "  on:   Enable compressor (default).");

	auto string_prop = sec_prop.Add_string("crossfeed", when_idle, "off");
	string_prop->Set_help(
	        "Enable crossfeed globally on all stereo channels for headphone listening:\n"
	        "  off:     No crossfeed (default).\n"
	        "  on:      Enable crossfeed (normal preset).\n"
	        "  light:   Light crossfeed (strength 15).\n"
	        "  normal:  Normal crossfeed (strength 40).\n"
	        "  strong:  Strong crossfeed (strength 65).\n"
	        "Note: You can fine-tune each channel's crossfeed strength using the MIXER.");
	string_prop->Set_values({"off", "on", "light", "normal", "strong"});

	string_prop = sec_prop.Add_string("reverb", when_idle, "off");
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
	        "Note: You can fine-tune each channel's reverb level using the MIXER.");
	string_prop->Set_values(
	        {"off", "on", "tiny", "small", "medium", "large", "huge"});

	string_prop = sec_prop.Add_string("chorus", when_idle, "off");
	string_prop->Set_help(
	        "Enable chorus globally to add a sense of stereo movement to the sound:\n"
	        "  off:     No chorus (default).\n"
	        "  on:      Enable chorus (normal preset).\n"
	        "  light:   A light chorus effect (especially suited for synth music that\n"
	        "           features lots of white noise).\n"
	        "  normal:  Normal chorus that works well with a wide variety of games.\n"
	        "  strong:  An obvious and upfront chorus effect.\n"
	        "Note: You can fine-tune each channel's chorus level using the MIXER.");
	string_prop->Set_values({"off", "on", "light", "normal", "strong"});

	MAPPER_AddHandler(handle_toggle_mute, SDL_SCANCODE_F8, PRIMARY_MOD, "mute", "Mute");
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
