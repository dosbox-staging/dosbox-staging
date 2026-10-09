// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/clap.h"

#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "audio/channel_names.h"
#include "audio/clap/plugin_manager.h"
#include "audio/mixer.h"
#include "config/config.h"
#include "config/setup.h"
#include "misc/ansi_code_markup.h"
#include "utils/env_utils.h"
#include "utils/string_utils.h"

static std::unordered_map<std::string, std::string> parse_clap_environment(std::string settings)
{
	std::unordered_map<std::string, std::string> variables = {};
	trim(settings);
	while (!settings.empty()) {
		if (settings.front() == '"' &&
		    settings.find('"', 1) == std::string::npos) {
			constexpr auto message = "CLAPSYNTH: Unclosed quote in 'clap_env'";
			LOG_WARNING("%s", message);
			throw std::runtime_error(message);
		}
		const auto assignment = strip_word(settings);
		const auto separator  = assignment.find('=');
		const auto name       = assignment.substr(0, separator);
		if (separator == std::string::npos || name.empty() ||
		    name.find_first_of(" \t\r\n\"\v\f") != std::string::npos) {
			constexpr auto message = "CLAPSYNTH: Expected NAME=VALUE assignments in 'clap_env'";
			LOG_WARNING("%s", message);
			throw std::runtime_error(message);
		}
		variables.insert_or_assign(name, assignment.substr(separator + 1));
		trim(settings);
	}
	return variables;
}

MidiDeviceClap::MidiDeviceClap()
{
	const auto section = get_section("clap");
	const auto variables = parse_clap_environment(section->GetString("clap_env"));
	for (const auto& [name, value] : variables) {
		set_env_var(name.c_str(), value.c_str(), Env::Overwrite);
	}

	auto& manager           = Clap::PluginManager::GetInstance();
	const auto library_name = section->GetString("clap_library");
	const auto plugin_name  = section->GetString("clap_plugin");
	const auto candidate    = manager.FindPlugin(library_name, plugin_name);
	if (!candidate) {
		const auto message = format_str("CLAPSYNTH: No plugin matches library '%s' and plugin '%s'",
		                                library_name.c_str(),
		                                plugin_name.c_str());
		LOG_WARNING("%s", message.c_str());
		throw std::runtime_error(message);
	}

	auto plugin = manager.LoadPlugin(*candidate);
	if (!plugin) {
		const auto message = format_str(
		        "CLAPSYNTH: Failed to load MIDI synth '%s' from '%s'",
		        candidate->name.c_str(),
		        candidate->library_path.string().c_str());
		LOG_WARNING("%s", message.c_str());
		throw std::runtime_error(message);
	}
	plugin_info = *candidate;
	const auto sample_rate_setting = section->GetString("clap_sample_rate");
	const auto configured_rate_hz  = parse_int(sample_rate_setting);
	auto sample_rate_hz            = MIXER_GetSampleRate();
	if (configured_rate_hz && *configured_rate_hz >= 8000 &&
	    *configured_rate_hz <= 192000) {
		sample_rate_hz = *configured_rate_hz;
	} else if (!sample_rate_setting.empty() &&
	           !iequals(sample_rate_setting, "auto")) {
		LOG_WARNING("CLAPSYNTH: Invalid 'clap_sample_rate' value '%s'; using the mixer rate of %d Hz",
		            sample_rate_setting.c_str(),
		            sample_rate_hz);
	}
	clap.Initialize(std::move(plugin), sample_rate_hz);

	MIXER_LockMixerThread();
	mixer_channel = MIXER_AddChannel(std::bind_front(&MidiSynth::MixerCallback,
	                                                 this),
	                                 sample_rate_hz,
	                                 ChannelName::Clap,
	                                 {ChannelFeature::Sleep,
	                                  ChannelFeature::Stereo,
	                                  ChannelFeature::Synthesizer});
	mixer_channel->SetResampleMethod(ResampleMethod::Resample);
	mixer_channel->Set0dbScalar(Max16BitSampleValue);
	StartRenderer(sample_rate_hz, "dosbox:clap");
	MIXER_UnlockMixerThread();

	LOG_MSG("CLAPSYNTH: Initialised '%s' from '%s' at %d Hz",
	        plugin_info.name.c_str(),
	        plugin_info.library_path.filename().string().c_str(),
	        sample_rate_hz);
}

