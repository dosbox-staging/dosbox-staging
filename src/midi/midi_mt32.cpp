/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
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
#include <vector>

#include <SDL_endian.h>

#include "../ints/int10.h"
#include "ansi_code_markup.h"
#include "channel_names.h"
#include "control.h"
#include "cross.h"
#include "fs_utils.h"
#include "math_utils.h"
#include "midi.h"
#include "midi_lasynth_model.h"
#include "mixer.h"
#include "mpu401.h"
#include "pic.h"
#include "string_utils.h"
#include "support.h"

// #define DEBUG_MT32

// mt32emu Settings
// ----------------

// Analogue circuit modes: DIGITAL_ONLY, COARSE, ACCURATE, OVERSAMPLED
//
// Accurate mode achieves finer emulation of LPF circuit. Output signal is
// upsampled to 48 kHz to allow emulation of audible mirror spectra above 16
// kHz which is passed through the LPF circuit without significant
// attenuation.
constexpr auto AnalogMode = MT32Emu::AnalogOutputMode_ACCURATE;

constexpr auto AccurateAnalogModeSampleRateHz = 48'000;

// DAC Emulation modes: NICE, PURE, GENERATION1, and GENERATION2
//
// "Nice" mode produces samples at double the volume, without tricks, and
// results in nicer overdrive characteristics than the DAC hacks (it simply
// clips samples within range). Higher quality than the real devices.
constexpr auto DacEmulationMode = MT32Emu::DACInputMode_NICE;

// Analog rendering types: BIT16S, FLOAT
//
// Use 16-bit signed samples in the renderer and the accurate wave generator
// model based on logarithmic fixed-point computations and LUTs. Maximum
// emulation accuracy and speed (it's a lot faster than the FLOAT renderer).
constexpr auto RenderingType = MT32Emu::RendererType_BIT16S;

// In this mode, we want to ensure that amp ramp never jumps to the target
// value and always gradually increases or decreases. It seems that real units
// do not bother to always check if a newly started ramp leads to a jump.
// We also prefer the quality improvement over the emulation accuracy,
// so this mode is enabled by default.
constexpr bool UseNiceRamp = true;

// Despite the Roland's manual specifies allowed panpot values in range 0-14,
// the LA-32 only receives 3-bit pan setting in fact. In particular, this
// makes it impossible to set the "middle" panning for a single partial.
// In the NicePanning mode, we enlarge the pan setting accuracy to 4 bits
// making it smoother thus sacrificing the emulation accuracy.
constexpr bool UseNicePanning = true;

// LA-32 is known to mix partials either in-phase (so that they are added)
// or in counter-phase (so that they are subtracted instead).
// In some cases, this quirk isn't highly desired because a pair of closely
// sounding partials may occasionally cancel out.
// In the NicePartialMixing mode, the mixing is always performed in-phase,
// thus making the behaviour more predictable.
constexpr bool UseNicePartialMixing = true;

// Do not attempt to emulate delays introduced by the slow MIDI transfer
// protocol.
//
// Enabling delay emulation could result in missed MIDI events if the MT-32
// receives a large number of events in quick bursts, causing its internal
// ring buffers to overflow. This can be heard as missed notes and
// wrong sounding instrument (e.g., in the intro tune of Bumpy's Arcade
// Fantasy).
//
// Emulating MIDI protocol induced micro-delays are neither musically
// significant nor desirable. Most importantly, these odd up to 1-3 ms delays
// on some events are not noticeable. Losing MIDI events that potentially set
// up the correct sounds when the game starts is a much more important concern.
//
// While such transfer bursts are most likely an MPU-401 intelligent mode
// emulation bug in DOSBox, disabling the delay mode emulation in libmt32
// effectively fixes the problem and makes the resulting music sound closer to
// the composer's intentions.
//
constexpr auto MidiDelayMode = MT32Emu::MIDIDelayMode_IMMEDIATE;

using Rom = LASynthModel::Rom;

// MT-32
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

