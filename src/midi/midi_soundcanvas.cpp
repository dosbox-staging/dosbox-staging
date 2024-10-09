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
#include "pic.h"
#include "std_filesystem.h"
#include "string_utils.h"

// Symbolic model aliases
namespace BestModelAlias {
constexpr auto Sc55    = "sc55";
constexpr auto Sc55mk2 = "sc55mk2";
} // namespace BestModelAlias

struct ScModel {
	SoundCanvasModel model         = {};
	const char* config_name        = {};
	const char* display_name_short = {};
	const char* display_name_long  = {};
};

const ScModel sc55_100_model = {SoundCanvasModel::Sc55_100,
                                "sc55_100",
                                "100",
                                "Roland SC-55 v1.00"};

const ScModel sc55_110_model = {SoundCanvasModel::Sc55_110,
                                "sc55_110",
                                "110",
                                "Roland SC-55 v1.10"};

const ScModel sc55_120_model = {SoundCanvasModel::Sc55_120,
                                "sc55_120",
                                "120",
                                "Roland SC-55 v1.20"};

const ScModel sc55_121_model = {SoundCanvasModel::Sc55_121,
                                "sc55_121",
                                "121",
                                "Roland SC-55 v1.21"};

const ScModel sc55_200_model = {SoundCanvasModel::Sc55_200,
                                "sc55_200",
                                "200",
                                "Roland SC-55 v2.00"};

const ScModel sc55mk2_100 = {SoundCanvasModel::Sc55mk2_100,
                             "sc55mk2_100",
                             "mk2_100",
                             "Roland SC-55mk2 v1.00"};

const ScModel sc55mk2_101 = {SoundCanvasModel::Sc55mk2_101,
                             "sc55mk2_101",
                             "mk2_101",
                             "Roland SC-55mk2 v1.01"};

// Listed in resolution priority order
const std::vector<const ScModel*> all_models = {&sc55_121_model,
                                                &sc55_120_model,
                                                &sc55_110_model,
                                                &sc55_100_model,
                                                &sc55_200_model,
                                                &sc55mk2_101,
                                                &sc55mk2_100};

// Listed in resolution priority order
const std::vector<const ScModel*> sc55_models = {&sc55_121_model,
                                                 &sc55_120_model,
                                                 &sc55_110_model,
                                                 &sc55_100_model,
                                                 &sc55_200_model};

// Listed in resolution priority order
const std::vector<const ScModel*> sc55mk2_models = {&sc55mk2_101, &sc55mk2_100};

static Section_prop* get_soundcanvas_section()
{
	assert(control);

	auto section = static_cast<Section_prop*>(control->GetSection("soundcanvas"));
	assert(section);

	return section;
}

static const std::optional<Clap::PluginInfo> find_plugin_for_model(
        const SoundCanvasModel model, const std::vector<Clap::PluginInfo>& plugin_infos)
{
	// Search for plugins that are likely Roland SC-55 emulators by
	// inspecting ther descriptions

	for (const auto& plugin_info : plugin_infos) {
		auto has = [&](const char* s) -> bool {
			return find_in_case_insensitive(s, plugin_info.name);
		};

		const auto has_sc55 = has(" sc-55") || has(" sc55");

		switch (model) {
		case SoundCanvasModel::Sc55_100:
			if (has_sc55 && (has(" 1.00") || has(" v1.00"))) {
				return plugin_info;
			}
			break;

		case SoundCanvasModel::Sc55_110:
			if (has_sc55 && (has(" 1.10") || has(" v1.10"))) {
				return plugin_info;
			}
			break;

		case SoundCanvasModel::Sc55_120:
			if (has_sc55 && (has(" 1.20") || has(" v1.20"))) {
				return plugin_info;
			}
			break;

		case SoundCanvasModel::Sc55_121:
			if (has_sc55 && (has(" 1.21") || has(" v1.21"))) {
				return plugin_info;
			}
			break;

		case SoundCanvasModel::Sc55_200:
			if (has_sc55 && (has(" 2.00") || has(" v2.00"))) {
				return plugin_info;
			}
			break;

		case SoundCanvasModel::Sc55mk2_100:
			if (has_sc55 &&
			    (has(" mk2") && (has(" 1.00") || has(" v1.00")))) {
				return plugin_info;
			}
			break;

		case SoundCanvasModel::Sc55mk2_101:
			if (has_sc55 && ((has(" mk2") || has("-mk2") || has("55mk2")) &&
			                 (has(" 1.01") || has(" v1.01")))) {
				return plugin_info;
			}
			break;

		default: assertm(false, "Invalid SoundCanvasModel value");
		};
	}

	// Request model not found
	return {};
}