void MidiDeviceClap::ProcessWorkItem(const MidiWork& work)
{
	clap.ProcessWorkItem(work);
}

void MidiDeviceClap::RenderAudioFramesToFifo(const int num_frames)
{
	clap.RenderAudioFramesToFifo(num_frames, audio_frame_fifo);
}

static std::string format_clap_plugin_list(const std::vector<Clap::PluginInfo>& infos,
                                           const Clap::PluginInfo* active)
{
	std::string result   = {};
	std_fs::path library = {};
	for (const auto& info : infos) {
		if (info.library_path != library) {
			library = info.library_path;
			result += format_str("  %s\n",
			                     library.filename().string().c_str());
		}
		const auto is_active = active &&
		                       info.library_path == active->library_path &&
		                       info.id == active->id;
		if (is_active) {
			result += convert_ansi_markup("[color=light-green]");
		}
		result += format_str("  %c %u - %s\n",
		                     is_active ? '*' : ' ',
		                     info.index,
		                     info.name.c_str());
		if (is_active) {
			result += convert_ansi_markup("[reset]");
		}
	}
	return result;
}

void CLAP_ListDevices(const MidiDeviceClap* device, MoreOutputStrings& output)
{
	const auto infos = Clap::PluginManager::GetInstance().GetPluginInfos();
	if (infos.empty()) {
		output.AddString("  %s\n\n", MSG_Get("CLAP_NO_PLUGINS").c_str());
		return;
	}
	const auto listing = format_clap_plugin_list(
	        infos, device ? &device->GetPluginInfo() : nullptr);
	output.AddString("%s\n", listing.c_str());
}

static void init_clap_config_settings(SectionProp& section)
{
	constexpr auto WhenIdle = Property::Changeable::WhenIdle;
	auto property = section.AddString("clap_library", WhenIdle, "");
	property->SetHelp(
	        "CLAP plugin library to use (unset by default). Libraries are searched in the\n"
	        "'plugins' directory in your DOSBox installation or configuration directory.\n"
	        "Use a case-insensitive partial filename, with or without the '.clap' extension.\n"
	        "Leave unset to search all libraries. Use 'MIXER /LISTMIDI' to list plugins.");
	property = section.AddString("clap_plugin", WhenIdle, "");
	property->SetHelp(
	        "CLAP MIDI synth to use (unset by default). Use a case-insensitive partial plugin\n"
	        "name or its numeric index within the library. The first matching plugin is used.\n"
	        "Leave unset to use the first plugin in the selected library. The plugin must\n"
	        "accept MIDI on its first note input and provide a stereo first audio output.");
	property = section.AddString("clap_env", WhenIdle, "");
	property->SetPreserveQuotes(true);
	property->SetHelp(
	        "Environment variables to set before loading the CLAP plugin (unset by default).\n"
	        "Separate NAME=VALUE assignments with spaces. Quote an entire assignment if its\n"
	        "value contains spaces, for example: \"SOUNDCANVAS_ROM_PATH=C:\\My ROMs\".\n"
	        "Existing environment variables are overwritten.");
	property = section.AddString("clap_sample_rate", WhenIdle, "auto");
	property->SetHelp(
	        "Sample rate of the CLAP MIDI synth in Hz ('auto' by default). Leave unset or\n"
	        "use 'auto' to match the mixer rate, or specify a rate from 8000 to 192000 Hz.\n"
	        "Choose the plugin's native sample rate when known; the mixer resamples its\n"
	        "output as needed.");
}

static void notify_clap_setting_updated([[maybe_unused]] SectionProp& section,
                                        [[maybe_unused]] const std::string& property)
{
	if (dynamic_cast<MidiDeviceClap*>(MIDI_GetCurrentDevice())) {
		MIDI_Init();
	}
}

void CLAP_AddConfigSection(const ConfigPtr& conf)
{
	auto section = conf->AddSection("clap");
	section->AddUpdateHandler(notify_clap_setting_updated);
	init_clap_config_settings(*section);
	MSG_Add("CLAP_NO_PLUGINS", "No available CLAP plugins");
}
