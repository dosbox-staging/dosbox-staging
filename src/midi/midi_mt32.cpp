/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2012-2021  sergm <sergm@bigmir.net>
 *  Copyright (C) 2020-2021  Nikos Chantziaras <realnc@gmail.com> (settings)
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

#include "midi_mt32.h"

#if C_MT32EMU

#include <cassert>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>

#include <SDL_endian.h>

#include "control.h"
#include "cross.h"
#include "fs_utils.h"
#include "midi.h"
#include "midi_lasynth_model.h"
#include "mixer.h"
#include "string_utils.h"
#include "support.h"
#include "../ints/int10.h"

// mt32emu Settings
// ----------------

// Synth granularity in frames. We keep four buffers in-flight at any given
// time: when playback exhausts the "head" buffer, we ask MT-32 to render the
// next buffer, asynchronously, which is then placed at the back of the queue.
// These four buffers mean we typically have 2048 frames or ~48 ms in backlog.
static constexpr int FRAMES_PER_BUFFER = 512;

// Analogue circuit modes: DIGITAL_ONLY, COARSE, ACCURATE, OVERSAMPLED
constexpr auto ANALOG_MODE = MT32Emu::AnalogOutputMode_ACCURATE;

// DAC Emulation modes: NICE, PURE, GENERATION1, and GENERATION2
constexpr auto DAC_MODE = MT32Emu::DACInputMode_NICE;

// Analog rendering types: BIT16S, FLOAT
constexpr auto RENDERING_TYPE = MT32Emu::RendererType_FLOAT;

// Sample rate conversion quality: FASTEST, FAST, GOOD, BEST
constexpr auto RATE_CONVERSION_QUALITY = MT32Emu::SamplerateConversionQuality_BEST;

// Prefer higher ramp resolution over the coarser volume steps used by the hardware
constexpr bool USE_NICE_RAMP = true;

// Prefer higher panning resolution over the coarser positions used by the hardware
constexpr bool USE_NICE_PANNING = true;

// Prefer the rich sound offered by the hardware's existing partial mixer
constexpr bool USE_NICE_PARTIAL_MIXING = false;

using Rom = LASynthModel::Rom;
constexpr auto versioned = LASynthModel::ROM_TYPE::VERSIONED;
constexpr auto unversioned = LASynthModel::ROM_TYPE::UNVERSIONED;

// Traditional ROMs
const Rom mt32_pcm_any_f = {"pcm_mt32", "MT32_PCM.ROM", unversioned};
const Rom mt32_ctrl_any_f = {"ctrl_mt32", "MT32_CONTROL.ROM", unversioned};
const Rom cm32l_pcm_any_f = {"pcm_cm32l", "CM32L_PCM.ROM", unversioned};
const Rom cm32l_ctrl_any_f = {"ctrl_cm32l", "CM32L_CONTROL.ROM", unversioned};

// MAME ROMs (versioned)
const Rom mt32_pcm_100_f = {"pcm_mt32", "r15449121.ic37.bin", versioned};
const Rom mt32_pcm_100_l = {"pcm_mt32_l", "r15179844.ic21.bin", versioned};
const Rom mt32_pcm_100_h = {"pcm_mt32_h", "r15179845.ic22.bin", versioned};
const Rom mt32_ctrl_104_a = {"ctrl_mt32_1_04_a", "mt32_1.0.4.ic27.bin", versioned};
const Rom mt32_ctrl_104_b = {"ctrl_mt32_1_04_b", "mt32_1.0.4.ic26.bin", versioned};
const Rom mt32_ctrl_105_a = {"ctrl_mt32_1_05_a", "mt32_1.0.5.ic27.bin", versioned};
const Rom mt32_ctrl_105_b = {"ctrl_mt32_1_05_b", "mt32_1.0.5.ic26.bin", versioned};
const Rom mt32_ctrl_106_a = {"ctrl_mt32_1_06_a", "mt32_1.0.6.ic27.bin", versioned};
const Rom mt32_ctrl_106_b = {"ctrl_mt32_1_06_b", "mt32_1.0.6.ic26.bin", versioned};
const Rom mt32_ctrl_107_a = {"ctrl_mt32_1_07_a", "mt32_1.0.7.ic27.bin", versioned};
const Rom mt32_ctrl_107_b = {"ctrl_mt32_1_07_b", "mt32_1.0.7.ic26.bin", versioned};
const Rom mt32_ctrl_bluer_a = {"ctrl_mt32_bluer_a", "blue_ridge__mt32a.bin", versioned};
const Rom mt32_ctrl_bluer_b = {"ctrl_mt32_bluer_b", "blue_ridge__mt32b.bin", versioned};
const Rom mt32_ctrl_204_f = {"ctrl_mt32_2_04", "mt32_2.0.4.ic28.bin", versioned};
const Rom cm32l_ctrl_100_f = {"ctrl_cm32l_1_00", "lapc-i.v1.0.0.ic3.bin", versioned};
const Rom cm32l_ctrl_102_f = {"ctrl_cm32l_1_02", "cm32l_control.rom", versioned};
const Rom cm32l_pcm_100_h = {"pcm_cm32l_h", "r15179945.ic8.bin", versioned};
const Rom &cm32l_pcm_100_l = mt32_pcm_100_f; // Lower half of samples comes from MT-32