PluginAndModel MidiDeviceSoundCanvas::TryLoadPlugin(const SoundCanvasModel model)
{
	auto& plugin_manager    = Clap::PluginManager::GetInstance();
	const auto plugin_infos = plugin_manager.GetPluginInfos();

	if (auto p = find_plugin_for_model(model, plugin_infos); p) {
		const auto plugin_info = *p;

		auto plugin = plugin_manager.LoadPlugin(plugin_info.path,
		                                        plugin_info.id);

		if (plugin) {
			return {std::move(plugin), model};
		}
	}

	// Request model not found
	return {};
}

PluginAndModel MidiDeviceSoundCanvas::LoadModel(const std::string& wanted_model_name)
{
	// Determine the list of model candidates and the lookup method for
	// the requested model name:
	//
	// - Symbolic model names ('auto', 'sc55', 'sc55mk2') resolve the first
	//   available model from the list of candidates in the listed priority.
	//   The lookup only fails if none of the candidate models could be
	//   resolved.
	//
	// - Concrete versioned model names always try to resolve a single model
	//   version or fail if the requested model is not present.
	//
	auto [load_first_available, candidate_models] =
	        [&]() -> std::pair<bool, std::vector<const ScModel*>> {
		// Symbolic mode names (resolve the best match from a list of
		// candidates).
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
		// model from the list of all supported models).
		return {false, all_models};
	}();

	// Perform model lookup from the list of candidate models
	for (const auto& model : candidate_models) {
		if (load_first_available || wanted_model_name == model->config_name) {
			if (auto p = TryLoadPlugin(model->model); p.plugin) {
				return p;
			}
		}
	}

	// Couldn't load
	return {};
}