const Rom mt32_pcm_100_f = {"pcm_mt32", LASynthModel::ROM_TYPE::PCM};
const Rom mt32_pcm_100_l = {"pcm_mt32_l", LASynthModel::ROM_TYPE::PCM};
const Rom mt32_pcm_100_h = {"pcm_mt32_h", LASynthModel::ROM_TYPE::PCM};

// CM-32L & CM-32LN
const Rom cm32l_ctrl_100_f = {"ctrl_cm32l_1_00", LASynthModel::ROM_TYPE::CONTROL};
const Rom cm32l_ctrl_102_f = {"ctrl_cm32l_1_02", LASynthModel::ROM_TYPE::CONTROL};
const Rom cm32ln_ctrl_100_f = {"ctrl_cm32ln_1_00", LASynthModel::ROM_TYPE::CONTROL};

const Rom cm32l_pcm_100_f = {"pcm_cm32l", LASynthModel::ROM_TYPE::PCM};
const Rom cm32l_pcm_100_h = {"pcm_cm32l_h", LASynthModel::ROM_TYPE::PCM};

// Lower half of samples comes from MT-32
const Rom& cm32l_pcm_100_l = mt32_pcm_100_f;

// Roland LA models (composed of ROMs)
const LASynthModel mt32_104_model = {"mt32_104",
                                     &mt32_pcm_100_f,
                                     &mt32_pcm_100_l,
                                     &mt32_pcm_100_h,
                                     &mt32_ctrl_104_f,
                                     &mt32_ctrl_104_a,
                                     &mt32_ctrl_104_b};

const LASynthModel mt32_105_model = {"mt32_105",
                                     &mt32_pcm_100_f,
                                     &mt32_pcm_100_l,
                                     &mt32_pcm_100_h,
                                     &mt32_ctrl_105_f,
                                     &mt32_ctrl_105_a,
                                     &mt32_ctrl_105_b};

const LASynthModel mt32_106_model = {"mt32_106",
                                     &mt32_pcm_100_f,
                                     &mt32_pcm_100_l,
                                     &mt32_pcm_100_h,
                                     &mt32_ctrl_106_f,
                                     &mt32_ctrl_106_a,
                                     &mt32_ctrl_106_b};

