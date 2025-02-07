/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#include "midi_soundcanvas.h"

#include <optional>
#include <set>
#include <vector>

#include "clap/all.h"

#include "../audio/clap/plugin_manager.h"
#include "ansi_code_markup.h"
#include "channel_names.h"
#include "checks.h"
#include "pic.h"
#include "std_filesystem.h"
#include "string_utils.h"

CHECK_NARROWING();

namespace SoundCanvas {

// Symbolic model aliases
namespace BestModelAlias {
constexpr auto Sc55    = "sc55";
constexpr auto Sc55mk2 = "sc55mk2";
} // namespace BestModelAlias

const SynthModel sc55_100_model = {Model::Sc55_100, "sc55_100", "100", "Roland SC-55 v1.00"};

const SynthModel sc55_110_model = {Model::Sc55_110, "sc55_110", "110", "Roland SC-55 v1.10"};

const SynthModel sc55_120_model = {Model::Sc55_120, "sc55_120", "120", "Roland SC-55 v1.20"};

const SynthModel sc55_121_model = {Model::Sc55_121, "sc55_121", "121", "Roland SC-55 v1.21"};

const SynthModel sc55_200_model = {Model::Sc55_200, "sc55_200", "200", "Roland SC-55 v2.00"};

const SynthModel sc55mk2_100 = {Model::Sc55mk2_100,
                                "sc55mk2_100",
                                "mk2_100",
                                "Roland SC-55mk2 v1.00"};

const SynthModel sc55mk2_101 = {Model::Sc55mk2_101,
                                "sc55mk2_101",
                                "mk2_101",
                                "Roland SC-55mk2 v1.01"};

// Listed in resolution priority order
const std::vector<const SynthModel*> all_models = {&sc55_121_model,
                                                   &sc55_120_model,
                                                   &sc55_110_model,
                                                   &sc55_100_model,
                                                   &sc55_200_model,
                                                   &sc55mk2_101,
                                                   &sc55mk2_100};

// Listed in resolution priority order
const std::vector<const SynthModel*> sc55_models = {&sc55_121_model,
                                                    &sc55_120_model,
                                                    &sc55_110_model,
                                                    &sc55_100_model,
                                                    &sc55_200_model};

// Listed in resolution priority order
const std::vector<const SynthModel*> sc55mk2_models = {&sc55mk2_101, &sc55mk2_100};

struct PluginAndModel {
	std::unique_ptr<Clap::Plugin> plugin = nullptr;
	SynthModel model                     = {};
};

} // namespace SoundCanvas

static const std::optional<Clap::PluginInfo> find_plugin_for_model(
        const SoundCanvas::Model model,
        const std::vector<Clap::PluginInfo>& plugin_infos)
{
	using namespace SoundCanvas;

	// Search for plugins that are likely Roland SC-55 emulators by
	// inspecting ther descriptions

	for (const auto& plugin_info : plugin_infos) {
		auto has = [&](const char* s) -> bool {
			return find_in_case_insensitive(s, plugin_info.name);
		};

		const auto has_sc55 = has(" sc-55") || has(" sc55");

		switch (model) {
		case Model::Sc55_100:
			if (has_sc55 && (has(" 1.00") || has(" v1.00"))) {
				return plugin_info;
			}
			break;

		case Model::Sc55_110:
			if (has_sc55 && (has(" 1.10") || has(" v1.10"))) {
				return plugin_info;
			}
			break;

		case Model::Sc55_120:
			if (has_sc55 && (has(" 1.20") || has(" v1.20"))) {
				return plugin_info;
			}
			break;

		case Model::Sc55_121:
			if (has_sc55 && (has(" 1.21") || has(" v1.21"))) {
				return plugin_info;
			}
			break;

		case Model::Sc55_200:
			if (has_sc55 && (has(" 2.00") || has(" v2.00"))) {
				return plugin_info;
			}
			break;

		case Model::Sc55mk2_100:
			if (has_sc55 &&
			    (has(" mk2") && (has(" 1.00") || has(" v1.00")))) {
				return plugin_info;
			}
			break;

		case Model::Sc55mk2_101:
			if (has_sc55 && ((has(" mk2") || has("-mk2") || has("55mk2")) &&
			                 (has(" 1.01") || has(" v1.01")))) {
				return plugin_info;
			}
			break;

		default: assertm(false, "Invalid Model value");
		};
	}

	// Requested model not found
	return {};
}

