// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/clap.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "audio/channel_names.h"
#include "audio/clap/plugin_manager.h"
#include "config/setup.h"
#include "misc/ansi_code_markup.h"
#include "shell/command_line.h"
#include "utils/string_utils.h"

static void set_clap_environment(const std::string& settings)
{
	// Space-separated NAME=VALUE assignments; double quotes preserve spaces
	// in values. Backslashes, semicolons, and additional '=' are literal.
	if (std::count(settings.begin(), settings.end(), '"') % 2 != 0) {
		throw std::runtime_error("CLAP: Unmatched quote in 'clap_env'");
	}
	const CommandLine command_line("", settings);
	std::vector<std::string> assignments = {};
	std::string assignment               = {};
	for (unsigned int i = 1; command_line.FindCommand(i, assignment); ++i) {
		assignments.push_back(assignment);
	}

	// Validate the entire list before changing the process environment.
	for (const auto& assignment : assignments) {
		const auto separator = assignment.find('=');
		if (separator == std::string::npos || separator == 0) {
			throw std::runtime_error(
			        "CLAP: Expected NAME=VALUE assignments in 'clap_env'");
		}
	}
	for (const auto& assignment : assignments) {
		const auto separator = assignment.find('=');
		MidiDeviceClapBase::SetEnvironmentVariable(
		        assignment.substr(0, separator),
		        assignment.substr(separator + 1));
	}
}

MidiDeviceClap::MidiDeviceClap()
{
	const auto section            = get_section("clap");
	const std::string plugin_name = section->GetString("clap_plugin");
	if (plugin_name.empty()) {
		throw std::runtime_error("CLAP: No plugin selected in 'clap_plugin'");
	}

	set_clap_environment(section->GetString("clap_env"));

	auto& plugin_manager = Clap::PluginManager::GetInstance();
	std::unique_ptr<Clap::Plugin> plugin = {};
	for (const auto& info : plugin_manager.GetPluginInfos()) {
		if (iequals(info.name, plugin_name)) {
			plugin = plugin_manager.LoadPlugin(info);
			if (plugin) {
				plugin_info = info;
				break;
			}
		}
	}
	if (!plugin) {
		throw std::runtime_error(format_str("CLAP: Failed to load plugin '%s'",
		                                    plugin_name.c_str()));
	}

	Config config         = {};
	config.sample_rate_hz = static_cast<float>(
	        section->GetInt("clap_sample_rate"));
	config.mixer_channel_name   = ChannelName::Clap;
	config.channel_features     = {ChannelFeature::Sleep,
	                               ChannelFeature::Stereo,
	                               ChannelFeature::Synthesizer};
	config.log_prefix           = "CLAP";
	config.renderer_thread_name = "dosbox:clap";
	Initialize(std::move(plugin), config);
}

void CLAP_ListDevices(MidiDeviceClap* device, MoreOutputStrings& output)
{
	const auto plugin_infos = Clap::PluginManager::GetInstance().GetPluginInfos();
	if (plugin_infos.empty()) {
		output.AddString("  %s\n\n", MSG_Get("CLAP_NO_PLUGINS").c_str());
		return;
	}

	const auto active_plugin = device ? &device->GetPluginInfo() : nullptr;
	for (const auto& info : plugin_infos) {
		const auto is_active = active_plugin &&
		                       active_plugin->id == info.id &&
		                       active_plugin->library_path == info.library_path;
		if (is_active) {
			output.AddString(convert_ansi_markup(
			                         "[color=light-green]* %s[reset]\n"),
			                 info.name.c_str());
		} else {
			output.AddString("  %s\n", info.name.c_str());
		}
	}
	output.AddString("\n");
}

static void notify_clap_setting_updated([[maybe_unused]] SectionProp& section,
                                        [[maybe_unused]] const std::string& prop_name)
{
	if (dynamic_cast<MidiDeviceClap*>(MIDI_GetCurrentDevice())) {
		MIDI_Init();
	}
}

void CLAP_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);
	const auto section = conf->AddSection("clap");
	section->AddUpdateHandler(notify_clap_setting_updated);
	MSG_Add("CLAP_NO_PLUGINS", "No CLAP plugins found");

	constexpr auto WhenIdle = Property::Changeable::WhenIdle;

	auto sec_prop = section->AddString("clap_plugin", WhenIdle, "");
	sec_prop->SetHelp(
		"CLAP plugin to load. The plugin must be in the 'plugins' directory in your DOSBox \n"
		"installation or configuration directory.\n"
	);

	auto* sample_rate = section->AddInt("clap_sample_rate", WhenIdle, 48000);
	sample_rate->SetMinMax(8000, 192000);
	sample_rate->SetHelp(
		"Audio sample rate the CLAP plugin will render at. If this is different fromn the \n"
		"host sample rate, audio from the plugin will be resampled. Value range:\n"
		"  min: 8000 \n"
		"  max: 192000 \n"
	);

	sec_prop = section->AddString("clap_env", WhenIdle, "");
	sec_prop->SetHelp(
		"Space separated environment variables to set before loading the CLAP plugin. \n"
		"Variables are in the form 'ENV_VAR_NAME_1=env_value ENV_VAR_NAME_2=another_val'. \n"
		"Double quotes may be used if environment variables include spaces. Eg: \n"
		"'\"ENV_VAR_NAME=val with spaces\" ENV_VAR_NAME_2=no_spaces'\n"
	);
}
