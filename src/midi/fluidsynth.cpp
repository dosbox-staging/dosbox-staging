//  SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
//  SPDX-License-Identifier: GPL-2.0-or-later

#include "private/fluidsynth.h"

#include <bitset>
#include <cassert>
#include <compare>
#include <numeric>
#include <string>
#include <tuple>
#include <vector>

#include "audio/channel_names.h"
#include "audio/mixer.h"
#include "config/config.h"
#include "dos/programs.h"
#include "hardware/pic.h"
#include "ints/int10.h"
#include "misc/ansi_code_markup.h"
#include "misc/cross.h"
#include "misc/notifications.h"
#include "misc/support.h"
#include "utils/fs_utils.h"
#include "utils/math_utils.h"
#include "utils/string_utils.h"

constexpr auto SoundFontExtension = ".sf2";

constexpr auto ChorusSettingName    = "fsynth_chorus";
constexpr auto DefaultChorusSetting = "auto";
constexpr auto NumChorusParams      = 5;

constexpr auto ReverbSettingName    = "fsynth_reverb";
constexpr auto DefaultReverbSetting = "auto";
constexpr auto NumReverbParams      = 4;

// clang-format off

// Use reasonable chorus and reverb settings matching ScummVM's defaults
constexpr ChorusParameters DefaultChorusParameters = {
	3,   // voice count
	1.2, // level
	0.3, // speed
	8.0, // depth
	fluid_chorus_mod::FLUID_CHORUS_MOD_SINE // mod wave
};

constexpr ReverbParameters DefaultReverbParameters = {
	0.61, // room size
	0.23, // damping
	0.76, // width
	0.56  // level
};
// clang-format on

