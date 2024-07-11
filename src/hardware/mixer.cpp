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
#include "ring_buffer.h"
#include "setup.h"
#include "string_utils.h"
#include "timer.h"
#include "tracy.h"
#include "video.h"

#include "mverb/MVerb.h"
#include "tal-chorus/ChorusEngine.h"

CHECK_NARROWING();

// #define DEBUG_MIXER

constexpr auto DefaultSampleRateHz = 48000;

// Over how many milliseconds will we permit a signal to grow from
// zero up to peak amplitude? (recommended 10 to 20ms)
constexpr auto EnvelopeMaxExpansionOverMs = 15;

// Regardless if the signal needed to be eveloped or not, how long
// should the envelope monitor the initial signal? (recommended > 5s)
constexpr auto EnvelopeExpiresAfterSeconds = 10;

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
	           const float highpass_freq_hz, const int sample_rate_hz)
	{
		assert(highpass_freq_hz > 0);
		assert(sample_rate_hz > 0);

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

		// Always max gain (no attenuation)
		mverb.setParameter(EmVerb::GAIN, 1.0f);

		// Always 100% wet output signal
		mverb.setParameter(EmVerb::MIX, 1.0f);

		mverb.setSampleRate(static_cast<float>(sample_rate_hz));

		for (auto& f : highpass_filter) {
			f.setup(sample_rate_hz, highpass_freq_hz);
		}
	}
};

struct ChorusSettings {
	ChorusEngine chorus_engine = ChorusEngine(DefaultSampleRateHz);

	ChorusPreset preset            = ChorusPreset::None;
	float synthesizer_send_level   = 0.0f;
	float digital_audio_send_level = 0.0f;

	void Setup(const float synth_level, const float digital_level,
	           const int sample_rate_hz)
	{
		assert(sample_rate_hz > 0);

		synthesizer_send_level   = synth_level;
		digital_audio_send_level = digital_level;

		chorus_engine.setSampleRate(static_cast<float>(sample_rate_hz));

		constexpr auto Chorus1Enabled  = true;
		constexpr auto Chorus2Disabled = false;
		chorus_engine.setEnablesChorus(Chorus1Enabled, Chorus2Disabled);

		// The chorus effect can only operates in 100% wet output mode,
		// so we don't need to configure it for that.
	}
};

struct MixerSettings {
	RingBuffer<AudioFrame, MixerBufferByteSize> work       = {};
	RingBuffer<AudioFrame, MixerBufferByteSize> aux_reverb = {};
	RingBuffer<AudioFrame, MixerBufferByteSize> aux_chorus = {};

	std::vector<float> temp_buf = {};
	std::vector<float> out_buf  = {};

	AudioFrame master_volume = {1.0f, 1.0f};

	std::map<std::string, MixerChannelPtr> channels = {};

	std::map<std::string, MixerChannelSettings> channel_settings_cache = {};

	// Counters accessed by multiple threads
	std::atomic<int> pos               = 0;
	std::atomic<int> frames_done       = 0;
	std::atomic<int> frames_needed     = 0;
	std::atomic<int> min_frames_needed = 0;
	std::atomic<int> max_frames_needed = 0;
	std::atomic<float> frames_per_tick = 0;

	float frame_counter = 0;

	// Sample rate negotiated with SDL (technically, this is the rate of
	// sample *frames* per second).
	std::atomic<int> sample_rate_hz = 0;

	// Matches SDL AudioSpec.samples type
	int blocksize    = 0;
	int prebuffer_ms = 25;

	SDL_AudioDeviceID sdl_device = 0;

