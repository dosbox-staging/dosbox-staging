// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/emu88.h"

#include <algorithm>
#include <deque>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "audio/channel_names.h"
#include "audio/clap/plugin_manager.h"
#include "config/setup.h"
#include "dos/programs.h"
#include "misc/ansi_code_markup.h"
#include "misc/cross.h"
#include "misc/std_filesystem.h"
#include "utils/checks.h"
#include "utils/env_utils.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

namespace Emu88 {

constexpr auto SectionName = "88emu";

// The 88emu plugins add the directories in this path list to their ROM search
// paths. The ROMs are identified by their contents, so file names don't
// matter, and subdirectories are searched too.
constexpr auto RomPathVariable = "EMU88_ROM_PATH";

// clang-format off
const SynthModel mt32_new = {"mt32_new", "Roland MT-32 (v2.x)",                "mt322x",    Family::Mt32};
const SynthModel mt32_old = {"mt32_old", "Roland MT-32 (v1.x, experimental)",  "mt321x",    Family::Mt32};
const SynthModel cm32l    = {"cm32l",    "Roland CM-32L",                      "cm32l",     Family::Mt32};
const SynthModel cm32ln   = {"cm32ln",   "Roland CM-32LN (experimental)",      "cm32ln",    Family::Mt32};
const SynthModel cm32p    = {"cm32p",    "Roland CM-32P",                      "cm32p",     Family::Mt32};
const SynthModel cm64     = {"cm64",     "Roland CM-64",                       "cm64",      Family::Mt32};

const SynthModel sc55     = {"sc55",     "Roland SC-55",                       "sc55",      Family::Sc55};
const SynthModel sc55mk2  = {"sc55mk2",  "Roland SC-55mkII",                   "sc55mkii",  Family::Sc55};
const SynthModel sc55st   = {"sc55st",   "Roland SC-55st",                     "sc55st",    Family::Sc55};
const SynthModel scc1     = {"scc1",     "Roland CM-300 / SCC-1",              "cm300scc1", Family::Sc55};
const SynthModel scc1a    = {"scc1a",    "Roland SCC-1A",                      "scc1a",     Family::Sc55};
const SynthModel scb55    = {"scb55",    "Roland SCB-55",                      "scb55",     Family::Sc55};
const SynthModel rlp3237  = {"rlp3237",  "Roland RLP-3237",                    "rlp3237",   Family::Sc55};
const SynthModel sc155    = {"sc155",    "Roland SC-155",                      "sc155",     Family::Sc55};
const SynthModel sc155mk2 = {"sc155mk2", "Roland SC-155mkII",                  "sc155mkii", Family::Sc55};

const SynthModel sc88     = {"sc88",     "Roland SC-88",                       "sc88",      Family::Sc88};
const SynthModel sc88vl   = {"sc88vl",   "Roland SC-88VL",                     "sc88vl",    Family::Sc88};
const SynthModel sc88pro  = {"sc88pro",  "Roland SC-88Pro",                    "sc88pro",   Family::Sc88};
const SynthModel sk88pro  = {"sk88pro",  "Roland SK-88Pro",                    "sk88pro",   Family::Sc88};
const SynthModel sc880    = {"sc880",    "Roland SC-880",                      "sc880",     Family::Sc88};
const SynthModel xpgs     = {"xpgs",     "Roland XPGS / G-800",                "xpgsg800",  Family::Sc88};
const SynthModel vegspro  = {"vegspro",  "Roland VE-GS Pro",                   "vegspro",   Family::Sc88};

const SynthModel sc8820   = {"sc8820",   "Roland SC-8820",                     "sc8820",    Family::Sc8850};
const SynthModel sc8850   = {"sc8850",   "Roland SC-8850",                     "sc8850",    Family::Sc8850};

const SynthModel mv30     = {"mv30",     "Roland MV-30 (experimental)",        "mv30",      Family::Other};
const SynthModel proe     = {"proe",     "Roland PRO-E",                       "proe",      Family::Other};
// clang-format on

// All models, in display order
const std::vector<const SynthModel*> all_models = {
        &mt32_old, &mt32_new, &cm32l,   &cm32ln,  &cm32p, &cm64,

        &sc55,     &sc55mk2,  &sc55st,  &scc1,    &scc1a, &scb55, &rlp3237,
        &sc155,    &sc155mk2,

        &sc88,     &sc88vl,   &sc88pro, &sk88pro, &sc880, &xpgs,  &vegspro,

        &sc8820,   &sc8850,

        &mv30,     &proe,
};

// Symbolic model aliases
namespace BestModelAlias {
constexpr auto Auto = "auto";
constexpr auto Mt32 = "mt32";
} // namespace BestModelAlias

// The candidates of 'auto', in resolution priority order. MT-32 first, as it
// is what most DOS games with internal MIDI support target. The CM-32P has no
// LA synth and the MV-30 and PRO-E are not general purpose sound modules, so
// they are only used when asked for by name.
const std::vector<const SynthModel*> auto_models = {
        &mt32_new, &mt32_old, &cm32l,   &cm64,    &cm32ln,

        &sc55,     &sc55mk2,  &sc55st,  &scc1,    &scc1a,  &scb55, &sc155,
        &sc155mk2, &rlp3237,

        &sc88,     &sc88vl,   &sc88pro, &sk88pro, &sc880,  &xpgs,  &vegspro,

        &sc8820,   &sc8850,
};

// The candidates of 'mt32', in resolution priority order
const std::vector<const SynthModel*> mt32_models = {&mt32_new, &mt32_old};

static std::string get_plugin_id(const SynthModel& model)
{
	return std::string(PluginIdPrefix) + model.plugin_id_suffix;
}

} // namespace Emu88