static void init_fluidsynth_config_settings(SectionProp& secprop)
{
	constexpr auto WhenIdle = Property::Changeable::WhenIdle;

	// Name 'default.sf2' picks the default SoundFont if it's installed
	// in the OS (usually "Fluid_R3").
	auto str_prop = secprop.AddString("soundfont", WhenIdle, "default.sf2");
	str_prop->SetHelp(
	        "Name or path of SoundFont file to use ('default.sf2' by default). The SoundFont\n"
	        "will be looked up in the following locations in order:\n"
	        "\n"
	        "  - The user-defined SoundFont directory (see 'soundfont_dir').\n"
	        "  - The 'soundfonts' directory in your DOSBox configuration directory.\n"
	        "  - Other common system locations.\n"
	        "\n"
	        "The '.sf2' extension can be omitted. You can use paths relative to the above\n"
	        "locations or absolute paths as well.\n"
	        "\n"
	        "Note: Run `MIXER /LISTMIDI` to see the list of available SoundFonts.");

	str_prop = secprop.AddString("soundfont_dir", WhenIdle, "");
	str_prop->SetHelp(
	        "Extra user-defined SoundFont directory (unset by default). If this is set,\n"
	        "SoundFonts are looked up in this directory first, then in the the standard\n"
	        "system locations.");

	constexpr auto DefaultVolume = 100;
	constexpr auto MinVolume     = 1;
	constexpr auto MaxVolume     = 800;

	auto int_prop = secprop.AddInt("soundfont_volume", WhenIdle, DefaultVolume);
	int_prop->SetMinMax(MinVolume, MaxVolume);
	int_prop->SetHelp(
	        format_str("Set the SoundFont's volume as a percentage (%d by default). This is useful for\n"
	                   "normalising the volume of different SoundFonts. The percentage value can range\n"
	                   "from %d to %d.",
	                   DefaultVolume,
	                   MinVolume,
	                   MaxVolume));

	str_prop = secprop.AddString(ChorusSettingName, WhenIdle, DefaultChorusSetting);
	str_prop->SetHelp(
	        "Configure the FluidSynth chorus ('auto' by default). Possible values:\n"
	        "\n"
	        "  auto:      Enable chorus, except for known problematic SoundFonts (default).\n"
	        "  on:        Always enable chorus.\n"
	        "  off:       Disable chorus.\n"
	        "\n"
	        "  <custom>:  Custom setting via five space-separated values:\n"
	        "               - voice-count:      Integer from 0 to 99\n"
	        "               - level:            Decimal from 0.0 to 10.0\n"
	        "               - speed:            Decimal from 0.1 to 5.0 (in Hz)\n"
	        "               - depth:            Decimal from 0.0 to 21.0\n"
	        "               - modulation-wave:  'sine' or 'triangle'\n"
	        "             For example: 'fsynth_chorus = 3 1.2 0.3 8.0 sine'\n"
	        "\n"
	        "Note: You can disable the FluidSynth chorus and enable the mixer-level chorus\n"
	        "      on the FluidSynth channel instead, or enable both chorus effects at the\n"
	        "      same time. Whether this sounds good depends on the SoundFont and the\n"
	        "      chorus settings being used.");

	str_prop = secprop.AddString(ReverbSettingName, WhenIdle, DefaultReverbSetting);
	;
	str_prop->SetHelp(
	        "Configure the FluidSynth reverb ('auto' by default). Possible values:\n"
	        "\n"
	        "  auto:      Enable reverb (default).\n"
	        "  on:        Enable reverb.\n"
	        "  off:       Disable reverb.\n"
	        "\n"
	        "  <custom>:  Custom setting via four space-separated values:\n"
	        "               - room-size:  Decimal from 0.0 to 1.0\n"
	        "               - damping:    Decimal from 0.0 to 1.0\n"
	        "               - width:      Decimal from 0.0 to 100.0\n"
	        "               - level:      Decimal from 0.0 to 1.0\n"
	        "             For example: 'fsynth_reverb = 0.61 0.23 0.76 0.56'\n"
	        "\n"
	        "Note: You can disable the FluidSynth reverb and enable the mixer-level reverb\n"
	        "      on the FluidSynth channel instead, or enable both reverb effects at the\n"
	        "      same time. Whether this sounds good depends on the SoundFont and the\n"
	        "      reverb settings being used.");

	str_prop = secprop.AddString("fsynth_filter", WhenIdle, "off");
	assert(str_prop);
	str_prop->SetHelp(
	        "Filter for the FluidSynth audio output ('off' by default). Possible values:\n"
	        "\n"
	        "  off:       Don't filter the output (default).\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

#if defined(WIN32)

static std::vector<std_fs::path> get_platform_data_dirs()
{
	return {
	        get_config_dir() / DefaultSoundfontsDir,

	        // C:\soundfonts is the default place where FluidSynth places
	        // default.sf2
	        // https://www.fluidsynth.org/api/fluidsettings.xml#synth.default-soundfont
	        std::string("C:\\") + DefaultSoundfontsDir + "\\",
	};
}

#elif defined(MACOSX)

static std::vector<std_fs::path> get_platform_data_dirs()
{
	return {
	        get_config_dir() / DefaultSoundfontsDir,
	        resolve_home("~/Library/Audio/Sounds/Banks"),
	};
}

#else

static std::vector<std_fs::path> get_platform_data_dirs()
{
	// First priority is user-specific data location
	const auto xdg_data_home = get_xdg_data_home();

	std::vector<std_fs::path> dirs = {
	        xdg_data_home / "dosbox" / DefaultSoundfontsDir,
	        xdg_data_home / DefaultSoundfontsDir,
	        xdg_data_home / "sounds/sf2",
	};

	// Second priority are the $XDG_DATA_DIRS
	for (const auto& data_dir : get_xdg_data_dirs()) {
		dirs.emplace_back(data_dir / DefaultSoundfontsDir);
		dirs.emplace_back(data_dir / "sounds/sf2");
	}

	// Third priority is $XDG_CONF_HOME, for convenience
	dirs.emplace_back(get_config_dir() / DefaultSoundfontsDir);

	return dirs;
}

#endif

static SectionProp* get_fluidsynth_section()
{
	auto section = get_section("fluidsynth");
	assert(section);

	return section;
}

static std::vector<std_fs::path> get_data_dirs()
{
	auto dirs = get_platform_data_dirs();

	auto sf_dir = get_fluidsynth_section()->GetString("soundfont_dir");
	if (!sf_dir.empty()) {
		// The user-provided SoundFont dir might use a different casing
		// of the actual path on Linux & Windows, so we need to
		// normalise that to avoid some subtle bugs downstream (see
		// `find_sf_file()` as well).
		if (path_exists(sf_dir)) {
			std::error_code err = {};
			const auto canonical_path = std_fs::canonical(sf_dir, err); //-V821
			if (!err) {
				dirs.insert(dirs.begin(), canonical_path);
			}
		} else {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "FSYNTH",
			                      "FLUIDSYNTH_INVALID_SOUNDFONT_DIR",
			                      sf_dir.c_str());

			set_section_property_value("fluidsynth", "soundfont_dir", "");
		}
	}
	return dirs;
}

static std_fs::path find_sf_file(const std::string& sf_name)
{
	const std_fs::path sf_path = resolve_home(sf_name);
	if (path_exists(sf_path)) {
		return sf_path;
	}

	for (const auto& dir : get_data_dirs()) {
		for (const auto& sf :
		     {dir / sf_name, dir / (sf_name + SoundFontExtension)}) {
#if 0
			LOG_MSG("FSYNTH: FluidSynth checking if '%s' exists", sf.c_str());
#endif
			if (path_exists(sf)) {
				// Parts of the path come from the `soundfont`
				// setting, and `soundfont = FluidR3_GM.sf2` and
				// `soundfont = fluidr3_gm.sf2` refer to the
				// same file on case-preserving filesystems on
				// Windows and macOS.
				//
				// `std_fs::canonical` returns the absolute path
				// and matches its casing to that of the actual
				// physical file. This prevents certain subtle
				// bugs downstream when we use this path in
				// comparisons.
				std::error_code err = {};
				const auto canonical_path = std_fs::canonical(sf, err);

				if (err) {
					return {};
				}
				return canonical_path;
			}
		}
	}
	return {};
}

static void log_unknown_midi_message(const std::vector<uint8_t>& msg)
{
	auto append_as_hex = [](const std::string& str, const uint8_t val) {
		constexpr char HexChars[] = "0123456789ABCDEF";
		std::string hex_str;

		hex_str.reserve(2);
		hex_str += HexChars[val >> 4];
		hex_str += HexChars[val & 0x0F];

		return str + (str.empty() ? "" : ", ") + hex_str;
	};

	const auto hex_values = std::accumulate(msg.begin(),
	                                        msg.end(),
	                                        std::string(),
	                                        append_as_hex);

	LOG_WARNING("FSYNTH: Unknown MIDI message sequence (hex): %s",
	            hex_values.c_str());
}

// Checks if the passed effect parameter value is within the valid range and
// returns the default if it's not
static std::optional<double> validate_effect_parameter(
        const char* setting_name, const char* param_name,
        const std::string& value, const double min_value,
        const double max_value, const std::string& default_setting_value)
{
	// Convert the string to a double
	const auto val = parse_float(value);

	if (!val || (*val < min_value || *val > max_value)) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "FSYNTH",
		                      "FLUIDSYNTH_INVALID_EFFECT_PARAMETER",
		                      setting_name,
		                      param_name,
		                      value.c_str(),
		                      min_value,
		                      max_value,
		                      default_setting_value.c_str());
	}
	return val;
}

