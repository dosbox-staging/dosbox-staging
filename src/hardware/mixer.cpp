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
#include <Tracy.hpp>

#include "ansi_code_markup.h"
#include "control.h"
#include "mem.h"
#include "pic.h"
#include "mixer.h"
#include "timer.h"
#include "setup.h"
#include "cross.h"
#include "string_utils.h"
#include "setup.h"
#include "mapper.h"
#include "hardware.h"
#include "programs.h"
#include "midi.h"

#include "../src/libs/mverb/MVerb.h"

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

using EmVerb = MVerb<float>;

enum ReverbPreset { Tiny, Small, Medium, Large, Huge, NumPresets };

struct reverb_preset_t {
	std::array<float, EmVerb::NUM_PARAMS> params = {};

	float synthesizer_send_level   = 0.0f;
	float digital_audio_send_level = 0.0f;

	float highpass_cutoff_freq = 1.0f;
};

static std::array<reverb_preset_t, ReverbPreset::NumPresets> reverb_presets = {};

struct mixer_t {
	// complex types
	matrix<float, MIXER_BUFSIZE, 2> work = {};
	matrix<float, MIXER_BUFSIZE, 2> aux_reverb = {};

	std::vector<float> resample_temp = {};
	std::vector<float> resample_out = {};

	AudioFrame mastervol = {1.0f, 1.0f};
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

	SDL_AudioDeviceID sdldevice = 0;

	MixerState state = MixerState::Uninitialized; // use MIXER_SetState() to change

	std::array<Iir::Butterworth::HighPass<2>, 2> highpass_filter = {};

	struct {
		bool enabled = false;
		ReverbPreset current_preset = {};

		EmVerb mverb = {};

		// MVerb does not have an integrated high-pass filter to shape
		// the low-end response like other reverbs. So we're adding one
		// here. This helps take control over low-frequency build-up,
		// resulting in a more pleasant sound.
		std::array<Iir::Butterworth::HighPass<2>, 2> highpass_filter = {};
	} reverb = {};
};

static struct mixer_t mixer = {};

alignas(sizeof(float)) uint8_t MixTemp[MIXER_BUFSIZE] = {};

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
          features(_features)
{}

bool MixerChannel::HasFeature(const ChannelFeature feature)
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

	const std::string crossfeed_pref = sect->Get_string("crossfeed");
	if (crossfeed_pref == "on") {
		constexpr auto default_crossfeed_strength = 0.3f;
		crossfeed = default_crossfeed_strength;
	} else if (crossfeed_pref == "off") {
		crossfeed = 0.0f;
	} else {
		const auto cf = to_finite<float>(crossfeed_pref);
		if (std::isfinite(cf) && cf >= 0.0 && cf <= 100.0) {
			crossfeed = cf / 100.0f;
		} else {
			LOG_WARNING("MIXER: Invalid crossfeed value '%s', using off",
			            crossfeed_pref.c_str());
		}
	}
	channel->SetCrossfeedStrength(crossfeed);
}

static void init_reverb_presets()
{
	// tiny
	auto &tiny = reverb_presets[ReverbPreset::Tiny];

	tiny.params[EmVerb::PREDELAY]      = 0.0f;
	tiny.params[EmVerb::EARLYMIX]      = 1.0f;
	tiny.params[EmVerb::SIZE]          = 0.05f;
	tiny.params[EmVerb::DENSITY]       = 0.5f;
	tiny.params[EmVerb::BANDWIDTHFREQ] = 0.5f;
	tiny.params[EmVerb::DECAY]         = 0.0f;
	tiny.params[EmVerb::DAMPINGFREQ]   = 1.0f;

	tiny.synthesizer_send_level   = 0.87f; // -5.2 dB
	tiny.digital_audio_send_level = 0.87f; // -5.2 dB

	tiny.highpass_cutoff_freq = 200.0f;

	// small
	auto &small = reverb_presets[ReverbPreset::Small];

	small.params[EmVerb::PREDELAY]      = 0.0f;
	small.params[EmVerb::EARLYMIX]      = 1.0f;
	small.params[EmVerb::SIZE]          = 0.17f;
	small.params[EmVerb::DENSITY]       = 0.42f;
	small.params[EmVerb::BANDWIDTHFREQ] = 0.50f;
	small.params[EmVerb::DECAY]         = 0.50f;
	small.params[EmVerb::DAMPINGFREQ]   = 0.70f;

	small.synthesizer_send_level   = 0.40f; // -24.0 dB
	small.digital_audio_send_level = 0.08f; // -36.8 dB

	small.highpass_cutoff_freq = 200.0f;

	// medium
	auto &medium = reverb_presets[ReverbPreset::Medium];

	medium.params[EmVerb::PREDELAY]      = 0.0f;
	medium.params[EmVerb::EARLYMIX]      = 0.75f;
	medium.params[EmVerb::SIZE]          = 0.50f;
	medium.params[EmVerb::DENSITY]       = 0.50f;
	medium.params[EmVerb::BANDWIDTHFREQ] = 0.95f;
	medium.params[EmVerb::DECAY]         = 0.42f;
	medium.params[EmVerb::DAMPINGFREQ]   = 0.21f;

	medium.synthesizer_send_level   = 0.54f; // -18.4 dB
	medium.digital_audio_send_level = 0.07f; // -37.2 dB

	medium.highpass_cutoff_freq = 170.0f;

	// large
	auto &large = reverb_presets[ReverbPreset::Large];

	large.params[EmVerb::PREDELAY]      = 0.0f;
	large.params[EmVerb::EARLYMIX]      = 0.75f;
	large.params[EmVerb::SIZE]          = 0.75f;
	large.params[EmVerb::DENSITY]       = 0.50f;
	large.params[EmVerb::BANDWIDTHFREQ] = 0.95f;
	large.params[EmVerb::DECAY]         = 0.52f;
	large.params[EmVerb::DAMPINGFREQ]   = 0.21f;

	large.synthesizer_send_level   = 0.70f; // -12.0 dB
	large.digital_audio_send_level = 0.05f; // -38.0 dB

	large.highpass_cutoff_freq = 140.0f;

	// huge
	auto &huge = reverb_presets[ReverbPreset::Huge];

	huge                        = large;
	huge.synthesizer_send_level = 0.85f; // -6.0 dB
}