// Roland LA Models (composed of ROMs)
const LASynthModel mt32_any_model = {"mt32",  &mt32_pcm_any_f,  nullptr,
                                     nullptr, &mt32_ctrl_any_f, nullptr,
                                     nullptr};
const LASynthModel mt32_104_model = {"mt32_104",      &mt32_pcm_100_f,
                                     &mt32_pcm_100_l, &mt32_pcm_100_h,
                                     nullptr,         &mt32_ctrl_104_a,
                                     &mt32_ctrl_104_b};
const LASynthModel mt32_105_model = {"mt32_105",      &mt32_pcm_100_f,
                                     &mt32_pcm_100_l, &mt32_pcm_100_h,
                                     nullptr,         &mt32_ctrl_105_a,
                                     &mt32_ctrl_105_b};
const LASynthModel mt32_106_model = {"mt32_106",      &mt32_pcm_100_f,
                                     &mt32_pcm_100_l, &mt32_pcm_100_h,
                                     nullptr,         &mt32_ctrl_106_a,
                                     &mt32_ctrl_106_b};
const LASynthModel mt32_107_model = {"mt32_107",      &mt32_pcm_100_f,
                                     &mt32_pcm_100_l, &mt32_pcm_100_h,
                                     nullptr,         &mt32_ctrl_107_a,
                                     &mt32_ctrl_107_b};
const LASynthModel mt32_bluer_model = {"mt32_bluer",      &mt32_pcm_100_f,
                                       &mt32_pcm_100_l,   &mt32_pcm_100_h,
                                       nullptr,           &mt32_ctrl_bluer_a,
                                       &mt32_ctrl_bluer_b};
const LASynthModel mt32_204_model = {"mt32_204",       &mt32_pcm_100_f,
                                     &mt32_pcm_100_l,  &mt32_pcm_100_h,
                                     &mt32_ctrl_204_f, nullptr,
                                     nullptr};
const LASynthModel cm32l_any_model = {"cm32l", &cm32l_pcm_any_f,  nullptr,
                                      nullptr, &cm32l_ctrl_any_f, nullptr,
                                      nullptr};
const LASynthModel cm32l_100_model = {
        "cm32l_100",       nullptr, &cm32l_pcm_100_l, &cm32l_pcm_100_h,
        &cm32l_ctrl_100_f, nullptr, nullptr};
const LASynthModel cm32l_102_model = {
        "cm32l_102",       nullptr, &cm32l_pcm_100_l, &cm32l_pcm_100_h,
        &cm32l_ctrl_102_f, nullptr, nullptr};

// Aliased models
const LASynthModel mt32_new_model = {"mt32_new", // new is 2.04
                                     &mt32_pcm_100_f, &mt32_pcm_100_l,
                                     &mt32_pcm_100_h, &mt32_ctrl_204_f,
                                     nullptr,         nullptr};
const LASynthModel mt32_old_model = {"mt32_old", // old is 1.07
                                     &mt32_pcm_100_f,  &mt32_pcm_100_l,
                                     &mt32_pcm_100_h,  nullptr,
                                     &mt32_ctrl_107_a, &mt32_ctrl_107_b};