	MixerState state = MixerState::Uninitialized;

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

// TODO This is hacky and should be removed. Only the PS1 Audio uses it.
alignas(sizeof(float)) uint8_t MixTemp[MixerBufferByteSize] = {};

[[maybe_unused]] static const char* to_string(const ResampleMethod m)
{
	switch (m) {
	case ResampleMethod::LerpUpsampleOrResample:
		return "LERP upsample or resample";

	case ResampleMethod::ZeroOrderHoldAndResample:
		return "zero-order-hold and resample";

	case ResampleMethod::Resample: return "Resample";

	default: assertm(false, "Invalid ResampleMethod"); return "";
	}
}

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

int MIXER_GetPreBufferMs()
{
	assert(mixer.prebuffer_ms > 0);
	assert(mixer.prebuffer_ms <= MaxPrebufferMs);

	return mixer.prebuffer_ms;
}

int MIXER_GetSampleRate()
{
	const auto sample_rate_hz = mixer.sample_rate_hz.load();
	assert(sample_rate_hz > 0);
	return sample_rate_hz;
}

void MIXER_LockAudioDevice()
{
	if (mixer.sdl_device > 0) {
		SDL_LockAudioDevice(mixer.sdl_device);
	}
}

void MIXER_UnlockAudioDevice()
{
	if (mixer.sdl_device > 0) {
		SDL_UnlockAudioDevice(mixer.sdl_device);
	}
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

static void set_global_crossfeed(const MixerChannelPtr& channel)
{
	assert(channel);

	if (!mixer.do_crossfeed || !channel->HasFeature(ChannelFeature::Stereo)) {
		channel->SetCrossfeedStrength(0.0f);
	} else {
		channel->SetCrossfeedStrength(mixer.crossfeed.global_strength);
	}
}

static void set_global_reverb(const MixerChannelPtr& channel)
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

static void set_global_chorus(const MixerChannelPtr& channel)
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
		return *pref_has_bool ? DefaultCrossfeedPreset
		                      : CrossfeedPreset::None;
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
		LOG_MSG("MIXER: Crossfeed enabled ('%s' preset)",
		        to_string(c.preset));
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

void MIXER_DeregisterChannel(MixerChannelPtr& channel_to_remove)
{
	if (!channel_to_remove) {
		return;
	}

	MIXER_LockAudioDevice();

	auto it = mixer.channels.cbegin();
	while (it != mixer.channels.cend()) {
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

MixerChannelPtr MIXER_AddChannel(MIXER_Handler handler,
                                 const int sample_rate_hz, const char* name,
                                 const std::set<ChannelFeature>& features)
{
	// We allow 0 for the UseMixerRate special value
	assert(sample_rate_hz >= 0);

	auto chan = std::make_shared<MixerChannel>(handler, name, features);
	chan->SetSampleRate(sample_rate_hz);
	chan->SetAppVolume({1.0f, 1.0f});

	const auto chan_rate_hz = chan->GetSampleRate();
	if (chan_rate_hz == mixer.sample_rate_hz) {
		LOG_MSG("%s: Operating at %d Hz without resampling", name, chan_rate_hz);
	} else {
		LOG_MSG("%s: Operating at %d Hz and %s to the output rate",
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
		chan->SetChannelMap(StereoMap);

		set_global_crossfeed(chan);
		set_global_reverb(chan);
		set_global_chorus(chan);
	}

	MIXER_LockAudioDevice();
	mixer.channels[name] = chan; // replace the old, if it exists
	MIXER_UnlockAudioDevice();

	return chan;
}

MixerChannelPtr MIXER_FindChannel(const char* name)
{
	MIXER_LockAudioDevice();

	auto it = mixer.channels.find(name);

	if (it == mixer.channels.end()) {
		// Deprecated alias SPKR to PCSPEAKER
		if (std::string_view(name) == "SPKR") {
			LOG_WARNING(
			        "MIXER: 'SPKR' is deprecated due to inconsistent "
			        "naming, please use 'PCSPEAKER' instead");
			it = mixer.channels.find(ChannelName::PcSpeaker);

			// Deprecated alias FM to OPL
		} else if (std::string_view(name) == "FM") {
			LOG_WARNING(
			        "MIXER: 'FM' is deprecated due to inconsistent "
			        "naming, please use '%s' instead",
			        ChannelName::Opl);
			it = mixer.channels.find(ChannelName::Opl);
		}
	}

	const auto chan = (it != mixer.channels.end()) ? it->second : nullptr;

	MIXER_UnlockAudioDevice();

	return chan;
}

std::map<std::string, MixerChannelPtr>& MIXER_GetChannels()
{
	return mixer.channels;
}

void MixerChannel::Set0dbScalar(const float scalar)
{
	// Realistically we expect some channels might need a fixed boost
	// to get to 0dB, but others might need a range mapping, like from
	// a unity float [-1.0f, +1.0f] to  16-bit int [-32k,+32k] range.
	assert(scalar >= 0.0f && scalar <= static_cast<int16_t>(Max16BitSampleValue));

	db0_volume_gain = scalar;

	UpdateCombinedVolume();
}

void MixerChannel::UpdateCombinedVolume()
{
	// TODO Now that we use floats, we should apply the master volume at the
	// very end as it has no risk of overloading the 16-bit range
	combined_volume_gain = user_volume_gain * app_volume_gain *
	                       mixer.master_volume * db0_volume_gain;
}

const AudioFrame MixerChannel::GetUserVolume() const
{
	return user_volume_gain;
}

void MixerChannel::SetUserVolume(const AudioFrame volume)
{
	// Allow unconstrained user-defined values
	user_volume_gain = volume;
	UpdateCombinedVolume();
}

const AudioFrame MixerChannel::GetAppVolume() const
{
	return app_volume_gain;
}

void MixerChannel::SetAppVolume(const AudioFrame volume)
{
	// Constrain application-defined volume between 0% and 100%
	auto clamp_to_unity = [](const float vol) {
		constexpr auto MinUnityVolume = 0.0f;
		constexpr auto MaxUnityVolume = 1.0f;
		return clamp(vol, MinUnityVolume, MaxUnityVolume);
	};
	app_volume_gain = {clamp_to_unity(volume.left),
	                   clamp_to_unity(volume.right)};
	UpdateCombinedVolume();

#ifdef DEBUG
	LOG_MSG("MIXER: %-7s channel: application requested volume "
	        "{%3.0f%%, %3.0f%%}, and was set to {%3.0f%%, %3.0f%%}",
	        name,
	        static_cast<double>(left),
	        static_cast<double>(right),
	        static_cast<double>(app_volume_gain.left  * 100.0f),
	        static_cast<double>(app_volume_gain.right * 100.0f));
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
		channel->UpdateCombinedVolume();
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
		freq_counter = 0;
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
		frames_done   = 0;
		frames_needed = 0;

		prev_frame = {0.0f, 0.0f};
		next_frame = {0.0f, 0.0f};

		ClearResampler();
	}

	is_enabled = should_enable;

	MIXER_UnlockAudioDevice();
}

// Depending on the resampling method and the channel, mixer and ZoH upsampler
// rates, the following scenarios are possible:
//
// LerpUpsampleOrResample
// ----------------------
//   - Linear interpolation resampling only if:
//         channel_rate_hz < mixer_rate_hz
//
//   - Speex resampling only if:
//         channel_rate_hz > mixer_rate_hz
//
//   - No resampling if:
//         channel_rate_hz == mixer_rate_hz
//
// ZeroOrderHoldAndResample
// ------------------------
//   - Speex resampling only if:
//         channel_rate_hz > zoh_target_freq_hz AND
//         channel_rate_hz != mixer_rate_hz
//
//   - neither ZoH upsampling nor Speex resampling if:
//         channel_rate_hz >= zoh_target_rate_hz AND
//         channel_rate_hz == mixer_rate_hz
//
//   - ZoH upsampling only if:
//         channel_rate_hz < zoh_target_freq_hz AND
//         zoh_target_rate_hz == mixer_rate_hz
//
//   - both ZoH upsampling AND Speex resampling if:
//         channel_rate_hz < zoh_target_rate_hz AND
//         zoh_target_rate_hz != mixer_rate_hz
//
// Resample
// --------
//   - Speex resampling if:
//   	   channel_rate_hz != mixer_rate_hz
//
void MixerChannel::ConfigureResampler()
{
#ifdef DEBUG_MIXER
	LOG_TRACE("%s: Configuring resampler", name.c_str());
#endif

	const auto channel_rate_hz = sample_rate_hz;
	const auto mixer_rate_hz   = mixer.sample_rate_hz.load();

	do_lerp_upsample = false;
	do_zoh_upsample  = false;
	do_resample      = false;

	auto configure_speex_resampler = [&](const int _in_rate_hz) {
		const spx_uint32_t in_rate_hz  = _in_rate_hz;
		const spx_uint32_t out_rate_hz = mixer_rate_hz;

		// Only init the resampler once
		if (!speex_resampler.state) {
			constexpr auto NumChannels     = 2; // always stereo
			constexpr auto ResampleQuality = 5;

			speex_resampler.state = speex_resampler_init(NumChannels,
			                                             in_rate_hz,
			                                             out_rate_hz,
			                                             ResampleQuality,
			                                             nullptr);
		}

		speex_resampler_set_rate(speex_resampler.state, in_rate_hz, out_rate_hz);

		LOG_DEBUG("%s: Speex resampler is on, input rate: %d Hz, output rate: %d Hz",
		          name.c_str(),
		          in_rate_hz,
		          out_rate_hz);
	};

	switch (resample_method) {
	case ResampleMethod::LerpUpsampleOrResample:
		if (channel_rate_hz < mixer_rate_hz) {
			do_lerp_upsample = true;
			InitLerpUpsamplerState();
#ifdef DEBUG_MIXER
			LOG_DEBUG("%s: Linear interpolation resampler is on",
			          name.c_str());
#endif
		} else if (channel_rate_hz > mixer_rate_hz) {
			do_resample = true;
			configure_speex_resampler(channel_rate_hz);

		} else {
			// channel_rate_hz == mixer_rate_hz
			// no resampling is needed
		}
		break;

	case ResampleMethod::ZeroOrderHoldAndResample:
		if (channel_rate_hz < zoh_upsampler.target_rate_hz) {
			do_zoh_upsample = true;
			InitZohUpsamplerState();
#ifdef DEBUG_MIXER
			LOG_DEBUG("%s: Zero-order-hold upsampler is on, target rate: %d Hz ",
			          name.c_str(),
			          zoh_upsampler.target_rate_hz);
#endif
			if (zoh_upsampler.target_rate_hz != mixer_rate_hz) {
				do_resample = true;
				configure_speex_resampler(zoh_upsampler.target_rate_hz);
			}

		} else {
			// channel_rate_hz >= zoh_upsampler.target_rate_hz
			// We cannot ZoH upsample, but we might need to resample
			//
			if (channel_rate_hz != mixer_rate_hz) {
				do_resample = true;
				configure_speex_resampler(channel_rate_hz);
			}
		}
		break;

	case ResampleMethod::Resample:
		if (channel_rate_hz != mixer_rate_hz) {
			do_resample = true;
			configure_speex_resampler(channel_rate_hz);
		}
		break;
	}
}

// Clear the resampler and prime its input queue with zeroes
void MixerChannel::ClearResampler()
{
	if (do_lerp_upsample) {
		InitLerpUpsamplerState();
	}
	if (do_zoh_upsample) {
		InitZohUpsamplerState();
	}
	if (do_resample) {
		assert(speex_resampler.state);
		speex_resampler_reset_mem(speex_resampler.state);
		speex_resampler_skip_zeros(speex_resampler.state);

#ifdef DEBUG_MIXER
		LOG_DEBUG("%s: Speex resampler cleared and primed %d-frame input queue",
		          name.c_str(),
		          speex_resampler_get_input_latency(speex_resampler.state));
#endif
	}
}

void MixerChannel::SetSampleRate(const int new_sample_rate_hz)
{
	// We allow 0 for the UseMixerRate special value
	assert(new_sample_rate_hz >= 0);

	// If the requested rate is zero, then avoid resampling by running the
	// channel at the mixer's rate
	const int target_rate_hz = (new_sample_rate_hz == UseMixerRate)
	                                 ? mixer.sample_rate_hz.load()
	                                 : new_sample_rate_hz;
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

int MixerChannel::GetSampleRate() const
{
	return sample_rate_hz;
}

void MixerChannel::SetPeakAmplitude(const int peak)
{
	peak_amplitude = peak;
	envelope.Update(sample_rate_hz,
	                peak_amplitude,
	                EnvelopeMaxExpansionOverMs,
	                EnvelopeExpiresAfterSeconds);
}

void MixerChannel::Mix(const int frames_requested)
{
	assert(frames_requested >= 0);

	if (!is_enabled) {
		return;
	}

	frames_needed = frames_requested;

	auto stretch_factor = static_cast<float>(sample_rate_hz) /
	                      static_cast<float>(mixer.sample_rate_hz);

	while (frames_needed > frames_done) {
		auto frames_remaining = iceil(
		        static_cast<float>(frames_needed - frames_done) *
		        stretch_factor);

		// Avoid underflow
		if (frames_remaining <= 0) {
			break;
		}
		// Avoid overflow
		frames_remaining = std::min(frames_remaining, MixerBufferByteSize);

		handler(frames_remaining);
	}

	if (do_sleep) {
		sleeper.MaybeSleep();
	}
}

void MixerChannel::AddSilence()
{
	MIXER_LockAudioDevice();

	if (frames_done < frames_needed) {
		if (prev_frame.left == 0.0f && prev_frame.right == 0.0f) {
			frames_done = frames_needed;

			// Make sure the next samples are zero when they get
			// switched to prev
			next_frame = {0.0f, 0.0f};

		} else {
			bool stereo = last_samples_were_stereo;

			const auto mapped_output_left  = output_map.left;
			const auto mapped_output_right = output_map.right;

			auto work_pos = mixer.work.begin() + mixer.pos + frames_done;

			while (frames_done < frames_needed) {
				// Fade gradually to silence to avoid clicks.
				// Maybe the fade factor f depends on the sample
				// rate.
				constexpr auto f = 4.0f;

				for (auto ch = 0; ch < 2; ++ch) {
					if (prev_frame[ch] > f) {
						next_frame[ch] = prev_frame[ch] - f;
					} else if (prev_frame[ch] < -f) {
						next_frame[ch] = prev_frame[ch] + f;
					} else {
						next_frame[ch] = 0.0f;
					}
				}

				const auto frame_with_gain =
				        (stereo ? prev_frame
				                : AudioFrame{prev_frame.left}) *
				        combined_volume_gain;

				AudioFrame out_frame = {};
				out_frame[mapped_output_left]  = frame_with_gain.left;
				out_frame[mapped_output_right] = frame_with_gain.right;

				*work_pos++ += out_frame;

				prev_frame = next_frame;
				++frames_done;
			}
		}
	}

	last_samples_were_silence = true;

	MIXER_UnlockAudioDevice();
}

static void log_filter_settings(const std::string& channel_name,
                                const std::string& filter_name, const int order,
                                const int cutoff_freq_hz)
{
	assert(order > 0);
	assert(cutoff_freq_hz > 0);

	constexpr auto DbPerOrder = 6;

	LOG_MSG("%s: %s filter enabled (%d dB/oct at %d Hz)",
	        channel_name.c_str(),
	        filter_name.c_str(),
	        order * DbPerOrder,
	        cutoff_freq_hz);
}

void MixerChannel::SetHighPassFilter(const FilterState state)
{
	filters.highpass.state = state;

	if (filters.highpass.state == FilterState::On) {
		assert(filters.highpass.order > 0);
		assert(filters.highpass.cutoff_freq_hz > 0);

		log_filter_settings(name,
		                    "High-pass",
		                    filters.highpass.order,
		                    filters.highpass.cutoff_freq_hz);
	}
}

void MixerChannel::SetLowPassFilter(const FilterState state)
{
	filters.lowpass.state = state;

	if (filters.lowpass.state == FilterState::On) {
		assert(filters.lowpass.order > 0);
		assert(filters.lowpass.cutoff_freq_hz > 0);

		log_filter_settings(name,
		                    "Low-pass",
		                    filters.lowpass.order,
		                    filters.lowpass.cutoff_freq_hz);
	}
}

FilterState MixerChannel::GetHighPassFilterState() const
{
	return filters.highpass.state;
}

FilterState MixerChannel::GetLowPassFilterState() const
{
	return filters.lowpass.state;
}

static int clamp_filter_cutoff_freq([[maybe_unused]] const std::string& channel_name,
                                    const int cutoff_freq_hz)
{
	assert(cutoff_freq_hz > 0);

	const auto max_cutoff_freq_hz = mixer.sample_rate_hz / 2 - 1;

	if (cutoff_freq_hz <= max_cutoff_freq_hz) {
		return cutoff_freq_hz;
	} else {
		LOG_DEBUG(
		        "%s: Filter cutoff frequency %d Hz is above half the "
		        "sample rate, clamping to %d Hz",
		        channel_name.c_str(),
		        cutoff_freq_hz,
		        max_cutoff_freq_hz);

		return max_cutoff_freq_hz;
	}
}

void MixerChannel::ConfigureHighPassFilter(const int order, const int _cutoff_freq_hz)
{
	assert(order > 0);
	assert(_cutoff_freq_hz > 0);

	const auto cutoff_freq_hz = clamp_filter_cutoff_freq(name, _cutoff_freq_hz);

	assert(order > 0 && order <= MaxFilterOrder);
	for (auto& f : filters.highpass.hpf) {
		f.setup(order, mixer.sample_rate_hz, cutoff_freq_hz);
	}

	filters.highpass.order          = order;
	filters.highpass.cutoff_freq_hz = cutoff_freq_hz;
}

void MixerChannel::ConfigureLowPassFilter(const int order, const int _cutoff_freq_hz)
{
	assert(order > 0);
	assert(_cutoff_freq_hz > 0);

	const auto cutoff_freq_hz = clamp_filter_cutoff_freq(name, _cutoff_freq_hz);

	assert(order > 0 && order <= MaxFilterOrder);
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
		LOG_WARNING(
		        "%s: Invalid custom filter definition: '%s'. Must be "
		        "specified in \"lpf|hpf ORDER CUTOFF_FREQUENCY\" format",
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
		    order > MaxFilterOrder) {
			LOG_WARNING(
			        "%s: Invalid custom %s filter order: '%s'. "
			        "Must be an integer between 1 and %d.",
			        name.c_str(),
			        filter_name,
			        order_pref.c_str(),
			        MaxFilterOrder);
			return false;
		}

		int cutoff_freq_hz;
		if (!sscanf(cutoff_freq_pref.c_str(), "%d", &cutoff_freq_hz) ||
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

		const auto filter_type           = parts[i++];
		const auto filter_order          = parts[i++];
		const auto filter_cutoff_freq_hz = parts[i++];

		return set_filter(filter_type, filter_order, filter_cutoff_freq_hz);
	} else {
		auto i = 0;

		const auto filter1_type           = parts[i++];
		const auto filter1_order          = parts[i++];
		const auto filter1_cutoff_freq_hz = parts[i++];

		const auto filter2_type           = parts[i++];
		const auto filter2_order          = parts[i++];
		const auto filter2_cutoff_freq_hz = parts[i++];

		if (filter1_type == filter2_type) {
			LOG_WARNING(
			        "%s: Invalid custom filter definition: '%s'. "
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

void MixerChannel::SetZeroOrderHoldUpsamplerTargetRate(const int target_rate_hz)
{
	assert(target_rate_hz > 0);

	// TODO make sure that the ZOH target frequency cannot be set after the
	// filter has been configured
	zoh_upsampler.target_rate_hz = target_rate_hz;

#ifdef DEBUG_MIXER
	LOG_DEBUG("%s: Set zero-order-hold upsampler target rate to %d Hz",
	          name.c_str(),
	          target_rate_hz);
#endif

	ConfigureResampler();
}

void MixerChannel::InitZohUpsamplerState()
{
	assert(sample_rate_hz < zoh_upsampler.target_rate_hz);

	zoh_upsampler.step = static_cast<float>(sample_rate_hz) /
	                     static_cast<float>(zoh_upsampler.target_rate_hz);
	assert(zoh_upsampler.step < 1.0f);

	zoh_upsampler.pos  = 0.0f;
}

void MixerChannel::InitLerpUpsamplerState()
{
	assert(sample_rate_hz < mixer.sample_rate_hz);

	lerp_upsampler.step = static_cast<float>(sample_rate_hz) /
	                      static_cast<float>(mixer.sample_rate_hz.load());
	assert(lerp_upsampler.step < 1.0f);

	lerp_upsampler.pos        = 0.0f;
	lerp_upsampler.last_frame = {0.0f, 0.0f};
}

void MixerChannel::SetResampleMethod(const ResampleMethod method)
{
#ifdef DEBUG_MIXER
	LOG_ERR("%s: Set resample method to %s", name.c_str(), to_string(method));
#endif

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
	auto p = (1.0f - strength) / 2.0f;

	constexpr auto Center = 0.5f;
	crossfeed.pan_left    = Center - p;
	crossfeed.pan_right   = Center + p;

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
	constexpr auto LevelMin    = 0.0f;
	constexpr auto LevelMax    = 1.0f;
	constexpr auto LevelMinDb = -40.0f;
	constexpr auto LevelMaxDb = 0.0f;

	assert(level >= LevelMin);
	assert(level <= LevelMax);

	do_reverb_send = (HasFeature(ChannelFeature::ReverbSend) && level > LevelMin);

	if (!do_reverb_send) {
#ifdef DEBUG_MIXER
		LOG_DEBUG("%s: Reverb send is off", name.c_str());
#endif
		reverb.level     = LevelMin;
		reverb.send_gain = LevelMinDb;
		return;
	}

	reverb.level = level;

	const auto level_db = remap(LevelMin, LevelMax, LevelMinDb, LevelMaxDb, level);

	reverb.send_gain = decibel_to_gain(level_db);

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
	constexpr auto LevelMin    = 0.0f;
	constexpr auto LevelMax    = 1.0f;
	constexpr auto LevelMinDb = -24.0f;
	constexpr auto LevelMaxDb = 0.0f;

	assert(level >= LevelMin);
	assert(level <= LevelMax);

	do_chorus_send = (HasFeature(ChannelFeature::ChorusSend) && level > LevelMin);

	if (!do_chorus_send) {
#ifdef DEBUG_MIXER
		LOG_DEBUG("%s: Chorus send is off", name.c_str());
#endif
		chorus.level     = LevelMin;
		chorus.send_gain = LevelMinDb;
		return;
	}

	chorus.level = level;

	const auto level_db = remap(LevelMin, LevelMax, LevelMinDb, LevelMaxDb, level);

	chorus.send_gain = decibel_to_gain(level_db);

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
static constexpr int16_t u8to16(const int u_val)
{
	assert(u_val >= 0 && u_val <= UINT8_MAX);
	const auto s_val = u_val - 128;
	if (s_val > 0) {
		constexpr auto Scalar = Max16BitSampleValue / 127.0;
		return static_cast<int16_t>(round(s_val * Scalar));
	}
	return static_cast<int16_t>(s_val * 256);
}

// 8-bit to 16-bit lookup tables
int16_t lut_u8to16[UINT8_MAX + 1] = {};

static constexpr int16_t* lut_s8to16 = lut_u8to16 + 128;

static constexpr void fill_8to16_lut()
{
	for (int i = 0; i <= UINT8_MAX; ++i) {
		lut_u8to16[i] = u8to16(i);
	}
}

template <class Type, bool stereo, bool signeddata, bool nativeorder>
AudioFrame MixerChannel::ConvertNextFrame(const Type* data, const int pos)
{
	assert(pos >= 0);

	AudioFrame frame = {};

	const auto left_pos  = pos * 2 + 0;
	const auto right_pos = pos * 2 + 1;

	if (sizeof(Type) == 1) {
		// Integer types
		// unsigned 8-bit
		if (!signeddata) {
			if (stereo) {
				frame.left  = lut_u8to16[static_cast<uint8_t>(data[left_pos])];
				frame.right = lut_u8to16[static_cast<uint8_t>(data[right_pos])];
			} else {
				frame.left = lut_u8to16[static_cast<uint8_t>(data[pos])];
			}
		}
		// signed 8-bit
		else {
			if (stereo) {
				frame.left  = lut_s8to16[static_cast<int8_t>(data[left_pos])];
				frame.right = lut_s8to16[static_cast<int8_t>(data[right_pos])];
			} else {
				frame.left = lut_s8to16[static_cast<int8_t>(data[pos])];
			}
		}
	} else {
		// 16-bit and 32-bit both contain 16-bit data internally
		if (signeddata) {
			if (stereo) {
				if (nativeorder) {
					frame.left  = static_cast<float>(data[left_pos]);
					frame.right = static_cast<float>(data[right_pos]);
				} else {
					auto host_pt0 = reinterpret_cast<const uint8_t* const>(
					        data + left_pos);
					auto host_pt1 = reinterpret_cast<const uint8_t* const>(
					        data + right_pos);

					if (sizeof(Type) == 2) {
						frame.left  = (int16_t)host_readw(host_pt0);
						frame.right = (int16_t)host_readw(host_pt1);
					} else {
						frame.left = static_cast<float>(
						        (int32_t)host_readd(host_pt0));
						frame.right = static_cast<float>(
						        (int32_t)host_readd(host_pt1));
					}
				}
			} else { // mono
				if (nativeorder) {
					frame.left = static_cast<float>(data[pos]);
				} else {
					auto host_pt = reinterpret_cast<const uint8_t* const>(
					        data + pos);

					if (sizeof(Type) == 2) {
						frame.left = (int16_t)host_readw(host_pt);
					} else {
						frame.left = static_cast<float>(
						        (int32_t)host_readd(host_pt));
					}
				}
			}
		} else { // unsigned
			const auto offs = 32768;
			if (stereo) {
				if (nativeorder) {
					frame.left = static_cast<float>(
					        static_cast<int>(data[left_pos]) -
					        offs);
					frame.right = static_cast<float>(
					        static_cast<int>(data[right_pos]) -
					        offs);
				} else {
					auto host_pt0 = reinterpret_cast<const uint8_t* const>(
					        data + left_pos);
					auto host_pt1 = reinterpret_cast<const uint8_t* const>(
					        data + right_pos);

					if (sizeof(Type) == 2) {
						frame.left = static_cast<float>(
						        static_cast<int>(host_readw(
						                host_pt0)) -
						        offs);
						frame.right = static_cast<float>(
						        static_cast<int>(host_readw(
						                host_pt1)) -
						        offs);
					} else {
						frame.left = static_cast<float>(
						        static_cast<int>(host_readd(
						                host_pt0)) -
						        offs);
						frame.right = static_cast<float>(
						        static_cast<int>(host_readd(
						                host_pt1)) -
						        offs);
					}
				}
			} else { // mono
				if (nativeorder) {
					frame.left = static_cast<float>(
					        static_cast<int>(data[pos]) - offs);
				} else {
					auto host_pt = reinterpret_cast<const uint8_t* const>(
					        data + pos);

					if (sizeof(Type) == 2) {
						frame.left = static_cast<float>(
						        static_cast<int>(host_readw(
						                host_pt)) -
						        offs);
					} else {
						frame.left = static_cast<float>(
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
void MixerChannel::ConvertSamplesAndMaybeZohUpsample(const Type* data,
                                                     const int num_frames,
                                                     std::vector<float>& out)
{
	assert(num_frames >= 0);

	const auto mapped_output_left  = output_map.left;
	const auto mapped_output_right = output_map.right;

	const auto mapped_channel_left  = channel_map.left;
	const auto mapped_channel_right = channel_map.right;

	auto pos             = 0;
	AudioFrame out_frame = {};

	// We set size to zero which will not change the data in the container
	// at all. Then we overwrite the data below with `emplace_back()` which
	// will set the correct length.
	out.resize(0);

	while (pos < num_frames) {
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

		AudioFrame frame_with_gain = {};
		if (stereo) {
			frame_with_gain = {prev_frame[mapped_channel_left],
			                   prev_frame[mapped_channel_right]};
		} else {
			frame_with_gain = {prev_frame[mapped_channel_left]};
		}
		frame_with_gain *= combined_volume_gain;

		// Process initial samples through an expanding envelope to
		// prevent severe clicks and pops. Becomes a no-op when done.
		envelope.Process(stereo, frame_with_gain);

		out_frame = {};
		out_frame[mapped_output_left]  += frame_with_gain.left;
		out_frame[mapped_output_right] += frame_with_gain.right;

		out.emplace_back(out_frame.left);
		out.emplace_back(out_frame.right);

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

static spx_uint32_t estimate_max_out_frames(SpeexResamplerState* resampler_state,
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
	const auto a = pan(frame.left,  crossfeed.pan_left);
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
		fadeout_or_sleep_after_ms = wait_ms;

		fadeout_decrement_per_ms = 1.0f / static_cast<float>(fade_ms);

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
		set_wait_and_fade(DefaultWaitMs, DefaultWaitMs);
		wants_fadeout = true;
		return true;
	}

	// Let the fade-out last between 10 ms and 3 seconds.
	constexpr auto MinFadeMs = 10;
	constexpr auto MaxFadeMs = 3000;

	// Custom setting in 'WAIT FADE' syntax, where both are milliseconds.
	if (auto prefs_vec = split(prefs); prefs_vec.size() == 2) {
		const auto wait_ms = parse_int(prefs_vec[0]);
		const auto fade_ms = parse_int(prefs_vec[1]);
		if (wait_ms && fade_ms) {
			const auto wait_is_valid = (*wait_ms >= MinWaitMs &&
			                            *wait_ms <= MaxWaitMs);

			const auto fade_is_valid = (*fade_ms >= MinFadeMs &&
			                            *fade_ms <= MaxFadeMs);

			if (wait_is_valid && fade_is_valid) {
				set_wait_and_fade(*wait_ms, *fade_ms);
				wants_fadeout = true;
				return true;
			}
		}
	}
	// Otherwise inform the user and disable the fade
	LOG_WARNING(
	        "%s: Invalid custom fade-out definition: '%s'. Must be "
	        "specified in \"WAIT FADE\" format where WAIT is between "
	        "%d and %d (in milliseconds) and FADE is between %d and "
	        "%d (in milliseconds); using 'off'.",
	        channel.GetName().c_str(),
	        prefs.c_str(),
	        MinWaitMs,
	        MaxWaitMs,
	        MinFadeMs,
	        MaxFadeMs);

	wants_fadeout = false;
	return false;
}

void MixerChannel::Sleeper::DecrementFadeLevel(const int awake_for_ms)
{
	assert(awake_for_ms >= 0);
	assert(awake_for_ms >= fadeout_or_sleep_after_ms);
	const auto elapsed_fade_ms = static_cast<float>(
	        awake_for_ms - fadeout_or_sleep_after_ms);

	const auto decrement = fadeout_decrement_per_ms * elapsed_fade_ms;

	constexpr auto MinLevel = 0.0f;
	constexpr auto MaxLevel = 1.0f;
	fadeout_level = std::clamp(MaxLevel - decrement, MinLevel, MaxLevel);
}

MixerChannel::Sleeper::Sleeper(MixerChannel& c, const int sleep_after_ms)
        : channel(c),
          fadeout_or_sleep_after_ms(sleep_after_ms)
{
	assert(sleep_after_ms >= 0);

	// The constructed sleep period is programmatically controlled (so assert)
	assert(fadeout_or_sleep_after_ms >= MinWaitMs);
	assert(fadeout_or_sleep_after_ms <= MaxWaitMs);
}

// Either fades the frame or checks if the channel had any signal output.
AudioFrame MixerChannel::Sleeper::MaybeFadeOrListen(const AudioFrame& frame)
{
	if (wants_fadeout) {
		// When fading, we actively drive down the channel level
		return frame * fadeout_level;
	}
	if (!had_signal) {
		// Otherwise, we simply passively listen for signal
		constexpr auto SilenceThreshold = 1.0f;
		had_signal = fabsf(frame.left)  > SilenceThreshold ||
		             fabsf(frame.right) > SilenceThreshold;
	}
	return frame;
}

void MixerChannel::Sleeper::MaybeSleep()
{
	// A signed integer can a durration of ~24 days in milliseconds, which is
	// surely more than enough.
	const auto awake_for_ms = check_cast<int>(GetTicksSince(woken_at_ms));

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
	woken_at_ms   = GetTicks();
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
void MixerChannel::AddSamples(const int num_frames, const Type* data)
{
	assert(num_frames > 0);

	last_samples_were_stereo = stereo;

	// All possible resampling scenarios:
	//
	// - No upsampling or resampling
	// - LERP  upsampling only
	// - ZoH   upsampling only
	// - Speex resampling only
	// - ZoH upsampling followed by Speex resampling

	// Zero-order-hold upsampling is performed in
	// ConvertSamplesAndMaybeZohUpsample to reduce the number of temporary
	// buffers and to simplify the code.
	//
	auto& convert_dest_buf = (do_lerp_upsample || do_resample)
	                               ? mixer.temp_buf
	                               : mixer.out_buf;

	ConvertSamplesAndMaybeZohUpsample<Type, stereo, signeddata, nativeorder>(
	        data, num_frames, convert_dest_buf);

	if (do_lerp_upsample) {
		auto& s = lerp_upsampler;

		auto in_pos = mixer.temp_buf.cbegin();
		auto& out   = mixer.out_buf;

		// We set size to zero which will not change the data in the
		// container at all. Then we overwrite the data below with
		// `emplace_back()` which will set the correct length.
		out.resize(0);

		while (in_pos != mixer.temp_buf.cend()) {
			AudioFrame curr_frame = {*in_pos, *(in_pos + 1)};

			assert(s.pos >= 0.0f && s.pos <= 1.0f);
			const auto out_left = lerp(s.last_frame.left,
			                           curr_frame.left,
			                           s.pos);

			const auto out_right = lerp(s.last_frame.right,
			                            curr_frame.right,
			                            s.pos);

			out.emplace_back(out_left);
			out.emplace_back(out_right);

			s.pos += s.step;
#if 0
			LOG_DEBUG(
			        "%s: AddSamples last %.1f:%.1f curr %.1f:%.1f"
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
	}

	if (do_resample) {
		auto in_frames = check_cast<spx_uint32_t>(
		        mixer.temp_buf.size() / 2);

		auto out_frames = check_cast<spx_uint32_t>(
		        estimate_max_out_frames(speex_resampler.state, in_frames));


		mixer.out_buf.resize(out_frames * 2);

		speex_resampler_process_interleaved_float(speex_resampler.state,
		                                          mixer.temp_buf.data(),
		                                          &in_frames,
		                                          mixer.out_buf.data(),
		                                          &out_frames);

		// 'out_frames' now contains the actual number of
		// resampled frames, so ensure the number of output frames
		// is within the logical size.
		assert(out_frames <= mixer.out_buf.size() / 2);
		mixer.out_buf.resize(out_frames * 2); // only shrinks
	}

	MIXER_LockAudioDevice();

	// Optionally filter, apply crossfeed, then mix the results to the
	// master output
	const auto pos_offset = mixer.pos + frames_done;
	auto work_pos         = mixer.work.begin() + pos_offset;
	auto aux_reverb_pos   = mixer.aux_reverb.begin() + pos_offset;
	auto aux_chorus_pos   = mixer.aux_chorus.begin() + pos_offset;

	auto out_buf_pos = mixer.out_buf.cbegin();

	while (out_buf_pos != mixer.out_buf.cend()) {
		AudioFrame frame = {*out_buf_pos++, *out_buf_pos++};

		if (filters.highpass.state == FilterState::On) {
			frame = {filters.highpass.hpf[0].filter(frame.left),
			         filters.highpass.hpf[1].filter(frame.right)};
		}
		if (filters.lowpass.state == FilterState::On) {
			frame = {filters.lowpass.lpf[0].filter(frame.left),
			         filters.lowpass.lpf[1].filter(frame.right)};
		}

		if (do_crossfeed) {
			frame = ApplyCrossfeed(frame);
		}
		if (do_reverb_send) {
			// Accumulate reverb sends from the individual channels
			// in the reverb aux buffer. Once we've done this for
			// all our channels, we can feed the accumulated inputs
			// through the reverb only once. The reverb is
			// configured for 100% wet output.
			//
			// Our reverb algorithm is a linear process, so applying
			// the reverb effect individually to N channels would
			// yield bit-identical results, but it would be N-times
			// more expensive.
			*aux_reverb_pos++ += (frame * reverb.send_gain);
		}
		if (do_chorus_send) {
			// Similarly to reverb processing, we accumulate chorus
			// sends from the individual channels in the chorus aux
			// buffer. The chorus configured for 100% wet output.
			*aux_chorus_pos++ += (frame * chorus.send_gain);
		}

		if (do_sleep) {
			frame = sleeper.MaybeFadeOrListen(frame);
		}

		// Mix samples to the master output
		*work_pos++ += frame;
	}

	const auto out_frames = check_cast<int>(mixer.out_buf.size() / 2);

	frames_done += out_frames;

	MIXER_UnlockAudioDevice();
}

// TODO Move this into the Sound Blaster code.
// This is only called in the "Direct DAC" (non-DMA) operational mode of the
// Sound Blaster where the DAC is controlled by the CPU.
//
void MixerChannel::AddStretched(const int len, int16_t* data)
{
	assert(len >= 0);

	MIXER_LockAudioDevice();

	if (frames_done >= frames_needed) {
		LOG_MSG("Can't add, buffer full");
		MIXER_UnlockAudioDevice();
		return;
	}

	// Stretch mono input stream into this many output frames
	auto frames_remaining = frames_needed - frames_done;

	// Used for time-stretching the audio
	float pos = 0;
	float step = static_cast<float>(len) / static_cast<float>(frames_remaining);

	auto work_pos = mixer.work.begin() + mixer.pos + frames_done;

	const auto mapped_output_left  = output_map.left;
	const auto mapped_output_right = output_map.right;

	while (frames_remaining--) {
		auto prev_sample = prev_frame.left;
		auto curr_sample = static_cast<float>(*data);
		float out_sample = 0;

		switch (resample_method) {
		case ResampleMethod::LerpUpsampleOrResample:
		case ResampleMethod::Resample:
			out_sample = lerp(prev_sample, curr_sample, pos);
			break;

		case ResampleMethod::ZeroOrderHoldAndResample:
			out_sample = curr_sample;
		}

		auto frame_with_gain = AudioFrame{out_sample} * combined_volume_gain;

		if (do_sleep) {
			frame_with_gain = sleeper.MaybeFadeOrListen(frame_with_gain);
		}

		AudioFrame out_frame           = {};
		out_frame[mapped_output_left]  = frame_with_gain.left;
		out_frame[mapped_output_right] = frame_with_gain.right;

		*work_pos++ += out_frame;

		// Advance input position
		pos += step;
		if (pos > 1.0f) {
			pos -= 1.0f;
			prev_frame = {curr_sample};
			++data;
		}
	}

	frames_done = frames_needed;

	MIXER_UnlockAudioDevice();
}

void MixerChannel::AddSamples_m8(const int num_frames, const uint8_t* data)
{
	AddSamples<uint8_t, false, false, true>(num_frames, data);
}

void MixerChannel::AddSamples_s8(const int num_frames, const uint8_t* data)
{
	AddSamples<uint8_t, true, false, true>(num_frames, data);
}

void MixerChannel::AddSamples_m8s(const int num_frames, const int8_t* data)
{
	AddSamples<int8_t, false, true, true>(num_frames, data);
}

void MixerChannel::AddSamples_s8s(const int num_frames, const int8_t* data)
{
	AddSamples<int8_t, true, true, true>(num_frames, data);
}

void MixerChannel::AddSamples_m16(const int num_frames, const int16_t* data)
{
	AddSamples<int16_t, false, true, true>(num_frames, data);
}

void MixerChannel::AddSamples_s16(const int num_frames, const int16_t* data)
{
	AddSamples<int16_t, true, true, true>(num_frames, data);
}

void MixerChannel::AddSamples_m16u(const int num_frames, const uint16_t* data)
{
	AddSamples<uint16_t, false, false, true>(num_frames, data);
}

void MixerChannel::AddSamples_s16u(const int num_frames, const uint16_t* data)
{
	AddSamples<uint16_t, true, false, true>(num_frames, data);
}

void MixerChannel::AddSamples_mfloat(const int num_frames, const float* data)
{
	AddSamples<float, false, true, true>(num_frames, data);
}

void MixerChannel::AddSamples_sfloat(const int num_frames, const float* data)
{
	AddSamples<float, true, true, true>(num_frames, data);
}

void MixerChannel::AddSamples_m16_nonnative(const int num_frames, const int16_t* data)
{
	AddSamples<int16_t, false, true, false>(num_frames, data);
}

void MixerChannel::AddSamples_s16_nonnative(const int num_frames, const int16_t* data)
{
	AddSamples<int16_t, true, true, false>(num_frames, data);
}

void MixerChannel::AddSamples_m16u_nonnative(const int num_frames, const uint16_t* data)
{
	AddSamples<uint16_t, false, false, false>(num_frames, data);
}

void MixerChannel::AddSamples_s16u_nonnative(const int num_frames, const uint16_t* data)
{
	AddSamples<uint16_t, true, false, false>(num_frames, data);
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
		        frames_remaining, 0, static_cast<int>(MixerBufferByteSize));

		MIXER_LockAudioDevice();
		Mix(frames_to_mix);
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
	if (output_map == StereoMap) {
		return MSG_Get("SHELL_CMD_MIXER_CHANNEL_STEREO");
	}
	if (output_map == ReverseMap) {
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

static constexpr float calc_frames_per_tick(const int freq_hz)
{
	assert(freq_hz > 0);

	constexpr auto MillisecondsPerTick = 1000;
	return static_cast<float>(freq_hz) / MillisecondsPerTick;
}

// Mix a certain amount of new sample frames
static void mix_samples(const int frames_requested)
{
	assert(frames_requested >= 0);

	constexpr auto CaptureBufFrames = 1024;

	const auto frames_added = std::min(frames_requested - mixer.frames_done,
	                                   CaptureBufFrames);

	const auto pos_offset     = mixer.pos + mixer.frames_done;
	const auto start_work_pos = mixer.work.begin() + pos_offset;

	// Render all channels and accumulate results in the master mixbuffer
	for (const auto& [_, channel] : mixer.channels) {
		channel->Mix(frames_requested);
	}

	if (mixer.do_reverb) {
		// Use the contents of the reverb aux buffer as the reverb's
		// input, then mix its output to the master mix buffer.
		auto work_pos       = start_work_pos;
		auto aux_reverb_pos = mixer.aux_reverb.begin() + pos_offset;
		auto num_frames     = frames_added;

		while (num_frames--) {
			auto in_frame = *aux_reverb_pos++;

			// High-pass filter the reverb input
			auto& hpf = mixer.reverb.highpass_filter;

			in_frame = {hpf[0].filter(in_frame.left),
			            hpf[1].filter(in_frame.right)};

			// MVerb operates on two non-interleaved sample streams
			float* in_buf[2] = {&in_frame.left, &in_frame.right};

			AudioFrame out_frame = {};
			float* out_buf[2] = {&out_frame.left, &out_frame.right};

			constexpr auto NumFrames = 1;
			mixer.reverb.mverb.process(in_buf, out_buf, NumFrames);

			*work_pos++ += out_frame;
		}
	}

	if (mixer.do_chorus) {
		// Apply chorus effect to the chorus aux buffer, then mix the
		// results to the master output
		auto work_pos       = start_work_pos;
		auto aux_chorus_pos = mixer.aux_chorus.begin() + pos_offset;
		auto num_frames     = frames_added;

		while (num_frames--) {
			auto frame = *aux_chorus_pos++;
			mixer.chorus.chorus_engine.process(&frame.left, &frame.right);

			*work_pos++ += frame;
		}
	}

	// Apply high-pass filter to the master output
	{
		auto work_pos   = start_work_pos;
		auto num_frames = frames_added;

		while (num_frames--) {
			auto& hpf = mixer.highpass_filter;

			const auto frame = *work_pos;
			*work_pos++      = {hpf[0].filter(frame.left),
			                    hpf[1].filter(frame.right)};
		}
	}

	if (mixer.do_compressor) {
		// Apply compressor to the master output as the very last step
		auto work_pos   = start_work_pos;
		auto num_frames = frames_added;

		while (num_frames--) {
			const auto frame = *work_pos;
			*work_pos++      = mixer.compressor.Process(frame);
		}
	}

	// Capture audio output if requested
	if (CAPTURE_IsCapturingAudio() || CAPTURE_IsCapturingVideo()) {
		int16_t out[CaptureBufFrames][2] = {};

		auto work_pos   = start_work_pos;

		for (auto i = 0; i < frames_added; ++i) {
			const auto in_frame = *work_pos++;

			const auto left = static_cast<uint16_t>(
			        clamp_to_int16(static_cast<int>(in_frame.left)));

			const auto right = static_cast<uint16_t>(
			        clamp_to_int16(static_cast<int>(in_frame.right)));

			out[i][0] = static_cast<int16_t>(host_to_le16(left));
			out[i][1] = static_cast<int16_t>(host_to_le16(right));
		}

		CAPTURE_AddAudioData(mixer.sample_rate_hz,
		                     frames_added,
		                     reinterpret_cast<int16_t*>(out));
	}

	// Reset the frames_per_tick
	mixer.frames_per_tick = calc_frames_per_tick(mixer.sample_rate_hz);
	mixer.frames_done     = frames_requested;
}

static void handle_mix_samples()
{
	MIXER_LockAudioDevice();

	mix_samples(mixer.frames_needed);

	mixer.frame_counter += mixer.frames_per_tick;
	mixer.frames_needed += ifloor(mixer.frame_counter);

	mixer.frame_counter -= floor(mixer.frame_counter);

	MIXER_UnlockAudioDevice();
}

static void reduce_channels_done_counts(const int at_most)
{
	assert(at_most >= 0);

	for (const auto& [_, channel] : mixer.channels) {
		channel->frames_done -= std::min(channel->frames_done.load(), at_most);
	}
}

static void handle_mix_no_sound()
{
	MIXER_LockAudioDevice();

	mix_samples(mixer.frames_needed);

	// Clear piece we've just generated
	auto work_pos   = mixer.work.begin() + mixer.pos;
	auto num_frames = mixer.frames_needed.load();

	mixer.pos += num_frames;

	const auto work_size = check_cast<int>(mixer.work.size());
	if (mixer.pos >= work_size) {
		mixer.pos -= work_size;
	}

	constexpr AudioFrame Silence = {0.0f};
	while (num_frames--) {
		*work_pos++ = Silence;
	}

	reduce_channels_done_counts(mixer.frames_needed);

	// Set values for next tick
	mixer.frame_counter += mixer.frames_per_tick;
	mixer.frames_needed = ifloor(mixer.frame_counter);

	mixer.frame_counter -= floor(mixer.frame_counter);
	mixer.frames_done = 0;

	MIXER_UnlockAudioDevice();
}

// We use floats in the range of 16 bit integers everywhere.
// SDL expects floats to be normalized from 1.0 to -1.0
// It might be better for us to use normalized floats elsewhere in the future.
// For now, that probably breaks some assumptions elsewhere in the mixer.
// So just normalize as a final step before sending the data to SDL.
static float normalize_sample(float sample)
{
	return sample / 32768.0f;
}

static void SDLCALL mixer_callback([[maybe_unused]] void* userdata,
                                   Uint8* stream, int bytes_requested)
{
	assert(bytes_requested >= 0);

	ZoneScoped;
	memset(stream, 0, static_cast<size_t>(bytes_requested));

	constexpr auto BytesPer32BitSample = 4;
	constexpr auto BytesPerSampleFrame = BytesPer32BitSample * 2; // stereo

	const auto frames_requested = bytes_requested / BytesPerSampleFrame;

	auto reduce_frames = 0;

	// Used for time-stretching the audio in the buffer overrun scenario.
	float index     = 0;
	float index_add = 0;

	if (mixer.frames_done < frames_requested) {
		// Buffer underrun
#if 0
		LOG_WARNING("MIXER: Full underrun requested %d, have %d, min %d",
		            frames_requested,
		            mixer.frames_done.load(),
		            mixer.min_frames_needed.load());
#endif

		reduce_frames = mixer.frames_done;

	} else if (mixer.frames_done < mixer.max_frames_needed) {
		auto frames_remaining = mixer.frames_done - frames_requested;

		if (frames_remaining < mixer.min_frames_needed) {
			frames_remaining = (mixer.min_frames_needed - frames_remaining);

			frames_remaining = 1 + (2 * frames_remaining) /
			                               mixer.min_frames_needed;
#if 0
			LOG_WARNING("MIXER: Needed underrun requested %d, have %d, min %d, remaining %d",
			            frames_requested,
			            mixer.frames_done.load(),
			            mixer.min_frames_needed.load(),
			            frames_remaining);
#endif
			reduce_frames = frames_requested - frames_remaining;

		} else {
			reduce_frames = frames_requested;
#if 0
			LOG_MSG("MIXER: Regular run requested %d, have %d, min %d, remaining %d",
			        frames_requested,
			        mixer.frames_done.load(),
			        mixer.min_frames_needed.load(),
			        frames_remaining);
#endif
		}

	} else {
		// Buffer overrun -- this usually happens in fast-forward mode.
#if 0
		LOG_WARNING("MIXER: Overflow run requested %d, have %d, min % d ",
		            frames_requested,
		            mixer.frames_done.load(),
		            mixer.min_frames_needed.load());
#endif
		if (mixer.frames_done > MixerBufferByteSize) {
			index_add = static_cast<float>(
			        MixerBufferByteSize - 2 * mixer.min_frames_needed);
		} else {
			index_add = static_cast<float>(
			        mixer.frames_done - 2 * mixer.min_frames_needed);
		}

		index_add = index_add / static_cast<float>(frames_requested);
		reduce_frames = mixer.frames_done - 2 * mixer.min_frames_needed;
	}

	reduce_channels_done_counts(reduce_frames);

	// Reset mixer.frames_per_tick
	mixer.frames_per_tick = calc_frames_per_tick(mixer.sample_rate_hz);

	mixer.frames_done -= reduce_frames;

	const auto frames_needed = mixer.frames_needed - reduce_frames;
	mixer.frames_needed      = frames_needed;

	auto output = reinterpret_cast<float*>(stream);

	if (frames_requested != reduce_frames) {
		// We're doing a very crude sample-skipping style audio
		// stretching here. However, it's worth keeping it as this path
		// is almost exclusively used in the fast-forward mode to
		// achieves that cool "tape speed-up" effect.
		//
		// Without this effect, the audio becomes a crackling mess when
		// fast-forwarding.
		auto num_frames = std::min(reduce_frames, frames_requested);
		auto work_pos   = mixer.work.begin() + mixer.pos.load();

		while (num_frames--) {
			index += index_add;

			const auto frame = *(work_pos + static_cast<int>(index));

			*output++ = normalize_sample(frame.left);
			*output++ = normalize_sample(frame.right);
		}
	} else {
		auto num_frames = reduce_frames;
		auto work_pos   = mixer.work.begin() + mixer.pos.load();

		while (num_frames--) {
			const auto frame = *work_pos++;

			*output++ = normalize_sample(frame.left);
			*output++ = normalize_sample(frame.right);
		}
	}

	// Clear the used buffers
	{
		auto num_frames = reduce_frames;

		const auto pos_offset = mixer.pos.load();
		auto work_pos         = mixer.work.begin() + pos_offset;
		auto aux_reverb_pos   = mixer.aux_reverb.begin() + pos_offset;
		auto aux_chorus_pos   = mixer.aux_chorus.begin() + pos_offset;

		while (num_frames--) {
			constexpr AudioFrame Silence = {0.0f};

			*work_pos++       = Silence;
			*aux_reverb_pos++ = Silence;
			*aux_chorus_pos++ = Silence;
		}
	}

	mixer.pos += reduce_frames;

	const auto work_size = check_cast<int>(mixer.work.size());
	if (mixer.pos >= work_size) {
		mixer.pos -= work_size;
	}
}

static void stop_mixer([[maybe_unused]] Section* sec) {}

[[maybe_unused]] static const char* to_string(const MixerState s)
{
	switch (s) {
	case MixerState::Uninitialized: return "Uninitialized";
	case MixerState::NoSound: return "No sound";
	case MixerState::On: return "On";
	case MixerState::Muted: return "Mute";
	}
	return "unknown!";
}

static void set_mixer_state(const MixerState new_state)
{
	// Only Muted <-> On state transitions are allowed
	assert(new_state == MixerState::Muted || new_state == MixerState::On);

#ifdef DEBUG_MIXER
	LOG_MSG("MIXER: Changing mixer state from %s to '%s'",
	        to_string(mixer.state),
	        to_string(new_state));
#endif

	if (mixer.state == new_state) {
		// Nothing to do.
#ifdef DEBUG_MIXER
		LOG_MSG("MIXER: Already in '%s' state, skipping",
		        to_string(mixer.state));
#endif

	} else if (new_state == MixerState::Muted) {
		TIMER_DelTickHandler(handle_mix_samples);
		TIMER_AddTickHandler(handle_mix_no_sound);

	} else if (new_state == MixerState::On) {
		TIMER_DelTickHandler(handle_mix_no_sound);
		TIMER_AddTickHandler(handle_mix_samples);

	} else {
		assertm(false, "Invalid mixer state transition");
	}

	mixer.state = new_state;

	// Finally, we start the audio device either paused or unpaused:
	if (mixer.sdl_device > 0) {
		const auto pause = (mixer.state != MixerState::On);
		SDL_PauseAudioDevice(mixer.sdl_device, pause);
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

	if (mixer.sdl_device > 0) {
		SDL_CloseAudioDevice(mixer.sdl_device);
		mixer.sdl_device = 0;
	}
	mixer.state = MixerState::Uninitialized;
}

// Sets `mixer.sample_rate_hz` and `mixer.blocksize` on success
static bool init_sdl_sound(const int requested_sample_rate_hz,
                           const int requested_blocksize_in_frames,
                           const bool allow_negotiate)
{
	SDL_AudioSpec desired  = {};
	SDL_AudioSpec obtained = {};

	constexpr auto NumStereoChannels = 2;

	desired.channels = NumStereoChannels;
	desired.format   = AUDIO_F32SYS;
	desired.freq     = requested_sample_rate_hz;
	desired.samples  = check_cast<uint16_t>(requested_blocksize_in_frames);

	desired.callback = mixer_callback;
	desired.userdata = nullptr;

	int sdl_allow_flags = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE;

	if (allow_negotiate) {
		// Allow negotiating the audio buffer size, hopefully to obtain
		// a block size that achieves stutter-free playback at a low
		// latency.
		sdl_allow_flags |= SDL_AUDIO_ALLOW_SAMPLES_CHANGE;
	}

	// Open the audio device
	constexpr auto SdlError = 0;

	// Null requests the most reasonable default device
	constexpr auto DeviceName = nullptr;
	// Non-zero is the device is to be opened for recording as well
	constexpr auto IsCapture = 0;

	if ((mixer.sdl_device = SDL_OpenAudioDevice(
	             DeviceName, IsCapture, &desired, &obtained, sdl_allow_flags)) ==
	    SdlError) {
		LOG_WARNING("MIXER: Can't open audio device: '%s'; sound output is disabled",
		            SDL_GetError());

		set_section_property_value("mixer", "nosound", "off");
		return false;
	}

	// Opening SDL audio device succeeded
	//
	// An opened audio device starts out paused, and should be enabled for
	// playing by calling `SDL_PauseAudioDevice()` when you are ready for your
	// audio callback function to be called. We do that in
	// `set_mixer_state()`.
	//
	const auto obtained_sample_rate_hz = obtained.freq;
	const auto obtained_blocksize      = obtained.samples;

	mixer.sample_rate_hz = obtained_sample_rate_hz;
	mixer.blocksize      = obtained_blocksize;

	assert(obtained.channels == NumStereoChannels);
	assert(obtained.format == desired.format);

	// Did SDL negotiate a different playback rate?
	if (obtained_sample_rate_hz != requested_sample_rate_hz) {
		LOG_WARNING("MIXER: SDL negotiated the requested sample rate of %d to %d Hz",
		            requested_sample_rate_hz,
		            obtained_sample_rate_hz);

		set_section_property_value("mixer",
		                           "rate",
		                           format_str("%d",
		                                      mixer.sample_rate_hz.load()));
	}

	// Did SDL negotiate a different blocksize?
	if (obtained_blocksize != requested_blocksize_in_frames) {
		LOG_MSG("MIXER: SDL negotiated the requested blocksize of "
		        "%d to %d frames",
		        requested_blocksize_in_frames,
		        obtained_blocksize);

		set_section_property_value("mixer",
		                           "blocksize",
		                           format_str("%d", mixer.blocksize));
	}

	LOG_MSG("MIXER: Initialised stereo %d Hz audio with %d sample frame buffer",
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
	constexpr auto HighpassCutoffFreqHz = 20.0;
	for (auto& f : mixer.highpass_filter) {
		f.setup(mixer.sample_rate_hz, HighpassCutoffFreqHz);
	}
}

void MIXER_Init(Section* sec)
{
	Section_prop* secprop = static_cast<Section_prop*>(sec);
	assert(secprop);

	if (mixer.state == MixerState::Uninitialized) {
		// Initialize the 8-bit to 16-bit lookup table
		fill_8to16_lut();

		const auto mixer_state = secprop->Get_bool("nosound")
		                               ? MixerState::NoSound
		                               : MixerState::On;

		auto set_no_sound = [&] {
			assert(mixer.sdl_device == 0);

			LOG_MSG("MIXER: Sound output disabled ('nosound' mode)");

			mixer.state = MixerState::NoSound;

			// This ensures we internally generate the audio
			// stream, so it can be captured, and the
			// "rendered frames" counters keep progressing.
			//
			// Without this, certain programs that rely on
			// these counters for timing purposes would be
			// stalled (typically trackers, e.g., Fast
			// Tracker II, PT4DOS, etc.)
			//
			TIMER_AddTickHandler(handle_mix_no_sound);
		};

		mixer.sample_rate_hz = secprop->Get_int("rate");
		mixer.blocksize      = secprop->Get_int("blocksize");

		if (mixer_state == MixerState::NoSound) {
			set_no_sound();

		} else {
			if (init_sdl_sound(secprop->Get_int("rate"),
							   secprop->Get_int("blocksize"),
							   secprop->Get_bool("negotiate"))) {

				// This also unpauses the audio device which is
				// opened in paused mode by SDL.
				set_mixer_state(MixerState::On);
			} else {
				set_no_sound();
			}
		}

		const auto requested_prebuffer_ms = secprop->Get_int("prebuffer");
		mixer.prebuffer_ms = clamp(requested_prebuffer_ms, 1, MaxPrebufferMs);

		const auto prebuffer_frames = (mixer.sample_rate_hz *
		                               mixer.prebuffer_ms) /
		                              1000;
		mixer.pos = 0;
		mixer.frames_per_tick = calc_frames_per_tick(mixer.sample_rate_hz);
		mixer.frame_counter     = 0;
		mixer.frames_done       = 0;
		mixer.frames_needed     = mixer.min_frames_needed + 1;
		mixer.min_frames_needed = 0;
		mixer.max_frames_needed = mixer.blocksize * 2 + 2 * prebuffer_frames;

		sec->AddDestroyFunction(&stop_mixer);
	}

	// Initialise crossfeed
	const auto new_crossfeed_preset = crossfeed_pref_to_preset(
	        secprop->Get_string("crossfeed"));

	if (mixer.crossfeed.preset != new_crossfeed_preset) {
		MIXER_SetCrossfeedPreset(new_crossfeed_preset);
	}

	// Initialise reverb
	const auto new_reverb_preset = reverb_pref_to_preset(
	        secprop->Get_string("reverb"));

	if (mixer.reverb.preset != new_reverb_preset) {
		MIXER_SetReverbPreset(new_reverb_preset);
	}

	// Initialise chorus
	const auto new_chorus_preset = chorus_pref_to_preset(
	        secprop->Get_string("chorus"));

	if (mixer.chorus.preset != new_chorus_preset) {
		MIXER_SetChorusPreset(new_chorus_preset);
	}

	init_master_highpass_filter();

	// Initialise master compressor
	init_compressor(secprop->Get_bool("compressor"));
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

static void init_mixer_dosbox_settings(Section_prop& sec_prop)
{
#if defined(WIN32)
	// Longstanding known-good defaults for Windows
	constexpr auto DefaultBlocksize      = 1024;
	constexpr auto DefaultPrebufferMs    = 25;
	constexpr bool DefaultAllowNegotiate = false;

#else
	// Non-Windows platforms tolerate slightly lower latency
	constexpr auto DefaultBlocksize      = 512;
	constexpr auto DefaultPrebufferMs    = 20;
	constexpr bool DefaultAllowNegotiate = true;
#endif

	constexpr auto WhenIdle    = Property::Changeable::WhenIdle;
	constexpr auto OnlyAtStart = Property::Changeable::OnlyAtStart;

	auto bool_prop = sec_prop.Add_bool("nosound", OnlyAtStart, false);
	assert(bool_prop);
	bool_prop->Set_help(
	        "Enable silent mode (disabled by default).\n"
	        "Sound is still emulated in silent mode, but DOSBox outputs no sound to the host.\n"
	        "Capturing the emulated audio output to a WAV file works in silent mode.");

	auto int_prop = sec_prop.Add_int("rate", OnlyAtStart, DefaultSampleRateHz);
	assert(int_prop);
	int_prop->SetMinMax(8000, 96000);
	int_prop->Set_help(
	        "Sample rate of DOSBox's internal audio mixer in Hz (%s by default).\n"
	        "Valid range is 8000 to 96000 Hz. The vast majority of consumer-grade audio\n"
	        "hardware uses a native rate of 48000 Hz. Recommend leaving this as-is unless\n"
	        "you have good reason to change it. The OS will most likely resample non-standard\n"
	        "sample rates to 48000 Hz anyway.");

	int_prop = sec_prop.Add_int("blocksize", OnlyAtStart, DefaultBlocksize);
	int_prop->SetMinMax(64, 8192);
	int_prop->Set_help(
	        "Block size of the host audio device in sample frames (%s by default).\n"
	        "Valid range is 64 to 8192. Should be set to power-of-two values (e.g., 256,\n"
	        "512, 1024, etc.) Larger values might help with sound stuttering but will\n"
	        "introduce more latency. Also see 'negotiate'.");

	int_prop = sec_prop.Add_int("prebuffer", OnlyAtStart, DefaultPrebufferMs);
	int_prop->SetMinMax(0, MaxPrebufferMs);
	int_prop->Set_help(
	        "How many milliseconds of sound to render in advance on top of 'blocksize'\n"
	        "(%s by default). Larger values might help with sound stuttering but will\n"
	        "introduce more latency.");

	bool_prop = sec_prop.Add_bool("negotiate", OnlyAtStart, DefaultAllowNegotiate);
	bool_prop->Set_help(
	        "Negotiate a possibly better 'blocksize' setting (%s by default).\n"
	        "Enable it if you're not getting audio or the sound is stuttering with your\n"
	        "'blocksize' setting. Disable it to force the manually set 'blocksize' value.");

	constexpr auto DefaultOn = true;
	bool_prop = sec_prop.Add_bool("compressor", WhenIdle, DefaultOn);
	bool_prop->Set_help(
	        "Enable the auto-leveling compressor on the master channel to prevent clipping\n"
	        "of the audio output:\n"
	        "  off:  Disable compressor.\n"
	        "  on:   Enable compressor (default).");

	auto string_prop = sec_prop.Add_string("crossfeed", WhenIdle, "off");
	string_prop->Set_help(
	        "Enable crossfeed globally on all stereo channels for headphone listening:\n"
	        "  off:     No crossfeed (default).\n"
	        "  on:      Enable crossfeed (normal preset).\n"
	        "  light:   Light crossfeed (strength 15).\n"
	        "  normal:  Normal crossfeed (strength 40).\n"
	        "  strong:  Strong crossfeed (strength 65).\n"
	        "Note: You can fine-tune each channel's crossfeed strength using the MIXER.");
	string_prop->Set_values({"off", "on", "light", "normal", "strong"});

	string_prop = sec_prop.Add_string("reverb", WhenIdle, "off");
	string_prop->Set_help(
	        "Enable reverb globally to add a sense of space to the sound:\n"
	        "  off:     No reverb (default).\n"
	        "  on:      Enable reverb (medium preset).\n"
	        "  tiny:    Simulates the sound of a small integrated speaker in a room;\n"
	        "           specifically designed for small-speaker audio systems\n"
	        "           (PC speaker, Tandy, PS/1 Audio, and LPT DAC devices).\n"
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

	string_prop = sec_prop.Add_string("chorus", WhenIdle, "off");
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

void MIXER_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	constexpr auto ChangeableAtRuntime = true;

	Section_prop* sec = conf->AddSection_prop("mixer",
	                                          &MIXER_Init,
	                                          ChangeableAtRuntime);
	assert(sec);
	init_mixer_dosbox_settings(*sec);
}