static void set_reverb_preset(const ReverbPreset preset)
{
	assert(preset >= 0);
	assert(preset < ReverbPreset::NumPresets);

	auto &param_values = reverb_presets[preset].params;
	for (auto param = 0; param < EmVerb::NUM_PARAMS; ++param) {
		const auto value = param_values[param];
		assert(value >= 0.0f);
		assert(value <= 1.0f);
		mixer.reverb.mverb.setParameter(param, value);
	}

	// Always max gain (no attenuation)
	mixer.reverb.mverb.setParameter(EmVerb::GAIN, 1.0f);

	// Always 100% wet signal
	mixer.reverb.mverb.setParameter(EmVerb::MIX, 1.0f);

	// Configure input high-pass filter
	for (auto &f : mixer.reverb.highpass_filter)
		f.setup(mixer.sample_rate,
		        reverb_presets[preset].highpass_cutoff_freq);

	mixer.reverb.current_preset = preset;
	mixer.reverb.enabled        = true;
}

static void set_reverb_level_from_current_preset(mixer_channel_t channel)
{
	if (!channel->HasFeature(ChannelFeature::ReverbSend))
		return;

	auto &p = reverb_presets[mixer.reverb.current_preset];

	if (channel->HasFeature(ChannelFeature::Synthesizer)) {
		channel->SetReverbLevel(p.synthesizer_send_level);

	} else if (channel->HasFeature(ChannelFeature::DigitalAudio)) {
		channel->SetReverbLevel(p.digital_audio_send_level);
	}
}

static void configure_reverb()
{
	const auto sect = static_cast<Section_prop *>(control->GetSection("mixer"));
	assert(sect);

	const std::string reverb_pref = sect->Get_string("reverb");

	auto enable_reverb = [&reverb_pref](const ReverbPreset preset) {
		mixer.reverb.mverb.setSampleRate(mixer.sample_rate);
		set_reverb_preset(preset);
		LOG_MSG("MIXER: Reverb enabled ('%s' preset)", reverb_pref.c_str());
	};

	if (reverb_pref == "off") {
		mixer.reverb.enabled = false;

	} else if (reverb_pref == "on") {
		enable_reverb(ReverbPreset::Medium);

	} else if (reverb_pref == "tiny") {
		enable_reverb(ReverbPreset::Tiny);

	} else if (reverb_pref == "small") {
		enable_reverb(ReverbPreset::Small);

	} else if (reverb_pref == "medium") {
		enable_reverb(ReverbPreset::Medium);

	} else if (reverb_pref == "large") {
		enable_reverb(ReverbPreset::Large);

	} else if (reverb_pref == "huge") {
		enable_reverb(ReverbPreset::Huge);

	} else {
		LOG_WARNING("MIXER: Invalid reverb setting '%s', using off",
		            reverb_pref.c_str());
		mixer.reverb.enabled = false;
	}

	if (mixer.reverb.enabled) {
		for (auto &it : mixer.channels)
			set_reverb_level_from_current_preset(it.second);
	} else {
		for (auto &it : mixer.channels)
			it.second->SetReverbLevel(0.0f);

		LOG_MSG("MIXER: Reverb disabled");
	}
}

mixer_channel_t MIXER_AddChannel(MIXER_Handler handler, const int freq,
                                 const char *name,
                                 const std::set<ChannelFeature> &features)
{
	auto chan = std::make_shared<MixerChannel>(handler, name, features);
	chan->SetSampleRate(freq);
	chan->SetVolumeScale(1.0);
	chan->SetVolume(1, 1);
	chan->ChangeChannelMap(LEFT, RIGHT);
	chan->Enable(false);

	set_global_crossfeed(chan);

	if (mixer.reverb.enabled)
		set_reverb_level_from_current_preset(chan);

	const auto chan_rate = chan->GetSampleRate();
	if (chan_rate == mixer.sample_rate)
		LOG_MSG("MIXER: %s channel operating at %u Hz without resampling",
		        name, chan_rate);
	else
		LOG_MSG("MIXER: %s channel operating at %u Hz and %s to the output rate", name,
		        chan_rate, chan_rate > mixer.sample_rate ? "downsampling" : "upsampling");

	MIXER_LockAudioDevice();
	mixer.channels[name] = chan; // replace the old, if it exists
	MIXER_UnlockAudioDevice();
	return chan;
}