std::optional<ChorusParameters> parse_custom_chorus_params(const std::string& chorus_pref)
{
	const auto params = split(chorus_pref);

	if (params.size() != NumChorusParams) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "FSYNTH",
		                      "FLUIDSYNTH_INVALID_NUM_EFFECT_PARAMS",
		                      ChorusSettingName,
		                      params.size(),
		                      NumChorusParams);
		return {};
	}

	auto validate = [&](const char* param_name,
	                    const std::string& value,
	                    const double min_value,
	                    const double max_value) {
		return validate_effect_parameter(ChorusSettingName,
		                                 param_name,
		                                 value,
		                                 min_value,
		                                 max_value,
		                                 DefaultChorusSetting);
	};

	const auto voice_count_opt = validate("params voice-count", params[0], 0, 99);
	const auto level_opt = validate("params level", params[1], 0.0, 10.0);
	const auto speed_opt = validate("params speed", params[2], 0.1, 5.0);
	const auto depth_opt = validate("params depth", params[3], 0.0, 21.0);

	const auto mod_wave_opt = [&]() -> std::optional<int> {
		if (params[4] != "triangle" && params[4] != "sine") {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "FSYNTH",
			                      "FLUIDSYNTH_INVALID_CHORUS_WAVE",
			                      params[4].c_str(),
			                      DefaultChorusSetting);
			return {};
		} else {
			return (params[4] == "sine"
			                ? fluid_chorus_mod::FLUID_CHORUS_MOD_SINE
			                : fluid_chorus_mod::FLUID_CHORUS_MOD_TRIANGLE);
		}
	}();

	// One or more parameter couldn't be parsed
	if (!(voice_count_opt && level_opt && speed_opt && depth_opt && mod_wave_opt)) {
		return {};
	}

	// Success
	return ChorusParameters{iround(*voice_count_opt),
	                        *level_opt,
	                        *speed_opt,
	                        *depth_opt,
	                        *mod_wave_opt};
}

void MidiDeviceFluidSynth::SetChorusParams(const ChorusParameters& params)
{
	// Apply setting to all groups
	constexpr int FxGroup = -1;

	fluid_synth_set_chorus_group_nr(synth.get(), FxGroup, params.voice_count);

	fluid_synth_set_chorus_group_level(synth.get(), FxGroup, params.level);

	fluid_synth_set_chorus_group_speed(synth.get(), FxGroup, params.speed);

	fluid_synth_set_chorus_group_depth(synth.get(), FxGroup, params.depth);

	fluid_synth_set_chorus_group_type(synth.get(), FxGroup, params.mod_wave);

	LOG_MSG("FSYNTH: Chorus enabled with %d voices at level %.2f, "
	        "%.2f Hz speed, %.2f depth, and %s-wave modulation",
	        params.voice_count,
	        params.level,
	        params.speed,
	        params.depth,
	        (params.mod_wave == fluid_chorus_mod::FLUID_CHORUS_MOD_SINE
	                 ? "sine"
	                 : "triangle"));
}