const LASynthModel mt32_107_model = {"mt32_107",
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

const LASynthModel mt32_203_model = {"mt32_203",
                                     &mt32_pcm_100_f,
                                     &mt32_pcm_100_l,
                                     &mt32_pcm_100_h,
                                     &mt32_ctrl_203_f,
                                     nullptr,
                                     nullptr};

const LASynthModel mt32_204_model = {"mt32_204",
                                     &mt32_pcm_100_f,
                                     &mt32_pcm_100_l,
                                     &mt32_pcm_100_h,
                                     &mt32_ctrl_204_f,
                                     nullptr,
                                     nullptr};

const LASynthModel mt32_206_model = {"mt32_206",
                                     &mt32_pcm_100_f,
                                     &mt32_pcm_100_l,
                                     &mt32_pcm_100_h,
                                     &mt32_ctrl_206_f,
                                     nullptr,
                                     nullptr};

const LASynthModel mt32_207_model = {"mt32_207",
                                     &mt32_pcm_100_f,
                                     &mt32_pcm_100_l,
                                     &mt32_pcm_100_h,
                                     &mt32_ctrl_207_f,
                                     nullptr,
                                     nullptr};

const LASynthModel cm32l_100_model = {"cm32l_100",
                                      &cm32l_pcm_100_f,
                                      &cm32l_pcm_100_l,
                                      &cm32l_pcm_100_h,
                                      &cm32l_ctrl_100_f,
                                      nullptr,
                                      nullptr};

const LASynthModel cm32l_102_model = {"cm32l_102",
                                      &cm32l_pcm_100_f,
                                      &cm32l_pcm_100_l,
                                      &cm32l_pcm_100_h,
                                      &cm32l_ctrl_102_f,
                                      nullptr,
                                      nullptr};

const LASynthModel cm32ln_100_model = {"cm32ln_100",
                                       &cm32l_pcm_100_f,
                                       &cm32l_pcm_100_l,
                                       &cm32l_pcm_100_h,
                                       &cm32ln_ctrl_100_f,
                                       nullptr,
                                       nullptr};

// In order that 'model = auto' will load.
//
// Notes:
//
// - Blue Ridge is the *least* desirable "old" MT-32 ROM as it crashes Wing
//   Commander 1 & 2 while 1.07 works just fine.
//
// - CM-32LN has a different (faster) vibrato that all the other MT-32/CM-32L
//   models, which causes some soundtracks to sound quite wrong on it. This
//   makes it the *least* desirable CM-32L model.
//
// - For the rest of the models, higher versions are better as they fix
//   certain bugs of previous models.
//
// ------
// More info:
//
// Detailed info about various ROM versions
// - https://www.vogons.org/viewtopic.php?p=848507#p848507
// - https://www.vogons.org/viewtopic.php?p=848518#p848518
//
// Blue Ridge problems
// - https://www.vogons.org/viewtopic.php?p=803448#p803448
//
// CM-32LN vibrato issue
// - https://www.vogons.org/viewtopic.php?p=605136#p605136

// Listed in resolution priority order
static const std::vector<const LASynthModel*> all_models = {
        &cm32l_102_model,
        &cm32l_100_model,
        &cm32ln_100_model,

        &mt32_107_model,
        &mt32_106_model,
        &mt32_105_model,
        &mt32_104_model,
        &mt32_bluer_model,

        &mt32_207_model,
        &mt32_206_model,
        &mt32_204_model,
        &mt32_203_model,
};

// Listed in resolution priority order
static const std::vector<const LASynthModel*> mt32_models = {
        &mt32_107_model,
        &mt32_106_model,
        &mt32_105_model,
        &mt32_104_model,
        &mt32_bluer_model,

        &mt32_207_model,
        &mt32_206_model,
        &mt32_204_model,
        &mt32_203_model,
};

// Listed in resolution priority order
static const std::vector<const LASynthModel*> mt32_old_models = {
        &mt32_107_model,
        &mt32_106_model,
        &mt32_105_model,
        &mt32_104_model,
        &mt32_bluer_model,
};

// Listed in resolution priority order
static const std::vector<const LASynthModel*> mt32_new_models = {
        &mt32_207_model,
        &mt32_206_model,
        &mt32_204_model,
        &mt32_203_model,
};

// Listed in resolution priority order
static const std::vector<const LASynthModel*> cm32l_models = {
        &cm32l_102_model,
        &cm32l_100_model,
        &cm32ln_100_model,
};

// Symbolic model aliases
namespace BestModelAlias {
constexpr auto Mt32Any = "mt32";
constexpr auto Mt32Old = "mt32_old";
constexpr auto Mt32New = "mt32_new";
constexpr auto Cm32l   = "cm32l";
} // namespace BestModelAlias

MidiDeviceMt32 mt32_instance;

static void init_mt32_dosbox_settings(Section_prop& sec_prop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto* str_prop = sec_prop.Add_string("model", when_idle, "auto");

	// Listed in resolution priority order
	str_prop->Set_values({"auto",
	                      BestModelAlias::Cm32l,
	                      cm32l_102_model.GetName(),
	                      cm32l_100_model.GetName(),
	                      cm32ln_100_model.GetName(),

	                      BestModelAlias::Mt32Any,
	                      BestModelAlias::Mt32Old,
	                      mt32_107_model.GetName(),
	                      mt32_106_model.GetName(),
	                      mt32_105_model.GetName(),
	                      mt32_104_model.GetName(),
	                      mt32_bluer_model.GetName(),

	                      BestModelAlias::Mt32New,
	                      mt32_207_model.GetName(),
	                      mt32_206_model.GetName(),
	                      mt32_204_model.GetName(),
	                      mt32_203_model.GetName()});
	str_prop->Set_help(
	        "The Roland MT-32/CM-32ML model to use.\n"
	        "You must have the ROM files for the selected model available (see 'romdir').\n"
	        "The lookup for the best models is performed in order as listed.\n"
	        "  auto:       Pick the best available model (default).\n"
	        "  cm32l:      Pick the best available CM-32L model.\n"
	        "  mt32_old:   Pick the best available \"old\" MT-32 model (v1.0x).\n"
	        "  mt32_new:   Pick the best available \"new\" MT-32 model (v2.0x).\n"
	        "  mt32:       Pick the best available MT-32 model.\n"
	        "  <version>:  Use the exact specified model version (e.g., 'mt32_204').");

	str_prop = sec_prop.Add_string("romdir", when_idle, "");
	str_prop->Set_help(
	        "The directory containing the Roland MT-32/CM-32ML ROMs (unset by default).\n"
	        "The directory can be absolute or relative, or leave it unset to use the\n"
	        "'mt32-roms' directory in your DOSBox configuration directory. Other common\n"
	        "system locations will be checked as well.\n"
	        "Notes:\n"
	        "  - The file names of the ROM files do not matter; the ROMS are identified\n"
	        "    by their checksums.\n"
	        "  - Both interleaved and non-interlaved ROM files are supported.");

	str_prop = sec_prop.Add_string("mt32_filter", when_idle, "off");
	assert(str_prop);
	str_prop->Set_help(
	        "Filter for the Roland MT-32/CM-32L audio output:\n"
	        "  off:       Don't filter the output (default).\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

static void register_mt32_text_messages()
{
	MSG_Add("MT32_NO_SUPPORTED_MODELS", "No supported models present");

	MSG_Add("MT32_ROM_NOT_LOADED", "No model is currently active");

	MSG_Add("MT32_INVENTORY_TABLE_MISSING_LETTER", "-");
	MSG_Add("MT32_INVENTORY_TABLE_AVAILABLE_LETTER", "y");

	MSG_Add("MT32_ROMS_LABEL", "MT-32  models   ");
	MSG_Add("CM32L_ROMS_LABEL", "CM-32L models   ");
	MSG_Add("MT32_ACTIVE_ROM_LABEL", "Active model  ");
	MSG_Add("MT32_SOURCE_DIR_LABEL", "ROM path      ");
}

#if defined(WIN32)

static std::deque<std_fs::path> get_platform_rom_dirs()
{
	return {
	        GetConfigDir() / DefaultMt32RomsDir,
	        "C:\\mt32-rom-data\\",
	};
}

#elif defined(MACOSX)

static std::deque<std_fs::path> get_platform_rom_dirs()
{
	return {
	        GetConfigDir() / DefaultMt32RomsDir,
	        resolve_home("~/Library/Audio/Sounds/MT32-Roms/"),
	        "/usr/local/share/mt32-rom-data/",
	        "/usr/share/mt32-rom-data/",
	};
}
#else

static std::deque<std_fs::path> get_platform_rom_dirs()
{
	// First priority is user-specific data location
	const auto xdg_data_home = get_xdg_data_home();

	std::deque<std_fs::path> dirs = {
	        xdg_data_home / "dosbox" / DefaultMt32RomsDir,
	        xdg_data_home / "mt32-rom-data",
	};

	// Second priority are the $XDG_DATA_DIRS
	for (const auto& data_dir : get_xdg_data_dirs()) {
		dirs.emplace_back(data_dir / "mt32-rom-data");
	}

	// Third priority is $XDG_CONF_HOME, for convenience
	dirs.emplace_back(GetConfigDir() / DefaultMt32RomsDir);

	return dirs;
}

#endif

static std::deque<std_fs::path> get_rom_dirs()
{
	const auto section = static_cast<Section_prop*>(control->GetSection("mt32"));
	assert(section);

	// Get potential ROM directories from the environment and/or system
	auto rom_dirs = get_platform_rom_dirs();

	// Get the user's configured ROM directory; otherwise use 'mt32-roms'
	std::string selected_romdir = section->Get_string("romdir");

	if (selected_romdir.empty()) { // already trimmed
		selected_romdir = DefaultMt32RomsDir;
	}
	if (selected_romdir.back() != '/' && selected_romdir.back() != '\\') {
		selected_romdir += CROSS_FILESPLIT;
	}

	// Make sure we search the user's configured directory first
	rom_dirs.emplace_front(resolve_home(selected_romdir));
	return rom_dirs;
}

static std::string get_model_setting()
{
	const auto section = static_cast<Section_prop*>(control->GetSection("mt32"));
	assert(section);
	return section->Get_string("model");
}

static std::set<const LASynthModel*> find_models(const Mt32ServicePtr& service,
                                                 const std_fs::path& dir)
{
	std::set<const LASynthModel*> models = {};
	for (const auto& model : all_models) {
		if (model->InDir(service, dir)) {
			models.insert(model);
		}
	}
	return models;
}

static std::optional<ModelAndDir> load_model(const Mt32ServicePtr& service,
                                             const std::string& wanted_model_name,
                                             const std::deque<std_fs::path>& rom_dirs)
{
	// Determine the list of ROM model candidates and the lookup method for
	// the requested model name:
	//
	// - Symbolic model names (e.g., 'auto', 'mt32', 'mt32_old', etc.)
	//   resolve the first available model from the list of candidates in
	//   the listed priority. The lookup only fails if none of the candidate
	//   models could be resolved.
	//
	// - Concrete versioned model names always try to resolve a single model
	//   version or fail if the requested model is not present.
	//
	auto [load_first_available, candidate_models] =
	        [&]() -> std::pair<bool, const std::vector<const LASynthModel*>&> {
		// Symbolic mode names (resolve the best match from a list of
		// candidates).
		if (wanted_model_name == "auto") {
			return {true, all_models};
		}
		if (wanted_model_name == BestModelAlias::Mt32Any) {
			return {true, mt32_models};
		}
		if (wanted_model_name == BestModelAlias::Mt32Old) {
			return {true, mt32_old_models};
		}
		if (wanted_model_name == BestModelAlias::Mt32New) {
			return {true, mt32_new_models};
		}
		if (wanted_model_name == BestModelAlias::Cm32l) {
			return {true, cm32l_models};
		}

		// Concrete versioned model name (resolve the specific requested
		// model from the list of all supported models).
		return {false, all_models};
	}();

	// Perform model lookup from the list of candidate models
	for (const auto& model : candidate_models) {
		if (load_first_available || model->Matches(wanted_model_name)) {
			for (const auto& dir : rom_dirs) {
				if (model->Load(service, dir)) {
					return {
					        {model, simplify_path(dir)}
                                        };
				}
			}
		}
	}
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

		static void printDebug([[maybe_unused]] void* instance_data,
		                       [[maybe_unused]] const char* fmt,
		                       [[maybe_unused]] va_list list)
		{
#if !defined(NDEBUG)
			// Skip processing MT-32 debug output in release builds
			// because it can be bursty
			char msg[1024] = "MT32: ";
			assert(fmt);
			safe_strcat(msg, fmt);
			LOG_DEBUG(msg, list);
#endif
		}

		static void onErrorControlROM(void*)
		{
			LOG_WARNING("MT32: Couldn't open Control ROM file");
		}

		static void onErrorPCMROM(void*)
		{
			LOG_WARNING("MT32: Couldn't open PCM ROM file");
		}

		static void showLCDMessage(void*, const char* message)
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

Mt32ServicePtr MidiDeviceMt32::GetService()
{
	const std::lock_guard<std::mutex> lock(service_mutex);
	Mt32ServicePtr mt32_service = std::make_unique<MT32Emu::Service>();
	// Has libmt32emu already created a context?
	if (!mt32_service->getContext()) {
		mt32_service->createContext(get_report_handler_interface(), this);
	}
	return mt32_service;
}

// Returns the set of models supported by all of the directories, and also
// populates the provide map of the modules supported by each directory.

using DirsWithModels = std::map<std_fs::path, std::set<const LASynthModel*>>;

static std::set<const LASynthModel*> populate_available_models(
        const Mt32ServicePtr& service, DirsWithModels& dirs_with_models)
{
	std::set<const LASynthModel*> available_models;

	for (const auto& dir : get_rom_dirs()) {
		const auto models = find_models(service, dir);
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
MIDI_RC MidiDeviceMt32::ListDevices(Program* caller)
{
	// Table layout constants
	constexpr char column_delim[] = " ";
	constexpr char indent[]       = "  ";

	const std::vector<const LASynthModel*> mt32_model_list = {&mt32_104_model,
	                                                          &mt32_105_model,
	                                                          &mt32_106_model,
	                                                          &mt32_107_model,
	                                                          &mt32_bluer_model,
	                                                          &mt32_203_model,
	                                                          &mt32_204_model,
	                                                          &mt32_206_model,
	                                                          &mt32_207_model};

	const std::vector<const LASynthModel*> cm32_model_list = {
	        &cm32l_100_model, &cm32l_102_model, &cm32ln_100_model};

	// Get the set of directories and the models they support
	DirsWithModels dirs_with_models;
	const auto available_models = populate_available_models(GetService(),
	                                                        dirs_with_models);

	if (available_models.empty()) {
		caller->WriteOut("%s%s\n", indent, MSG_Get("MT32_NO_SUPPORTED_MODELS"));
		return MIDI_RC::OK;
	}

	auto highlight_model = [&](const LASynthModel* model,
	                           const char* display_name) -> std::string {
		constexpr auto darkgray = "[color=dark-gray]";
		constexpr auto green    = "[color=light-green]";
		constexpr auto reset    = "[reset]";

		const bool is_missing = (available_models.find(model) ==
		                         available_models.end());

		const auto is_active = (model_and_dir &&
		                        model_and_dir->first == model);

		const auto color = (is_missing ? darkgray
		                               : (is_active ? green : reset));

		const auto active_prefix = (is_active ? "*" : " ");
		const auto model_string  = format_str(
                        "%s%s%s%s", color, active_prefix, display_name, reset);

		return convert_ansi_markup(model_string.c_str());
	};

	// Print available MT-32 ROMs
	caller->WriteOut("%s%s", indent, MSG_Get("MT32_ROMS_LABEL"));

	for (const auto& model : mt32_model_list) {
		const auto display_name = model->GetVersion();
		caller->WriteOut("%s%s",
		                 highlight_model(model, display_name).c_str(),
		                 column_delim);
	}
	caller->WriteOut("\n");

	// Print available CM-32L ROMs
	caller->WriteOut("%s%s", indent, MSG_Get("CM32L_ROMS_LABEL"));

	for (const auto& model : cm32_model_list) {
		const auto display_name = (model->GetName() == cm32ln_100_model.GetName()
		                                   ? model->GetName()
		                                   : model->GetVersion());
		caller->WriteOut("%s%s",
		                 highlight_model(model, display_name).c_str(),
		                 column_delim);
	}
	caller->WriteOut("\n");

	caller->WriteOut("%s---\n", indent);

	// Print info about the loaded ROM
	if (model_and_dir && service) {
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
		const std::string dir_label = MSG_Get("MT32_SOURCE_DIR_LABEL");

		const auto dir_max_length = INT10_GetTextColumns() -
		                            (dir_label.length() +
		                             std::string_view(indent).length());

		const auto truncated_dir =
		        model_and_dir->second.string().substr(0, dir_max_length);

		caller->WriteOut("%s%s%s\n",
		                 indent,
		                 dir_label.c_str(),
		                 truncated_dir.c_str());
	} else {
		caller->WriteOut("%s%s\n", indent, MSG_Get("MT32_ROM_NOT_LOADED"));
	}

	return MIDI_RC::OK;
}

bool MidiDeviceMt32::Open([[maybe_unused]] const char* conf)
{
	Close();

	auto mt32_service     = GetService();
	const auto model_name = get_model_setting();
	const auto rom_dirs   = get_rom_dirs();

	// Load the selected model and print info about it
	auto loaded_model_and_dir = load_model(mt32_service, model_name, rom_dirs);
	if (!loaded_model_and_dir) {
		LOG_WARNING("MT32: Failed to find ROMs for model %s in:",
		            model_name.c_str());

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

	const auto sample_rate_hz = AccurateAnalogModeSampleRateHz;

	ms_per_audio_frame = MillisInSecond / sample_rate_hz;

	mt32_service->setAnalogOutputMode(AnalogMode);
	mt32_service->selectRendererType(RenderingType);
	mt32_service->setDACInputMode(DacEmulationMode);
	mt32_service->setNiceAmpRampEnabled(UseNiceRamp);
	mt32_service->setNicePanningEnabled(UseNicePanning);
	mt32_service->setNicePartialMixingEnabled(UseNicePartialMixing);
	mt32_service->setMIDIDelayMode(MidiDelayMode);

	const auto rc = mt32_service->openSynth();
	if (rc != MT32EMU_RC_OK) {
		LOG_WARNING("MT32: Error initialising emulation: %i", rc);
		return false;
	}

	MIXER_LockMixerThread();

	// Set up the mixer callback
	const auto mixer_callback = std::bind(&MidiDeviceMt32::MixerCallBack,
	                                      this,
	                                      std::placeholders::_1);

	auto mixer_channel = MIXER_AddChannel(mixer_callback,
	                                      sample_rate_hz,
	                                      ChannelName::RolandMt32,
	                                      {ChannelFeature::Sleep,
	                                       ChannelFeature::Stereo,
	                                       ChannelFeature::Synthesizer});

	mixer_channel->SetResampleMethod(ResampleMethod::Resample);

	// libmt32emu renders float audio frames between -1.0f and +1.0f, so we
	// ask the channel to scale all the samples up to its 0db level.
	mixer_channel->Set0dbScalar(Max16BitSampleValue);

	const auto section = static_cast<Section_prop*>(control->GetSection("mt32"));
	assert(section);

	const std::string filter_prefs = section->Get_string("mt32_filter");

	if (!mixer_channel->TryParseAndSetCustomFilter(filter_prefs)) {
		if (filter_prefs != "off") {
			LOG_WARNING("MT32: Invalid 'mt32_filter' value: '%s', using 'off'",
			            filter_prefs.c_str());
		}

		mixer_channel->SetHighPassFilter(FilterState::Off);
		mixer_channel->SetLowPassFilter(FilterState::Off);

		set_section_property_value("mt32", "mt32_filter", "off");
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

	// MIDI has a baud rate of 31250; at optimum, this is 31250 bits per
	// second. A MIDI byte is 8 bits plus a start and stop bit, and each
	// MIDI message is three bytes, which gives a total of 30 bits per
	// message. This means that under optimal conditions, a maximum of 1042
	// messages per second can be obtained via the MIDI protocol.

	// We have measured DOS games sending hundreds of MIDI messages within a
	// short handful of millseconds, so a safe but very generous upper bound
	// is used.
	//
	// (Note: the actual memory used by the FIFO is incremental based on
	// actual usage).
	//
	static constexpr uint16_t midi_spec_max_msg_rate_hz = 1042;
	work_fifo.Resize(midi_spec_max_msg_rate_hz * 10);

	// Move the local objects into the member variables
	service       = std::move(mt32_service);
	channel       = std::move(mixer_channel);
	model_and_dir = std::move(loaded_model_and_dir);

	// Start rendering audio
	const auto render = std::bind(&MidiDeviceMt32::Render, this);
	renderer          = std::thread(render);
	set_thread_name(renderer, "dosbox:mt32");

	// Start playback
	is_open = true;
	MIXER_UnlockMixerThread();
	return true;
}

MidiDeviceMt32::~MidiDeviceMt32()
{
	Close();
}

void MidiDeviceMt32::Close()
{
	if (!is_open) {
		return;
	}

	LOG_MSG("MT32: Shutting down");

	if (had_underruns) {
		LOG_WARNING(
		        "MT32: Fix underruns by lowering CPU load "
		        "or increasing your conf's prebuffer");
		had_underruns = false;
	}

	MIXER_LockMixerThread();

	// Stop playback
	if (channel) {
		channel->Enable(false);
	}

	// Stop queueing new MIDI work and audio frames
	work_fifo.Stop();
	audio_frame_fifo.Stop();

	// Wait for the rendering thread to finish
	if (renderer.joinable()) {
		renderer.join();
	}

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

	last_rendered_ms   = 0.0;
	ms_per_audio_frame = 0.0;

	is_open = false;
	MIXER_UnlockMixerThread();
}

int MidiDeviceMt32::GetNumPendingAudioFrames()
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

	return num_audio_frames;
}

// The request to play the channel message is placed in the MIDI work FIFO
void MidiDeviceMt32::SendMidiMessage(const MidiMessage& msg)
{
	std::vector<uint8_t> message(msg.data.begin(), msg.data.end());
	MidiWork work{std::move(message),
	              GetNumPendingAudioFrames(),
	              MessageType::Channel};
	work_fifo.Enqueue(std::move(work));
}

// The request to play the sysex message is placed in the MIDI work FIFO
void MidiDeviceMt32::SendSysExMessage(uint8_t* sysex, size_t len)
{
	std::vector<uint8_t> message(sysex, sysex + len);
	MidiWork work{std::move(message), GetNumPendingAudioFrames(), MessageType::SysEx};
	work_fifo.Enqueue(std::move(work));
}

// The callback operates at the audio frame-level, steadily adding samples to
// the mixer until the requested numbers of audio frames is met.
void MidiDeviceMt32::MixerCallBack(const int requested_audio_frames)
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
		assert(check_cast<int>(audio_frames.size()) == requested_audio_frames);
		channel->AddSamples_sfloat(requested_audio_frames,
		                           &audio_frames[0][0]);

		last_rendered_ms = PIC_FullIndex();
	} else {
		assert(!audio_frame_fifo.IsRunning());
		channel->AddSilence();
	}
}

void MidiDeviceMt32::RenderAudioFramesToFifo(const int num_frames)
{
	static std::vector<AudioFrame> audio_frames = {};

	// Maybe expand the vector
	if (check_cast<int>(audio_frames.size()) < num_frames) {
		audio_frames.resize(num_frames);
	}

	std::unique_lock<std::mutex> lock(service_mutex);
	service->renderFloat(&audio_frames[0][0], num_frames);
	lock.unlock();

	audio_frame_fifo.BulkEnqueue(audio_frames, num_frames);
}

// The next MIDI work task is processed, which includes rendering audio frames
// prior to applying channel and sysex messages to the service
void MidiDeviceMt32::ProcessWorkFromFifo()
{
	const auto work = work_fifo.Dequeue();
	if (!work) {
		return;
	}

#ifdef DEBUG_MT32
	LOG_TRACE(
	        "MT32: %2u audio frames prior to %s message, followed by "
	        "%2lu more messages. Have %4lu audio frames queued",
	        work->num_pending_audio_frames,
	        work->message_type == MessageType::Channel ? "channel" : "sysex",
	        work_fifo.Size(),
	        audio_frame_fifo.Size());
#endif

	if (work->num_pending_audio_frames > 0) {
		RenderAudioFramesToFifo(work->num_pending_audio_frames);
	}

	// Request exclusive access prior to applying messages
	const std::lock_guard<std::mutex> lock(service_mutex);

	if (work->message_type == MessageType::Channel) {
		assert(work->message.size() <= MaxMidiMessageLen);

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
void MidiDeviceMt32::Render()
{
	while (work_fifo.IsRunning()) {
		work_fifo.IsEmpty() ? RenderAudioFramesToFifo()
		                    : ProcessWorkFromFifo();
	}
}

static void mt32_init([[maybe_unused]] Section* sec) {}

void MT32_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);
	Section_prop* sec_prop = conf->AddSection_prop("mt32", &mt32_init);

	assert(sec_prop);
	init_mt32_dosbox_settings(*sec_prop);

	register_mt32_text_messages();
}

#endif // C_MT32EMU