mixer_channel_t MIXER_FindChannel(const char *name)
{
	MIXER_LockAudioDevice();
	auto it = mixer.channels.find(name);

	// Deprecated alias SPKR to PCSPEAKER
	if (it == mixer.channels.end() && std::string_view(name) == "SPKR") {
		LOG_WARNING("MIXER: '%s' is deprecated due to inconsistent "
		            "naming, please use 'PCSPEAKER' instead",
		            name);
		it = mixer.channels.find("PCSPEAKER");
	}

	const auto chan = (it != mixer.channels.end()) ? it->second : nullptr;
	MIXER_UnlockAudioDevice();

	return chan;
}

void MixerChannel::RegisterLevelCallBack(apply_level_callback_f cb)
{
	apply_level = cb;
	apply_level(volume);
}

void MixerChannel::UpdateVolume()
{
	// Don't scale by volume if the level is being managed by the source
	const float gain_left  = apply_level ? 1.0f : volume.left;
	const float gain_right = apply_level ? 1.0f : volume.right;

	volume_gain.left  = scale.left  * gain_left  * mixer.mastervol.left;
	volume_gain.right = scale.right * gain_right * mixer.mastervol.right;
}

void MixerChannel::SetVolume(float left, float right)
{
	// Allow unconstrained user-defined values
	volume = {left, right};

	if (apply_level)
		apply_level(volume);

	UpdateVolume();
}

void MixerChannel::SetVolumeScale(float f) {
	SetVolumeScale(f, f);
}

void MixerChannel::SetVolumeScale(float left, float right)
{
	// Constrain application-defined volume between 0% and 100%
	constexpr auto min_volume = 0.0f;
	constexpr auto max_volume = 1.0f;

	left  = clamp(left, min_volume, max_volume);
	right = clamp(right, min_volume, max_volume);

	if (scale.left != left || scale.right != right) {
		scale.left  = left;
		scale.right = right;
		UpdateVolume();
#ifdef DEBUG
		LOG_MSG("MIXER %-7s channel: application changed left and right volumes to %3.0f%% and %3.0f%%, respectively",
		        name,
		        scale.left * 100.0f,
		        scale.right * 100.0f);
#endif
	}
}

static void MIXER_UpdateAllChannelVolumes()
{
	MIXER_LockAudioDevice();

	for (auto &it : mixer.channels)
		it.second->UpdateVolume();

	MIXER_UnlockAudioDevice();
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
	}
	is_enabled = should_enable;
	MIXER_UnlockAudioDevice();
}

void MixerChannel::ConfigureResampler()
{
	const auto in_rate = zoh_upsampler.enabled ? zoh_upsampler.target_freq
	                                           : sample_rate;
	const auto out_rate = mixer.sample_rate.load();
	if (in_rate == out_rate) {
		resampler.enabled = false;
	} else {
		if (!resampler.state) {
			constexpr auto num_channels = 2; // always stereo
			constexpr auto quality = 5;
			resampler.state = speex_resampler_init(
			        num_channels, in_rate, out_rate, quality, nullptr);
		}
		speex_resampler_set_rate(resampler.state, in_rate, out_rate);
		resampler.enabled = true;
	}
}

void MixerChannel::SetSampleRate(const int rate)
{
	if (rate) {
		sample_rate = rate;
	} else {
		// If the channel rate is zero, then avoid resampling by running
		// the channel at the same rate as the mixer
		assert(mixer.sample_rate > 0);
		sample_rate = mixer.sample_rate;
	}
	freq_add = (sample_rate << FREQ_SHIFT) / mixer.sample_rate;
	envelope.Update(sample_rate, peak_amplitude,
	                ENVELOPE_MAX_EXPANSION_OVER_MS, ENVELOPE_EXPIRES_AFTER_S);

	ConfigureResampler();
	UpdateZOHUpsamplerState();
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
	frames_needed = frames_requested;
	while (is_enabled && frames_needed > frames_done) {
		auto frames_remaining = frames_needed - frames_done;
		frames_remaining *= freq_add;
		frames_remaining = (frames_remaining >> FREQ_SHIFT) +
		                   ((frames_remaining & FREQ_MASK) != 0);
		if (frames_remaining <= 0) // avoid underflow
			break;
		frames_remaining = std::min(frames_remaining, MIXER_BUFSIZE); // avoid overflow
		handler(check_cast<uint16_t>(frames_remaining));
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
				        prev_frame.left * volume_gain.left;

				mixer.work[mixpos][mapped_output_right] +=
				        (stereo ? prev_frame.right : prev_frame.left) *
				        volume_gain.right;

				prev_frame = next_frame;
				mixpos++;
				frames_done++;
				freq_counter = FREQ_NEXT;
			}
		}
	}
	last_samples_were_silence = true;
	offset[0] = offset[1] = 0;

	MIXER_UnlockAudioDevice();
}

static const std::map<FilterState, std::string> filter_state_map = {
        {FilterState::Off, "disabled"},
        {FilterState::On, "enabled"},
        {FilterState::ForcedOn, "enabled (forced)"},
};

static std::string filter_to_string(const uint8_t order, const uint16_t cutoff_freq)
{
	char buf[100] = {};
	safe_sprintf(buf, " (%d dB/oct at %d Hz)", order * 6, cutoff_freq);
	return std::string(buf);
}

