// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mixer.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <sys/types.h>

#include <speex/speex_resampler.h>

#include "mverb/MVerb.h"
#include "tal-chorus/ChorusEngine.h"

#include "private/compressor.h"

#include "capture/capture.h"
#include "channel_names.h"
#include "config/config.h"
#include "config/setup.h"
#include "dosbox_config.h"
#include "gui/mapper.h"
#include "hardware/audio/gus.h"
#include "hardware/audio/lpt_dac.h"
#include "hardware/audio/pcspeaker.h"
#include "hardware/audio/ps1audio.h"
#include "hardware/audio/soundblaster.h"
#include "hardware/audio/tandy_sound.h"
#include "hardware/memory.h"
#include "hardware/pic.h"
#include "hardware/timer.h"
#include "hardware/video/reelmagic/reelmagic.h"
#include "midi/midi.h"
#include "misc/cross.h"
#include "misc/notifications.h"
#include "misc/video.h"
#include "utils/checks.h"
#include "utils/math_utils.h"
#include "utils/rwqueue.h"
#include "utils/string_utils.h"

// must be included after dosbox_config.h
#include <SDL.h>

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

// This shows up nicely as 50% and -6.00 dB in the MIXER command's output
constexpr auto Minus6db = 0.501f;

struct MixerSettings {
	RWQueue<AudioFrame> final_output{1};
	RWQueue<int16_t> capture_queue{1};

	std::thread thread = {};

	// The default master gain is -6dB (50% volume) to minimize the change
	// for clipping.
	std::atomic<AudioFrame> master_gain = AudioFrame(Minus6db, Minus6db);

	// Output by mix_samples, to be enqueud into the final_output queue
	std::vector<AudioFrame> output_buffer = {};

	// Temporary mixing buffers
	std::vector<AudioFrame> reverb_aux_buffer   = {};
	std::vector<AudioFrame> chorus_aux_buffer   = {};
	std::vector<int16_t> capture_buffer         = {};
	std::vector<AudioFrame> fast_forward_buffer = {};

	std::map<std::string, MixerChannelPtr> channels = {};

	std::map<std::string, MixerChannelSettings> channel_settings_cache = {};

	std::atomic<bool> thread_should_quit = false;

	// Sample rate negotiated with SDL (technically, this is the rate of
	// sample *frames* per second).
	std::atomic<int> sample_rate_hz = 0;

	// Matches SDL AudioSpec.samples type
	int blocksize    = 0;
	int prebuffer_ms = 25;

	SDL_AudioDeviceID sdl_device = 0;

	std::atomic<MixerState> state = {};

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

	std::atomic<bool> fast_forward_mode = false;

	std::recursive_mutex mutex = {};
};

static struct MixerSettings mixer = {};

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
	std::lock_guard lock(mutex);
	output_map = map;
}