static SectionProp* get_emu88_section()
{
	return get_section(Emu88::SectionName);
}

static std::string get_model_setting()
{
	return get_emu88_section()->GetString("88emu_model");
}

// The ROM directories, in search order: the configured one (or '88emu-roms'),
// then the '88emu-roms', 'mt32-roms' and 'soundcanvas-roms' directories in the
// DOSBox configuration directory, so ROMs set up for the other synths are found
// as well.
static std::deque<std_fs::path> get_rom_dirs()
{
	std_fs::path selected_romdir = get_emu88_section()->GetString("88emu_rom_dir");
	if (selected_romdir.empty()) { // already trimmed
		selected_romdir = DefaultEmu88RomsDir;
	}

	return {resolve_home(selected_romdir.string()),
	        get_config_dir() / DefaultEmu88RomsDir,
	        get_config_dir() / DefaultMt32RomsDir,
	        get_config_dir() / DefaultSoundCanvasRomsDir};
}

static void set_rom_path_env_var()
{
	std::error_code ec = {};

	std::vector<std_fs::path> dirs = {};
	for (const auto& rom_dir : get_rom_dirs()) {
		if (!std_fs::is_directory(rom_dir, ec) || ec) {
			continue;
		}
		const auto canonical_rom_dir = std_fs::canonical(rom_dir, ec);
		if (ec) {
			continue;
		}
		if (std::find(dirs.begin(), dirs.end(), canonical_rom_dir) ==
		    dirs.end()) {
			dirs.push_back(canonical_rom_dir);
		}
	}

	std::string env_list = {};
	for (const auto& dir : dirs) {
		if (!env_list.empty()) {
			env_list.append(env_path_separator);
		}
		env_list.append(dir.string());
	}

	LOG_MSG("88EMU: Setting %s env variable to '%s'",
	        Emu88::RomPathVariable,
	        env_list.c_str());

	set_env_var(Emu88::RomPathVariable, env_list.c_str(), Env::Overwrite);
}

// Each load of the 88emu library scans its ROM directories and hashes the ROMs
// from scratch. A library is unloaded with its last plugin instance, so probing
// several models loads it once and creates every plugin from it.
static std::shared_ptr<Clap::Library> load_library()
{
	auto& plugin_manager = Clap::PluginManager::GetInstance();

	const auto plugin_info = plugin_manager.FindPluginInLibrary(Emu88::LibraryName);
	if (!plugin_info) {
		return {};
	}
	return plugin_manager.GetLibrary(*plugin_info);
}

static std::unique_ptr<Clap::Plugin> try_load_plugin(
        const std::shared_ptr<Clap::Library>& library, const Emu88::SynthModel& model)
{
	auto& plugin_manager = Clap::PluginManager::GetInstance();

	const auto plugin_info = plugin_manager.FindPluginInLibraryById(
	        Emu88::LibraryName, Emu88::get_plugin_id(model));

	if (!plugin_info) {
		return {};
	}

	// Fails if the ROMs of the model are missing
	return plugin_manager.LoadPlugin(library, *plugin_info);
}