// In order that "model = auto" will load
const LASynthModel *all_models[] = {
        &cm32l_any_model, &cm32l_102_model,  &cm32l_100_model, &mt32_any_model,
        &mt32_old_model,  &mt32_107_model,   &mt32_106_model,  &mt32_105_model,
        &mt32_104_model,  &mt32_bluer_model, &mt32_new_model,  &mt32_204_model,
};

MidiHandler_mt32 mt32_instance;

static void init_mt32_dosbox_settings(Section_prop &sec_prop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	const char *models[] = {"auto",
	                        cm32l_any_model.GetName(),
	                        cm32l_102_model.GetName(),
	                        cm32l_100_model.GetName(),
	                        mt32_any_model.GetName(),
	                        mt32_old_model.GetName(),
	                        mt32_107_model.GetName(),
	                        mt32_106_model.GetName(),
	                        mt32_105_model.GetName(),
	                        mt32_104_model.GetName(),
	                        mt32_bluer_model.GetName(),
	                        mt32_new_model.GetName(),
	                        mt32_204_model.GetName(),
	                        0};
	auto *str_prop = sec_prop.Add_string("model", when_idle, "auto");
	str_prop->Set_values(models);
	str_prop->Set_help(
	        "Model of synthesizer to use.\n"
	        "'auto' picks the first model with available ROMs, in order as listed.\n"
	        "'cm32l' and 'mt32' pick the first model of their type, in the order listed.\n"
	        "'mt32_old' and 'mt32_new' are aliases for 1.07 and 2.04, respectively.");

	str_prop = sec_prop.Add_string("romdir", when_idle, "");
	str_prop->Set_help("The directory containing ROMs for one or more models.\n"
	                   "The directory can be absolute or relative, or leave it blank to\n"
	                   "use the 'mt32-roms' directory in your DOSBox configuration\n"
	                   "directory. Other common system locations will be checked as well.\n"
	                   "ROM files inside this directory may include any of the following:\n"
	                   "  - MT32_CONTROL.ROM and MT32_PCM.ROM, for the 'mt32' model.\n"
	                   "  - CM32L_CONTROL.ROM and CM32L_PCM.ROM, for the 'cm32l' model.\n"
	                   "  - Unzipped MAME MT-32 and CM-32L ROMs, for the versioned models.");
}

#if defined(WIN32)

static std::deque<std::string> get_rom_dirs()
{
	return {
	        CROSS_GetPlatformConfigDir() + "mt32-roms\\",
	        "C:\\mt32-rom-data\\",
	};
}

#elif defined(MACOSX)

static std::deque<std::string> get_rom_dirs()
{
	return {
	        CROSS_GetPlatformConfigDir() + "mt32-roms/",
	        CROSS_ResolveHome("~/Library/Audio/Sounds/MT32-Roms/"),
	        "/usr/local/share/mt32-rom-data/",
	        "/usr/share/mt32-rom-data/",
	};
}

#else

static std::deque<std::string> get_rom_dirs()
{
	// First priority is $XDG_DATA_HOME
	const char *xdg_data_home_env = getenv("XDG_DATA_HOME");
	const auto xdg_data_home = CROSS_ResolveHome(
	        xdg_data_home_env ? xdg_data_home_env : "~/.local/share");

	std::deque<std::string> dirs = {
	        xdg_data_home + "/dosbox/mt32-roms/",
	        xdg_data_home + "/mt32-rom-data/",
	};

	// Second priority are the $XDG_DATA_DIRS
	const char *xdg_data_dirs_env = getenv("XDG_DATA_DIRS");
	if (!xdg_data_dirs_env)
		xdg_data_dirs_env = "/usr/local/share:/usr/share";

	for (auto xdg_data_dir : split(xdg_data_dirs_env, ':')) {
		trim(xdg_data_dir);
		if (xdg_data_dir.empty()) {
			continue;
		}
		const auto resolved_dir = CROSS_ResolveHome(xdg_data_dir);
		dirs.emplace_back(resolved_dir + "/mt32-rom-data/");
	}

	// Third priority is $XDG_CONF_HOME, for convenience
	dirs.emplace_back(CROSS_GetPlatformConfigDir() + "mt32-roms/");

	return dirs;
}

#endif