MidiDeviceSoundCanvas::~MidiDeviceSoundCanvas()
{
	LOG_MSG("SOUNDCANVAS: Shutting down");

	if (had_underruns) {
		LOG_WARNING(
		        "SOUNDCANVAS: Fix underruns by lowering CPU load "
		        "or increasing your conf's prebuffer");
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

	last_rendered_ms   = 0.0;
	ms_per_audio_frame = 0.0;

	active_model = {};

	MIXER_UnlockMixerThread();
}

MidiDevice::ListDevicesResult MidiDeviceSoundCanvas::ListDevices(Program* caller)
{
	// Table layout constants
	constexpr auto ColumnDelim = " ";
	constexpr auto Indent      = "  ";

	const auto available_models = [&] {
		std::set<const ScModel*> available_models = {};

		for (auto m : all_models) {
			if (const auto p = TryLoadPlugin(m->model); p.plugin) {
				available_models.insert(m);
			}
		}
		return available_models;
	}();

	if (available_models.empty()) {
		caller->WriteOut("%s%s\n",
		                 Indent,
		                 MSG_Get("MIDI_DEVICE_NO_SUPPORTED_MODELS"));

		return ListDevicesResult::Ok;
	}

	const auto active_sc_model = [&]() -> std::optional<const ScModel*> {
		const auto it = std::find_if(
		        all_models.begin(), all_models.end(), [&](const ScModel* m) {
			        return (active_model && *active_model == m->model);
		        });

		if (it != all_models.end()) {
			return *it;
		} else {
			return {};
		}
	}();

	auto highlight_model = [&](const ScModel* model,
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
	const std::vector<const ScModel*> models_old_to_new = {&sc55_100_model,
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

	return ListDevicesResult::Ok;
}

MidiDeviceSoundCanvas::MidiDeviceSoundCanvas([[maybe_unused]] const char* conf)
{
	const auto model_name = get_soundcanvas_section()->Get_string(
	        "soundcanvas_model");

	auto plugin_wrapper = LoadModel(model_name);
	if (!plugin_wrapper.plugin) {
		LOG_WARNING("SOUNDCANVAS: Failed to load '%s' Sound Canvas model",
		            model_name.c_str());
		throw std::runtime_error();
	}

	active_model = plugin_wrapper.model;
	assert(active_model);

	clap.plugin = std::move(plugin_wrapper.plugin);

	const auto it = std::find_if(all_models.begin(),
	                             all_models.end(),
	                             [&](const ScModel* m) {
		                             return *active_model == m->model;
	                             });
	assert(it != all_models.end());

	const auto sc_model = *it;
	LOG_MSG("SOUNDCANVAS: Initialised %s", sc_model->display_name_long);

	// Determine native sample rate of the Sound Canvas model, so we can run
	// the plugin at the native rate
	//
	const auto sample_rate_hz = [&] {
		switch (*active_model) {
		// Roland SC-55
		case SoundCanvasModel::Sc55_100:
		case SoundCanvasModel::Sc55_110:
		case SoundCanvasModel::Sc55_120:
		case SoundCanvasModel::Sc55_121:
		case SoundCanvasModel::Sc55_200: return 32000.0;

		// Roland SC-55mk2
		case SoundCanvasModel::Sc55mk2_100:
		case SoundCanvasModel::Sc55mk2_101: return 33103.0;

		default: assertm(false, "Invalid SoundCanvasModel"); return 0.0;
		}
	}();

	ms_per_audio_frame = MillisInSecond / sample_rate_hz;

	MIXER_LockMixerThread();

	// Set up the mixer callback
	const auto mixer_callback = std::bind(&MidiDeviceSoundCanvas::MixerCallback,
	                                      this,
	                                      std::placeholders::_1);

	mixer_channel = MIXER_AddChannel(mixer_callback,
	                                 sample_rate_hz,
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

	if (!mixer_channel->TryParseAndSetCustomFilter(filter_prefs)) {
		if (filter_prefs != "off") {
			LOG_WARNING(
			        "SOUNDCANVAS: Invalid 'soundcanvas_filter' value: '%s', "
			        "using 'off'",
			        filter_prefs.c_str());
		}

		mixer_channel->SetHighPassFilter(FilterState::Off);
		mixer_channel->SetLowPassFilter(FilterState::Off);

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

	// TODO simplify all this
	// Size the in-bound work FIFO
	//
	// MIDI has a baud rate of 31250; at optimum, this is 31250 bits per
	// second. A MIDI byte is 8 bits plus a start and stop bit, and each
	// MIDI message is at least two bytes, which gives a total of 20 bits
	// per message. This means that under optimal conditions, a maximum of
	// 1562 messages per second can be obtained via the MIDI protocol.
	//
	// We have measured DOS games sending hundreds of MIDI messages within a
	// short handful of millseconds, so a safe but very generous upper bound
	// is used.
	//
	// (Note: the actual memory used by the FIFO is incremental based on
	// actual usage).
	//
	static constexpr uint16_t midi_spec_max_msg_rate_hz = 1562;

	work_fifo.Resize(midi_spec_max_msg_rate_hz * 10);

	clap.plugin->Activate(sample_rate_hz);

	// Start rendering audio
	const auto render = std::bind(&MidiDeviceSoundCanvas::Render, this);
	renderer          = std::thread(render);
	set_thread_name(renderer, "dosbox:soundcanvas");

	// Start playback
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
void MidiDeviceSoundCanvas::SendMessage(const MidiMessage& msg)
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

		last_rendered_ms = PIC_FullIndex();
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

// Keep the fifo populated with freshly rendered buffers
void MidiDeviceSoundCanvas::Render()
{
	while (work_fifo.IsRunning()) {
		work_fifo.IsEmpty() ? RenderAudioFramesToFifo()
		                    : ProcessWorkFromFifo();
	}
}

static void init_soundcanvas_dosbox_settings(Section_prop& sec_prop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto* str_prop = sec_prop.Add_string("soundcanvas_model", when_idle, "auto");

	// Listed in resolution priority order
	str_prop->Set_values({"auto",
	                      BestModelAlias::Sc55,
	                      sc55_121_model.config_name,
	                      sc55_120_model.config_name,
	                      sc55_110_model.config_name,
	                      sc55_100_model.config_name,
	                      sc55_200_model.config_name,

	                      BestModelAlias::Sc55mk2,
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

	str_prop = sec_prop.Add_string("soundcanvas_filter", when_idle, "off");
	assert(str_prop);
	str_prop->Set_help(
	        "Filter for the Roland Sound Canvas audio output:\n"
	        "  off:       Don't filter the output (default).\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

static void register_soundcanvas_text_messages()
{
	MSG_Add("SC55_MODELS_LABEL", "SC-55 models   ");
	MSG_Add("SOUNDCANVAS_ACTIVE_MODEL_LABEL", "Active model:   ");
}

static void soundcanvas_init([[maybe_unused]] Section* sec) {}

void SOUNDCANVAS_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);
	Section_prop* sec_prop = conf->AddSection_prop("soundcanvas",
	                                               &soundcanvas_init);

	assert(sec_prop);
	init_soundcanvas_dosbox_settings(*sec_prop);

	register_soundcanvas_text_messages();
}