struct PluginAndModel {
	std::unique_ptr<Clap::Plugin> plugin = nullptr;
	const Emu88::SynthModel* model       = nullptr;
};

static PluginAndModel load_model(const std::string& wanted_model_name)
{
	using namespace Emu88;

	// Symbolic model names resolve the first available model from a list
	// of candidates; a concrete model name only resolves that model.
	const auto [load_first_available, candidate_models] =
	        [&]() -> std::pair<bool, const std::vector<const SynthModel*>&> {
		if (wanted_model_name == BestModelAlias::Auto) {
			return {true, auto_models};
		}
		if (wanted_model_name == BestModelAlias::Mt32) {
			return {true, mt32_models};
		}
		return {false, all_models};
	}();

	const auto library = load_library();

	for (const auto model : candidate_models) {
		if (load_first_available || wanted_model_name == model->config_name) {
			if (auto plugin = try_load_plugin(library, *model); plugin) {
				return {std::move(plugin), model};
			}
		}
	}

	return {};
}

static void setup_filter(MixerChannelPtr& channel, const bool filter_enabled)
{
	channel->SetHighPassFilter(FilterState::Off);

	if (!filter_enabled) {
		channel->SetLowPassFilter(FilterState::Off);
		return;
	}

	// The Roland SC-55 models gently soften the high-frequency response of
	// the output. Measured on different SC-55 units (original, SC-55mk2,
	// SC-55st, SC-55K) and recordings made by different individuals, so
	// composers must have compensated for it. See 'soundcanvas.cpp'.
	constexpr auto LpfOrder        = 1;
	constexpr auto LpfCutoffFreqHz = 11000;

	channel->ConfigureLowPassFilter(LpfOrder, LpfCutoffFreqHz);
	channel->SetLowPassFilter(FilterState::On);
}