static std::deque<std::string> get_selected_dirs()
{
	const auto section = static_cast<Section_prop *>(
	        control->GetSection("mt32"));
	assert(section);

	// Get potential ROM directories from the environment and/or system
	auto rom_dirs = get_rom_dirs();

	// Get the user's configured ROM directory; otherwise use 'mt32-roms'
	std::string selected_romdir = section->Get_string("romdir");
	if (selected_romdir.empty()) // already trimmed
		selected_romdir = "mt32-roms";
	if (selected_romdir.back() != '/' && selected_romdir.back() != '\\')
		selected_romdir += CROSS_FILESPLIT;

	// Make sure we search the user's configured directory first
	rom_dirs.emplace_front(CROSS_ResolveHome((selected_romdir)));
	return rom_dirs;
}

static const char *get_selected_model()
{
	const auto section = static_cast<Section_prop *>(control->GetSection("mt32"));
	assert(section);
	return section->Get_string("model");
}

static std::set<const LASynthModel *> has_models(const MidiHandler_mt32::service_t &service,
                                                 const std::string &dir)
{
	std::set<const LASynthModel *> models = {};
	for (const auto &model : all_models)
		if (model->InDir(service, dir))
			models.insert(model);
	return models;
}

static std::string load_model(const MidiHandler_mt32::service_t &service,
                              const std::string &selected_model,
                              const std::deque<std::string> &rom_dirs)
{
	const bool is_auto = (selected_model == "auto");
	for (const auto &model : all_models)
		if (is_auto || model->Matches(selected_model))
			for (const auto &dir : rom_dirs)
				if (model->Load(service, dir))
					return dir;
	return "";
}

static mt32emu_report_handler_i get_report_handler_interface()
{
	class ReportHandler {
	public:
		static mt32emu_report_handler_version getReportHandlerVersionID(mt32emu_report_handler_i)
		{
			return MT32EMU_REPORT_HANDLER_VERSION_0;
		}

		static void printDebug(MAYBE_UNUSED void *instance_data,
		                       const char *fmt,
		                       va_list list)
		{
			char msg[1024];
			safe_sprintf(msg, fmt, list);
			DEBUG_LOG_MSG("MT32: %s", msg);
		}

		static void onErrorControlROM(void *)
		{
			LOG_MSG("MT32: Couldn't open Control ROM file");
		}

		static void onErrorPCMROM(void *)
		{
			LOG_MSG("MT32: Couldn't open PCM ROM file");
		}

		static void showLCDMessage(void *, const char *message)
		{
			LOG_MSG("MT32: LCD-Message: %s", message);
		}
	};

	static const mt32emu_report_handler_i_v0 REPORT_HANDLER_V0_IMPL = {
	        ReportHandler::getReportHandlerVersionID,
	        ReportHandler::printDebug,
	        ReportHandler::onErrorControlROM,
	        ReportHandler::onErrorPCMROM,
	        ReportHandler::showLCDMessage,
	        nullptr, // explicit empty function pointers
	        nullptr,
	        nullptr,
	        nullptr,
	        nullptr,
	        nullptr,
	        nullptr,
	        nullptr,
	        nullptr,
	        nullptr};

	static const mt32emu_report_handler_i REPORT_HANDLER_I = {
	        &REPORT_HANDLER_V0_IMPL};

	return REPORT_HANDLER_I;
}

MidiHandler_mt32::MidiHandler_mt32()
        : soft_limiter("MT32"),
          keep_rendering(false)
{}

MidiHandler_mt32::service_t MidiHandler_mt32::GetService()
{
	service_t mt32_service = std::make_unique<MT32Emu::Service>();
	// Has libmt32emu already created a context?
	if (!mt32_service->getContext())
		mt32_service->createContext(get_report_handler_interface(), this);
	return mt32_service;
}

// Calculates the maximum width available to print the rom directory, given
// the terminal's width, indent size, and space needed for the model names:
// [indent][max_dir_width][N columns + N delimeters]
static size_t get_max_dir_width(const LASynthModel *(&models_without_aliases)[10],
                                const char *indent,
                                const char *column_delim)
{
	const size_t column_delim_width = strlen(column_delim);
	size_t header_width = strlen(indent);
	for (const auto &model : models_without_aliases)
		header_width += strlen(model->GetVersion()) + column_delim_width;

	const size_t term_width = INT10_GetTextColumns() - column_delim_width;
	assert(term_width > header_width);
	const auto max_dir_width = term_width - header_width;
	return max_dir_width;
}