void MixerChannel::SetHighPassFilter(const FilterState state)
{
	filters.highpass.state = state;

	const auto it = filter_state_map.find(state);
	if (it != filter_state_map.end()) {
		const auto filter_state = it->second;
		const auto filter_string =
		        state == FilterState::Off
		                ? ""
		                : filter_to_string(filters.highpass.order,
		                                   filters.highpass.cutoff_freq);
		LOG_MSG("MIXER: %s channel highpass filter %s%s",
		        name.c_str(),
		        filter_state.c_str(),
		        filter_string.c_str());
	}
}

void MixerChannel::SetLowPassFilter(const FilterState state)
{
	filters.lowpass.state = state;

	const auto it = filter_state_map.find(state);
	if (it != filter_state_map.end()) {
		const auto filter_state = it->second;
		const auto filter_string =
		        state == FilterState::Off
		                ? ""
		                : filter_to_string(filters.lowpass.order,
		                                   filters.lowpass.cutoff_freq);
		LOG_MSG("MIXER: %s channel lowpass filter %s%s",
		        name.c_str(),
		        filter_state.c_str(),
		        filter_string.c_str());
	}
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

void MixerChannel::ConfigureZeroOrderHoldUpsampler(const uint16_t target_freq)
{
	zoh_upsampler.target_freq = target_freq;
	ConfigureResampler();
	UpdateZOHUpsamplerState();
}

void MixerChannel::EnableZeroOrderHoldUpsampler(const bool enabled)
{
	zoh_upsampler.enabled = enabled;
	ConfigureResampler();
	UpdateZOHUpsamplerState();
}

void MixerChannel::UpdateZOHUpsamplerState()
{
	// we only allow upsampling
	zoh_upsampler.step = std::min(static_cast<float>(sample_rate) /
	                                      zoh_upsampler.target_freq,
	                              1.0f);
	zoh_upsampler.pos = 0.0f;
}

void MixerChannel::SetCrossfeedStrength(const float strength)
{
	assert(strength >= 0.0f);
	assert(strength <= 1.0f);

	if (!HasFeature(ChannelFeature::Stereo))
		return;

	crossfeed.strength = strength;

	// map [0, 1] range to [0.5, 0]
	auto p = (1.0f - strength) / 2.0f;
	constexpr auto center = 0.5f;
	crossfeed.pan_left = center - p;
	crossfeed.pan_right = center + p;
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

	if (!HasFeature(ChannelFeature::ReverbSend))
		return;

	reverb.level = level;

	float level_db = {};
	if (level == level_min) {
		level_db = -INFINITY;

		reverb.send_gain = 0.0f;

	} else {
		level_db = remap(level_min, level_max, level_min_db, level_max_db, level);
		reverb.send_gain = decibel_to_gain(level_db);
	}

	DEBUG_LOG_MSG("MIXER: SetReverbLevel: level: %4f, level_db: %.4f, gain: %.4f",
	              level,
	              level_db,
	              reverb.send_gain);
}

float MixerChannel::GetReverbLevel()
{
	return reverb.level;
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
						        (HostPt)&data[pos * 2 + 0]);
						frame[1] = (int16_t)host_readw(
						        (HostPt)&data[pos * 2 + 1]);
					} else {
						frame[0] = static_cast<float>((int32_t)host_readd(
						        (HostPt)&data[pos * 2 + 0]));
						frame[1] = static_cast<float>((int32_t)host_readd(
						        (HostPt)&data[pos * 2 + 1]));
					}
				}
			} else { // mono
				if (nativeorder) {
					frame[0] = static_cast<float>(data[pos]);
				} else {
					if (sizeof(Type) == 2) {
						frame[0] = (int16_t)host_readw(
						        (HostPt)&data[pos]);
					} else {
						frame[0] = static_cast<float>(
						        (int32_t)host_readd(
						                (HostPt)&data[pos]));
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
						                (HostPt)&data[pos * 2 + 0])) -
						        offs);
						frame[1] = static_cast<float>(
						        static_cast<int>(host_readw(
						                (HostPt)&data[pos * 2 + 1])) -
						        offs);
					} else {
						frame[0] = static_cast<float>(
						        static_cast<int>(host_readd(
						                (HostPt)&data[pos * 2 + 0])) -
						        offs);
						frame[1] = static_cast<float>(
						        static_cast<int>(host_readd(
						                (HostPt)&data[pos * 2 + 1])) -
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
						                (HostPt)&data[pos])) -
						        offs);
					} else {
						frame[0] = static_cast<float>(
						        static_cast<int>(host_readd(
						                (HostPt)&data[pos])) -
						        offs);
					}
				}
			}
		}
	}
	return frame;
}

// Convert sample data to floats and remove clicks.
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
				next_frame.left = static_cast<float>(data[pos * 2 + 0]);
				next_frame.right = static_cast<float>(data[pos * 2 + 1]);
			} else {
				next_frame.left = static_cast<float>(data[pos]);
			}
		} else {
			next_frame = ConvertNextFrame<Type, stereo, signeddata, nativeorder>(
			        data, pos);
		}

		// Process initial samples through an expanding envelope to
		// prevent severe clicks and pops. Becomes a no-op when done.
		envelope.Process(stereo, prev_frame);

		const auto left = prev_frame[mapped_channel_left] * volume_gain.left;
		const auto right = (stereo ? prev_frame[mapped_channel_right]
		                           : prev_frame[mapped_channel_left]) *
		                   volume_gain.right;

		out_frame = {0.0f, 0.0f};
		out_frame[mapped_output_left] += left;
		out_frame[mapped_output_right] += right;

		out.emplace_back(out_frame[0]);
		out.emplace_back(out_frame[1]);

		if (zoh_upsampler.enabled) {
			zoh_upsampler.pos += zoh_upsampler.step;
			if (zoh_upsampler.pos > 1.0f) {
				zoh_upsampler.pos -= 1.0f;
				++pos;
			}
		} else
			++pos;
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

