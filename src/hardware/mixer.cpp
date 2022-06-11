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

#define MIXER_SSIZE 4

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

static constexpr int16_t MIXER_CLIP(const int SAMP)
{
	if (SAMP <= MIN_AUDIO) return MIN_AUDIO;
	if (SAMP >= MAX_AUDIO) return MAX_AUDIO;
	
	return static_cast<int16_t>(SAMP);
}

struct mixer_t {
	// complex types
	matrix<float, MIXER_BUFSIZE, 2> work = {};
	std::vector<float> resample_temp = {};
	std::vector<float> resample_out = {};

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
	std::atomic<int> sample_rate = 0; // sample rate negotiated with SDL
	uint16_t blocksize = 0; // matches SDL AudioSpec.samples type

	SDL_AudioDeviceID sdldevice = 0;
	bool nosound = false;
};

static struct mixer_t mixer = {};

uint8_t MixTemp[MIXER_BUFSIZE] = {};

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

static void set_global_channel_settings(mixer_channel_t channel)
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

mixer_channel_t MIXER_AddChannel(MIXER_Handler handler, const int freq,
                                 const char *name,
                                 const std::set<ChannelFeature> &features)
{
	auto chan = std::make_shared<MixerChannel>(handler, name, features);
	chan->SetSampleRate(freq);
	chan->SetScale(1.0);
	chan->SetVolume(1, 1);
	chan->ChangeChannelMap(LEFT, RIGHT);
	chan->Enable(false);

	set_global_channel_settings(chan);

	const auto chan_rate = chan->GetSampleRate();
	if (chan_rate == mixer.sample_rate)
		LOG_MSG("MIXER: %s channel operating at %u Hz without resampling",
		        name, chan_rate);
	else
		LOG_MSG("MIXER: %s channel operating at %u Hz and %s to the output rate", name,
		        chan_rate, chan_rate > mixer.sample_rate ? "downsampling" : "upsampling");

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

		prev_frame[0] = 0.0f;
		prev_frame[1] = 0.0f;
		next_frame[0] = 0.0f;
		next_frame[1] = 0.0f;
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
		if (prev_frame[0] == 0.0f && prev_frame[1] == 0.0f) {
			done = needed;
			// Make sure the next samples are zero when they get
			// switched to prev
			next_frame[0] = 0.0f;
			next_frame[1] = 0.0f;
			// This should trigger an instant request for new samples
			freq_counter = FREQ_NEXT;
		} else {
			bool stereo = last_samples_were_stereo;

			const auto mapped_output_left  = output_map.left;
			const auto mapped_output_right = output_map.right;

			// Position where to write the data
			auto mixpos = check_cast<work_index_t>(mixer.pos + done);
			while (done < needed) {
				// Fade gradually to silence to avoid clicks.
				// Maybe the fade factor f depends on the sample
				// rate.
				constexpr auto f = 4.0f;

				if (prev_frame[0] > f)
					next_frame[0] = prev_frame[0] - f;
				else if (prev_frame[0] < -f)
					next_frame[0] = prev_frame[0] + f;
				else
					next_frame[0] = 0.0f;

				if (prev_frame[1] > f)
					next_frame[1] = prev_frame[1] - f;
				else if (prev_frame[1] < -f)
					next_frame[1] = prev_frame[1] + f;
				else
					next_frame[1] = 0.0f;

				mixpos &= MIXER_BUFMASK;

				mixer.work[mixpos][mapped_output_left] +=
				        prev_frame[0] * volmul[0];

				mixer.work[mixpos][mapped_output_right] +=
				        (stereo ? prev_frame[1] : prev_frame[0]) *
				        volmul[1];

				prev_frame[0] = next_frame[0];
				prev_frame[1] = next_frame[1];
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

void MixerChannel::SetLowPassFilter(const FilterState state)
{
	filter.state = state;

	static const std::map<FilterState, std::string> filter_state_map = {
	        {FilterState::Off, "disabled"},
	        {FilterState::On, "enabled"},
	        {FilterState::ForcedOn, "enabled (forced)"},
	};
	auto it = filter_state_map.find(state);
	if (it != filter_state_map.end()) {
		auto filter_state = it->second;
		LOG_MSG("MIXER: %s channel filter %s", name.c_str(), filter_state.c_str());
	}
}

void MixerChannel::ConfigureLowPassFilter(const uint8_t order,
                                          const uint16_t cutoff_freq)
{
	assert(order > 0 && order <= max_filter_order);
	for (auto &f : filter.lpf)
		f.setup(order, mixer.sample_rate, cutoff_freq);
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
				frame[0] = lut_u8to16[data[pos * 2 + 0]];
				frame[1] = lut_u8to16[data[pos * 2 + 1]];
			} else {
				frame[0] = lut_u8to16[data[pos]];
			}
		}
		// signed 8-bit
		else {
			if (stereo) {
				frame[0] = lut_s8to16[data[pos * 2 + 0]];
				frame[1] = lut_s8to16[data[pos * 2 + 1]];
			} else {
				frame[0] = lut_s8to16[data[pos]];
			}
		}
	} else {
		// 16-bit and 32-bit both contain 16-bit data internally
		if (signeddata) {
			if (stereo) {
				if (nativeorder) {
					frame[0] = data[pos * 2 + 0];
					frame[1] = data[pos * 2 + 1];
				} else {
					if (sizeof(Type) == 2) {
						frame[0] = (int16_t)host_readw(
						        (HostPt)&data[pos * 2 + 0]);
						frame[1] = (int16_t)host_readw(
						        (HostPt)&data[pos * 2 + 1]);
					} else {
						frame[0] = (int32_t)host_readd(
						        (HostPt)&data[pos * 2 + 0]);
						frame[1] = (int32_t)host_readd(
						        (HostPt)&data[pos * 2 + 1]);
					}
				}
			} else { // mono
				if (nativeorder) {
					frame[0] = data[pos];
				} else {
					if (sizeof(Type) == 2) {
						frame[0] = (int16_t)host_readw(
						        (HostPt)&data[pos]);
					} else {
						frame[0] = (int32_t)host_readd(
						        (HostPt)&data[pos]);
					}
				}
			}
		} else { // unsigned
			const auto offset = 32768;
			if (stereo) {
				if (nativeorder) {
					frame[0] = static_cast<int>(
					                   data[pos * 2 + 0]) -
					           offset;
					frame[1] = static_cast<int>(
					                   data[pos * 2 + 1]) -
					           offset;
				} else {
					if (sizeof(Type) == 2) {
						frame[0] = static_cast<int>(host_readw(
						                   (HostPt)&data[pos * 2 + 0])) -
						           offset;
						frame[1] = static_cast<int>(host_readw(
						                   (HostPt)&data[pos * 2 + 1])) -
						           offset;
					} else {
						frame[0] = static_cast<int>(host_readd(
						                   (HostPt)&data[pos * 2 + 0])) -
						           offset;
						frame[1] = static_cast<int>(host_readd(
						                   (HostPt)&data[pos * 2 + 1])) -
						           offset;
					}
				}
			} else { // mono
				if (nativeorder) {
					frame[0] = static_cast<int>(data[pos]) - offset;
				} else {
					if (sizeof(Type) == 2) {
						frame[0] = static_cast<int>(host_readw(
						                   (HostPt)&data[pos])) -
						           offset;
					} else {
						frame[0] = static_cast<int>(host_readd(
						                   (HostPt)&data[pos])) -
						           offset;
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
		prev_frame[0] = next_frame[0];
		if (stereo) {
			prev_frame[1] = next_frame[1];
		}

		if (std::is_same<Type, float>::value) {
			// TODO
		} else {
			next_frame = ConvertNextFrame<Type, stereo, signeddata, nativeorder>(
			        data, pos);
		}

		// Process initial samples through an expanding envelope to
		// prevent severe clicks and pops. Becomes a no-op when done.
		envelope.Process(stereo, prev_frame);

		const auto left  = prev_frame[mapped_channel_left] * volmul[0];
		const auto right = (stereo ? prev_frame[mapped_channel_right]
		                           : prev_frame[mapped_channel_left]) *
		                   volmul[1];

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

	MIXER_LockAudioDevice();
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

	// Optionally low-pass filter, apply crossfeed, then mix the results
	// to the master output
	auto pos = mixer.resample_out.begin();
	auto mixpos = check_cast<work_index_t>(mixer.pos + done);

	const auto do_filter = filter.state == FilterState::On ||
	                       filter.state == FilterState::ForcedOn;

	const auto do_crossfeed = crossfeed.strength > 0.0f;

	while (pos != mixer.resample_out.end()) {
		mixpos &= MIXER_BUFMASK;

		AudioFrame frame = {*pos++, *pos++};
		if (do_filter) {
			frame.left = filter.lpf[0].filter(frame.left);
			frame.right = filter.lpf[1].filter(frame.right);
		}
		if (do_crossfeed)
			frame = ApplyCrossfeed(frame);

		mixer.work[mixpos][0] += frame.left;
		mixer.work[mixpos][1] += frame.right;
		++mixpos;
	}

	auto out_frames = mixer.resample_out.size() / 2;
	done += out_frames;

	last_samples_were_silence = false;
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
			prev_frame[0] = data[0];
			data++;
		}
		const auto diff = data[0] - static_cast<int16_t>(prev_frame[0]);
		const auto diff_mul = index & FREQ_MASK;
		index += index_add;
		mixpos &= MIXER_BUFMASK;
		const auto sample = prev_frame[0] + ((diff * diff_mul) >> FREQ_SHIFT);
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
			const auto s1 = static_cast<uint16_t>(MIXER_CLIP(static_cast<int>(sample_1)));
			const auto s2 = static_cast<uint16_t>(MIXER_CLIP(static_cast<int>(sample_2)));
			convert[i][0] = static_cast<int16_t>(host_to_le16(s1));
			convert[i][1] = static_cast<int16_t>(host_to_le16(s2));
			readpos = (readpos + 1) & MIXER_BUFMASK;
		}
		CAPTURE_AddWave(mixer.sample_rate, added, reinterpret_cast<int16_t*>(convert));
	}
	//Reset the the tick_add for constant speed
	if( Mixer_irq_important() )
		mixer.tick_add = calc_tickadd(mixer.sample_rate);
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
		mixer.tick_add = calc_tickadd(mixer.sample_rate + mixer.min_needed);
	} else if (mixer.done < mixer.max_needed) {
		auto left = mixer.done - need;
		if (left < mixer.min_needed) {
			if (!Mixer_irq_important()) {
				auto needed = mixer.needed - need;
				auto diff = (mixer.min_needed > needed ? mixer.min_needed.load()
				                                       : needed) - left;
				mixer.tick_add = calc_tickadd(mixer.sample_rate +
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
				mixer.tick_add = calc_tickadd(mixer.sample_rate -
				                              (diff / 5));
			else if (diff > (mixer.min_needed >> 2))
				mixer.tick_add = calc_tickadd(mixer.sample_rate -
				                              (diff >> 3));
			else
				mixer.tick_add = calc_tickadd(mixer.sample_rate);
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
		mixer.tick_add = calc_tickadd(mixer.sample_rate - (mixer.min_needed / 5));
	}
	MIXER_ReduceChannelsDoneCounts(reduce);

	// Reset mixer.tick_add when irqs are important
	if( Mixer_irq_important() )
		mixer.tick_add = calc_tickadd(mixer.sample_rate);

	mixer.done -= reduce;
	mixer.needed -= reduce;
	pos = mixer.pos;
	mixer.pos = (mixer.pos + reduce) & MIXER_BUFMASK;
	if (need != reduce) {
		while (need--) {
			const auto i = check_cast<work_index_t>((pos + (index >> INDEX_SHIFT_LOCAL)) & MIXER_BUFMASK);
			index += index_add;
			sample = static_cast<int>(mixer.work[i][0]);
			*output++ = MIXER_CLIP(sample);
			sample = static_cast<int>(mixer.work[i][1]);
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
			sample = static_cast<int>(mixer.work[pos][0]);
			*output++ = MIXER_CLIP(sample);
			sample = static_cast<int>(mixer.work[pos][1]);
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
		auto is_master = false;

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

			// Global commands that apply to all channels
			if (!is_master && !curr_chan) {
				std::lock_guard lock(mixer.channel_mutex);

				float value = 0.0f;
				if (parse_prefixed_percentage('X', arg, value)) {
					for (auto &it : mixer.channels) {
						it.second->SetCrossfeedStrength(value);
					}
					continue;
				}

			// Only setting the volume is allowed for the
			// MASTER channel
			} else if (is_master) {
				std::lock_guard lock(mixer.channel_mutex);
				ParseVolume(arg,
				            mixer.mastervol[0],
				            mixer.mastervol[1]);

			// Adjust settings of a regular non-master channel
			} else if (curr_chan) {
				std::lock_guard lock(mixer.channel_mutex);

				float value = 0.0f;
				if (parse_prefixed_percentage('X', arg, value)) {
					curr_chan->SetCrossfeedStrength(value);
					continue;
				}

				if (curr_chan->ChangeLineoutMap(arg))
					continue;

				float left_vol = 0;
				float right_vol = 0;
				ParseVolume(arg, left_vol, right_vol);

				curr_chan->SetVolume(left_vol, right_vol);
				curr_chan->UpdateVolume();
			}
		}

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
		        "    Crossfeed: [color=white]x0[reset] to [color=white]x100[reset]\n"
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
		        "  [color=green]mixer[reset] [color=white]x30[reset] [color=cyan]fm[reset] [color=white]150[reset] [color=cyan]sb[reset] [color=white]x10[reset]");
	}

	void ParseVolume(const std::string &s, float &vol_left, float &vol_right)
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
					vol_out = powf(10.0f, vol / 20.0f);
				else
					vol_out = vol / 100.0f;

				const auto min_vol = powf(10.0f, -99.99f / 20.0f);
				constexpr auto max_vol = 99.99f;
				if (vol_out < min_vol)
					vol_out = 0;
				else
					vol_out = std::min(vol_out, max_vol);
			} else {
				vol_out = 0;
			}
		};

		parse_vol_pref(vol_parts[0], vol_left);
		if (vol_parts.size() > 1)
			parse_vol_pref(vol_parts[1], vol_right);
		else
			vol_right = vol_left;
	}

	void ShowMixerStatus()
	{
		auto show_channel = [this](const std::string &name,
		                           const float vol_left,
		                           const float vol_right,
		                           const int rate,
		                           const std::string &mode,
		                           const std::string &xfeed) {
			WriteOut("%-21s %4.0f:%-4.0f %+6.2f:%-+6.2f %8d  %-8s %5s\n",
			         name.c_str(),
			         vol_left * 100.0f,
			         vol_right * 100.0f,
			         20.0f * log(vol_left) / log(10.0f),
			         20.0f * log(vol_right) / log(10.0f),
			         rate,
			         mode.c_str(),
			         xfeed.c_str());
		};

		std::lock_guard lock(mixer.channel_mutex);

		WriteOut(convert_ansi_markup("[color=white]Channel     Volume    Volume(dB)   Rate(Hz)  Mode     Xfeed[reset]\n")
		                 .c_str());

		show_channel(convert_ansi_markup("[color=cyan]MASTER[reset]"),
		             mixer.mastervol[0],
		             mixer.mastervol[1],
		             mixer.sample_rate,
		             "Stereo",
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

			auto channel_name = std::string("[color=cyan]") + name +
			                    std::string("[reset]");

			auto mode = chan->HasFeature(ChannelFeature::Stereo)
			                    ? chan->DescribeLineout()
			                    : "Mono";

			show_channel(convert_ansi_markup(channel_name),
			             chan->volmain[0],
			             chan->volmain[1],
			             chan->GetSampleRate(),
			             mode,
			             xfeed);
		}
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
	mixer.sample_rate = section->Get_int("rate");
	mixer.blocksize = static_cast<uint16_t>(section->Get_int("blocksize"));
	const auto negotiate = section->Get_bool("negotiate");

	/* Start the Mixer using SDL Sound at 22 khz */
	SDL_AudioSpec spec;
	SDL_AudioSpec obtained;

	spec.freq = static_cast<int>(mixer.sample_rate);
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
		mixer.tick_add = calc_tickadd(mixer.sample_rate);
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else if ((mixer.sdldevice = SDL_OpenAudioDevice(NULL, 0, &spec, &obtained, sdl_allow_flags)) ==0 ) {
		mixer.nosound = true;
		LOG_WARNING("MIXER: Can't open audio: %s , running in nosound mode.",SDL_GetError());
		mixer.tick_add = calc_tickadd(mixer.sample_rate);
		TIMER_AddTickHandler(MIXER_Mix_NoSound);
	} else {
		// Does SDL want something other than stereo output?
		if (obtained.channels != spec.channels)
			E_Exit("SDL gave us %u-channel output but we require %u channels",
			       obtained.channels, spec.channels);

		// Does SDL want a different playback rate?
		if (obtained.freq != mixer.sample_rate) {
			LOG_WARNING("MIXER: SDL changed the playback rate from %d to %d Hz", mixer.sample_rate.load(), obtained.freq);
			mixer.sample_rate = obtained.freq;
		}

		// Does SDL want a different blocksize?
		const auto obtained_blocksize = obtained.samples;
		if (obtained_blocksize != mixer.blocksize) {
			LOG_WARNING("MIXER: SDL changed the blocksize from %u to %u frames",
			        mixer.blocksize, obtained_blocksize);
			mixer.blocksize = obtained_blocksize;
		}
		mixer.tick_add = calc_tickadd(mixer.sample_rate);
		TIMER_AddTickHandler(MIXER_Mix);
		SDL_PauseAudioDevice(mixer.sdldevice, 0);

		LOG_MSG("MIXER: Negotiated %u-channel %u-Hz audio in %u-frame blocks",
		        obtained.channels, mixer.sample_rate.load(), mixer.blocksize);
	}

	//1000 = 8 *125
	mixer.tick_counter = (mixer.sample_rate % 125) ? TICK_NEXT : 0;
	const auto requested_prebuffer = section->Get_int("prebuffer");
	mixer.min_needed = static_cast<uint16_t>(clamp(requested_prebuffer, 0, 100));
	mixer.min_needed = (mixer.sample_rate * mixer.min_needed) / 1000;
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

void init_mixer_dosbox_settings(Section_prop &sec_prop)
{
	constexpr int default_rate = 48000;
#if defined(WIN32)
	// Longstanding known-good defaults for Windows
	constexpr int default_blocksize = 1024;
	constexpr int default_prebuffer = 25;
	constexpr bool default_allow_negotiate = false;

#else
	// Non-Windows platforms tolerate slightly lower latency
	constexpr int default_blocksize = 512;
	constexpr int default_prebuffer = 20;
	constexpr bool default_allow_negotiate = true;
#endif

	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto bool_prop = sec_prop.Add_bool("nosound", only_at_start, false);
	assert(bool_prop);
	bool_prop->Set_help("Enable silent mode, sound is still emulated though.");

	auto int_prop = sec_prop.Add_int("rate", only_at_start, default_rate);
	assert(int_prop);
	const char *rates[] = {
	        "8000", "11025", "16000", "22050", "32000", "44100", "48000", "49716", 0};
	int_prop->Set_values(rates);
	int_prop->Set_help(
	        "Mixer sample rate, setting any device's rate higher than this will probably lower their sound quality.");

	const char *blocksizes[] = {"128", "256", "512", "1024", "2048", "4096", "8192", 0};

	int_prop = sec_prop.Add_int("blocksize", only_at_start, default_blocksize);
	int_prop->Set_values(blocksizes);
	int_prop->Set_help(
	        "Mixer block size; larger values might help with sound stuttering but sound will also be more lagged.");

	int_prop = sec_prop.Add_int("prebuffer", only_at_start, default_prebuffer);
	int_prop->SetMinMax(0, 100);
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
}

void MIXER_AddConfigSection(const config_ptr_t &conf)
{
	assert(conf);
	Section_prop *sec = conf->AddSection_prop("mixer", &MIXER_Init);
	assert(sec);
	init_mixer_dosbox_settings(*sec);
}