// Returns the set of models supported by all of the directories, and also
// populates the provide map of the modules supported by each directory.
static std::set<const LASynthModel *> populate_available_models(
        const MidiHandler_mt32::service_t &service,
        std::map<std::string, std::set<const LASynthModel *>> &dirs_with_models)
{
	std::set<const LASynthModel *> available_models;
	for (const std::string &dir : get_selected_dirs()) {
		const auto models = has_models(service, dir);
		if (!models.empty()) {
			dirs_with_models[dir] = models;
			available_models.insert(models.begin(), models.end());
		}
	}
	return available_models;
}

// Prints a table of directories and supported models. Models are printed
// across the first row and directories are printed down the left column.
// Long directories are truncated and model versions are used to avoid text
// wrapping.
MIDI_RC MidiHandler_mt32::ListAll(Program *caller)
{
	// Table layout constants
	constexpr char column_delim[] = " ";
	constexpr char indent[] = "  ";
	constexpr char trailing_dots[] = "..";
	const auto delim_width = strlen(column_delim);

	const LASynthModel *models_without_aliases[] = {
	        &cm32l_any_model, &cm32l_102_model, &cm32l_100_model,
	        &mt32_any_model,  &mt32_107_model,  &mt32_106_model,
	        &mt32_105_model,  &mt32_104_model,  &mt32_bluer_model,
	        &mt32_204_model};

	const size_t max_dir_width = get_max_dir_width(models_without_aliases,
	                                               indent, column_delim);
	const size_t truncated_dir_width = max_dir_width - strlen(trailing_dots);
	assert(truncated_dir_width < max_dir_width);

	// Get the set of directories and the models they support
	std::map<std::string, std::set<const LASynthModel *>> dirs_with_models;
	const auto available_models = populate_available_models(GetService(),
	                                                        dirs_with_models);
	if (available_models.empty()) {
		caller->WriteOut("  No supported models present.\n");
		return MIDI_RC::OK;
	}

	// Text colors and highlight selector
	constexpr char gray[] = "\033[30;1m";
	constexpr char green[] = "\033[32;1m";
	constexpr char nocolor[] = "\033[0m";
	auto get_highlight = [&](bool is_missing, bool is_matching) {
		return is_missing ? gray : is_matching ? green : nocolor;
	};

	int num_matches = 0;
	const std::string selected_model = get_selected_model();
	auto first_match = [&](const LASynthModel *model) {
		return model->Matches(selected_model) && !num_matches++;
	};

	// Print the header row of all models
	const std::string dirs_padding(max_dir_width, ' ');
	caller->WriteOut("%s%s", indent, dirs_padding.c_str());
	for (const auto &model : models_without_aliases) {
		const bool is_missing = (available_models.find(model) == available_models.end());
		const bool is_first_match = !is_missing && first_match(model);
		const auto color = get_highlight(is_missing, is_first_match);
		caller->WriteOut("%s%s%s%s", color, model->GetVersion(),
		                 nocolor, column_delim);
	}
	caller->WriteOut("\n");

	// Iterate over the found directories and models
	num_matches = 0;
	for (const auto &dir_and_models : dirs_with_models) {
		const std::string &dir = dir_and_models.first;
		const auto &dir_models = dir_and_models.second;

		// Print the directory, and truncate it if it's too long
		if (dir.size() > max_dir_width) {
			const auto truncated_dir = dir.substr(0, truncated_dir_width);
			caller->WriteOut("%s%s%s", indent, truncated_dir.c_str(), trailing_dots);
		}
		// Otherwise print the directory with padding
		else {
			const auto pad_width = max_dir_width - dir.size();
			const auto dir_padding = std::string(pad_width, ' ');
			caller->WriteOut("%s%s%s", indent, dir.c_str(), dir_padding.c_str());
		}
		// Print an indicator if the directory has the model
		for (const auto &model : models_without_aliases) {
			const auto column_width = strlen(model->GetVersion());
			std::string textbox(column_width + delim_width, ' ');
			assert(textbox.size() > 2);
			const size_t text_center = (textbox.size() / 2) - 1;

			const bool is_missing = (dir_models.find(model) == dir_models.end());
			const bool is_first_match = !is_missing && first_match(model);
			textbox[text_center] = is_missing ? '-' : 'y';
			const auto color = get_highlight(is_missing, is_first_match);
			caller->WriteOut("%s%s%s", color, textbox.c_str(), nocolor);
		}
		caller->WriteOut("\n");
	}
	return MIDI_RC::OK;
}