StereoLine MixerChannel::GetLineoutMap()
{
	std::lock_guard lock(mutex);
	return output_map;
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

void MIXER_EnableFastForwardMode()
{
	mixer.fast_forward_mode = true;
}

void MIXER_DisableFastForwardMode()
{
	mixer.fast_forward_mode = false;
}

bool MIXER_FastForwardModeEnabled()
{
	return mixer.fast_forward_mode;
}

// The queues listed here are for audio devices that run on the main thread.
// The mixer thread can be waiting on the main thread to produce audio in these
// queues. We need to stop them before aquiring a mutex lock to avoid a
// deadlock. These are called infrequently when global mixer state is changed
// (mostly on device init/destroy and in the MIXER command line program).
// Individual channels also have a mutex which can be safely aquired without
// stopping these queues.
void MIXER_LockMixerThread()
{
	PCSPEAKER_NotifyLockMixer();
	TANDYDAC_NotifyLockMixer();
	PS1DAC_NotifyLockMixer();
	LPTDAC_NotifyLockMixer();
	GUS_NotifyLockMixer();
	REELMAGIC_NotifyLockMixer();
	SBLASTER_NotifyLockMixer();

	mixer.mutex.lock();
}

void MIXER_UnlockMixerThread()
{
	PCSPEAKER_NotifyUnlockMixer();
	TANDYDAC_NotifyUnlockMixer();
	PS1DAC_NotifyUnlockMixer();
	LPTDAC_NotifyUnlockMixer();
	GUS_NotifyUnlockMixer();
	REELMAGIC_NotifyUnlockMixer();
	SBLASTER_NotifyUnlockMixer();

	mixer.mutex.unlock();
}

MixerChannel::MixerChannel(MIXER_Handler _handler, const std::string& _name,
                           const std::set<ChannelFeature>& _features)
        : sleeper(*this),
          name(_name),
          envelope(_name),
          handler(_handler),
          features(_features)
{
	do_sleep = HasFeature(ChannelFeature::Sleep);
}

bool MixerChannel::HasFeature(const ChannelFeature feature)
{
	std::lock_guard lock(mutex);
	return features.contains(feature);
}

std::set<ChannelFeature> MixerChannel::GetFeatures()
{
	std::lock_guard lock(mutex);
	return features;
}

bool StereoLine::operator==(const StereoLine other) const
{
	return left == other.left && right == other.right;
}

static void set_global_crossfeed(const MixerChannelPtr& channel)
{
	assert(channel);

	const auto apply_crossfeed = (channel->GetName() == ChannelName::Opl &&
	                              channel->HasFeature(ChannelFeature::Stereo)) ||
	                             (channel->GetName() == ChannelName::Cms);

	if (!mixer.do_crossfeed || !apply_crossfeed) {
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

	MIXER_LockMixerThread();

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

	MIXER_UnlockMixerThread();
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

	// The conf system programmatically guarantees only the above prefs are
	// used
	constexpr auto SettingName  = "reverb";
	constexpr auto DefaultValue = "off";

	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      "MIXER",
	                      "PROGRAM_CONFIG_INVALID_SETTING",
	                      SettingName,
	                      pref.c_str(),
	                      DefaultValue);

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
	MIXER_LockMixerThread();

	auto& r = mixer.reverb; // short-hand reference

	r.preset = new_preset;

	const auto rate_hz = mixer.sample_rate_hz.load();

	// clang-format off
	switch (r.preset) { //             PREDLY EARLY  SIZE   DENSITY BW_FREQ DECAY  DAMP_LV SYN_LV DIG_LV HIPASS_HZ
	case ReverbPreset::Tiny:   r.Setup(0.00f, 1.00f, 0.05f, 0.50f,  0.50f,  0.00f, 1.00f,  0.65f, 0.65f, 200.0f, rate_hz); break;
	case ReverbPreset::Small:  r.Setup(0.00f, 1.00f, 0.17f, 0.42f,  0.50f,  0.50f, 0.70f,  0.40f, 0.08f, 200.0f, rate_hz); break;
	case ReverbPreset::Medium: r.Setup(0.00f, 0.75f, 0.50f, 0.50f,  0.95f,  0.42f, 0.21f,  0.54f, 0.07f, 170.0f, rate_hz); break;
	case ReverbPreset::Large:  r.Setup(0.00f, 0.75f, 0.75f, 0.50f,  0.95f,  0.52f, 0.21f,  0.70f, 0.05f, 140.0f, rate_hz); break;
	case ReverbPreset::Huge:   r.Setup(0.00f, 0.75f, 0.75f, 0.50f,  0.95f,  0.52f, 0.21f,  0.85f, 0.05f, 140.0f, rate_hz); break;
	case ReverbPreset::None:   break;
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

	MIXER_UnlockMixerThread();
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
	constexpr auto SettingName  = "chorus";
	constexpr auto DefaultValue = "off";

	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      "MIXER",
	                      "PROGRAM_CONFIG_INVALID_SETTING",
	                      SettingName,
	                      pref.c_str(),
	                      DefaultValue);

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

	MIXER_LockMixerThread();

	// Set new
	assert(c.preset != new_preset);
	c.preset = new_preset;

	const auto rate_hz = mixer.sample_rate_hz.load();

	// clang-format off
	switch (c.preset) { //             SYN_LV  DIG_LV
	case ChorusPreset::Light:  c.Setup(0.33f,  0.00f, rate_hz); break;
	case ChorusPreset::Normal: c.Setup(0.54f,  0.00f, rate_hz); break;
	case ChorusPreset::Strong: c.Setup(0.75f,  0.00f, rate_hz); break;
	case ChorusPreset::None:   break;
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

	MIXER_UnlockMixerThread();
}

static void init_compressor(const bool compressor_enabled)
{
	mixer.do_compressor = compressor_enabled;
	if (!mixer.do_compressor) {
		LOG_MSG("MIXER: Master compressor disabled");
		return;
	}

	MIXER_LockMixerThread();

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

	MIXER_UnlockMixerThread();

	LOG_MSG("MIXER: Master compressor enabled");
}

void MIXER_DeregisterChannel(MixerChannelPtr& channel_to_remove)
{
	if (!channel_to_remove) {
		return;
	}

	MIXER_LockMixerThread();

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

	MIXER_UnlockMixerThread();
}

MixerChannelPtr MIXER_AddChannel(MIXER_Handler handler,
                                 const int sample_rate_hz, const std::string& name,
                                 const std::set<ChannelFeature>& features)
{
	// We allow 0 for the UseMixerRate special value
	assert(sample_rate_hz >= 0);

	auto chan = std::make_shared<MixerChannel>(handler, name, features);
	chan->SetSampleRate(sample_rate_hz);
	chan->SetAppVolume({1.0f, 1.0f});

	const auto chan_rate_hz = chan->GetSampleRate();
	if (chan_rate_hz == mixer.sample_rate_hz) {
		LOG_MSG("%s: Operating at %d Hz without resampling", name.c_str(), chan_rate_hz);
	} else {
		LOG_MSG("%s: Operating at %d Hz and %s to the output rate",
		        name.c_str(),
		        chan_rate_hz,
		        chan_rate_hz > mixer.sample_rate_hz ? "downsampling"
		                                            : "upsampling");
	}

	// Try to restore saved channel settings from the cache first.
	if (mixer.channel_settings_cache.contains(name)) {
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

	MIXER_LockMixerThread();
	mixer.channels[name] = chan; // replace the old, if it exists
	MIXER_UnlockMixerThread();

	return chan;
}

MixerChannelPtr MIXER_FindChannel(const char* name)
{
	auto it = mixer.channels.find(name); //-V838

	const auto chan = (it != mixer.channels.end()) ? it->second : nullptr;

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

	std::lock_guard lock(mutex);

	db0_volume_gain = scalar;
	UpdateCombinedVolume();
}

void MixerChannel::UpdateCombinedVolume()
{
	std::lock_guard lock(mutex);
	combined_volume_gain = user_volume_gain * app_volume_gain * db0_volume_gain;
}

const AudioFrame MixerChannel::GetUserVolume()
{
	std::lock_guard lock(mutex);
	return user_volume_gain;
}

void MixerChannel::SetUserVolume(const AudioFrame gain)
{
	std::lock_guard lock(mutex);

	// Allow unconstrained user-defined values
	user_volume_gain = gain;
	UpdateCombinedVolume();
}

const AudioFrame MixerChannel::GetAppVolume()
{
	std::lock_guard lock(mutex);
	return app_volume_gain;
}

void MixerChannel::SetAppVolume(const AudioFrame gain)
{
	std::lock_guard lock(mutex);

	// Constrain application-defined volume between 0% and 100%
	auto clamp_to_unity = [](const float vol) {
		constexpr auto MinUnityVolume = 0.0f;
		constexpr auto MaxUnityVolume = 1.0f;
		return clamp(vol, MinUnityVolume, MaxUnityVolume);
	};
	app_volume_gain = {clamp_to_unity(gain.left), clamp_to_unity(gain.right)};
	UpdateCombinedVolume();

#ifdef DEBUG_MIXER
	LOG_MSG("MIXER: %-7s channel: application requested volume "
	        "{%3.0f%%, %3.0f%%}, and was set to {%3.0f%%, %3.0f%%}",
	        name,
	        static_cast<double>(left),
	        static_cast<double>(right),
	        static_cast<double>(app_volume_gain.left * 100.0f),
	        static_cast<double>(app_volume_gain.right * 100.0f));
#endif
}

const AudioFrame MIXER_GetMasterVolume()
{
	return mixer.master_gain.load(std::memory_order_relaxed);
}

void MIXER_SetMasterVolume(const AudioFrame gain)
{
	mixer.master_gain.store(gain, std::memory_order_relaxed);
}

void MixerChannel::SetChannelMap(const StereoLine map)
{
	assert(map.left == Left || map.left == Right);
	assert(map.right == Left || map.right == Right);

	std::lock_guard lock(mutex);

	channel_map = map;

#ifdef DEBUG_MIXER
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
	std::lock_guard lock(mutex);

	// Prepare the channel to go dormant
	if (!should_enable) {
		// Clear the current counters and sample values to
		// start clean if/when this channel is re-enabled.
		// Samples can be buffered into disable channels, so
		// we don't perform this zero'ing in the enable phase.

		frames_needed = 0;
		audio_frames.clear();

		prev_frame = {0.0f, 0.0f};
		next_frame = {0.0f, 0.0f};

		ClearResampler();
	}

	is_enabled = should_enable;
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

	std::lock_guard lock(mutex);

	const auto channel_rate_hz = sample_rate_hz.load();
	const auto mixer_rate_hz   = mixer.sample_rate_hz.load();

	do_lerp_upsample = false;
	do_zoh_upsample  = false;
	do_resample      = false;

	auto configure_speex_resampler = [&](const int _in_rate_hz) {
		const spx_uint32_t in_rate_hz  = _in_rate_hz;
		const spx_uint32_t out_rate_hz = mixer_rate_hz;

		// Only init the resampler once
		if (!speex_resampler.state) {

			// Always stereo
			constexpr auto NumChannels = 2;

			// According to the Speex authors, a quality of 3 is
			// acceptable for most desktop uses and even a qualitiy
			// of 0 has decent enough sound (but artifacts might be
			// heard).
			//
			// In our testing on a large number of DOS soundtracks,
			// a quality of 4 yielded virtually identical sounding
			// resampled audio (e.g., on OPL music with lots of
			// treble-heavy fast transients), so we went with a
			// quality 5 to be on the safe side. This has a good
			// balance between quality, and latency, and processing
			// power.
			//
			// Relevant comment from an Opus developer:
			//
			// "The higher settings are mostly for audiophiles,
			// spectrograms and bragging rights. In practice, I
			// cannot hear the difference between settings 5 and 10,
			// despite the large difference in complexity (and
			// overly sensitive spectrograms)."
			//
			// Source:
			// https://hydrogenaud.io/index.php/topic,113655.msg935704.html#msg935704
			//
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
	std::lock_guard lock(mutex);

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

	std::lock_guard lock(mutex);

	sample_rate_hz = target_rate_hz;

	envelope.Update(sample_rate_hz,
	                peak_amplitude,
	                EnvelopeMaxExpansionOverMs,
	                EnvelopeExpiresAfterSeconds);

	if (do_noise_gate) {
		InitNoiseGate();
	}

	if (filters.highpass.state == FilterState::On) {
		InitHighPassFilter();
	}
	if (filters.lowpass.state == FilterState::On) {
		InitLowPassFilter();
	}

	ConfigureResampler();
}

const std::string& MixerChannel::GetName()
{
	std::lock_guard lock(mutex);
	return name;
}

int MixerChannel::GetSampleRate()
{
	std::lock_guard lock(mutex);
	return sample_rate_hz;
}

static float get_mixer_frames_per_tick()
{
	return static_cast<float>(mixer.sample_rate_hz) / 1000.0f;
}

float MixerChannel::GetFramesPerTick()
{
	std::lock_guard lock(mutex);

	const float stretch_factor = static_cast<float>(sample_rate_hz) /
	                             static_cast<float>(mixer.sample_rate_hz);

	return get_mixer_frames_per_tick() * stretch_factor;
}

float MixerChannel::GetFramesPerBlock()
{
	std::lock_guard lock(mutex);

	const float stretch_factor = static_cast<float>(sample_rate_hz) /
	                             static_cast<float>(mixer.sample_rate_hz);

	return static_cast<float>(mixer.blocksize) * stretch_factor;
}

double MixerChannel::GetMillisPerFrame()
{
	std::lock_guard lock(mutex);

	// Note: the double return value is used for PIC timing (which uses
	// doubles)

	return MillisInSecond / sample_rate_hz;
}

void MixerChannel::SetPeakAmplitude(const int peak)
{
	std::lock_guard lock(mutex);

	peak_amplitude = peak;
	envelope.Update(sample_rate_hz,
	                peak_amplitude,
	                EnvelopeMaxExpansionOverMs,
	                EnvelopeExpiresAfterSeconds);
}

void MixerChannel::Mix(const int frames_requested)
{
	assert(frames_requested > 0);

	if (!is_enabled) {
		return;
	}

	frames_needed = frames_requested;

	while (frames_needed > audio_frames.size()) {
		std::unique_lock lock(mutex);

		auto stretch_factor = static_cast<float>(sample_rate_hz) /
		                      static_cast<float>(mixer.sample_rate_hz);

		auto frames_remaining = iceil(
		        static_cast<float>(frames_needed - audio_frames.size()) *
		        stretch_factor);

		// Avoid underflow
		if (frames_remaining <= 0) {
			break;
		}

		lock.unlock();
		handler(frames_remaining);
	}
}

void MixerChannel::AddSilence()
{
	std::lock_guard lock(mutex);

	if (audio_frames.size() < frames_needed) {
		if (prev_frame.left == 0.0f && prev_frame.right == 0.0f) {
			while (audio_frames.size() < frames_needed) {
				audio_frames.emplace_back(0.0f, 0.0f);
			}

			// Make sure the next samples are zero when they get
			// switched to prev
			next_frame = {0.0f, 0.0f};

		} else {
			bool stereo = last_samples_were_stereo;

			const auto mapped_output_left  = output_map.left;
			const auto mapped_output_right = output_map.right;

			while (audio_frames.size() < frames_needed) {
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
				out_frame[mapped_output_left] = frame_with_gain.left;
				out_frame[mapped_output_right] = frame_with_gain.right;

				audio_frames.push_back(out_frame);
				prev_frame = next_frame;
			}
		}
	}

	last_samples_were_silence = true;
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
	std::lock_guard lock(mutex);

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
	std::lock_guard lock(mutex);

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

void MixerChannel::ConfigureNoiseGate(const float threshold_db,
                                      const float attack_time_ms,
                                      const float release_time_ms)
{
	assert(attack_time_ms > 0.0f);
	assert(release_time_ms > 0.0f);

	std::lock_guard lock(mutex);

	noise_gate.threshold_db    = threshold_db;
	noise_gate.attack_time_ms  = attack_time_ms;
	noise_gate.release_time_ms = release_time_ms;

	InitNoiseGate();
}

void MixerChannel::EnableNoiseGate(const bool enabled)
{
	std::lock_guard lock(mutex);

	LOG_MSG("%s: Noise gate %s", name.c_str(), (enabled ? "enabled" : "disabled"));
	do_noise_gate = enabled;
}

void MixerChannel::InitNoiseGate()
{
	assert(noise_gate.attack_time_ms > 0.0f);
	assert(noise_gate.release_time_ms > 0.0f);

	std::lock_guard lock(mutex);

	const auto _0dbfs_sample_value = Max16BitSampleValue;
	noise_gate.processor.Configure(sample_rate_hz,
	                               _0dbfs_sample_value,
	                               noise_gate.threshold_db,
	                               noise_gate.attack_time_ms,
	                               noise_gate.release_time_ms);
}

FilterState MixerChannel::GetHighPassFilterState()
{
	std::lock_guard lock(mutex);
	return filters.highpass.state;
}

FilterState MixerChannel::GetLowPassFilterState()
{
	std::lock_guard lock(mutex);
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
	assert(order > 0 && order <= MaxFilterOrder);
	assert(_cutoff_freq_hz > 0);

	std::lock_guard lock(mutex);

	const auto cutoff_freq_hz = clamp_filter_cutoff_freq(name, _cutoff_freq_hz);

	filters.highpass.order          = order;
	filters.highpass.cutoff_freq_hz = cutoff_freq_hz;

	InitHighPassFilter();
}

void MixerChannel::InitHighPassFilter()
{
	assert(filters.highpass.order > 0 && filters.highpass.order <= MaxFilterOrder);
	assert(filters.highpass.cutoff_freq_hz > 0);

	std::lock_guard lock(mutex);

	for (auto& f : filters.highpass.hpf) {
		f.setup(filters.highpass.order,
		        mixer.sample_rate_hz,
		        filters.highpass.cutoff_freq_hz);
	}
}

void MixerChannel::ConfigureLowPassFilter(const int order, const int _cutoff_freq_hz)
{
	assert(order > 0 && order <= MaxFilterOrder);
	assert(_cutoff_freq_hz > 0);

	std::lock_guard lock(mutex);

	const auto cutoff_freq_hz = clamp_filter_cutoff_freq(name, _cutoff_freq_hz);

	filters.lowpass.order          = order;
	filters.lowpass.cutoff_freq_hz = cutoff_freq_hz;

	InitLowPassFilter();
}

void MixerChannel::InitLowPassFilter()
{
	assert(filters.lowpass.order > 0 && filters.lowpass.order <= MaxFilterOrder);
	assert(filters.lowpass.cutoff_freq_hz > 0);

	std::lock_guard lock(mutex);

	for (auto& f : filters.lowpass.lpf) {
		f.setup(filters.lowpass.order,
		        mixer.sample_rate_hz,
		        filters.lowpass.cutoff_freq_hz);
	}
}

// Tries to set custom filter settings from the passed in filter preferences.
// Returns true if the custom filters could be successfully set, false
// otherwise and disables all filters for the channel.
bool MixerChannel::TryParseAndSetCustomFilter(const std::string& filter_prefs)
{
	std::lock_guard lock(mutex);

	SetLowPassFilter(FilterState::Off);
	SetHighPassFilter(FilterState::Off);

	if (!(filter_prefs.starts_with("lpf") || filter_prefs.starts_with("hpf"))) {
		return false;
	}

	const auto parts = split_with_empties(filter_prefs, ' ');

	const auto single_filter = (parts.size() == 3);
	const auto dual_filter   = (parts.size() == 6);

	if (!(single_filter || dual_filter)) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      name,
		                      "MIXER_INVALID_CUSTOM_FILTER",
		                      filter_prefs.c_str());
		return false;
	}

	auto set_filter = [&](const std::string& type_pref,
	                      const std::string& order_pref,
	                      const std::string& cutoff_freq_pref) {
		const auto filter_name = (type_pref == "lpf") ? "low-pass"
		                                              : "high-pass";

		const auto order = parse_int(order_pref);
		if (!order || *order < 1 || *order > MaxFilterOrder) {

			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      name,
			                      "MIXER_INVALID_CUSTOM_FILTER_ORDER",
			                      filter_name,
			                      order_pref.c_str(),
			                      MaxFilterOrder);
			return false;
		}

		const auto cutoff_freq_hz = parse_int(cutoff_freq_pref);
		if (!cutoff_freq_hz || *cutoff_freq_hz <= 0) {

			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      name,
			                      "MIXER_INVALID_CUSTOM_FILTER_CUTOFF",
			                      filter_name,
			                      cutoff_freq_pref.c_str());
			return false;
		}

		if (type_pref == "lpf") {
			ConfigureLowPassFilter(*order, *cutoff_freq_hz);
			SetLowPassFilter(FilterState::On);

		} else if (type_pref == "hpf") {
			ConfigureHighPassFilter(*order, *cutoff_freq_hz);
			SetHighPassFilter(FilterState::On);

		} else {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      name,
			                      "MIXER_INVALID_CUSTOM_FILTER_TYPE",
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
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      name,
			                      "MIXER_INVALID_CUSTOM_FILTER_DUPLICATE",
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

	std::lock_guard lock(mutex);

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

	std::lock_guard lock(mutex);

	zoh_upsampler.step = static_cast<float>(sample_rate_hz) /
	                     static_cast<float>(zoh_upsampler.target_rate_hz);
	assert(zoh_upsampler.step < 1.0f);

	zoh_upsampler.pos = 0.0f;
}

void MixerChannel::InitLerpUpsamplerState()
{
	assert(sample_rate_hz < mixer.sample_rate_hz);

	std::lock_guard lock(mutex);

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

	std::lock_guard lock(mutex);

	resample_method = method;

	ConfigureResampler();
}

void MixerChannel::SetCrossfeedStrength(const float strength)
{
	std::lock_guard lock(mutex);

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

float MixerChannel::GetCrossfeedStrength()
{
	std::lock_guard lock(mutex);
	return crossfeed.strength;
}

void MixerChannel::SetReverbLevel(const float level)
{
	std::lock_guard lock(mutex);

	constexpr auto LevelMin   = 0.0f;
	constexpr auto LevelMax   = 1.0f;
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

float MixerChannel::GetReverbLevel()
{
	std::lock_guard lock(mutex);
	return reverb.level;
}

void MixerChannel::SetChorusLevel(const float level)
{
	std::lock_guard lock(mutex);

	constexpr auto LevelMin   = 0.0f;
	constexpr auto LevelMax   = 1.0f;
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

float MixerChannel::GetChorusLevel()
{
	std::lock_guard lock(mutex);
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

static constexpr void fill_8to16_lut()
{
	for (int i = 0; i <= UINT8_MAX; ++i) {
		lut_u8to16[i] = u8to16(i);
	}
}

// clang-format off
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
				frame.left  = lut_u8to16[static_cast<uint8_t>(data[pos])];
			}
		}
		// signed 8-bit
		else {
			if (stereo) {
				frame.left  = lut_s8to16[static_cast<int8_t>(data[left_pos])];
				frame.right = lut_s8to16[static_cast<int8_t>(data[right_pos])];
			} else {
				frame.left  = lut_s8to16[static_cast<int8_t>(data[pos])];
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
					auto host_pt0 = reinterpret_cast<const uint8_t* const>(data + left_pos);
					auto host_pt1 = reinterpret_cast<const uint8_t* const>(data + right_pos);

					if (sizeof(Type) == 2) {
						frame.left  = (int16_t)host_readw(host_pt0);
						frame.right = (int16_t)host_readw(host_pt1);
					} else {
						frame.left  = static_cast<float>((int32_t)host_readd(host_pt0));
						frame.right = static_cast<float>((int32_t)host_readd(host_pt1));
					}
				}
			} else { // mono
				if (nativeorder) {
					frame.left = static_cast<float>(data[pos]);
				} else {
					auto host_pt = reinterpret_cast<const uint8_t* const>(data + pos);

					if (sizeof(Type) == 2) {
						frame.left = (int16_t)host_readw(host_pt);
					} else {
						frame.left = static_cast<float>((int32_t)host_readd(host_pt));
					}
				}
			}
		} else { // unsigned
			const auto offs = 32768;
			if (stereo) {
				if (nativeorder) {
					frame.left = static_cast<float>(static_cast<int>(data[left_pos]) - offs);

					frame.right = static_cast<float>(static_cast<int>(data[right_pos]) - offs);
				} else {
					auto host_pt0 = reinterpret_cast<const uint8_t* const>(data + left_pos);
					auto host_pt1 = reinterpret_cast<const uint8_t* const>(data + right_pos);

					if (sizeof(Type) == 2) {
						frame.left  = static_cast<float>(static_cast<int>(host_readw(host_pt0)) - offs);
						frame.right = static_cast<float>(static_cast<int>(host_readw(host_pt1)) - offs);
					} else {
						frame.left  = static_cast<float>(static_cast<int>(host_readd(host_pt0)) - offs);
						frame.right = static_cast<float>(static_cast<int>(host_readd(host_pt1)) - offs);
					}
				}
			} else { // mono
				if (nativeorder) {
					frame.left = static_cast<float>(static_cast<int>(data[pos]) - offs);
				} else {
					auto host_pt = reinterpret_cast<const uint8_t* const>(data + pos);

					if (sizeof(Type) == 2) {
						frame.left = static_cast<float>(static_cast<int>(host_readw(host_pt)) - offs);
					} else {
						frame.left = static_cast<float>(static_cast<int>(host_readd(host_pt)) - offs);
					}
				}
			}
		}
	}
	return frame;
}
// clang-format on

// Converts sample stream to floats, performs output channel mappings, removes
// clicks, and optionally performs zero-order-hold-upsampling.
template <class Type, bool stereo, bool signeddata, bool nativeorder>
void MixerChannel::ConvertSamplesAndMaybeZohUpsample(const Type* data,
                                                     const int num_frames)
{
	assert(num_frames > 0);
	convert_buffer.clear();

	const auto mapped_output_left  = output_map.left;
	const auto mapped_output_right = output_map.right;

	const auto mapped_channel_left  = channel_map.left;
	const auto mapped_channel_right = channel_map.right;

	auto pos = 0;

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

		AudioFrame out_frame = {};
		out_frame[mapped_output_left] += frame_with_gain.left;
		out_frame[mapped_output_right] += frame_with_gain.right;

		convert_buffer.push_back(out_frame);

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

AudioFrame MixerChannel::ApplyCrossfeed(const AudioFrame frame)
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
	std::lock_guard lock(mutex);
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
	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      channel.GetName(),
	                      "MIXER_INVALID_CUSTOM_FADEOUT",
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
		// Otherwise, we inspect the running signal for changes
		constexpr auto ChangeThreshold = 1.0f;

		had_signal = fabsf(frame.left - last_frame.left) > ChangeThreshold ||
		             fabsf(frame.right - last_frame.right) > ChangeThreshold;

		last_frame = frame;
	}
	return frame;
}

void MixerChannel::Sleeper::MaybeSleep()
{
	// A signed integer can a durration of ~24 days in milliseconds, which
	// is surely more than enough.
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
	if (channel.is_enabled) {
		channel.Enable(false);
		// LOG_INFO("MIXER: %s fell asleep", channel.name.c_str());
	}
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
	std::lock_guard lock(mutex);
	return sleeper.WakeUp();
}

template <class Type, bool stereo, bool signeddata, bool nativeorder>
void MixerChannel::AddSamples(const int num_frames, const Type* data)
{
	assert(num_frames > 0);

	if (num_frames <= 0) {
		return;
	}

	std::lock_guard lock(mutex);

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

	// Assert that we're not attempting to do both LERP and Speex resample
	// We can do one or neither
	assert((do_lerp_upsample && !do_resample) ||
	       (!do_lerp_upsample && do_resample) ||
	       (!do_lerp_upsample && !do_resample));

	ConvertSamplesAndMaybeZohUpsample<Type, stereo, signeddata, nativeorder>(
	        data, num_frames);

	// Starting index this function will start writing to
	// The audio_frames vector can contain previously converted/resampled audio
	const size_t audio_frames_starting_size = audio_frames.size();

	if (do_lerp_upsample) {
		assert(!do_resample);

		auto& s = lerp_upsampler;

		for (size_t i = 0; i < convert_buffer.size();) {
			const auto curr_frame = convert_buffer[i];

			assert(s.pos >= 0.0f && s.pos <= 1.0f);
			AudioFrame lerped_frame = {};
			lerped_frame.left       = std::lerp(s.last_frame.left,
			                                    curr_frame.left,
			                                    s.pos);

			lerped_frame.right = std::lerp(s.last_frame.right,
			                               curr_frame.right,
			                               s.pos);

			audio_frames.push_back(lerped_frame);

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
				i += 1;
			}
		}
	} else if (do_resample) {
		auto in_frames = check_cast<spx_uint32_t>(convert_buffer.size());

		auto out_frames = check_cast<spx_uint32_t>(
		        estimate_max_out_frames(speex_resampler.state, in_frames));

		// Store this as a temporary variable
		// out_frames gets modified by Speex to reflect the actual
		// frames it wrote
		const auto estimated_frames = out_frames;

		audio_frames.resize(audio_frames_starting_size + estimated_frames);

		// These are vectors of AudioFrame which is just 2 packed floats
		const auto input_ptr = reinterpret_cast<const float*>(
		        convert_buffer.data());

		auto output_ptr = reinterpret_cast<float*>(
		        audio_frames.data() + audio_frames_starting_size);

		speex_resampler_process_interleaved_float(speex_resampler.state,
		                                          input_ptr,
		                                          &in_frames,
		                                          output_ptr,
		                                          &out_frames);

		// 'out_frames' now contains the actual number of
		// resampled frames, so ensure the number of output frames
		// is within the logical size.
		assert(out_frames <= estimated_frames);
		audio_frames.resize(audio_frames_starting_size + out_frames); // only shrinks
	} else {
		audio_frames.insert(audio_frames.end(),
		                    convert_buffer.begin(),
		                    convert_buffer.end());
	}

	// Optionally gate, filter, and apply crossfeed.
	// Runs in-place over newly added frames.
	for (size_t i = audio_frames_starting_size; i < audio_frames.size(); ++i) {
		if (do_noise_gate) {
			audio_frames[i] = noise_gate.processor.Process(
			        audio_frames[i]);
		}

		if (filters.highpass.state == FilterState::On) {
			auto& hpf = filters.highpass.hpf;

			audio_frames[i] = {hpf[0].filter(audio_frames[i].left),
			                   hpf[1].filter(audio_frames[i].right)};
		}
		if (filters.lowpass.state == FilterState::On) {
			auto& lpf = filters.lowpass.lpf;

			audio_frames[i] = {lpf[0].filter(audio_frames[i].left),
			                   lpf[1].filter(audio_frames[i].right)};
		}

		if (do_crossfeed) {
			audio_frames[i] = ApplyCrossfeed(audio_frames[i]);
		}
	}
}

void MixerChannel::AddSamples_m8(const int num_frames, const uint8_t* data)
{
	AddSamples<uint8_t, false, false, true>(num_frames, data);
}

void MixerChannel::AddSamples_m16(const int num_frames, const int16_t* data)
{
	AddSamples<int16_t, false, true, true>(num_frames, data);
}

void MixerChannel::AddSamples_s16(const int num_frames, const int16_t* data)
{
	AddSamples<int16_t, true, true, true>(num_frames, data);
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

void MixerChannel::AddAudioFrames(const std::vector<AudioFrame>& frames)
{
	constexpr bool IsStereo = true;
	constexpr bool IsSigned = true;
	constexpr bool IsNative = true;

	if (!frames.empty()) {
		const auto num_frames = static_cast<int>(frames.size());
		const auto frame_ptr  = &frames.front()[0];

		AddSamples<float, IsStereo, IsSigned, IsNative>(num_frames, frame_ptr);
	}
}

std::string MixerChannel::DescribeLineout()
{
	std::lock_guard lock(mutex);

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

MixerChannelSettings MixerChannel::GetSettings()
{
	std::lock_guard lock(mutex);

	MixerChannelSettings s = {};

	s.is_enabled         = is_enabled;
	s.user_volume_gain   = GetUserVolume();
	s.lineout_map        = GetLineoutMap();
	s.crossfeed_strength = GetCrossfeedStrength();
	s.reverb_level       = GetReverbLevel();
	s.chorus_level       = GetChorusLevel();

	return s;
}

void MixerChannel::SetSettings(const MixerChannelSettings& s)
{
	std::lock_guard lock(mutex);

	is_enabled = s.is_enabled;

	SetUserVolume(s.user_volume_gain);
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

// We use floats in the range of 16 bit integers everywhere.
// SDL expects floats to be normalized from 1.0 to -1.0
// It might be better for us to use normalized floats elsewhere in the future.
// For now, that probably breaks some assumptions elsewhere in the mixer.
// So just normalize as a final step before sending the data to SDL.
static float normalize_sample(float sample)
{
	return sample / 32768.0f;
}

// Mix a certain amount of new sample frames
static void mix_samples(const int frames_requested)
{
	assert(frames_requested > 0);

	mixer.output_buffer.clear();
	mixer.output_buffer.resize(frames_requested);

	mixer.reverb_aux_buffer.clear();
	mixer.reverb_aux_buffer.resize(frames_requested);

	mixer.chorus_aux_buffer.clear();
	mixer.chorus_aux_buffer.resize(frames_requested);

	// Render all channels and accumulate results in the master mixbuffer
	for (const auto& [_, channel] : mixer.channels) {
		channel->Mix(frames_requested);

		std::lock_guard lock(channel->mutex);

		const size_t num_frames = std::min(mixer.output_buffer.size(),
		                                   channel->audio_frames.size());

		for (size_t i = 0; i < num_frames; ++i) {
			if (channel->do_sleep) {
				mixer.output_buffer[i] += channel->sleeper.MaybeFadeOrListen(
				        channel->audio_frames[i]);
			} else {
				mixer.output_buffer[i] += channel->audio_frames[i];
			}

			if (mixer.do_reverb && channel->do_reverb_send) {
				mixer.reverb_aux_buffer[i] += channel->audio_frames[i] *
				                              channel->reverb.send_gain;
			}

			if (mixer.do_chorus && channel->do_chorus_send) {
				mixer.chorus_aux_buffer[i] += channel->audio_frames[i] *
				                              channel->chorus.send_gain;
			}
		}

		channel->audio_frames.erase(channel->audio_frames.begin(),
		                            channel->audio_frames.begin() + num_frames);

		if (channel->do_sleep) {
			channel->sleeper.MaybeSleep();
		}
	}

	if (mixer.do_reverb) {
		// Apply reverb effect to the reverb aux buffer, then mix the
		// results to the master output.
		//
		for (size_t i = 0; i < mixer.reverb_aux_buffer.size(); ++i) {
			// High-pass filter the reverb input
			auto& hpf = mixer.reverb.highpass_filter;

			// MVerb operates on two non-interleaved sample streams
			AudioFrame in_frame = mixer.reverb_aux_buffer[i];
			in_frame.left       = hpf[0].filter(in_frame.left);
			in_frame.right      = hpf[1].filter(in_frame.right);

			float* in_buf[2] = {&in_frame.left, &in_frame.right};

			AudioFrame out_frame = {};
			float* out_buf[2] = {&out_frame.left, &out_frame.right};

			constexpr auto NumFrames = 1;
			mixer.reverb.mverb.process(in_buf, out_buf, NumFrames);

			mixer.output_buffer[i] += out_frame;
		}
	}

	if (mixer.do_chorus) {
		// Apply chorus effect to the chorus aux buffer, then mix the
		// results to the master output.
		//
		for (size_t i = 0; i < mixer.chorus_aux_buffer.size(); ++i) {
			auto frame = mixer.chorus_aux_buffer[i];
			mixer.chorus.chorus_engine.process(&frame.left, &frame.right);
			mixer.output_buffer[i] += frame;
		}
	}

	// Apply high-pass filter to the master output
	for (auto& frame : mixer.output_buffer) {
		auto& hpf = mixer.highpass_filter;
		frame = {hpf[0].filter(frame.left), hpf[1].filter(frame.right)};
	}

	// Apply master gain
	const auto gain = mixer.master_gain.load(std::memory_order_relaxed);
	for (auto& frame : mixer.output_buffer) {
		frame *= gain;
	}

	if (mixer.do_compressor) {
		// Apply compressor to the master output as the very last step
		for (auto& frame : mixer.output_buffer) {
			frame = mixer.compressor.Process(frame);
		}
	}

	// Capture audio output if requested
	if (CAPTURE_IsCapturingAudio() || CAPTURE_IsCapturingVideo()) {
		mixer.capture_buffer.clear();
		mixer.capture_buffer.reserve(mixer.output_buffer.size() * 2);

		for (const auto frame : mixer.output_buffer) {
			const auto left = static_cast<uint16_t>(
			        clamp_to_int16(static_cast<int>(frame.left)));

			const auto right = static_cast<uint16_t>(
			        clamp_to_int16(static_cast<int>(frame.right)));

			mixer.capture_buffer.push_back(
			        static_cast<int16_t>(host_to_le16(left)));

			mixer.capture_buffer.push_back(
			        static_cast<int16_t>(host_to_le16(right)));
		}

		if (mixer.capture_queue.Size() + mixer.capture_buffer.size() >
		    mixer.capture_queue.MaxCapacity()) {

			// We're producing more audio than the capture is
			// consuming. This usually happens when the main thread
			// is being slowed down by video encoding (e.g., slow
			// host CPU or using zlib rather than zlib_ng). Not
			// ideal as this results in an audible "skip forward".
			// Without this, it's a complete stuttery mess though so
			// it's the lesser of two evils.
			//
			mixer.capture_queue.Clear();
		}
		mixer.capture_queue.NonblockingBulkEnqueue(mixer.capture_buffer);
	}

	// Normalize the final output before sending to SDL
	for (auto& frame : mixer.output_buffer) {
		frame.left  = normalize_sample(frame.left);
		frame.right = normalize_sample(frame.right);
	}
}

// Run in the main thread by a PIC Callback
static void capture_callback()
{
	if (!(CAPTURE_IsCapturingAudio() || CAPTURE_IsCapturingVideo())) {
		return;
	}

	static float frame_counter = 0.0f;
	frame_counter += get_mixer_frames_per_tick();

	const int num_frames = ifloor(frame_counter);
	assert(num_frames > 0);

	frame_counter -= static_cast<float>(num_frames);

	const int num_samples = num_frames * 2;

	// We can't block waiting on the mixer thread
	// Some mixer channels block waiting on the main thread and this would
	// deadlock
	static std::vector<int16_t> frames = {};
	frames.clear();

	const int samples_available = check_cast<int>(mixer.capture_queue.Size());
	const int samples_requested = std::min(num_samples, samples_available);

	if (samples_requested > 0) {
		mixer.capture_queue.BulkDequeue(frames,
		                                std::min(num_samples,
		                                         samples_available));
	}

	// Fill with silence if needed
	frames.resize(num_samples);

	CAPTURE_AddAudioData(mixer.sample_rate_hz, num_frames, frames.data());
}

static void SDLCALL mixer_callback([[maybe_unused]] void* userdata,
                                   Uint8* stream, int bytes_requested)
{
	assert(bytes_requested > 0);

	constexpr int BytesPerAudioFrame = sizeof(AudioFrame);

	const auto frames_requested = check_cast<size_t>(bytes_requested /
	                                                 BytesPerAudioFrame);

	// Mac OSX has been observed to be problematic if we ever block inside
	// SDL's callback. This ensures that we do not block waiting for more
	// audio. In the queue has run dry, we write what we have available and
	// the rest of the request is silence.
	const auto frames_to_dequeue = std::min(mixer.final_output.Size(),
	                                        frames_requested);

	const auto frame_stream = reinterpret_cast<AudioFrame*>(stream);

	const auto frames_received = mixer.final_output.BulkDequeue(frame_stream,
	                                                            frames_to_dequeue);
	// Satisfy any shortfall with silence
	std::fill(frame_stream + frames_received,
	          frame_stream + frames_requested,
	          AudioFrame{});
}

static void mixer_thread_loop()
{
	double last_mixed = 0.0;
	while (!mixer.thread_should_quit) {
		std::unique_lock lock(mixer.mutex);

		// This code is mostly for the fast-forward button (hold Alt + F12)
		const double now         = PIC_AtomicIndex();
		const double actual_time = now - last_mixed;
		const double expected_time = (static_cast<double>(mixer.blocksize) /
		                              static_cast<double>(mixer.sample_rate_hz)) *
		                             1000.0;
		last_mixed = now;

		// "Underflow" is not a concern since moving to a threaded
		// mixer. If the CPU is running slower than real-time, the audio
		// drivers will naturally slow down the audio. Therefore, we can
		// always request at least a blocksize worth of audio.
		int frames_requested = mixer.blocksize;

		if (mixer.fast_forward_mode) {
			// Flag is set only by the fast-forward hotkey handler.
			// Usually this means the emulation core is running much
			// faster than real-time. We must consume more audio to
			// "catch up" but always request at least a blocksize.
			frames_requested = std::max(
			        mixer.blocksize,
			        ifloor(actual_time * get_mixer_frames_per_tick()));
		}

		mix_samples(frames_requested);
		assert(mixer.output_buffer.size() ==
		       check_cast<size_t>(frames_requested));

		lock.unlock();

		if (mixer.state == MixerState::NoSound) {
			// SDL callback is not running. Mixed sound gets
			// discarded. Sleep for the expected duration to
			// simulate the time it would have taken to playback the
			// audio.
			constexpr double NanosecondsPerMillisecond = 1000000.0;

			const auto nap_time = static_cast<uint64_t>(
			        expected_time * NanosecondsPerMillisecond);

			std::this_thread::sleep_for(
			        std::chrono::nanoseconds(nap_time));
			continue;

		} else if (mixer.state == MixerState::Muted) {
			// SDL callback remains active. Enqueue silence.
			mixer.output_buffer.clear();
			mixer.output_buffer.resize(mixer.blocksize);

			mixer.final_output.BulkEnqueue(mixer.output_buffer);
			continue;
		}

		// Only true if we were in fast-forward mode at the time we
		// calculated frames_requested. That variable could have changed
		// by now but we need to always squash down to a blocksize of
		// audio.
		const bool audio_needs_squashing = frames_requested > mixer.blocksize;

		auto& to_mix = audio_needs_squashing ? mixer.fast_forward_buffer
		                                     : mixer.output_buffer;

		if (audio_needs_squashing) {
			// This is "chipmunk mode" meant for fast-forward.
			// It's basic sample skipping to compress a large amount
			// of audio into a single blocksize.
			assert(frames_requested > mixer.blocksize);

			mixer.fast_forward_buffer.clear();
			mixer.fast_forward_buffer.reserve(mixer.blocksize);

			const float index_add = static_cast<float>(frames_requested) /
			                        static_cast<float>(mixer.blocksize);

			float float_index = 0.0f;

			for (int i = 0; i < mixer.blocksize; ++i) {
				const size_t src_index = std::min(
				        check_cast<size_t>(iroundf(float_index)),
				        mixer.output_buffer.size() - 1);

				mixer.fast_forward_buffer.push_back(
				        mixer.output_buffer[src_index]);

				float_index += index_add;
			}
		}

		assert(to_mix.size() == static_cast<size_t>(mixer.blocksize));
		mixer.final_output.BulkEnqueue(to_mix);
	}
}

[[maybe_unused]] static const char* to_string(const MixerState s)
{
	switch (s) {
	case MixerState::NoSound: return "No sound";
	case MixerState::On: return "On";
	case MixerState::Muted: return "Mute";
	default: assertm(false, "Invalid MixerState"); return "";
	}
}

static void set_mixer_state(const MixerState new_state)
{
	assert(new_state == MixerState::Muted || new_state == MixerState::On);

#ifdef DEBUG_MIXER
	LOG_MSG("MIXER: Changing mixer state from %s to '%s'",
	        to_string(mixer.state),
	        to_string(new_state));
#endif

	if (new_state == MixerState::Muted) {
		// Clear out any audio in the queue to avoid a stutter on un-mute
		mixer.final_output.Clear();
	}

	mixer.state = new_state;
}

void MIXER_CloseAudioDevice()
{
	TIMER_DelTickHandler(capture_callback);

	if (mixer.thread.joinable()) {
		mixer.thread_should_quit = true;
		mixer.final_output.Stop();
		mixer.thread.join();
	}

	for (const auto& [_, channel] : mixer.channels) {
		channel->Enable(false);
	}

	if (mixer.sdl_device > 0) {
		SDL_CloseAudioDevice(mixer.sdl_device);
		mixer.sdl_device = 0;
	}
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

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		LOG_ERR("SDL: Failed to init SDL audio subsystem: %s", SDL_GetError());
		return false;
	}

	if ((mixer.sdl_device = SDL_OpenAudioDevice(
	             DeviceName, IsCapture, &desired, &obtained, sdl_allow_flags)) ==
	    SdlError) {
		LOG_ERR("MIXER: Can't open audio device: '%s'; sound output is disabled",
		        SDL_GetError());

		set_section_property_value("mixer", "nosound", "off");
		return false;
	}

	LOG_MSG("SDL: %s audio initialised", SDL_GetCurrentAudioDriver());

	// Opening SDL audio device succeeded
	//
	// An opened audio device starts out paused, and should be enabled for
	// playing by calling `SDL_PauseAudioDevice()` when you are ready for
	// your audio callback function to be called. We do that in
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
		LOG_INFO("MIXER: SDL negotiated the requested sample rate of %d to %d Hz",
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
	MIXER_LockMixerThread();

	constexpr auto HighpassCutoffFreqHz = 20.0;
	for (auto& f : mixer.highpass_filter) {
		f.setup(mixer.sample_rate_hz, HighpassCutoffFreqHz);
	}

	MIXER_UnlockMixerThread();
}

static void init_denoiser(bool enabled)
{
	for (const auto& [_, channel] : mixer.channels) {
		if (channel->HasFeature(ChannelFeature::NoiseGate)) {
			channel->EnableNoiseGate(enabled);
		}
	}
}

void MIXER_Init()
{
	auto section = get_section("mixer");
	assert(section);

	MIXER_LockMixerThread();

	// Initialize the 8-bit to 16-bit lookup table
	fill_8to16_lut();

	const auto mixer_state = section->GetBool("nosound") ? MixerState::NoSound
	                                                     : MixerState::On;

	auto set_no_sound = [&] {
		assert(mixer.sdl_device == 0);

		LOG_MSG("MIXER: Sound output disabled ('nosound' mode)");

		mixer.state = MixerState::NoSound;
	};

	mixer.sample_rate_hz = section->GetInt("rate");
	mixer.blocksize      = section->GetInt("blocksize");

	if (mixer_state == MixerState::NoSound) {
		set_no_sound();

	} else {
		if (init_sdl_sound(section->GetInt("rate"),
		                   section->GetInt("blocksize"),
		                   section->GetBool("negotiate"))) {

			mixer.final_output.Start();

			// SDL starts out paused so unpause it when we first set
			// the mixer state. We always keep SDL running in the
			// future. When the mixer becomes muted, we just write
			// silence.
			constexpr int Unpause = 0;
			SDL_PauseAudioDevice(mixer.sdl_device, Unpause);

			set_mixer_state(MixerState::On);
		} else {
			set_no_sound();
		}
	}

	const auto requested_prebuffer_ms = section->GetInt("prebuffer");
	mixer.prebuffer_ms = clamp(requested_prebuffer_ms, 1, MaxPrebufferMs);

	const auto prebuffer_frames = (mixer.sample_rate_hz * mixer.prebuffer_ms) /
	                              1000;

	mixer.final_output.Resize(mixer.blocksize + prebuffer_frames);

	// One second of audio
	mixer.capture_queue.Resize(mixer.sample_rate_hz * 2);

	mixer.thread = std::thread(mixer_thread_loop);
	set_thread_name(mixer.thread, "dosbox:mixer");

	TIMER_AddTickHandler(capture_callback);

	init_master_highpass_filter();

	// Initialise reverb
	const auto new_reverb_preset = reverb_pref_to_preset(
	        section->GetString("reverb"));

	MIXER_SetReverbPreset(new_reverb_preset);

	// Initialise chorus
	const auto new_chorus_preset = chorus_pref_to_preset(
	        section->GetString("chorus"));

	MIXER_SetChorusPreset(new_chorus_preset);

	// Init per-channel denoisers
	init_denoiser(section->GetBool("denoiser"));

	// Initialise master compressor
	init_compressor(section->GetBool("compressor"));

	// Initialise crossfeed
	const auto new_crossfeed_preset = crossfeed_pref_to_preset(
	        section->GetString("crossfeed"));

	MIXER_SetCrossfeedPreset(new_crossfeed_preset);

	MIXER_UnlockMixerThread();
}

void MIXER_Destroy()
{
	MIXER_CloseAudioDevice();
}

static void notify_mixer_setting_updated(SectionProp& section,
                                         const std::string& prop_name)
{
	MIXER_LockMixerThread();

	if (prop_name == "chorus") {
		const auto new_chorus_preset = chorus_pref_to_preset(
		        section.GetString("chorus"));

		if (mixer.chorus.preset != new_chorus_preset) {
			MIXER_SetChorusPreset(new_chorus_preset);
		}

	} else if (prop_name == "compressor") {
		init_compressor(section.GetBool("compressor"));

	} else if (prop_name == "crossfeed") {
		const auto new_crossfeed_preset = crossfeed_pref_to_preset(
		        section.GetString("crossfeed"));

		if (mixer.crossfeed.preset != new_crossfeed_preset) {
			MIXER_SetCrossfeedPreset(new_crossfeed_preset);
		}

	} else if (prop_name == "denoiser") {
		init_denoiser(section.GetBool("denoiser"));

	} else if (prop_name == "reverb") {
		const auto new_reverb_preset = reverb_pref_to_preset(
		        section.GetString("reverb"));

		if (mixer.reverb.preset != new_reverb_preset) {
			MIXER_SetReverbPreset(new_reverb_preset);
		}
	}

	MIXER_UnlockMixerThread();
}

void MIXER_Mute()
{
	if (mixer.state == MixerState::On) {
		set_mixer_state(MixerState::Muted);
		MIDI_Mute();

		TITLEBAR_NotifyAudioMutedStatus(true);
		LOG_MSG("MIXER: Muted audio output");
	}
}

void MIXER_Unmute()
{
	if (mixer.state == MixerState::Muted) {
		set_mixer_state(MixerState::On);
		MIDI_Unmute();

		TITLEBAR_NotifyAudioMutedStatus(false);
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

static void init_mixer_config_settings(SectionProp& sec_prop)
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

	using enum Property::Changeable::Value;

	auto bool_prop = sec_prop.AddBool("nosound", OnlyAtStart, false);
	assert(bool_prop);
	bool_prop->SetHelp(
	        "Enable silent mode ('off' by default). Sound is still emulated in silent mode,\n"
	        "but DOSBox outputs no sound to the host. Capturing the emulated audio output to\n"
	        "a WAV file works in silent mode.");

	auto int_prop = sec_prop.AddInt("rate", OnlyAtStart, DefaultSampleRateHz);
	assert(int_prop);
	int_prop->SetMinMax(8000, 96000);
	int_prop->SetHelp(
	        "Sample rate of DOSBox's internal audio mixer in Hz (%s by default). Valid\n"
	        "range is 8000 to 96000 Hz. The vast majority of consumer-grade audio hardware\n"
	        "uses a native rate of 48000 Hz. Recommend leaving this as-is unless you have\n"
	        "good reason to change it. The OS will most likely resample non-standard sample\n"
	        "rates to 48000 Hz anyway.");

	int_prop = sec_prop.AddInt("blocksize", OnlyAtStart, DefaultBlocksize);
	int_prop->SetMinMax(64, 8192);
	int_prop->SetHelp(
	        "Block size of the host audio device in sample frames (%s by default). Valid\n"
	        "range is 64 to 8192. Should be set to power-of-two values (e.g., 256, 512, 1024,\n"
	        "etc.) Larger values might help with sound stuttering but will introduce more\n"
	        "latency. Also see 'negotiate'.");

	int_prop = sec_prop.AddInt("prebuffer", OnlyAtStart, DefaultPrebufferMs);
	int_prop->SetMinMax(0, MaxPrebufferMs);
	int_prop->SetHelp(
	        "How many milliseconds of sound to render in advance on top of 'blocksize'\n"
	        "(%s by default). Larger values might help with sound stuttering but will\n"
	        "introduce more latency.");

	bool_prop = sec_prop.AddBool("negotiate", OnlyAtStart, DefaultAllowNegotiate);
	bool_prop->SetHelp(
	        "Negotiate a possibly better 'blocksize' setting (%s by default). Enable it if\n"
	        "you're not getting audio or the sound is stuttering with your 'blocksize'\n"
	        "setting. Disable it to force the manually set 'blocksize' value.");

	constexpr auto DefaultOn = true;
	bool_prop = sec_prop.AddBool("compressor", WhenIdle, DefaultOn);
	bool_prop->SetHelp(
	        "Enable the auto-leveling compressor on the master channel to prevent clipping\n"
	        "of the audio output ('on' by default). Possible values:\n"
	        "\n"
	        "  off:  Disable compressor.\n"
	        "  on:   Enable compressor (default).");

	auto string_prop = sec_prop.AddString("crossfeed", WhenIdle, "off");
	string_prop->SetHelp(
	        "Set crossfeed on the OPL and CMS (Gameblaster) mixer channels ('off' by\n"
	        "default). Many games pan the instruments 100%% left and 100%% right in the\n"
	        "stereo field on these audio devices which is unpleasant to listen to in\n"
	        "headphones. With crossfeed enabled, a portion of the left channel signal is\n"
	        "mixed into the right channel and vice versa, creating a more natural listening\n"
	        "experience. Possible values:\n"
	        "\n"
	        "  off:     No crossfeed (default).\n"
	        "  on:      Enable crossfeed (normal preset).\n"
	        "  light:   Light crossfeed (strength 15).\n"
	        "  normal:  Normal crossfeed (strength 40).\n"
	        "  strong:  Strong crossfeed (strength 65).\n"
	        "\n"
	        "Notes:\n"
	        "  - Use the MIXER command to apply crossfeed to other audio channels as well\n"
	        "    and to fine-tune the crossfeed strength per channel.");
	string_prop->SetValues({"off", "on", "light", "normal", "strong"});

	string_prop = sec_prop.AddString("reverb", WhenIdle, "off");
	string_prop->SetHelp(
	        "Reverb effect that adds a sense of space to the sound ('off') by default.\n"
	        "Possible values:\n"
	        "\n"
	        "  off:     No reverb (default).\n"
	        "\n"
	        "  on:      Enable reverb (medium preset).\n"
	        "\n"
	        "  tiny:    Simulates the sound of a small integrated speaker in a room;\n"
	        "           specifically designed for small-speaker audio systems\n"
	        "           (PC speaker, Tandy, PS/1 Audio, and LPT DAC devices).\n"
	        "\n"
	        "  small:   Adds a subtle sense of space; good for games that use a single\n"
	        "           synth channel (typically OPL) for both music and sound effects.\n"
	        "\n"
	        "  medium:  Medium room preset that works well with a wide variety of games.\n"
	        "\n"
	        "  large:   Large hall preset recommended for games that use separate\n"
	        "           channels for music and digital audio.\n"
	        "\n"
	        "  huge:    A stronger variant of the large hall preset; works really well\n"
	        "           in some games with more atmospheric soundtracks.\n"
	        "\n"
	        "Notes:\n"
	        "  - The presets apply a noticeable amount of reverb to the synth mixer channels\n"
	        "    (except for synths with built-in reverb; e.g., the Roland MT-32), and a\n"
	        "    subtle amount to the digital audio channels.\n"
	        "\n"
	        "  - Use the MIXER command to fine-tune the reverb levels per channel.");
	string_prop->SetValues(
	        {"off", "on", "tiny", "small", "medium", "large", "huge"});

	string_prop = sec_prop.AddString("chorus", WhenIdle, "off");
	string_prop->SetHelp(
	        "Chorus effect that adds a sense of stereo movement to the sound ('off' by\n"
	        "default). Possible values:\n"
	        "\n"
	        "  off:     No chorus (default).\n"
	        "\n"
	        "  on:      Enable chorus (normal preset).\n"
	        "\n"
	        "  light:   A light chorus effect (especially suited for synth music that\n"
	        "           features lots of white noise).\n"
	        "\n"
	        "  normal:  Normal chorus that works well with a wide variety of games.\n"
	        "\n"
	        "  strong:  An obvious and upfront chorus effect.\n"
	        "\n"
	        "Notes:\n"
	        "  - The presets apply the chorus effect to the synth channels only (except\n"
	        "    for synths with built-in chorus; e.g. the Roland MT-32).\n"
	        "\n"
	        "  - Use the MIXER command to fine-tune the chorus levels per channel.");
	string_prop->SetValues({"off", "on", "light", "normal", "strong"});

	bool_prop = sec_prop.AddBool("denoiser", WhenIdle, DefaultOn);
	bool_prop->SetHelp(
	        "Remove low-level residual noise from the output of the OPL synth and the Roland\n"
	        "Sound Canvas ('on' by default). The emulation of these devices is accurate to\n"
	        "the original hardware units, which includes the emulation of a very low-level\n"
	        "semi-random noise. Although this is authentic, most people would find it\n"
	        "slightly annoying. Possible values:\n"
	        "\n"
	        "  off:  Disable the denoiser.\n"
	        "\n"
	        "  on:   Enable the denoiser on the OPL and SOUNDCANVAS channels (default).\n"
	        "        The denoiser does not introduce any sound quality degradation; it only\n"
	        "        removes the barely audible residual noise in quiet passages.");

	MAPPER_AddHandler(handle_toggle_mute, SDL_SCANCODE_F8, PRIMARY_MOD, "mute", "Mute");
}

static void register_mixer_text_messages()
{
	MSG_Add("MIXER_INVALID_CUSTOM_FILTER",
	        "Invalid custom filter definition: [color=white]'%s'[reset].\n"
	        "Must be specified in [color=light-cyan]"
	        "'lpf|hpf ORDER CUTOFF_FREQUENCY'[reset] format.");

	MSG_Add("MIXER_INVALID_CUSTOM_FILTER_ORDER",
	        "Invalid %s filter order: [color=white]'%s'[reset]. "
	        "Must be an integer between 1 and %d.");

	MSG_Add("MIXER_INVALID_CUSTOM_FILTER_CUTOFF",
	        "Invalid %s filter cutoff frequency: "
	        "[color=white]'%s'[reset]. Must be a positive number.");

	MSG_Add("MIXER_INVALID_CUSTOM_FILTER_TYPE",
	        "Invalid filter type: [color=white]'%s'[reset]. "
	        "Must be either 'hpf' or 'lpf'.");

	MSG_Add("MIXER_INVALID_CUSTOM_FILTER_DUPLICATE",
	        "Invalid custom filter definition: [color=white]'%s'[reset].\n"
	        "The two filters must be of different types.");

	MSG_Add("MIXER_INVALID_CUSTOM_FADEOUT",
	        "Invalid custom fade-out definition: [color=white]'%s'[reset].\n"
	        "Must be specified in [color=light-cyan]'WAIT FADE'[reset] format where [color=light-cyan]WAIT[reset] is between %d and %d\n"
	        "(in milliseconds) and [color=light-cyan]FADE[reset] is between %d and %d (in milliseconds);\n"
	        "using [color=white]'off'[reset].");
}

void MIXER_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("mixer");
	section->AddUpdateHandler(notify_mixer_setting_updated);

	init_mixer_config_settings(*section);
	register_mixer_text_messages();
}
