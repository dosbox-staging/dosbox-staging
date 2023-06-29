/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
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

#include "../ints/int10.h"
#include "control.h"
#include "cross.h"
#include "fs_utils.h"
#include "math_utils.h"
#include "midi.h"
#include "midi_lasynth_model.h"
#include "mixer.h"
#include "pic.h"
#include "string_utils.h"
#include "support.h"

// mt32emu Settings
// ----------------

// Analogue circuit modes: DIGITAL_ONLY, COARSE, ACCURATE, OVERSAMPLED
constexpr auto ANALOG_MODE = MT32Emu::AnalogOutputMode_ACCURATE;

// DAC Emulation modes: NICE, PURE, GENERATION1, and GENERATION2
constexpr auto DAC_MODE = MT32Emu::DACInputMode_NICE;

// Analog rendering types: int16_t, FLOAT
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

const Rom mt32_pcm_100_f  = {"pcm_mt32", LASynthModel::ROM_TYPE::PCM};
const Rom mt32_pcm_100_l  = {"pcm_mt32_l", LASynthModel::ROM_TYPE::PCM};
const Rom mt32_pcm_100_h  = {"pcm_mt32_h", LASynthModel::ROM_TYPE::PCM};
const Rom mt32_ctrl_104_f = {"ctrl_mt32_1_04", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_104_a = {"ctrl_mt32_1_04_a", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_104_b = {"ctrl_mt32_1_04_b", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_105_f = {"ctrl_mt32_1_05", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_105_a = {"ctrl_mt32_1_05_a", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_105_b = {"ctrl_mt32_1_05_b", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_106_f = {"ctrl_mt32_1_06", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_106_a = {"ctrl_mt32_1_06_a", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_106_b = {"ctrl_mt32_1_06_b", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_107_f = {"ctrl_mt32_1_07", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_107_a = {"ctrl_mt32_1_07_a", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_107_b = {"ctrl_mt32_1_07_b", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_bluer_f = {"ctrl_mt32_bluer", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_bluer_a = {"ctrl_mt32_bluer_a", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_bluer_b = {"ctrl_mt32_bluer_b", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_203_f = {"ctrl_mt32_2_03", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_204_f = {"ctrl_mt32_2_04", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_206_f = {"ctrl_mt32_2_06", LASynthModel::ROM_TYPE::CONTROL};
const Rom mt32_ctrl_207_f = {"ctrl_mt32_2_07", LASynthModel::ROM_TYPE::CONTROL};
const Rom cm32l_ctrl_100_f = {"ctrl_cm32l_1_00", LASynthModel::ROM_TYPE::CONTROL};
const Rom cm32l_ctrl_102_f = {"ctrl_cm32l_1_02", LASynthModel::ROM_TYPE::CONTROL};
const Rom cm32ln_ctrl_100_f = {"ctrl_cm32ln_1_00", LASynthModel::ROM_TYPE::CONTROL};
const Rom cm32l_pcm_100_f = {"pcm_cm32l", LASynthModel::ROM_TYPE::PCM};
const Rom cm32l_pcm_100_h  = {"pcm_cm32l_h", LASynthModel::ROM_TYPE::PCM};
const Rom& cm32l_pcm_100_l = mt32_pcm_100_f; // Lower half of samples comes from
                                             // MT-32

// Roland LA Models (composed of ROMs)
const LASynthModel mt32_104_model   = {"mt32_104",
                                       &mt32_pcm_100_f,
                                       &mt32_pcm_100_l,
                                       &mt32_pcm_100_h,
                                       &mt32_ctrl_104_f,
                                       &mt32_ctrl_104_a,
                                       &mt32_ctrl_104_b};
const LASynthModel mt32_105_model   = {"mt32_105",
                                       &mt32_pcm_100_f,
                                       &mt32_pcm_100_l,
                                       &mt32_pcm_100_h,
                                       &mt32_ctrl_105_f,
                                       &mt32_ctrl_105_a,
                                       &mt32_ctrl_105_b};
const LASynthModel mt32_106_model   = {"mt32_106",
                                       &mt32_pcm_100_f,
                                       &mt32_pcm_100_l,
                                       &mt32_pcm_100_h,
                                       &mt32_ctrl_106_f,
                                       &mt32_ctrl_106_a,
                                       &mt32_ctrl_106_b};
const LASynthModel mt32_107_model   = {"mt32_107",
                                       &mt32_pcm_100_f,
                                       &mt32_pcm_100_l,
                                       &mt32_pcm_100_h,
                                       &mt32_ctrl_107_f,
                                       &mt32_ctrl_107_a,
                                       &mt32_ctrl_107_b};
const LASynthModel mt32_bluer_model = {"mt32_bluer",
                                       &mt32_pcm_100_f,
                                       &mt32_pcm_100_l,
                                       &mt32_pcm_100_h,
                                       &mt32_ctrl_bluer_f,
                                       &mt32_ctrl_bluer_a,
                                       &mt32_ctrl_bluer_b};
const LASynthModel mt32_203_model   = {"mt32_203",
                                       &mt32_pcm_100_f,
                                       &mt32_pcm_100_l,
                                       &mt32_pcm_100_h,
                                       &mt32_ctrl_203_f,
                                       nullptr,
                                       nullptr};
const LASynthModel mt32_204_model   = {"mt32_204",
                                       &mt32_pcm_100_f,
                                       &mt32_pcm_100_l,
                                       &mt32_pcm_100_h,
                                       &mt32_ctrl_204_f,
                                       nullptr,
                                       nullptr};
const LASynthModel mt32_206_model   = {"mt32_206",
                                       &mt32_pcm_100_f,
                                       &mt32_pcm_100_l,
                                       &mt32_pcm_100_h,
                                       &mt32_ctrl_206_f,
                                       nullptr,
                                       nullptr};
const LASynthModel mt32_207_model   = {"mt32_207",
                                       &mt32_pcm_100_f,
                                       &mt32_pcm_100_l,
                                       &mt32_pcm_100_h,
                                       &mt32_ctrl_207_f,
                                       nullptr,
                                       nullptr};
const LASynthModel cm32l_100_model  = {"cm32l_100",
                                       &cm32l_pcm_100_f,
                                       &cm32l_pcm_100_l,
                                       &cm32l_pcm_100_h,
                                       &cm32l_ctrl_100_f,
                                       nullptr,
                                       nullptr};
const LASynthModel cm32l_102_model = {
        "cm32l_102",       &cm32l_pcm_100_f, &cm32l_pcm_100_l, &cm32l_pcm_100_h,
        &cm32l_ctrl_102_f, nullptr, nullptr};
const LASynthModel cm32ln_100_model = {"cm32ln_100",
                                       &cm32l_pcm_100_f,
                                       &cm32l_pcm_100_l,
                                       &cm32l_pcm_100_h,
                                       &cm32ln_ctrl_100_f,
                                       nullptr,
                                       nullptr};

// Aliased models
const LASynthModel mt32_new_model = {"mt32_new", // new is 2.04
                                     &mt32_pcm_100_f, &mt32_pcm_100_l,
                                     &mt32_pcm_100_h, &mt32_ctrl_204_f,
                                     nullptr,         nullptr};
const LASynthModel mt32_old_model = {"mt32_old", // old is 1.07
                                     &mt32_pcm_100_f,
                                     &mt32_pcm_100_l,
                                     &mt32_pcm_100_h,
                                     &mt32_ctrl_107_f,
                                     &mt32_ctrl_107_a,
                                     &mt32_ctrl_107_b};

// In order that "model = auto" will load
const LASynthModel* all_models[] = {&cm32ln_100_model,
                                    &cm32l_102_model,
                                    &cm32l_100_model,
                                    &mt32_old_model,
                                    &mt32_107_model,
                                    &mt32_106_model,
                                    &mt32_105_model,
                                    &mt32_104_model,
                                    &mt32_bluer_model,
                                    &mt32_new_model,
                                    &mt32_204_model,
                                    &mt32_207_model,
                                    &mt32_206_model,
                                    &mt32_203_model};

MidiHandler_mt32 mt32_instance;

static void init_mt32_dosbox_settings(Section_prop &sec_prop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	const char* models[] = {"auto",
	                        cm32ln_100_model.GetName(),
	                        cm32l_102_model.GetName(),
	                        cm32l_100_model.GetName(),
	                        mt32_old_model.GetName(),
	                        mt32_107_model.GetName(),
	                        mt32_106_model.GetName(),
	                        mt32_105_model.GetName(),
	                        mt32_104_model.GetName(),
	                        mt32_bluer_model.GetName(),
	                        mt32_new_model.GetName(),
	                        mt32_204_model.GetName(),
	                        mt32_207_model.GetName(),
	                        mt32_206_model.GetName(),
	                        mt32_203_model.GetName(),
	                        nullptr};
	auto *str_prop       = sec_prop.Add_string("model", when_idle, "auto");
	str_prop->Set_values(models);
	str_prop->Set_help(
	        "MT-32 model to use:\n"
	        "  auto:      Pick the first available model, in order as listed.\n"
	        "  mt32:      Pick the first available MT-32 model, in order as listed.\n"
	        "  cm32l:     Pick the first available CM-32L model, in order as listed.\n"
	        "  mt32_old:  Alias for MT-32 v1.07.\n"
	        "  mt32_new:  Alias for MT-32 v2.04.");

	str_prop = sec_prop.Add_string("romdir", when_idle, "");
	str_prop->Set_help(
	        "The directory containing ROMs for one or more models.\n"
	        "The directory can be absolute or relative, or leave it blank to\n"
	        "use the 'mt32-roms' directory in your DOSBox configuration\n"
	        "directory. Other common system locations will be checked as well.\n"
	        "ROM files inside this directory may include any of the following:\n"
	        "  - MT32_CONTROL.ROM and MT32_PCM.ROM, for the 'mt32' model.\n"
	        "  - CM32L_CONTROL.ROM and CM32L_PCM.ROM, for the 'cm32l' model.\n"
	        "  - Unzipped MAME MT-32 and CM-32L ROMs, for the versioned models.");

	str_prop = sec_prop.Add_string("mt32_filter", when_idle, "off");
	assert(str_prop);
	str_prop->Set_help(
	        "Filter for the Roland MT-32/CM-32L audio output:\n"
	        "  off:       Don't filter the output (default).\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

static void register_mt32_text_messages()
{
	MSG_Add("MT32_NO_SUPPORTED_MODELS", "No supported models present.");

	MSG_Add("MT32_ROM_NOT_LOADED", "No ROM is currently loaded");

	MSG_Add("MT32_INVENTORY_TABLE_MISSING_LETTER", "-");
	MSG_Add("MT32_INVENTORY_TABLE_AVAILABLE_LETTER", "y");

	// Translators can line up the colons or use different punctuation
	MSG_Add("MT32_ACTIVE_ROM_LABEL", "Active ROM  : ");
	MSG_Add("MT32_SOURCE_DIR_LABEL", "Loaded From : ");
}

#	if defined(WIN32)

static std::deque<std_fs::path> get_rom_dirs()
{
	return {
	        get_platform_config_dir() / "mt32-roms",
	        "C:\\mt32-rom-data\\",
	};
}

#elif defined(MACOSX)

static std::deque<std_fs::path> get_rom_dirs()
{
	return {
	        get_platform_config_dir() / "mt32-roms",
	        resolve_home("~/Library/Audio/Sounds/MT32-Roms/"),
	        "/usr/local/share/mt32-rom-data/",
	        "/usr/share/mt32-rom-data/",
	};
}

#else

static std::deque<std_fs::path> get_rom_dirs()
{
	// First priority is user-specific data location
	const auto xdg_data_home = get_xdg_data_home();

	std::deque<std_fs::path> dirs = {
	        xdg_data_home / "dosbox/mt32-roms",
	        xdg_data_home / "mt32-rom-data",
	};

	// Second priority are the $XDG_DATA_DIRS
	for (const auto& data_dir : get_xdg_data_dirs()) {
		dirs.emplace_back(data_dir / "mt32-rom-data");
	}

	// Third priority is $XDG_CONF_HOME, for convenience
	dirs.emplace_back(get_platform_config_dir() / "mt32-roms");

	return dirs;
}

#endif

static std::deque<std_fs::path> get_selected_dirs()
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
	rom_dirs.emplace_front(resolve_home(selected_romdir));
	return rom_dirs;
}

static const char *get_selected_model()
{
	const auto section = static_cast<Section_prop *>(control->GetSection("mt32"));
	assert(section);
	return section->Get_string("model");
}

static std::set<const LASynthModel*> has_models(const MidiHandler_mt32::service_t& service,
                                                const std_fs::path& dir)
{
	std::set<const LASynthModel *> models = {};
	for (const auto &model : all_models)
		if (model->InDir(service, dir))
			models.insert(model);
	return models;
}

static std::optional<model_and_dir_t> load_model(
        const MidiHandler_mt32::service_t& service,
        const std::string& selected_model, const std::deque<std_fs::path>& rom_dirs)
{
	const bool is_auto = (selected_model == "auto");
	for (const auto &model : all_models)
		if (is_auto || model->Matches(selected_model))
			for (const auto &dir : rom_dirs)
				if (model->Load(service, dir))
					return {{model, simplify_path(dir)}};
	return {};
}

static mt32emu_report_handler_i get_report_handler_interface()
{
	class ReportHandler {
	public:
		static mt32emu_report_handler_version getReportHandlerVersionID(mt32emu_report_handler_i)
		{
			return MT32EMU_REPORT_HANDLER_VERSION_0;
		}

		static void printDebug([[maybe_unused]] void *instance_data,
		                       const char *fmt,
		                       va_list list)
		{
			char msg[1024];
			safe_sprintf(msg, fmt, list);
			DEBUG_LOG_MSG("MT32: %s", msg);
		}

		static void onErrorControlROM(void *)
		{
			LOG_WARNING("MT32: Couldn't open Control ROM file");
		}

		static void onErrorPCMROM(void *)
		{
			LOG_WARNING("MT32: Couldn't open PCM ROM file");
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

MidiHandler_mt32::service_t MidiHandler_mt32::GetService()
{
	const std::lock_guard<std::mutex> lock(service_mutex);
	service_t mt32_service = std::make_unique<MT32Emu::Service>();
	// Has libmt32emu already created a context?
	if (!mt32_service->getContext())
		mt32_service->createContext(get_report_handler_interface(), this);
	return mt32_service;
}

// Calculates the maximum width available to print the rom directory, given
// the terminal's width, indent size, and space needed for the model names:
// [indent][max_dir_width][N columns + N delimeters]
static size_t get_max_dir_width(const LASynthModel* (&models_without_aliases)[12],
                                const char* indent, const char* column_delim)
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

using dirs_with_models_t = std::map<std_fs::path, std::set<const LASynthModel*>>;

static std::set<const LASynthModel*> populate_available_models(
        const MidiHandler_mt32::service_t& service, dirs_with_models_t& dirs_with_models)
{
	std::set<const LASynthModel *> available_models;
	for (const auto& dir : get_selected_dirs()) {
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

	const LASynthModel* models_without_aliases[] = {&cm32ln_100_model,
	                                                &cm32l_102_model,
	                                                &cm32l_100_model,
	                                                &mt32_107_model,
	                                                &mt32_106_model,
	                                                &mt32_105_model,
	                                                &mt32_104_model,
	                                                &mt32_bluer_model,
	                                                &mt32_204_model,
	                                                &mt32_207_model,
	                                                &mt32_206_model,
	                                                &mt32_203_model};

	const size_t max_dir_width = get_max_dir_width(models_without_aliases,
	                                               indent, column_delim);
	const size_t truncated_dir_width = max_dir_width - strlen(trailing_dots);
	assert(truncated_dir_width < max_dir_width);

	// Get the set of directories and the models they support
	dirs_with_models_t dirs_with_models;
	const auto available_models = populate_available_models(GetService(),
	                                                        dirs_with_models);
	if (available_models.empty()) {
		caller->WriteOut("%s%s\n", indent, MSG_Get("MT32_NO_SUPPORTED_MODELS"));
		return MIDI_RC::OK;
	}

	// Text colors and highlight selector
	constexpr char gray[]    = "\033[30;1m";
	constexpr char nocolor[] = "\033[0m";

	auto get_highlight = [&](bool is_missing) {
		return is_missing ? gray : nocolor;
	};

	// Print the header row of all models
	const std::string dirs_padding(max_dir_width, ' ');
	caller->WriteOut("%s%s", indent, dirs_padding.c_str());
	for (const auto &model : models_without_aliases) {
		const bool is_missing = (available_models.find(model) == available_models.end());
		const auto color = get_highlight(is_missing);
		caller->WriteOut("%s%s%s%s", color, model->GetVersion(),
		                 nocolor, column_delim);
	}
	caller->WriteOut("\n");

	// Get the single-character inventory presence indicators
	const std::string_view missing_indicator = MSG_Get(
	        "MT32_INVENTORY_TABLE_MISSING_LETTER");
	const std::string_view available_indicator = MSG_Get(
	        "MT32_INVENTORY_TABLE_AVAILABLE_LETTER");

	// Iterate over the found directories and models
	for (const auto &dir_and_models : dirs_with_models) {
		const auto dir = simplify_path(dir_and_models.first).string();

		const auto& dir_models = dir_and_models.second;

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

			const bool is_missing = (dir_models.find(model) ==
			                         dir_models.end());

			const auto indicator = is_missing ? missing_indicator
			                                  : available_indicator;

			const auto max_indicator_width = std::min(
			        indicator.length(), textbox.length() - text_center);

			textbox.replace(text_center, max_indicator_width, indicator);

			const auto color = get_highlight(is_missing);
			caller->WriteOut("%s%s%s", color, textbox.c_str(), nocolor);
		}
		caller->WriteOut("\n");
	}

	caller->WriteOut("%s---\n", indent);

	if (model_and_dir && service) {
		// Print the loaded ROM version
		mt32emu_rom_info rom_info = {};
		{
			// Request exclusive access prior to getting ROM info
			const std::lock_guard<std::mutex> lock(service_mutex);
			service->getROMInfo(&rom_info);
		}
		caller->WriteOut("%s%s%s (%s)\n",
		                 indent,
		                 MSG_Get("MT32_ACTIVE_ROM_LABEL"),
		                 model_and_dir->first->GetName(),
		                 rom_info.control_rom_description);

		// Print the loaded ROM's directory
		const std::string_view dir_label = MSG_Get("MT32_SOURCE_DIR_LABEL");

		const auto dir_max_length = INT10_GetTextColumns() -
		                            (dir_label.length() +
		                             std::string_view(indent).length());

		const auto truncated_dir =
		        model_and_dir->second.string().substr(0, dir_max_length);
		caller->WriteOut("%s%s%s\n",
		                 indent,
		                 dir_label.data(),
		                 truncated_dir.c_str());
	} else {
		caller->WriteOut("%s%s\n", indent, MSG_Get("MT32_ROM_NOT_LOADED"));
	}
	return MIDI_RC::OK;
}

bool MidiHandler_mt32::Open([[maybe_unused]] const char *conf)
{
	Close();

	service_t mt32_service = GetService();
	const std::string selected_model = get_selected_model();
	const auto rom_dirs = get_selected_dirs();

	// Load the selected model and print info about it
	auto loaded_model_and_dir = load_model(mt32_service, selected_model, rom_dirs);
	if (!loaded_model_and_dir) {
		LOG_WARNING("MT32: Failed to find ROMs for model %s in:",
		        selected_model.c_str());
		for (const auto& dir : rom_dirs) {
			const char div = (dir != rom_dirs.back() ? '|' : '`');
			LOG_MSG("MT32:  %c- %s", div, dir.string().c_str());
		}
		return false;
	}
	mt32emu_rom_info rom_info;
	mt32_service->getROMInfo(&rom_info);

	assert(loaded_model_and_dir.has_value());
	LOG_MSG("MT32: Initialised %s from %s",
	        rom_info.control_rom_description,
	        loaded_model_and_dir->second.string().c_str());

	const auto audio_frame_rate_hz = MIXER_GetSampleRate();
	ms_per_audio_frame             = millis_in_second / audio_frame_rate_hz;

	mt32_service->setAnalogOutputMode(ANALOG_MODE);
	mt32_service->selectRendererType(RENDERING_TYPE);
	mt32_service->setStereoOutputSampleRate(audio_frame_rate_hz);
	mt32_service->setSamplerateConversionQuality(RATE_CONVERSION_QUALITY);
	mt32_service->setDACInputMode(DAC_MODE);
	mt32_service->setNiceAmpRampEnabled(USE_NICE_RAMP);
	mt32_service->setNicePanningEnabled(USE_NICE_PANNING);
	mt32_service->setNicePartialMixingEnabled(USE_NICE_PARTIAL_MIXING);

	const auto rc = mt32_service->openSynth();
	if (rc != MT32EMU_RC_OK) {
		LOG_WARNING("MT32: Error initialising emulation: %i", rc);
		return false;
	}

	const auto mixer_callback = std::bind(&MidiHandler_mt32::MixerCallBack,
	                                      this,
	                                      std::placeholders::_1);

	auto mixer_channel = MIXER_AddChannel(mixer_callback,
	                                      audio_frame_rate_hz,
	                                      "MT32",
	                                      {ChannelFeature::Sleep,
	                                       ChannelFeature::Stereo,
	                                       ChannelFeature::Synthesizer});

	// libmt32emu renders float audio frames between -1.0f and +1.0f, so we
	// ask the channel to scale all the samples up to it's 0db level.
	mixer_channel->Set0dbScalar(MAX_AUDIO);

	const auto section = static_cast<Section_prop *>(control->GetSection("mt32"));
	assert(section);

	const std::string filter_prefs = section->Get_string("mt32_filter");
	//
	if (!mixer_channel->TryParseAndSetCustomFilter(filter_prefs)) {
		if (filter_prefs != "off")
			LOG_WARNING("MT32: Invalid 'mt32_filter' value: '%s', using 'off'",
			            filter_prefs.c_str());

		mixer_channel->SetHighPassFilter(FilterState::Off);
		mixer_channel->SetLowPassFilter(FilterState::Off);
	}

	// Double the baseline PCM prebuffer because MIDI is demanding and
	// bursty. The Mixer's default of ~20 ms becomes 40 ms here, which gives
	// slower systems a better to keep up (and prevent their audio frame
	// FIFO from running dry).
	const auto render_ahead_ms = MIXER_GetPreBufferMs() * 2;

	// Size the out-bound audio frame FIFO
	assert(audio_frame_rate_hz > 8000); // sane lower-bound of 8 KHz
	const auto audio_frames_per_ms = iround(audio_frame_rate_hz / millis_in_second);
	audio_frame_fifo.Resize(
	        check_cast<size_t>(render_ahead_ms * audio_frames_per_ms));

	// Size the in-bound work FIFO

	// MIDI has a Baud rate of 31250; at optimum this is 31250 bits per
	// second. A MIDI byte is 8 bits plus a start and stop bit, and each
	// MIDI message is three bytes, which gives a total of 30 bits per
	// message. This means that under optimal conditions, a maximum of 1042
	// messages per second can be obtained via > the MIDI protocol.

	// We have measured DOS games sending hundreds of MIDI messages within a
	// short handful of millseconds, so a safe but very generous upper bound
	// is used (Note: the actual memory used by the FIFO is incremental
	// based on actual usage).
	static constexpr uint16_t midi_spec_max_msg_rate_hz = 1042;
	work_fifo.Resize(midi_spec_max_msg_rate_hz * 10);

	// If we haven't failed yet, then we're ready to begin so move the local
	// objects into the member variables.
	service = std::move(mt32_service);
	channel = std::move(mixer_channel);
	model_and_dir = std::move(loaded_model_and_dir);

	// Start rendering audio
	const auto render = std::bind(&MidiHandler_mt32::Render, this);
	renderer = std::thread(render);
	set_thread_name(renderer, "dosbox:mt32");

	is_open = true;
	return true;
}

MidiHandler_mt32::~MidiHandler_mt32()
{
	Close();
}

void MidiHandler_mt32::Close()
{
	if (!is_open)
		return;

	LOG_MSG("MT32: Shutting down");

	if (had_underruns) {
		LOG_WARNING("MT32: Fix underruns by lowering CPU load "
		            "or increasing your conf's prebuffer");
		had_underruns = false;
	}

	// Stop playback
	if (channel)
		channel->Enable(false);

	// Stop queueing new MIDI work and audio frames
	work_fifo.Stop();
	audio_frame_fifo.Stop();

	// Wait for the rendering thread to finish
	if (renderer.joinable())
		renderer.join();

	// Stop the synthesizer
	if (service) {
		const std::lock_guard<std::mutex> lock(service_mutex);
		service->closeSynth();
		service->freeContext();
	}

	// Deregister the mixer channel and remove it
	assert(channel);
	MIXER_DeregisterChannel(channel);
	channel.reset();

	// Reset the members
	service.reset();

	last_rendered_ms = 0.0;
	ms_per_audio_frame = 0.0;

	is_open          = false;
}

uint16_t MidiHandler_mt32::GetNumPendingAudioFrames()
{
	const auto now_ms = PIC_FullIndex();

	// Wake up the channel and update the last rendered time datum.
	assert(channel);
	if (channel->WakeUp()) {
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
	return check_cast<uint16_t>(num_audio_frames);
}

// The request to play the channel message is placed in the MIDI work FIFO
void MidiHandler_mt32::PlayMsg(const MidiMessage& msg)
{
	std::vector<uint8_t> message(msg.data.begin(), msg.data.end());
	MidiWork work{std::move(message),
	              GetNumPendingAudioFrames(),
	              MessageType::Channel};
	work_fifo.Enqueue(std::move(work));
}

// The request to play the sysex message is placed in the MIDI work FIFO
void MidiHandler_mt32::PlaySysex(uint8_t *sysex, size_t len)
{
	std::vector<uint8_t> message(sysex, sysex + len);
	MidiWork work{std::move(message), GetNumPendingAudioFrames(), MessageType::SysEx};
	work_fifo.Enqueue(std::move(work));
}

// The callback operates at the audio frame-level, steadily adding samples to
// the mixer until the requested numbers of audio frames is met.
void MidiHandler_mt32::MixerCallBack(const uint16_t requested_audio_frames)
{
	assert(channel);

	// Report buffer underruns
	constexpr auto warning_percent = 5.0f;
	if (const auto percent_full = audio_frame_fifo.GetPercentFull();
	    percent_full < warning_percent) {
		static auto iteration = 0;
		if (iteration++ % 100 == 0) {
			LOG_WARNING("MT32: Audio buffer underrun");
		}
		had_underruns = true;
	}

	static std::vector<AudioFrame> audio_frames = {};

	const auto has_dequeued = audio_frame_fifo.BulkDequeue(audio_frames,
	                                                       requested_audio_frames);

	if (has_dequeued) {
		assert(audio_frames.size() == requested_audio_frames);
		channel->AddSamples_sfloat(requested_audio_frames,
		                           &audio_frames[0][0]);
		last_rendered_ms = PIC_FullIndex();
	} else {
		assert(!audio_frame_fifo.IsRunning());
		channel->AddSilence();
	}
}

void MidiHandler_mt32::RenderAudioFramesToFifo(const uint16_t num_frames)
{
	static std::vector<AudioFrame> audio_frames = {};
	// Maybe expand the vector
	if (audio_frames.size() < num_frames) {
		audio_frames.resize(num_frames);
	}

	std::unique_lock<std::mutex> lock(service_mutex);
	service->renderFloat(&audio_frames[0][0], num_frames);
	lock.unlock();

	audio_frame_fifo.BulkEnqueue(audio_frames, num_frames);
}

// The next MIDI work task is processed, which includes rendering audio frames
// prior to applying channel and sysex messages to the service
void MidiHandler_mt32::ProcessWorkFromFifo()
{
	const auto work = work_fifo.Dequeue();
	if (!work) {
		return;
	}

	/* // Comment-in to log inter-cycle rendering
	if (work.num_pending_audio_frames > 0) {
	        LOG_MSG("MT32: %2u audio frames prior to %s message, followed by"
	                "%2lu more messages. Have %4lu audio frames queued",
	                work.num_pending_audio_frames,
	                work.message_type == MessageType::Channel ? "channel" :
	                "sysex", work_fifo.Size(), audio_frame_fifo.Size());
	}*/

	if (work->num_pending_audio_frames > 0) {
		RenderAudioFramesToFifo(work->num_pending_audio_frames);
	}

	// Request exclusive access prior to applying messages
	const std::lock_guard<std::mutex> lock(service_mutex);

	if (work->message_type == MessageType::Channel) {
		assert(work->message.size() >= MaxMidiMessageLen);
		const auto& data   = work->message.data();
		const uint32_t msg = data[0] + (data[1] << 8) + (data[2] << 16);
		service->playMsg(msg);
	} else {
		assert(work->message_type == MessageType::SysEx);
		service->playSysex(work->message.data(),
		                   static_cast<uint32_t>(work->message.size()));
	}
}

// Keep the fifo populated with freshly rendered buffers
void MidiHandler_mt32::Render()
{
	while (work_fifo.IsRunning()) {
		work_fifo.IsEmpty() ? RenderAudioFramesToFifo()
		                    : ProcessWorkFromFifo();
	}
}

static void mt32_init([[maybe_unused]] Section *sec)
{}

void MT32_AddConfigSection(const config_ptr_t &conf)
{
	assert(conf);
	Section_prop *sec_prop = conf->AddSection_prop("mt32", &mt32_init);

	assert(sec_prop);
	init_mt32_dosbox_settings(*sec_prop);

	register_mt32_text_messages();
}

#endif // C_MT32EMU