bool MidiHandler_mt32::Open(MAYBE_UNUSED const char *conf)
{
	Close();

	service_t mt32_service = GetService();
	const std::string selected_model = get_selected_model();
	const auto rom_dirs = get_selected_dirs();

	// Load the selected model and print info about it
	const auto found_in = load_model(mt32_service, selected_model, rom_dirs);
	if (found_in.empty()) {
		LOG_MSG("MT32: Failed to find ROMs for model %s in:",
		        selected_model.c_str());
		for (const auto &dir : rom_dirs) {
			const char div = (dir != rom_dirs.back() ? '|' : '`');
			LOG_MSG("MT32:  %c- %s", div, dir.c_str());
		}
		return false;
	}
	mt32emu_rom_info rom_info;
	mt32_service->getROMInfo(&rom_info);
	LOG_MSG("MT32: Initialized %s from %s",
	        rom_info.control_rom_description, found_in.c_str());

	const auto mixer_callback = std::bind(&MidiHandler_mt32::MixerCallBack,
	                                      this, std::placeholders::_1);
	channel_t mixer_channel(MIXER_AddChannel(mixer_callback, 0, "MT32"),
	                        MIXER_DelChannel);

	// Let the mixer command adjust the MT32's services gain-level
	const auto set_mixer_level = std::bind(&MidiHandler_mt32::SetMixerLevel,
	                                       this, std::placeholders::_1);
	mixer_channel->RegisterLevelCallBack(set_mixer_level);

	const auto sample_rate = mixer_channel->GetSampleRate();

	mt32_service->setAnalogOutputMode(ANALOG_MODE);
	mt32_service->selectRendererType(RENDERING_TYPE);
	mt32_service->setStereoOutputSampleRate(sample_rate);
	mt32_service->setSamplerateConversionQuality(RATE_CONVERSION_QUALITY);
	mt32_service->setDACInputMode(DAC_MODE);
	mt32_service->setNiceAmpRampEnabled(USE_NICE_RAMP);
	mt32_service->setNicePanningEnabled(USE_NICE_PANNING);
	mt32_service->setNicePartialMixingEnabled(USE_NICE_PARTIAL_MIXING);

	const auto rc = mt32_service->openSynth();
	if (rc != MT32EMU_RC_OK) {
		LOG_MSG("MT32: Error initialising emulation: %i", rc);
		return false;
	}
	service = std::move(mt32_service);
	channel = std::move(mixer_channel);

	// Start rendering audio
	keep_rendering = true;
	const auto render = std::bind(&MidiHandler_mt32::Render, this);
	renderer = std::thread(render);
	set_thread_name(renderer, "dosbox:mt32");
	play_buffer = playable.Dequeue(); // populate the first play buffer

	// Start playback
	channel->Enable(true);
	is_open = true;
	return true;
}

MidiHandler_mt32::~MidiHandler_mt32()
{
	Close();
}

// When the user runs "mixer MT32 <percent-left>:<percent-right>", this function
// get those percents as floating point ratios (100% being 1.0f). Instead of
// post-scaling the rendered integer stream in the mixer, we instead provide the
// desired floating point scalar to the MT32 service via its gain() interface
// where it can more elegantly adjust the level of the synthesis.

// Another nuance is that MT32's gain interface takes in a single float, but
// DOSBox's mixer accept left-and-right, so we apply gain using the larger of
// the two and then use the limiter's left-right ratios to scale down by lesser
// ratio.
void MidiHandler_mt32::SetMixerLevel(const AudioFrame &levels) noexcept
{
	const float gain = std::max(levels.left, levels.right);
	{
		const std::lock_guard<std::mutex> lock(service_mutex);
		if (service)
			service->setOutputGain(gain);
	}

	const AudioFrame desired = {levels.left / gain, levels.right / gain};
	// mt32emu generates floats between -1 and 1, so we ask the
	// soft limiter to scale these up to the INT16 range
	soft_limiter.UpdateLevels(desired, INT16_MAX);
}