MidiDeviceEmu88::MidiDeviceEmu88()
{
	set_rom_path_env_var();

	const auto model_name = get_model_setting();

	auto plugin_and_model = load_model(model_name);
	if (!plugin_and_model.plugin) {
		const auto msg = format_str("88EMU: Failed to load the '%s' model",
		                            model_name.c_str());
		LOG_WARNING("%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	model = plugin_and_model.model;
	LOG_MSG("88EMU: Initialised %s", model->display_name);

	// 88emu resamples the output of the emulated device to any rate, so
	// render at the mixer rate to avoid a second resampling pass
	const auto sample_rate_hz = MIXER_GetSampleRate();
	clap.Initialize(std::move(plugin_and_model.plugin), sample_rate_hz);

	MIXER_LockMixerThread();

	const auto mixer_callback = std::bind_front(&MidiSynth::MixerCallback, this);

	mixer_channel = MIXER_AddChannel(mixer_callback,
	                                 sample_rate_hz,
	                                 ChannelName::Emu88,
	                                 {ChannelFeature::NoiseGate,
	                                  ChannelFeature::Sleep,
	                                  ChannelFeature::Stereo,
	                                  ChannelFeature::Synthesizer});

	mixer_channel->SetResampleMethod(ResampleMethod::Resample);

	// CLAP plugins render float audio frames between -1.0f and +1.0f, so we
	// ask the channel to scale all the samples up to its 0db level.
	mixer_channel->Set0dbScalar(Max16BitSampleValue);

	// The original SC-55 tends to output a low level constant noise from
	// time to time depending on previous MIDI input, which an accurate
	// emulation reproduces. The noise gate removes it; it is effectively
	// disabled for all other models.
	const auto threshold_db = (model == &Emu88::sc55) ? -70.0f : -1000.0f;

	constexpr auto AttackTimeMs  = 1.0f;
	constexpr auto ReleaseTimeMs = 1000.0f;
	mixer_channel->ConfigureNoiseGate(threshold_db, AttackTimeMs, ReleaseTimeMs);

	const auto denoiser_enabled = get_mixer_section()->GetBool("denoiser");
	mixer_channel->EnableNoiseGate(denoiser_enabled);

	// The built-in filter was measured on SC-55 units only
	const auto is_sc55 = (model->family == Emu88::Family::Sc55);

	const auto filter_prefs = get_emu88_section()->GetString("88emu_filter");

	if (const auto maybe_bool = parse_bool_setting(filter_prefs)) {
		setup_filter(mixer_channel, *maybe_bool && is_sc55);

	} else if (!mixer_channel->TryParseAndSetCustomFilter(filter_prefs)) {
		if (!has_false(filter_prefs)) {
			LOG_WARNING("88EMU: Invalid '88emu_filter' value: '%s', using 'on'",
			            filter_prefs.c_str());
		}
		setup_filter(mixer_channel, is_sc55);

		set_section_property_value(Emu88::SectionName, "88emu_filter", "on");
	}

	StartRenderer(sample_rate_hz, "dosbox:88emu");

	// Start playback
	MIXER_UnlockMixerThread();
}

const Emu88::SynthModel& MidiDeviceEmu88::GetModel() const
{
	assert(model);
	return *model;
}

void MidiDeviceEmu88::RenderAudioFramesToFifo(const int num_audio_frames)
{
	clap.RenderAudioFramesToFifo(num_audio_frames, audio_frame_fifo);
}

void MidiDeviceEmu88::ProcessWorkItem(const MidiWork& work)
{
	clap.ProcessWorkItem(work);
}

static std::set<const Emu88::SynthModel*> available_models = {};

static bool available_models_initialised = false;

void EMU88_ListDevices(MidiDeviceEmu88* device, MoreOutputStrings& output)
{
	using namespace Emu88;

	constexpr auto ColumnDelim = " ";
	constexpr auto Indent      = "  ";

	// Finding out which models have their ROMs means loading every plugin
	// once, so only do that again when the ROM directory changes
	if (!available_models_initialised) {
		set_rom_path_env_var();

		const auto library = load_library();

		available_models.clear();
		for (const auto m : all_models) {
			if (try_load_plugin(library, *m)) {
				available_models.insert(m);
			}
		}
		available_models_initialised = true;
	}

	if (available_models.empty()) {
		output.AddString("%s%s\n\n",
		                 Indent,
		                 MSG_Get("MIDI_DEVICE_NO_MODELS").c_str());
		return;
	}

	const SynthModel* active_model = device ? &device->GetModel() : nullptr;

	auto highlight_model = [&](const SynthModel* m) {
		constexpr auto darkgray = "[color=dark-gray]";
		constexpr auto green    = "[color=light-green]";
		constexpr auto reset    = "[reset]";

		const bool is_missing = !available_models.contains(m);
		const bool is_active  = (m == active_model);

		const auto color = (is_missing ? darkgray
		                               : (is_active ? green : reset));
		const auto active_prefix = (is_active ? "*" : " ");

		return convert_ansi_markup(format_str(
		        "%s%s%s%s", color, active_prefix, m->config_name, reset));
	};

	auto print_family = [&](const char* label_msg, const Family family) {
		output.AddString("%s%s", Indent, MSG_Get(label_msg).c_str());

		for (const auto m : all_models) {
			if (m->family == family) {
				output.AddString("%s%s",
				                 highlight_model(m).c_str(),
				                 ColumnDelim);
			}
		}
		output.AddString("\n");
	};

	print_family("EMU88_MT32_MODELS_LABEL", Family::Mt32);
	print_family("EMU88_SC55_MODELS_LABEL", Family::Sc55);
	print_family("EMU88_SC88_MODELS_LABEL", Family::Sc88);
	print_family("EMU88_SC8850_MODELS_LABEL", Family::Sc8850);
	print_family("EMU88_OTHER_MODELS_LABEL", Family::Other);

	output.AddString("%s---\n", Indent);

	if (active_model) {
		output.AddString("%s%s%s\n",
		                 Indent,
		                 MSG_Get("EMU88_ACTIVE_MODEL_LABEL").c_str(),
		                 active_model->display_name);
	} else {
		output.AddString("%s%s\n",
		                 Indent,
		                 MSG_Get("MIDI_DEVICE_NO_MODEL_ACTIVE").c_str());
	}

	output.AddString("\n");
}

static void init_emu88_config_settings(SectionProp& sec_prop)
{
	using namespace Emu88;

	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto* str_prop = sec_prop.AddString("88emu_model",
	                                    when_idle,
	                                    BestModelAlias::Auto);

	std::vector<std::string> values = {BestModelAlias::Auto,
	                                   BestModelAlias::Mt32};
	for (const auto m : all_models) {
		values.emplace_back(m->config_name);
	}
	str_prop->SetValues(values);

	str_prop->SetHelp(
	        "Roland MIDI sound module emulated by 88emu to use ('auto' by default).\n"
	        "Requires the '88emuClapHeadless.clap' CLAP plugin library in the 'plugins'\n"
	        "directory in your DOSBox installation or configuration directory, and the\n"
	        "ROMs of the selected model (see '88emu_rom_dir'). Possible values:\n"
	        "\n"
	        "  auto:       Pick the best available model (default). MT-32 models come\n"
	        "              first, then the Sound Canvas models from the SC-55 up to the\n"
	        "              SC-8850.\n"
	        "  mt32:       Pick the best available MT-32 model ('mt32_new', then\n"
	        "              'mt32_old').\n"
	        "  <model>:    Use the exact specified model:\n"
	        "\n"
	        "    MT-32 family:   mt32_old, mt32_new, cm32l, cm32ln, cm32p, cm64\n"
	        "    SC-55 family:   sc55, sc55mk2, sc55st, scc1 (CM-300 / SCC-1), scc1a,\n"
	        "                    scb55, rlp3237, sc155, sc155mk2\n"
	        "    SC-88 family:   sc88, sc88vl, sc88pro, sk88pro, sc880, xpgs, vegspro\n"
	        "    SC-8850 family: sc8820, sc8850\n"
	        "    Other:          mv30, proe\n"
	        "\n"
	        "Notes:\n"
	        "  - Run `MIXER /LISTMIDI` to see the list of available models.\n"
	        "\n"
	        "  - The MT-32 family is *NOT* General MIDI compatible; only use it if the\n"
	        "    game is known to support Roland MT-32 MIDI (which predates General MIDI).");

	str_prop = sec_prop.AddString("88emu_rom_dir", when_idle, "");
	str_prop->SetHelp(
	        "The directory containing the ROMs for 88emu (unset by default). The\n"
	        "directory can be absolute or relative, or leave it unset to use the\n"
	        "'88emu-roms' directory in your DOSBox configuration directory. The\n"
	        "'mt32-roms' and 'soundcanvas-roms' directories there are searched as well.\n"
	        "\n"
	        "Notes:\n"
	        "  - The file names of the ROM files do not matter; the ROMs are identified\n"
	        "    by their contents.\n"
	        "\n"
	        "  - Subdirectories are searched too, so a directory with one subdirectory\n"
	        "    per device works.");

	str_prop = sec_prop.AddString("88emu_filter", when_idle, "on");
	str_prop->SetHelp(
	        "Filter for the 88emu audio output ('on' by default). Possible values:\n"
	        "\n"
	        "  on:        Filter the output of the SC-55 family (default). The filter was\n"
	        "             measured on SC-55 units, so it's not applied to other models.\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

static void register_emu88_text_messages()
{
	MSG_Add("EMU88_MT32_MODELS_LABEL", "MT-32 models    ");
	MSG_Add("EMU88_SC55_MODELS_LABEL", "SC-55 models    ");
	MSG_Add("EMU88_SC88_MODELS_LABEL", "SC-88 models    ");
	MSG_Add("EMU88_SC8850_MODELS_LABEL", "SC-8850 models  ");
	MSG_Add("EMU88_OTHER_MODELS_LABEL", "Other models    ");
	MSG_Add("EMU88_ACTIVE_MODEL_LABEL", "Active model:    ");
}

static void notify_emu88_setting_updated([[maybe_unused]] SectionProp& section,
                                         const std::string& prop_name)
{
	if (prop_name == "88emu_rom_dir") {
		available_models_initialised = false;
	}

	const auto device = dynamic_cast<MidiDeviceEmu88*>(MIDI_GetCurrentDevice());
	if (!device) {
		return;
	}

	if (prop_name == "88emu_model") {
		if (device->GetModel().config_name != get_model_setting()) {
			MIDI_Init();
		}
	} else {
		MIDI_Init();
	}
}

void EMU88_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection(Emu88::SectionName);
	section->AddUpdateHandler(notify_emu88_setting_updated);

	init_emu88_config_settings(*section);
	register_emu88_text_messages();
}