template <class Type, bool stereo, bool signeddata, bool nativeorder>
void MixerChannel::AddSamples(const uint16_t frames, const Type *data)
{
	assert(frames > 0);

	last_samples_were_stereo = stereo;

	auto &convert_out = resampler.enabled ? mixer.resample_temp
	                                      : mixer.resample_out;
	ConvertSamples<Type, stereo, signeddata, nativeorder>(data, frames, convert_out);

	if (resampler.enabled) {
		spx_uint32_t in_frames = mixer.resample_temp.size() / 2;
		spx_uint32_t out_frames = estimate_max_out_frames(resampler.state,
		                                                  in_frames);
		mixer.resample_out.resize(out_frames * 2);

		speex_resampler_process_interleaved_float(resampler.state,
		                                          mixer.resample_temp.data(),
		                                          &in_frames,
		                                          mixer.resample_out.data(),
		                                          &out_frames);

		// out_frames now contains the actual number of resampled frames,
		// ensure the number of output frames is within the logical size.
		assert(out_frames <= mixer.resample_out.size() / 2);
		mixer.resample_out.resize(out_frames * 2);  // only shrinks
	}

	MIXER_LockAudioDevice();

	// Optionally filter, apply crossfeed, then mix the results to the master
	// output
	auto pos = mixer.resample_out.begin();
	auto mixpos = check_cast<work_index_t>(mixer.pos + frames_done);

	const auto do_lowpass_filter = filters.lowpass.state == FilterState::On ||
	                               filters.lowpass.state == FilterState::ForcedOn;

	const auto do_highpass_filter = filters.highpass.state == FilterState::On ||
	                                filters.highpass.state ==
	                                        FilterState::ForcedOn;

	const auto do_crossfeed = crossfeed.strength > 0.0f;

	const auto do_reverb_send = mixer.reverb.enabled &&
	                            HasFeature(ChannelFeature::ReverbSend) &&
	                            reverb.send_gain > 0.0f;

	while (pos != mixer.resample_out.end()) {
		mixpos &= MIXER_BUFMASK;

		AudioFrame frame = {*pos++, *pos++};

		if (do_highpass_filter) {
			frame.left  = filters.highpass.hpf[0].filter(frame.left);
			frame.right = filters.highpass.hpf[1].filter(frame.right);
		}
		if (do_lowpass_filter) {
			frame.left  = filters.lowpass.lpf[0].filter(frame.left);
			frame.right = filters.lowpass.lpf[1].filter(frame.right);
		}
		if (do_crossfeed)
			frame = ApplyCrossfeed(frame);

		// Mix samples to the master output
		mixer.work[mixpos][0] += frame.left;
		mixer.work[mixpos][1] += frame.right;

		if (do_reverb_send) {
			// Mix samples to the reverb aux buffer, scaled by the
			// reverb send volume
			mixer.aux_reverb[mixpos][0] += frame.left  * reverb.send_gain;
			mixer.aux_reverb[mixpos][1] += frame.right * reverb.send_gain;
		}

		++mixpos;
	}

	auto out_frames = mixer.resample_out.size() / 2;
	frames_done += out_frames;

	last_samples_were_silence = false;
	MIXER_UnlockAudioDevice();
}

void MixerChannel::AddStretched(uint16_t len, int16_t *data)
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

		mixer.work[mixpos][mapped_output_left]  += sample * volume_gain.left;
		mixer.work[mixpos][mapped_output_right] += sample * volume_gain.right;
		mixpos++;
	}

	frames_done = frames_needed;

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
void MixerChannel::AddSamples_mfloat(uint16_t len, const float *data)
{
	AddSamples<float, false, true, true>(len, data);
}
void MixerChannel::AddSamples_sfloat(uint16_t len, const float *data)
{
	AddSamples<float, true, true, true>(len, data);
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
	if (!is_enabled || frames_done < mixer.frames_done)
		return;
	const auto index = PIC_TickIndex();
	MIXER_LockAudioDevice();
	Mix(check_cast<uint16_t>(static_cast<int64_t>(index * mixer.frames_needed)));
	MIXER_UnlockAudioDevice();
}