void MidiDeviceFluidSynth::SetChorus()
{
	const auto chorus_pref = get_fluidsynth_section()->GetString(ChorusSettingName);
	const auto chorus_enabled_opt = parse_bool_setting(chorus_pref);

	auto enable_chorus = [&](const bool enabled) {
		// Apply setting to all groups
		constexpr int FxGroup = -1;
		fluid_synth_chorus_on(synth.get(), FxGroup, enabled);

		if (!enabled) {
			LOG_MSG("FSYNTH: Chorus disabled");
		}
	};

	auto handle_auto_setting = [&]() {
		// Does the SoundFont have known-issues with chorus?
		const auto is_problematic_font =
		        find_in_case_insensitive("FluidR3", soundfont_path.string()) ||
		        find_in_case_insensitive("zdoom", soundfont_path.string());

		if (is_problematic_font) {
			enable_chorus(false);

			LOG_INFO(
			        "FSYNTH: Chorus auto-disabled due to known issues with "
			        "the '%s' soundfont",
			        get_fluidsynth_section()->GetString("soundfont").c_str());
		} else {
			SetChorusParams(DefaultChorusParameters);
			enable_chorus(true);

			// TODO setting the recommended chorus setting for
			// GeneralUserGS will happen here
		}
	};

	if (chorus_enabled_opt) {
		const auto enabled = *chorus_enabled_opt;
		if (enabled) {
			SetChorusParams(DefaultChorusParameters);
		}
		enable_chorus(enabled);

	} else if (chorus_pref == "auto") {
		handle_auto_setting();

	} else {
		if (const auto chorus_params = parse_custom_chorus_params(chorus_pref);
		    chorus_params) {

			SetChorusParams(*chorus_params);
			enable_chorus(true);

		} else {
			set_section_property_value("fluidsynth",
			                           ChorusSettingName,
			                           DefaultChorusSetting);
			handle_auto_setting();
		}
	}
}

std::optional<ReverbParameters> parse_custom_reverb_params(const std::string& reverb_pref)
{
	const auto reverb = split(reverb_pref);

	if (reverb.size() != NumReverbParams) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "FSYNTH",
		                      "FLUIDSYNTH_INVALID_NUM_EFFECT_PARAMS",
		                      ReverbSettingName,
		                      reverb.size(),
		                      NumReverbParams);
		return {};
	}

	auto validate = [&](const char* param_name,
	                    const std::string& value,
	                    const double min_value,
	                    const double max_value) {
		return validate_effect_parameter(ReverbSettingName,
		                                 param_name,
		                                 value,
		                                 min_value,
		                                 max_value,
		                                 DefaultReverbSetting);
	};

	const auto room_size_opt = validate("reverb room-size", reverb[0], 0.0, 1.0);
	const auto damping_opt = validate("reverb damping", reverb[1], 0.0, 1.0);
	const auto width_opt = validate("reverb width", reverb[2], 0.0, 100.0);
	const auto level_opt = validate("reverb level", reverb[3], 0.0, 1.0);

	// One or more parameter couldn't be parsed
	if (!(room_size_opt && damping_opt && width_opt && level_opt)) {
		return {};
	}

	// Success
	return ReverbParameters{*room_size_opt, *damping_opt, *width_opt, *level_opt};
}

void MidiDeviceFluidSynth::SetReverbParams(const ReverbParameters& params)
{
	// Apply setting to all groups
	constexpr int FxGroup = -1;

	fluid_synth_set_reverb_group_roomsize(synth.get(), FxGroup, params.room_size);

	fluid_synth_set_reverb_group_damp(synth.get(), FxGroup, params.damping);

	fluid_synth_set_reverb_group_width(synth.get(), FxGroup, params.width);

	fluid_synth_set_reverb_group_level(synth.get(), FxGroup, params.level);

	LOG_MSG("FSYNTH: Reverb enabled with a %.2f room size, "
	        "%.2f damping, %.2f width, and level %.2f",
	        params.room_size,
	        params.damping,
	        params.width,
	        params.level);
}