void MidiHandler_mt32::Close()
{
	if (!is_open)
		return;

	// Stop playback
	if (channel)
		channel->Enable(false);

	// Stop rendering and drain the rings
	keep_rendering = false;
	if (!backstock.Size())
		backstock.Enqueue(std::move(play_buffer));
	while (playable.Size())
		play_buffer = playable.Dequeue();

	// Wait for the rendering thread to finish
	if (renderer.joinable())
		renderer.join();

	// Stop the synthesizer
	if (service) {
		service->closeSynth();
		service->freeContext();
	}

	soft_limiter.PrintStats();

	// Reset the members
	channel.reset();
	service.reset();
	soft_limiter.Reset();
	total_buffers_played = 0;
	last_played_frame = 0;

	is_open = false;
}

uint32_t MidiHandler_mt32::GetMidiEventTimestamp() const
{
	const uint32_t played_frames = total_buffers_played * FRAMES_PER_BUFFER;
	return service->convertOutputToSynthTimestamp(played_frames +
	                                              last_played_frame);
}

void MidiHandler_mt32::PlayMsg(const uint8_t *msg)
{
	const auto msg_words = reinterpret_cast<const uint32_t *>(msg);
	const std::lock_guard<std::mutex> lock(service_mutex);
	service->playMsgAt(SDL_SwapLE32(*msg_words), GetMidiEventTimestamp());
}

void MidiHandler_mt32::PlaySysex(uint8_t *sysex, size_t len)
{
	assert(len <= UINT32_MAX);
	const auto msg_len = static_cast<uint32_t>(len);
	const std::lock_guard<std::mutex> lock(service_mutex);
	service->playSysexAt(sysex, msg_len, GetMidiEventTimestamp());
}

// The callback operates at the frame-level, steadily adding samples to the
// mixer until the requested numbers of frames is met.
void MidiHandler_mt32::MixerCallBack(uint16_t requested_frames)
{
	while (requested_frames) {
		const auto frames_to_be_played = std::min(GetRemainingFrames(),
		                                          requested_frames);
		const auto sample_offset_in_buffer = play_buffer.data() +
		                                     last_played_frame * 2;
		channel->AddSamples_s16(frames_to_be_played, sample_offset_in_buffer);
		requested_frames -= frames_to_be_played;
		last_played_frame += frames_to_be_played;
	}
}

// Returns the number of frames left to play in the buffer.
uint16_t MidiHandler_mt32::GetRemainingFrames()
{
	// If the current buffer has some frames left, then return those ...
	if (last_played_frame < FRAMES_PER_BUFFER)
		return FRAMES_PER_BUFFER - last_played_frame;

	// Otherwise put the spent buffer in backstock and get the next buffer
	backstock.Enqueue(std::move(play_buffer));
	play_buffer = playable.Dequeue();
	total_buffers_played++;
	last_played_frame = 0; // reset the frame counter to the beginning

	return FRAMES_PER_BUFFER;
}

// Keep the playable queue populated with freshly rendered buffers
void MidiHandler_mt32::Render()
{
	// Allocate our buffers once and reuse for the duration.
	constexpr auto SAMPLES_PER_BUFFER = FRAMES_PER_BUFFER * 2; // L & R
	std::vector<float> render_buffer(SAMPLES_PER_BUFFER);
	std::vector<int16_t> playable_buffer(SAMPLES_PER_BUFFER);

	// Populate the backstock using copies of the current buffer.
	while (backstock.Size() < backstock.MaxCapacity() - 1)
		backstock.Enqueue(playable_buffer);
	backstock.Enqueue(std::move(playable_buffer));
	assert(backstock.Size() == backstock.MaxCapacity());

	while (keep_rendering.load()) {
		{
			const std::lock_guard<std::mutex> lock(service_mutex);
			service->renderFloat(render_buffer.data(), FRAMES_PER_BUFFER);
		}
		// Grab the next buffer from backstock and populate it ...
		playable_buffer = backstock.Dequeue();
		soft_limiter.Process(render_buffer, FRAMES_PER_BUFFER, playable_buffer);

		// and then move it into the playable queue
		playable.Enqueue(std::move(playable_buffer));
	}
}

static void mt32_init(MAYBE_UNUSED Section *sec)
{}

void MT32_AddConfigSection(Config *conf)
{
	assert(conf);
	Section_prop *sec_prop = conf->AddSection_prop("mt32", &mt32_init);
	assert(sec_prop);
	init_mt32_dosbox_settings(*sec_prop);
}

#endif // C_MT32EMU