std::string MixerChannel::DescribeLineout() const
{
	if (output_map == STEREO)
		return "Stereo";
	if (output_map == REVERSE)
		return "Reverse";

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
	if (resampler.state) {
		speex_resampler_destroy(resampler.state);
		resampler.state = nullptr;
	}
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

// Mix a certain amount of new sample frames
static void MIXER_MixData(int frames_requested)
{
	constexpr auto capture_buf_frames = 1024;

	const auto frames_added = check_cast<work_index_t>(
	        std::min(frames_requested - mixer.frames_done, capture_buf_frames));

	const auto start_pos = check_cast<work_index_t>(
	        (mixer.pos + mixer.frames_done) & MIXER_BUFMASK);

	// Render all channels and accumulate results in the master mixbuffer
	for (auto &it : mixer.channels)
		it.second->Mix(frames_requested);

	if (mixer.reverb.enabled) {
		// Apply reverb to the reverb aux buffer, then mix the results
		// to the master output
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

	// Apply high-pass filter to the master output as the very last step
	auto pos = start_pos;

	for (work_index_t i = 0; i < frames_added; ++i) {
		for (auto ch = 0; ch < 2; ++ch) {
			mixer.work[pos][ch] = mixer.highpass_filter[ch].filter(
			        mixer.work[pos][ch]);
		}
		pos = (pos + 1) & MIXER_BUFMASK;
	}

	// Capture audio output if requested
	if (CaptureState & (CAPTURE_WAVE | CAPTURE_VIDEO)) {
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

		CAPTURE_AddWave(mixer.sample_rate,
		                frames_added,
		                reinterpret_cast<int16_t *>(out));
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
	ZoneScoped
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

			++pos;
		}
	}
}

#undef INDEX_SHIFT_LOCAL

static void MIXER_Stop([[maybe_unused]] Section *sec)
{}

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

	void Run()
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

		mixer_channel_t curr_chan = {};
		auto is_master            = false;

		MIXER_LockAudioDevice();
		for (auto &arg : args) {
			// Does this argument set the target channel of
			// subsequent commands?
			upcase(arg);
			if (arg == "MASTER") {
				curr_chan = nullptr;
				is_master = true;
				continue;
			} else {
				auto chan = MIXER_FindChannel(arg.c_str());
				if (chan) {
					curr_chan = chan;
					is_master = false;
					continue;
				}
			}

			auto parse_prefixed_percentage = [](const char prefix,
			                                    const std::string &s,
			                                    float &value_out) {
				if (s.size() > 1 && s[0] == prefix) {
					float p = 0.0f;
					if (sscanf(s.c_str() + 1, "%f", &p)) {
						value_out = clamp(p / 100, 0.0f, 1.0f);
						return true;
					}
				}
				return false;
			};

			if (!is_master && !curr_chan) {
				// Global commands that apply to all channels
				float value = 0.0f;
				if (parse_prefixed_percentage('X', arg, value)) {
					for (auto &it : mixer.channels) {
						it.second->SetCrossfeedStrength(value);
					}
					continue;
				} else if (parse_prefixed_percentage('R', arg, value)) {
					for (auto &it : mixer.channels) {
						if (mixer.reverb.enabled)
							it.second->SetReverbLevel(value);
					}
					continue;
				}

			} else if (is_master) {
				// Only setting the volume is allowed for the
				// MASTER channel
				ParseVolume(arg, mixer.mastervol);

			} else if (curr_chan) {
				// Adjust settings of a regular non-master channel
				float value = 0.0f;
				if (parse_prefixed_percentage('X', arg, value)) {
					curr_chan->SetCrossfeedStrength(value);
					continue;
				} else if (parse_prefixed_percentage('R', arg, value)) {
					if (mixer.reverb.enabled)
						curr_chan->SetReverbLevel(value);
					continue;
				}

				if (curr_chan->ChangeLineoutMap(arg))
					continue;

				AudioFrame volume = {};
				ParseVolume(arg, volume);

				curr_chan->SetVolume(volume.left, volume.right);
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
		        "    Crossfeed: [color=white]x0[reset] to [color=white]x100[reset]    Reverb: [color=white]r0[reset] to [color=white]r100[reset]\n"
		        "\n"
		        "Notes:\n"
		        "  Running [color=green]mixer[reset] without an argument shows the current mixer settings.\n"
		        "  You may change the settings of more than one channel in a single command.\n"
		        "  If channel is unspecified, you can set crossfeed, reverb or chorus globally.\n"
		        "  You can view the list of available MIDI devices with /listmidi.\n"
		        "  The /noshow option applies the changes without showing the mixer settings.\n"
		        "\n"
		        "Examples:\n"
		        "  [color=green]mixer[reset] [color=cyan]cdda[reset] [color=white]50[reset] [color=cyan]sb[reset] [color=white]reverse[reset] /noshow\n"
		        "  [color=green]mixer[reset] [color=white]x30[reset] [color=cyan]fm[reset] [color=white]150[reset] [color=white]r50[reset] [color=cyan]sb[reset] [color=white]x10[reset]");
	}

	void ParseVolume(const std::string &s, AudioFrame &volume)
	{
		auto vol_parts = split(s, ':');
		if (vol_parts.empty())
			return;

		const auto is_decibel = toupper(vol_parts[0][0]) == 'D';
		if (is_decibel)
			vol_parts[0].erase(0, 1);

		auto parse_vol_pref = [is_decibel](const std::string &vol_pref,
		                                   float &vol_out) {
			const auto vol = to_finite<float>(vol_pref);
			if (std::isfinite(vol)) {
				if (is_decibel)
					vol_out = static_cast<float>(
					        decibel_to_gain(vol));
				else
					vol_out = vol / 100.0f;

				const auto min_vol = static_cast<float>(
				        decibel_to_gain(-99.99));

				constexpr auto max_vol = 99.99f;

				if (vol_out < min_vol)
					vol_out = 0;
				else
					vol_out = std::min(vol_out, max_vol);
			} else {
				vol_out = 0;
			}
		};

		parse_vol_pref(vol_parts[0], volume.left);
		if (vol_parts.size() > 1)
			parse_vol_pref(vol_parts[1], volume.right);
		else
			volume.right = volume.left;
	}

	void ShowMixerStatus()
	{
		auto show_channel = [this](const std::string &name,
		                           const AudioFrame &volume,
		                           const int rate,
		                           const std::string &mode,
		                           const std::string &xfeed,
		                           const std::string &reverb) {
			WriteOut("%-21s %4.0f:%-4.0f %+6.2f:%-+6.2f %8d  %-8s %5s %7s\n",
			         name.c_str(),
			         volume.left * 100.0f,
			         volume.right * 100.0f,
			         gain_to_decibel(volume.left),
			         gain_to_decibel(volume.right),
			         rate,
			         mode.c_str(),
			         xfeed.c_str(),
			         reverb.c_str());
		};

		WriteOut(convert_ansi_markup("[color=white]Channel     Volume    Volume(dB)   Rate(Hz)  Mode     Xfeed  Reverb[reset]\n")
		                 .c_str());

		MIXER_LockAudioDevice();
		show_channel(convert_ansi_markup("[color=cyan]MASTER[reset]"),
		             mixer.mastervol,
		             mixer.sample_rate,
		             "Stereo",
		             "-",
		             "-");

		for (auto &[name, chan] : mixer.channels) {
			std::string xfeed = "-";
			if (chan->HasFeature(ChannelFeature::Stereo)) {
				if (chan->GetCrossfeedStrength() > 0.0f) {
					xfeed = std::to_string(static_cast<uint8_t>(
					        round(chan->GetCrossfeedStrength() *
					              100)));
				} else {
					xfeed = "off";
				}
			}

			std::string reverb = "-";
			if (chan->HasFeature(ChannelFeature::ReverbSend)) {
				if (chan->GetReverbLevel() > 0.0f) {
					reverb = std::to_string(static_cast<uint8_t>(
					        round(chan->GetReverbLevel() * 100)));
				} else {
					reverb = "off";
				}
			}

			auto channel_name = std::string("[color=cyan]") + name +
			                    std::string("[reset]");

			auto mode = chan->HasFeature(ChannelFeature::Stereo)
			                  ? chan->DescribeLineout()
			                  : "Mono";

			show_channel(convert_ansi_markup(channel_name),
			             chan->volume,
			             chan->GetSampleRate(),
			             mode,
			             xfeed,
			             reverb);
		}
		MIXER_UnlockAudioDevice();
	}
};