SoundCanvas::PluginAndModel try_load_plugin(const SoundCanvas::SynthModel& model)
{
	auto& plugin_manager    = Clap::PluginManager::GetInstance();
	const auto plugin_infos = plugin_manager.GetPluginInfos();

	if (auto p = find_plugin_for_model(model.model, plugin_infos); p) {
		const auto plugin_info = *p;

		auto plugin = plugin_manager.LoadPlugin(plugin_info);
		if (plugin) {
			return {std::move(plugin), model};
		}
	}

	// Requested model not found
	return {};
}

SoundCanvas::PluginAndModel load_model(const std::string& wanted_model_name)
{
	using namespace SoundCanvas;

	// Determine the list of model candidates and the lookup method:
	//
	// - Symbolic model names ('auto', 'sc55', 'sc55mk2') resolve the first
	//   available model from the list of candidates. The lookup only fails
	//   if none of the candidate models are available.
	//
	// - Concrete versioned model names always try to resolve the requested
	//   version or fail if it's not available.
	//
	auto [load_first_available, candidate_models] =
	        [&]() -> std::pair<bool, std::vector<const SoundCanvas::SynthModel*>> {
		// Symbolic mode names (resolve the best match from a list of
		// candidates in priority order).
		if (wanted_model_name == "auto") {
			return {true, all_models};
		}
		if (wanted_model_name == BestModelAlias::Sc55) {
			return {true, sc55_models};
		}
		if (wanted_model_name == BestModelAlias::Sc55mk2) {
			return {true, sc55mk2_models};
		}

		// Concrete versioned model name (resolve the specific requested
		// model or fail).
		return {false, all_models};
	}();

	// Perform model lookup from the list of candidate models
	for (const auto& model : candidate_models) {
		if (load_first_available || wanted_model_name == model->config_name) {
			if (auto p = try_load_plugin(*model); p.plugin) {
				return p;
			}
		}
	}

	// Couldn't load
	return {};
}

static float native_sample_rate_hz_for_model(const SoundCanvas::Model model)
{
	using namespace SoundCanvas;

	switch (model) {
	// Roland SC-55
	case Model::Sc55_100:
	case Model::Sc55_110:
	case Model::Sc55_120:
	case Model::Sc55_121:
	case Model::Sc55_200: return 32000.0;

	// Roland SC-55mk2
	case Model::Sc55mk2_100:
	case Model::Sc55mk2_101: return 33103.0;

	default: assertm(false, "Invalid SoundCanvas::Model"); return 0.0;
	}
}

SoundCanvas::SynthModel MidiDeviceSoundCanvas::GetModel() const
{
	return model;
}

static Section_prop* get_soundcanvas_section()
{
	assert(control);

	auto section = static_cast<Section_prop*>(control->GetSection("soundcanvas"));
	assert(section);

	return section;
}

static std::string get_model_setting()
{
	return get_soundcanvas_section()->Get_string("soundcanvas_model");
}

static void setup_filter(MixerChannelPtr& channel, const bool filter_enabled)
{
	if (filter_enabled) {
		channel->SetHighPassFilter(FilterState::Off);

		// All Roland SC-55 models seem to very gently soften the
		// high-frequency response of the output which makes samples
		// with lots of treble content a bit softer and consequently
		// quieter in the mix (e.g., hi-hats, cymbals, harpsichord, some
		// synth sounds, etc.)
		//
		// On most materials, the filter makes little difference, but in
		// certain soundtracks the unfiltered high-frequency content can
		// get annoying for those with sensitive ears listening in
		// quality headphones.
		//
		// The filter response has been confirmed on different SC-55
		// models (original, SC-55mk2, SC-55st, SC-55K) and recordings
		// made by different individuals. This indicates this was
		// either a deliberate design choice by Roland, or they just
		// happened to use similar analog output stages in the entire
		// SC-55 product line. This also means composers must have
		// compensated for the softened treble resopnse, which can make
		// the unfiltered output a little bit too harsh.
		//
		constexpr auto LpfOrder        = 1;
		constexpr auto LpfCutoffFreqHz = 11000;

		channel->ConfigureLowPassFilter(LpfOrder, LpfCutoffFreqHz);
		channel->SetLowPassFilter(FilterState::On);

	} else {
		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
	}
}