void MidiDeviceFluidSynth::SetReverb()
{
	const auto reverb_pref = get_fluidsynth_section()->GetString(ReverbSettingName);
	const auto reverb_enabled_opt = parse_bool_setting(reverb_pref);

	auto enable_reverb = [&](const bool enabled) {
		// Apply setting to all groups
		constexpr int FxGroup = -1;
		fluid_synth_reverb_on(synth.get(), FxGroup, enabled);

		if (!enabled) {
			LOG_MSG("FSYNTH: Reverb disabled");
		}
	};

	auto handle_auto_setting = [&]() {
		// TODO setting the recommended reverb setting for GeneralUserGS
		// will happen here
		SetReverbParams(DefaultReverbParameters);
		enable_reverb(true);
	};

	if (reverb_enabled_opt) {
		const auto enabled = *reverb_enabled_opt;
		if (enabled) {
			SetReverbParams(DefaultReverbParameters);
		}
		enable_reverb(enabled);

	} else if (reverb_pref == "auto") {
		handle_auto_setting();

	} else {
		if (const auto reverb_params = parse_custom_reverb_params(reverb_pref);
		    reverb_params) {

			SetReverbParams(*reverb_params);
			enable_reverb(true);

		} else {
			set_section_property_value("fluidsynth",
			                           ReverbSettingName,
			                           DefaultReverbSetting);
			handle_auto_setting();
		}
	}
}

void MidiDeviceFluidSynth::SetVolume(const int volume_percent)
{
	const auto gain = static_cast<float>(volume_percent) / 100.0f;
	fluid_synth_set_gain(synth.get(), gain);
}

MidiDeviceFluidSynth::MidiDeviceFluidSynth()
{
	fluid_set_log_function(FLUID_DBG, NULL, NULL);

#ifdef NDEBUG
	fluid_set_log_function(FLUID_INFO, NULL, NULL);
	fluid_set_log_function(FLUID_ERR, NULL, NULL);
	fluid_set_log_function(FLUID_WARN, NULL, NULL);
#endif

	FluidSynthSettingsPtr fluid_settings(new_fluid_settings(),
	                                     delete_fluid_settings);
	if (!fluid_settings) {
		const auto msg = "FSYNTH: Failed to initialise the FluidSynth settings";
		LOG_ERR("%s", msg);
		throw std::runtime_error(msg);
	}

	auto section = get_fluidsynth_section();

	// Detailed explanation of all available FluidSynth settings:
	// http://www.fluidsynth.org/api/fluidsettings.xml

	// Per the FluidSynth API, the sample-rate should be part of the
	// settings used to instantiate the synth, so we use the mixer's
	// native rate to configure FluidSynth.
	const auto sample_rate_hz = MIXER_GetSampleRate();
	ms_per_audio_frame        = MillisInSecond / sample_rate_hz;

	fluid_settings_setnum(fluid_settings.get(), "synth.sample-rate", sample_rate_hz);

	FluidSynthPtr fluid_synth(new_fluid_synth(fluid_settings.get()),
	                          delete_fluid_synth);
	if (!fluid_synth) {
		const auto msg = "FSYNTH: Failed to create the FluidSynth synthesizer";
		LOG_ERR("%s", msg);
		throw std::runtime_error(msg);
	}

	// Load the requested SoundFont or quit if none provided
	const auto sf_name = section->GetString("soundfont");
	const auto sf_path = find_sf_file(sf_name);

	constexpr auto ResetPresets = true;
	if (fluid_synth_sfload(fluid_synth.get(),
	                       sf_path.string().c_str(),
	                       ResetPresets) == FLUID_FAILED) {

		const auto msg = format_str("FSYNTH: Error loading SoundFont '%s'",
		                            sf_name.c_str());

		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "FSYNTH",
		                      "FLUIDSYNTH_ERROR_LOADING_SOUNDFONT",
		                      sf_name.c_str());

		throw std::runtime_error(msg);
	}

	synth    = std::move(fluid_synth);
	settings = std::move(fluid_settings);

	const auto volume_percent = section->GetInt("soundfont_volume");
	SetVolume(volume_percent);

	// Let the user know that the SoundFont was loaded
	if (volume_percent == 100) {
		LOG_MSG("FSYNTH: Using SoundFont '%s'", sf_path.string().c_str());
	} else {
		LOG_MSG("FSYNTH: Using SoundFont '%s' with volume scaled to %d%%",
		        sf_path.string().c_str(),
		        volume_percent);
	}

	// Applies setting to all groups
	constexpr int FxGroup = -1;

	// Use a 7th-order (highest) polynomial to generate MIDI channel
	// waveforms
	fluid_synth_set_interp_method(fluid_synth.get(), FxGroup, FLUID_INTERP_HIGHEST);

	// Always use XG/GS mode which emulates the concave curve specific to
	// the Roland Sound Canvas family of sound modules. In this mode the
	// portamento time is 7 bits wide, using only CC5, and the concave curve
	// ranges from 0s to 480s. The curve was reverse engineered from a
	// Roland SC-55 v1.21 hardware unit.
	fluid_synth_set_portamento_time_mode(fluid_synth.get(),
	                                     FLUID_PORTAMENTO_TIME_MODE_XG_GS);

	SetChorus();
	SetReverb();

	MIXER_LockMixerThread();

	// Set up the mixer callback
	const auto mixer_callback = std::bind(&MidiDeviceFluidSynth::MixerCallback,
	                                      this,
	                                      std::placeholders::_1);

	mixer_channel = MIXER_AddChannel(mixer_callback,
	                                 sample_rate_hz,
	                                 ChannelName::FluidSynth,
	                                 {ChannelFeature::Sleep,
	                                  ChannelFeature::Stereo,
	                                  ChannelFeature::ReverbSend,
	                                  ChannelFeature::ChorusSend,
	                                  ChannelFeature::Synthesizer});

	// FluidSynth renders float audio frames between -1.0f and
	// +1.0f, so we ask the channel to scale all the samples up to
	// its 0db level.
	mixer_channel->Set0dbScalar(Max16BitSampleValue);

	SetFilter();

	// Double the baseline PCM prebuffer because MIDI is demanding
	// and bursty. The mixer's default of ~20 ms becomes 40 ms here,
	// which gives slower systems a better chance to keep up (and
	// prevent their audio frame FIFO from running dry).
	const auto render_ahead_ms = MIXER_GetPreBufferMs() * 2;

	// Size the out-bound audio frame FIFO
	assertm(sample_rate_hz >= 8000, "Sample rate must be at least 8 kHz");

	const auto audio_frames_per_ms = iround(sample_rate_hz / MillisInSecond);
	audio_frame_fifo.Resize(
	        check_cast<size_t>(render_ahead_ms * audio_frames_per_ms));

	// Size the in-bound work FIFO
	work_fifo.Resize(MaxMidiWorkFifoSize);

	soundfont_path = sf_path;

	// Start rendering audio
	const auto render = std::bind(&MidiDeviceFluidSynth::Render, this);
	renderer          = std::thread(render);
	set_thread_name(renderer, "dosbox:fsynth");

	// Start playback
	MIXER_UnlockMixerThread();
}