std::unique_ptr<Program> MIXER_ProgramCreate() {
	return ProgramCreate<MIXER>();
}

bool MIXER_IsManuallyMuted()
{
	return mixer.state == MixerState::Mute;
}

[[maybe_unused]] static const char * MixerStateToString(const MixerState s)
{
	switch (s) {
	case MixerState::Uninitialized: return "uninitialized";
	case MixerState::NoSound: return "no sound";
	case MixerState::Off: return "off";
	case MixerState::On: return "on";
	case MixerState::Mute: return "mute";
	}
	return "unknown!";
}

void MIXER_SetState(MixerState requested)
{
	// The mixer starts uninitialized but should never be programmatically
	// asked to become uninitialized
	assert(requested != MixerState::Uninitialized);

	// When sdldevice is zero then the SDL audio device doesn't exist, which
	// is a valid configuration. In these cases the only logical end-state
	// is NoSound. So switch the request and let the rest of the function
	// handle it.
	if (mixer.sdldevice == 0)
		requested = MixerState::NoSound;

	if (mixer.state == requested) {
		// Nothing to do.
		// LOG_MSG("MIXER: Is already %s, skipping",
		// MixerStateToString(mixer.state));
	} else if (mixer.state == MixerState::On && requested != MixerState::On) {
		TIMER_DelTickHandler(MIXER_Mix);
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
		// LOG_MSG("MIXER: Changed from on to %s",
		// MixerStateToString(requested));
	} else if (mixer.state != MixerState::On && requested == MixerState::On) {
		TIMER_DelTickHandler(MIXER_Mix_NoSound);
		TIMER_AddTickHandler(MIXER_Mix);
		// LOG_MSG("MIXER: Changed from %s to on",
		// MixerStateToString(mixer.state));
	} else {
		// Nothing to do.
		// LOG_MSG("MIXER: Skipping unecessary change from %s to %s",
		// MixerStateToString(mixer.state), MixerStateToString(requested));
	}
	mixer.state = requested;

	// Finally, we start the audio device either paused or unpaused:
	if (mixer.sdldevice)
		SDL_PauseAudioDevice(mixer.sdldevice, mixer.state != MixerState::On);
	//
	// When unpaused, the device pulls frames queued by the MIXER_Mix
	// function, which it fetches from each channel's callback (every
	// millisecond tick), and mixes into a stereo frame buffer.

	// When paused, the audio device stops reading frames from our buffer,
	// so it's imporant that the we no longer queue them, which is why we
	// use MIXER_Mix_NoSound (to throw away frames instead of queuing).
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
		sdl_allow_flags |= SDL_AUDIO_ALLOW_FREQUENCY_CHANGE;
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
		MIXER_SetState(MixerState::NoSound);

	} else if ((mixer.sdldevice = SDL_OpenAudioDevice(
	                    NULL, 0, &spec, &obtained, sdl_allow_flags)) == 0) {
		LOG_WARNING("MIXER: Can't open audio: %s , running in nosound mode.",
		            SDL_GetError());
		mixer.tick_add = calc_tickadd(mixer.sample_rate);
		MIXER_SetState(MixerState::NoSound);

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
		MIXER_SetState(MixerState::On);

		LOG_MSG("MIXER: Negotiated %u-channel %u-Hz audio in %u-frame blocks",
		        obtained.channels,
		        mixer.sample_rate.load(),
		        mixer.blocksize);
	}

	// 1000 = 8 *125
	mixer.tick_counter = (mixer.sample_rate % 125) ? TICK_NEXT : 0;

	const auto requested_prebuffer_ms = section->Get_int("prebuffer");

	const auto prebuffer_ms = static_cast<uint16_t>(
	        clamp(requested_prebuffer_ms, 0, max_prebuffer_ms));

	const auto prebuffer_frames = (mixer.sample_rate * prebuffer_ms) / 1000;

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
	init_reverb_presets();
	configure_reverb();
}