MidiDeviceSoundCanvas::MidiDeviceSoundCanvas()
{
	using namespace SoundCanvas;

	const auto model_name = get_model_setting();

	auto plugin_wrapper = load_model(model_name);
	if (!plugin_wrapper.plugin) {
		const auto msg = format_str("SOUNDCANVAS: Failed to load '%s' Sound Canvas model",
		                            model_name.c_str());
		LOG_WARNING("%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	model = plugin_wrapper.model;

	clap.plugin = std::move(plugin_wrapper.plugin);

	const auto it = std::find_if(all_models.begin(),
	                             all_models.end(),
	                             [&](const SoundCanvas::SynthModel* m) {
		                             return model.model == m->model;
	                             });
	assert(it != all_models.end());

	const auto sc_model = *it;
	LOG_MSG("SOUNDCANVAS: Initialised %s", sc_model->display_name_long);

	// Run the plugin at the native sample rate of the Sound Canvas model
	// to avoid any extra resampling passes.
	//
	const auto sample_rate_hz = native_sample_rate_hz_for_model(model.model);

	ms_per_audio_frame = MillisInSecond / sample_rate_hz;

	MIXER_LockMixerThread();

	// Set up the mixer callback
	const auto mixer_callback = std::bind(&MidiDeviceSoundCanvas::MixerCallback,
	                                      this,
	                                      std::placeholders::_1);

	mixer_channel = MIXER_AddChannel(mixer_callback,
	                                 iroundf(sample_rate_hz),
	                                 ChannelName::SoundCanvas,
	                                 {ChannelFeature::Sleep,
	                                  ChannelFeature::Stereo,
	                                  ChannelFeature::Synthesizer});

	mixer_channel->SetResampleMethod(ResampleMethod::Resample);

	// CLAP plugins render float audio frames between -1.0f and +1.0f, so we
	// ask the channel to scale all the samples up to its 0db level.
	mixer_channel->Set0dbScalar(Max16BitSampleValue);

	// Set up channel filter
	const auto filter_prefs = get_soundcanvas_section()->Get_string(
	        "soundcanvas_filter");

	if (const auto maybe_bool = parse_bool_setting(filter_prefs)) {
		const auto filter_enabled = *maybe_bool;
		setup_filter(mixer_channel, filter_enabled);

	} else if (!mixer_channel->TryParseAndSetCustomFilter(filter_prefs)) {
		if (filter_prefs != "off") {
			LOG_WARNING(
			        "SOUNDCANVAS: Invalid 'soundcanvas_filter' value: '%s', "
			        "using 'on'",
			        filter_prefs.c_str());
		}

		const auto filter_enabled = true;
		setup_filter(mixer_channel, filter_enabled);

		set_section_property_value("soundcanvas", "soundcanvas_filter", "off");
	}

	// Double the baseline PCM prebuffer because MIDI is demanding and
	// bursty. The mixer's default of ~20 ms becomes 40 ms here, which gives
	// slower systems a better chance to keep up (and prevent their audio
	// frame FIFO from running dry).
	const auto render_ahead_ms = MIXER_GetPreBufferMs() * 2;

	// Size the out-bound audio frame FIFO
	assertm(sample_rate_hz >= 8000, "Sample rate must be at least 8 kHz");

	const auto audio_frames_per_ms = iround(sample_rate_hz / MillisInSecond);
	audio_frame_fifo.Resize(
	        check_cast<size_t>(render_ahead_ms * audio_frames_per_ms));

	// Size the in-bound work FIFO
	work_fifo.Resize(MaxMidiWorkFifoSize);

	clap.plugin->Activate(iroundf(sample_rate_hz));

	// Start rendering audio
	const auto render = std::bind(&MidiDeviceSoundCanvas::Render, this);
	renderer          = std::thread(render);
	set_thread_name(renderer, "dosbox:sndcanv");

	// Start playback
	MIXER_UnlockMixerThread();
}

MidiDeviceSoundCanvas::~MidiDeviceSoundCanvas()
{
	LOG_MSG("SOUNDCANVAS: Shutting down");

	if (had_underruns) {
		LOG_WARNING(
		        "SOUNDCANVAS: Fix underruns by lowering the CPU load "
		        "or increasing the 'prebuffer' or 'blocksize' setting");
		had_underruns = false;
	}

	MIXER_LockMixerThread();

	// Stop playback
	if (mixer_channel) {
		mixer_channel->Enable(false);
	}

	// Stop queueing new MIDI work and audio frames
	work_fifo.Stop();
	audio_frame_fifo.Stop();

	// Wait for the rendering thread to finish
	if (renderer.joinable()) {
		renderer.join();
	}

	// Deregister the mixer channel and remove it
	assert(mixer_channel);
	MIXER_DeregisterChannel(mixer_channel);
	mixer_channel.reset();

	MIXER_UnlockMixerThread();
}

int MidiDeviceSoundCanvas::GetNumPendingAudioFrames()
{
	const auto now_ms = PIC_FullIndex();

	// Wake up the channel and update the last rendered time datum.
	assert(mixer_channel);
	if (mixer_channel->WakeUp()) {
		last_rendered_ms = now_ms;
		return 0;
	}
	if (last_rendered_ms >= now_ms) {
		return 0;
	}

	// Return the number of audio frames needed to get current again
	assert(ms_per_audio_frame > 0.0);

	const auto elapsed_ms = now_ms - last_rendered_ms;
	const auto num_audio_frames = iround(ceil(elapsed_ms / ms_per_audio_frame));
	last_rendered_ms += (num_audio_frames * ms_per_audio_frame);

	return num_audio_frames;
}

// The request to play the channel message is placed in the MIDI work FIFO
void MidiDeviceSoundCanvas::SendMidiMessage(const MidiMessage& msg)
{
	std::vector<uint8_t> message(msg.data.begin(), msg.data.end());

	MidiWork work{std::move(message),
	              GetNumPendingAudioFrames(),
	              MessageType::Channel};

	work_fifo.Enqueue(std::move(work));
}

// The request to play the sysex message is placed in the MIDI work FIFO
void MidiDeviceSoundCanvas::SendSysExMessage(uint8_t* sysex, size_t len)
{
	std::vector<uint8_t> message(sysex, sysex + len);
	MidiWork work{std::move(message), GetNumPendingAudioFrames(), MessageType::SysEx};
	work_fifo.Enqueue(std::move(work));
}

// The callback operates at the audio frame-level, steadily adding samples to
// the mixer until the requested numbers of audio frames is met.
void MidiDeviceSoundCanvas::MixerCallback(const int requested_audio_frames)
{
	assert(mixer_channel);

	// Report buffer underruns
	constexpr auto warning_percent = 5.0f;

	if (const auto percent_full = audio_frame_fifo.GetPercentFull();
	    percent_full < warning_percent) {
		static auto iteration = 0;
		if (iteration++ % 100 == 0) {
			LOG_WARNING("SOUNDCANVAS: Audio buffer underrun");
		}
		had_underruns = true;
	}

	static std::vector<AudioFrame> audio_frames = {};

	const auto has_dequeued = audio_frame_fifo.BulkDequeue(audio_frames,
	                                                       requested_audio_frames);

	if (has_dequeued) {
		mixer_channel->AddSamples_sfloat(requested_audio_frames,
		                                 &audio_frames[0][0]);

		last_rendered_ms = PIC_AtomicIndex();
	} else {
		assert(!audio_frame_fifo.IsRunning());
		mixer_channel->AddSilence();
	}
}

void MidiDeviceSoundCanvas::RenderAudioFramesToFifo(const int num_audio_frames)
{
	assert(num_audio_frames > 0);

	static std::vector<float> left  = {};
	static std::vector<float> right = {};

	// Maybe expand the vectors
	if (check_cast<int>(left.size()) < num_audio_frames) {
		left.resize(num_audio_frames);
		right.resize(num_audio_frames);
	}

	float* audio_out[] = {left.data(), right.data()};

	clap.plugin->Process(audio_out, num_audio_frames, clap.event_list);
	clap.event_list.Clear();

	for (auto i = 0; i < num_audio_frames; ++i) {
		audio_frame_fifo.Enqueue({left[i], right[i]});
	}
}

// The next MIDI work task is processed, which includes rendering audio frames
// prior to sending channel and sysex messages to the plugin
void MidiDeviceSoundCanvas::ProcessWorkFromFifo()
{
	const auto work = work_fifo.Dequeue();
	if (!work) {
		return;
	}

#if 0
	// To log inter-cycle rendering
	if (work->num_pending_audio_frames > 0) {
		LOG_MSG("SOUNDCANVAS: %2u audio frames prior to %s message, followed by "
		        "%2lu more messages. Have %4lu audio frames queued",
		        work->num_pending_audio_frames,
		        work->message_type == MessageType::Channel ? "channel" : "sysex",
		        work_fifo.Size(),
		        audio_frame_fifo.Size());
	}
#endif

	if (work->num_pending_audio_frames > 0) {
		RenderAudioFramesToFifo(work->num_pending_audio_frames);
	}

	if (work->message_type == MessageType::Channel) {
		assert(work->message.size() >= MaxMidiMessageLen);
		clap.event_list.AddMidiEvent(work->message, 0);

	} else {
		assert(work->message_type == MessageType::SysEx);
		clap.event_list.AddMidiSysExEvent(work->message, 0);
	}
}

// Keep the FIFO populated with freshly rendered buffers
void MidiDeviceSoundCanvas::Render()
{
	while (work_fifo.IsRunning()) {
		work_fifo.IsEmpty() ? RenderAudioFramesToFifo()
		                    : ProcessWorkFromFifo();
	}
}

static std::set<const SoundCanvas::SynthModel*> available_models = {};

static bool available_models_initialised = false;

void SOUNDCANVAS_ListDevices(MidiDeviceSoundCanvas* device, Program* caller)
{
	using namespace SoundCanvas;

	// Table layout constants
	constexpr auto ColumnDelim = " ";
	constexpr auto Indent      = "  ";

	if (!available_models_initialised) {
		for (auto m : all_models) {
			if (const auto p = try_load_plugin(*m); p.plugin) {
				available_models.insert(m);
			}
		}
		available_models_initialised = true;
	}

	if (available_models.empty()) {
		caller->WriteOut("%s%s\n\n", Indent, MSG_Get("MIDI_DEVICE_NO_MODELS"));
		return;
	}

	const std::optional<SynthModel> active_model = [&] {
		std::optional<SynthModel> model = {};
		if (device) {
			model = device->GetModel();
		}
		return model;
	}();

	const auto active_sc_model = [&]() -> std::optional<const SynthModel*> {
		const auto it = std::find_if(all_models.begin(),
		                             all_models.end(),
		                             [&](const SynthModel* m) {
			                             return (active_model &&
			                                     *active_model == m);
		                             });

		if (it != all_models.end()) {
			return *it;
		} else {
			return {};
		}
	}();

	auto highlight_model = [&](const SynthModel* model,
	                           const char* display_name) -> std::string {
		constexpr auto darkgray = "[color=dark-gray]";
		constexpr auto green    = "[color=light-green]";
		constexpr auto reset    = "[reset]";

		const bool is_missing = (available_models.find(model) ==
		                         available_models.end());

		const auto is_active = active_sc_model && *active_sc_model == model;

		const auto color = (is_missing ? darkgray
		                               : (is_active ? green : reset));

		const auto active_prefix = (is_active ? "*" : " ");

		const auto model_string = format_str(
		        "%s%s%s%s", color, active_prefix, display_name, reset);

		return convert_ansi_markup(model_string.c_str());
	};

	// Print available Sound Canvas models
	caller->WriteOut("%s%s", Indent, MSG_Get("SC55_MODELS_LABEL"));

	// Display order, from old to new
	const std::vector<const SynthModel*> models_old_to_new = {&sc55_100_model,
	                                                          &sc55_110_model,
	                                                          &sc55_120_model,
	                                                          &sc55_121_model,
	                                                          &sc55_200_model,
	                                                          &sc55mk2_100,
	                                                          &sc55mk2_101};

	for (const auto& model : models_old_to_new) {
		const auto display_name = model->display_name_short;
		caller->WriteOut("%s%s",
		                 highlight_model(model, display_name).c_str(),
		                 ColumnDelim);
	}
	caller->WriteOut("\n");

	caller->WriteOut("%s---\n", Indent);

	// Print info about the active model
	if (active_sc_model) {
		caller->WriteOut("%s%s%s\n",
		                 Indent,
		                 MSG_Get("SOUNDCANVAS_ACTIVE_MODEL_LABEL"),
		                 (*active_sc_model)->display_name_long);
	} else {
		caller->WriteOut("%s%s\n",
		                 Indent,
		                 MSG_Get("MIDI_DEVICE_NO_MODEL_ACTIVE"));
	}

	caller->WriteOut("\n");
}

static void init_soundcanvas_dosbox_settings(Section_prop& sec_prop)
{
	using namespace SoundCanvas;

	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto* str_prop = sec_prop.Add_string("soundcanvas_model", when_idle, "auto");

	// Listed in resolution priority order
	str_prop->Set_values({"auto",
	                      SoundCanvas::BestModelAlias::Sc55,
	                      sc55_121_model.config_name,
	                      sc55_120_model.config_name,
	                      sc55_110_model.config_name,
	                      sc55_100_model.config_name,
	                      sc55_200_model.config_name,

	                      SoundCanvas::BestModelAlias::Sc55mk2,
	                      sc55mk2_100.config_name,
	                      sc55mk2_101.config_name});

	str_prop->Set_help(
	        "The Roland Sound Canvas model to use.\n"
	        "One or more CLAP audio plugins that implement the supported Sound Canvas\n"
	        "models must be present in the 'plugin' directory in your DOSBox configuration\n"
	        "directory. DOSBox searches for the requested model by inspecting the plugin\n"
	        "descriptions.\n"
	        "The lookup for the best models is performed in order as listed:\n"
	        "  auto:       Pick the best available model (default).\n"
	        "  sc55:       Pick the best available original SC-55 model.\n"
	        "  sc55mk2:    Pick the best available SC-55mk2 model.\n"
	        "  <version>:  Use the exact specified model version (e.g., 'sc55_121').");

	str_prop = sec_prop.Add_string("soundcanvas_filter", when_idle, "on");
	assert(str_prop);
	str_prop->Set_help(
	        "Filter for the Roland Sound Canvas audio output:\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

static void register_soundcanvas_text_messages()
{
	MSG_Add("SC55_MODELS_LABEL", "SC-55 models    ");
	MSG_Add("SOUNDCANVAS_ACTIVE_MODEL_LABEL", "Active model:    ");
}

static void soundcanvas_init([[maybe_unused]] Section* sec)
{
	const auto device = MIDI_GetCurrentDevice();

	if (device && device->GetName() == MidiDeviceName::SoundCanvas) {
		const auto soundcanvas_device = dynamic_cast<MidiDeviceSoundCanvas*>(
		        device);

		const auto curr_model_setting =
		        soundcanvas_device ? soundcanvas_device->GetModel().config_name
		                           : "";

		const auto new_model_setting = get_model_setting();

		if (curr_model_setting != new_model_setting) {
			MIDI_Init();
		}
	}
}

void SOUNDCANVAS_AddConfigSection(const ConfigPtr& conf)
{
	constexpr auto ChangeableAtRuntime = true;

	assert(conf);
	Section_prop* sec_prop = conf->AddSection_prop("soundcanvas",
	                                               &soundcanvas_init,
	                                               ChangeableAtRuntime);
	assert(sec_prop);
	init_soundcanvas_dosbox_settings(*sec_prop);

	register_soundcanvas_text_messages();
}