MidiDeviceFluidSynth::~MidiDeviceFluidSynth()
{
	LOG_MSG("FSYNTH: Shutting down");

	if (had_underruns) {
		LOG_WARNING(
		        "FSYNTH: Fix underruns by lowering the CPU load, increasing "
		        "the 'prebuffer' or 'blocksize' settings, or using a simpler SoundFont");
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

void MidiDeviceFluidSynth::SetFilter()
{
	const std::string filter_prefs = get_fluidsynth_section()->GetString(
	        "fsynth_filter");

	if (!mixer_channel->TryParseAndSetCustomFilter(filter_prefs)) {
		if (!has_false(filter_prefs)) {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "FSYNTH",
			                      "PROGRAM_CONFIG_INVALID_SETTING",
			                      "fsynth_filter",
			                      filter_prefs.c_str(),
			                      "off");
		}

		mixer_channel->SetHighPassFilter(FilterState::Off);
		mixer_channel->SetLowPassFilter(FilterState::Off);

		set_section_property_value("fluidsynth", "fsynth_filter", "off");
	}
}

int MidiDeviceFluidSynth::GetNumPendingAudioFrames()
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
void MidiDeviceFluidSynth::SendMidiMessage(const MidiMessage& msg)
{
	std::vector<uint8_t> message(msg.data.begin(), msg.data.end());

	MidiWork work{std::move(message),
	              GetNumPendingAudioFrames(),
	              MessageType::Channel,
	              PIC_AtomicIndex()};

	work_fifo.Enqueue(std::move(work));
}

// The request to play the sysex message is placed in the MIDI work FIFO
void MidiDeviceFluidSynth::SendSysExMessage(uint8_t* sysex, size_t len)
{
	std::vector<uint8_t> message(sysex, sysex + len);

	MidiWork work{std::move(message),
	              GetNumPendingAudioFrames(),
	              MessageType::SysEx,
	              PIC_AtomicIndex()};

	work_fifo.Enqueue(std::move(work));
}

void MidiDeviceFluidSynth::ApplyChannelMessage(const std::vector<uint8_t>& msg)
{
	const auto status_byte = msg[0];
	const auto controller  = msg[1];
	const auto status      = get_midi_status(status_byte);
	const auto channel     = get_midi_channel(status_byte);

	// clang-format off
	switch (status) {
	case MidiStatus::NoteOff:         fluid_synth_noteoff(         synth.get(), channel, controller);             break;
	case MidiStatus::NoteOn:          fluid_synth_noteon(          synth.get(), channel, controller, msg[2]);     break;
	case MidiStatus::PolyKeyPressure: fluid_synth_key_pressure(    synth.get(), channel, controller, msg[2]);     break;
	case MidiStatus::ControlChange:   fluid_synth_cc(              synth.get(), channel, controller, msg[2]);     break;
	case MidiStatus::ProgramChange:   fluid_synth_program_change(  synth.get(), channel, controller);             break;
	case MidiStatus::ChannelPressure: fluid_synth_channel_pressure(synth.get(), channel, controller);             break;
	case MidiStatus::PitchBend:       fluid_synth_pitch_bend(      synth.get(), channel, msg[1] + (msg[2] << 7)); break;
	default: log_unknown_midi_message(msg); break;
	}
	// clang-format on
}

// Apply the sysex message to the service
void MidiDeviceFluidSynth::ApplySysExMessage(const std::vector<uint8_t>& msg)
{
	const auto data = reinterpret_cast<const char*>(msg.data());
	const auto n    = static_cast<int>(msg.size());

	fluid_synth_sysex(synth.get(), data, n, nullptr, nullptr, nullptr, false);
}

// The callback operates at the audio frame-level, steadily adding
// samples to the mixer until the requested numbers of audio frames is
// met.
void MidiDeviceFluidSynth::MixerCallback(const int requested_audio_frames)
{
	assert(mixer_channel);

	// Report buffer underruns
	constexpr auto WarningPercent = 5.0f;

	if (const auto percent_full = audio_frame_fifo.GetPercentFull();
	    percent_full < WarningPercent) {
		static auto iteration = 0;
		if (iteration++ % 100 == 0) {
			LOG_WARNING("FSYNTH: Audio buffer underrun");
		}
		had_underruns = true;
	}

	static std::vector<AudioFrame> audio_frames = {};

	const auto has_dequeued = audio_frame_fifo.BulkDequeue(audio_frames,
	                                                       requested_audio_frames);

	if (has_dequeued) {
		assert(check_cast<int>(audio_frames.size()) == requested_audio_frames);
		mixer_channel->AddSamples_sfloat(requested_audio_frames,
		                                 &audio_frames[0][0]);

		last_rendered_ms = PIC_AtomicIndex();
	} else {
		assert(!audio_frame_fifo.IsRunning());
		mixer_channel->AddSilence();
	}
}

void MidiDeviceFluidSynth::RenderAudioFramesToFifo(const int num_audio_frames)
{
	static std::vector<AudioFrame> audio_frames = {};

	// Maybe expand the vector
	if (check_cast<int>(audio_frames.size()) < num_audio_frames) {
		audio_frames.resize(num_audio_frames);
	}

	fluid_synth_write_float(synth.get(),
	                        num_audio_frames,
	                        &audio_frames[0][0],
	                        0,
	                        2,
	                        &audio_frames[0][0],
	                        1,
	                        2);

	audio_frame_fifo.BulkEnqueue(audio_frames, num_audio_frames);
}

void MidiDeviceFluidSynth::ProcessWorkFromFifo()
{
	const auto work = work_fifo.Dequeue();
	if (!work) {
		return;
	}

#if 0
	// To log inter-cycle rendering
	if (work->num_pending_audio_frames > 0) {
		LOG_MSG("FSYNTH: %2u audio frames prior to %s message, followed by "
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
		assert(work->message.size() <= MaxMidiMessageLen);
		ApplyChannelMessage(work->message);
	} else {
		assert(work->message_type == MessageType::SysEx);
		ApplySysExMessage(work->message);
	}
}

// Keep the fifo populated with freshly rendered buffers
void MidiDeviceFluidSynth::Render()
{
	while (work_fifo.IsRunning()) {
		work_fifo.IsEmpty() ? RenderAudioFramesToFifo()
		                    : ProcessWorkFromFifo();
	}
}

std_fs::path MidiDeviceFluidSynth::GetSoundFontPath()
{
	return soundfont_path;
}

std::string format_sf_line(size_t width, const std_fs::path& sf_path)
{
	assert(width > 0);
	std::vector<char> line_buf(width);

	const auto& name = sf_path.filename().string();
	const auto& path = simplify_path(sf_path).string();

	snprintf(line_buf.data(), width, "%-16s - %s", name.c_str(), path.c_str());
	std::string line = line_buf.data();

	// Formatted line did not fill the whole buffer - no further
	// formatting is necessary.
	if (line.size() + 1 < width) {
		return line;
	}

	// The description was too long and got trimmed; place three
	// dots in the end to make it clear to the user.
	const std::string cutoff = "...";

	assert(line.size() > cutoff.size());

	const auto start = line.end() - static_cast<int>(cutoff.size());
	line.replace(start, line.end(), cutoff);

	return line;
}

void FSYNTH_ListDevices(MidiDeviceFluidSynth* device, Program* caller)
{
	const size_t term_width = INT10_GetTextColumns();

	constexpr auto Indent = "  ";

	auto write_line = [&](const std_fs::path& sf_path) {
		const auto line = format_sf_line(term_width - 2, sf_path);

		const auto do_highlight = [&] {
			if (device) {
				const auto curr_sf_path = device->GetSoundFontPath();
				return curr_sf_path == sf_path;
			}
			return false;
		}();

		if (do_highlight) {
			constexpr auto Green = "[color=light-green]";
			constexpr auto Reset = "[reset]";

			const auto output = format_str("%s* %s%s\n",
			                               Green,
			                               line.c_str(),
			                               Reset);

			caller->WriteOut(convert_ansi_markup(output));
		} else {
			caller->WriteOut("%s%s\n", Indent, line.c_str());
		}
	};

	// Print SoundFont found from user config.
	std::error_code err = {};

	std::vector<std_fs::path> sf_files = {};

	// Go through all SoundFont directories and list all .sf2 files.
	for (const auto& dir_path : get_data_dirs()) {
		for (const auto& entry : std_fs::directory_iterator(dir_path, err)) {
			if (err) {
				// Problem iterating, so skip the directory
				break;
			}

			if (!entry.is_regular_file(err)) {
				// Problem with entry, move onto the
				// next one
				continue;
			}

			const auto& sf_path = entry.path();

			// Is it an .sf2 file?
			auto ext = sf_path.extension().string();
			lowcase(ext);
			if (ext != SoundFontExtension) {
				continue;
			}

			sf_files.emplace_back(sf_path);
		}
	}

	std::sort(sf_files.begin(),
	          sf_files.end(),
	          [](const std_fs::path& a, const std_fs::path& b) {
		          return a.filename() < b.filename();
	          });

	if (sf_files.empty()) {
		caller->WriteOut("%s%s\n",
		                 Indent,
		                 MSG_Get("FLUIDSYNTH_NO_SOUNDFONTS").c_str());
	} else {
		for (const auto& path : sf_files) {
			write_line(path);
		}
	}

	caller->WriteOut("\n");
}

void FSYNTH_Init()
{
	if (const auto device = MIDI_GetCurrentDevice();
	    device && device->GetName() == MidiDeviceName::FluidSynth) {
		MIDI_Init();
	}
}

static void notify_fluidsynth_setting_updated([[maybe_unused]] SectionProp& section,
                                              const std::string& prop_name)
{
	const auto device = dynamic_cast<MidiDeviceFluidSynth*>(
	        MIDI_GetCurrentDevice());

	if (!device) {
		return;
	}

	if (prop_name == ChorusSettingName) {
		device->SetChorus();

	} else if (prop_name == ReverbSettingName) {
		device->SetReverb();

	} else if (prop_name == "fsynth_filter") {
		device->SetFilter();

	} else if (prop_name == "soundfont_volume") {
		device->SetVolume(section.GetInt("soundfont_volume"));

	} else if (prop_name == "soundfont_dir") {
		// no-op; will take effect when loading a SoundFont

	} else {
		MIDI_Init();
	}
}

static void register_fluidsynth_text_messages()
{
	MSG_Add("FLUIDSYNTH_NO_SOUNDFONTS", "No available SoundFonts");

	MSG_Add("FLUIDSYNTH_INVALID_SOUNDFONT_DIR",
	        "Invalid [color=light-green]soundfont_dir[reset] setting; "
	        "cannot open directory [color=white]'%s'[reset], using ''");

	MSG_Add("FLUIDSYNTH_ERROR_LOADING_SOUNDFONT",
	        "Error loading SoundFont [color=white]'%s'[reset]");

	MSG_Add("FLUIDSYNTH_INVALID_EFFECT_PARAMETER",
	        "Invalid [color=light-green]'%s'[reset] synth parameter (%s): "
	        "[color=white]%s[reset];\n"
	        "must be between %.2f and %.2f, using [color=white]'%s'[reset]");

	MSG_Add("FLUIDSYNTH_INVALID_CHORUS_WAVE",
	        "Invalid [color=light-green]'fsynth_chorus'[reset] synth parameter "
	        "(modulation wave type): [color=white]%s[reset];\n"
	        "must be [color=white]'sine'[reset] or [color=white]'triangle'[reset]");

	MSG_Add("FLUIDSYNTH_INVALID_NUM_EFFECT_PARAMS",
	        "Invalid number of [color=light-green]'%s'[reset] parameters: "
	        "[color=white]%d[/reset];\n"
	        "must be %d space-separated values, using [color=white]'auto'[reset]");
}

void FSYNTH_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("fluidsynth");
	section->AddUpdateHandler(notify_fluidsynth_setting_updated);

	init_fluidsynth_config_settings(*section);
	register_fluidsynth_text_messages();
}