// Toggle the mixer on/off when a true-bool is passed.
static void ToggleMute(const bool was_pressed)
{
	// The "pressed" bool argument is used by the Mapper API, which sends a
	// true-bool for key down events and a false-bool for key up events.
	if (!was_pressed)
		return;

	switch (mixer.state) {
	case MixerState::NoSound:
		LOG_WARNING("MIXER: Mute requested, but sound is off (nosound mode)");
		break;
	case MixerState::Mute:
		MIXER_SetState(MixerState::On);
		LOG_MSG("MIXER: Unmuted");
		break;
	case MixerState::On:
		MIXER_SetState(MixerState::Mute);
		LOG_MSG("MIXER: Muted");
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
	constexpr int default_prebuffer_ms = 20;
	constexpr bool default_allow_negotiate = true;
#endif

	constexpr auto always        = Property::Changeable::Always;
	constexpr auto when_idle     = Property::Changeable::WhenIdle;
	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;

	auto bool_prop = sec_prop.Add_bool("nosound", always, false);
	assert(bool_prop);
	bool_prop->Set_help("Enable silent mode, sound is still emulated though.");

	auto int_prop = sec_prop.Add_int("rate", only_at_start, default_rate);
	assert(int_prop);
	const char *rates[] = {
	        "8000", "11025", "16000", "22050", "32000", "44100", "48000", 0};
	int_prop->Set_values(rates);
	int_prop->Set_help("Mixer sample rate.");

	const char *blocksizes[] = {"128", "256", "512", "1024", "2048", "4096", "8192", 0};

	int_prop = sec_prop.Add_int("blocksize", only_at_start, default_blocksize);
	int_prop->Set_values(blocksizes);
	int_prop->Set_help(
	        "Mixer block size; larger values might help with sound stuttering but sound will also be more lagged.");

	int_prop = sec_prop.Add_int("prebuffer", only_at_start, default_prebuffer_ms);
	int_prop->SetMinMax(0, max_prebuffer_ms);
	int_prop->Set_help(
	        "How many milliseconds of sound to render on top of the blocksize; larger values might help with sound stuttering but sound will also be more lagged.");

	bool_prop = sec_prop.Add_bool("negotiate",
	                              only_at_start,
	                              default_allow_negotiate);
	bool_prop->Set_help(
	        "Let the system audio driver negotiate (possibly) better rate and blocksize settings.");

	auto string_prop = sec_prop.Add_string("crossfeed", when_idle, "off");
	string_prop->Set_help(
	        "Set crossfeed globally on all stereo channels for headphone listening:\n"
	        "  off:         No crossfeed (default).\n"
	        "  on:          Enable crossfeed (at strength 30).\n"
	        "  <strength>:  Set crossfeed strength from 0 to 100, where 0 means no crossfeed (off)\n"
	        "               and 100 full crossfeed (effectively turning stereo content into mono).\n"
	        "Note: You can set per-channel crossfeed via mixer commands.");

	string_prop = sec_prop.Add_string("reverb", when_idle, "off");
	string_prop->Set_help(
	        "Enable reverb globally to add a sense of space to the sound:\n"
	        "  off:     No reverb (default).\n"
	        "  on:      Enable reverb (medium preset).\n"
	        "  tiny:    Simulates the sound of a small integrated speaker in a room;\n"
	        "           specifically designed for small-speaker audio systems\n"
	        "           (PC Speaker, Tandy, PS/1 Audio, and Disney).\n"
	        "  small:   Adds a subtle sense of space; good for games that use a single\n"
	        "           synth channel (typically OPL) for both music and sound effects.\n"
	        "  medium:  Medium room preset that works well with a wide variety of games.\n"
	        "  large:   Large hall preset recommended for games that use separate\n"
	        "  			channels for music and digital audio.\n"
	        "  huge:    A stronger variant of the large hall preset; works really well\n"
	        "           in some games with more atmospheric soundtracks.\n"
	        "Note: You can fine-tune per-channel reverb levels via mixer commands.");

	MAPPER_AddHandler(ToggleMute, SDL_SCANCODE_F8, PRIMARY_MOD, "mute", "Mute");
}

void MIXER_AddConfigSection(const config_ptr_t &conf)
{
	assert(conf);
	Section_prop *sec = conf->AddSection_prop("mixer", &MIXER_Init, true);
	assert(sec);
	init_mixer_dosbox_settings(*sec);
}
