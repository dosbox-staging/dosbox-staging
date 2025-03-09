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

#include "midi_fluidsynth.h"

#include <bitset>
#include <cassert>
#include <compare>
#include <numeric>
#include <string>
#include <tuple>
#include <vector>

#include "../ints/int10.h"
#include "ansi_code_markup.h"
#include "channel_names.h"
#include "control.h"
#include "cross.h"
#include "fs_utils.h"
#include "math_utils.h"
#include "mixer.h"
#include "pic.h"
#include "programs.h"
#include "string_utils.h"
#include "support.h"

constexpr auto SoundFontExtension = ".sf2";

/**
 * Platform specific FluidSynth shared library name
 */
#if defined(WIN32)
constexpr const char fsynth_dynlib_file[] = "libfluidsynth-3.dll";
#elif defined(MACOSX)
constexpr const char fsynth_dynlib_file[] = "libfluidsynth.3.dylib";
#else
constexpr const char fsynth_dynlib_file[] = "libfluidsynth.so.3";
#endif


struct FSynthVersion
{
	int major = 0;
	int minor = 0;
	int micro = 0;

	// Workaround for clang bug
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
	auto operator<=>(const FSynthVersion&) const = default;
	#pragma clang diagnostic pop
};

constexpr FSynthVersion min_fsynth_version = {2, 2, 3};
constexpr FSynthVersion max_fsynth_version_exclusive = {3, 0, 0};

namespace fsynth 
{

/* The following enums are copied from FluidSynth */

/**
 * Chorus modulation waveform type.
 */
enum fluid_chorus_mod
{
    FLUID_CHORUS_MOD_SINE = 0,            /**< Sine wave chorus modulation */
    FLUID_CHORUS_MOD_TRIANGLE = 1         /**< Triangle wave chorus modulation */
};

enum fluid_interp
{
    FLUID_INTERP_NONE = 0,        /**< No interpolation: Fastest, but questionable audio quality */
    FLUID_INTERP_LINEAR = 1,      /**< Straight-line interpolation: A bit slower, reasonable audio quality */
    FLUID_INTERP_4THORDER = 4,    /**< Fourth-order interpolation, best quality, the default */

    /**
     * Seventh-point sinc interpolation
     * @note This interpolation method was believed to provide highest quality. However, in Feb. 2025 it was discovered
     * that for certain samples it does introduce ringing artifacts, which
     * are not present in the 4th order interpolation. This is not a bug, it's rather a limitation of only using 7 points for the sinc interpolation.
     */
    FLUID_INTERP_7THORDER = 7,

    FLUID_INTERP_DEFAULT = FLUID_INTERP_4THORDER, /**< Default interpolation method */
    FLUID_INTERP_HIGHEST = FLUID_INTERP_7THORDER, /**< Highest interpolation method */
};

/**
 * FluidSynth log levels.
 */
enum fluid_log_level
{
    FLUID_PANIC,   /**< The synth can't function correctly any more */
    FLUID_ERR,     /**< Serious error occurred */
    FLUID_WARN,    /**< Warning */
    FLUID_INFO,    /**< Verbose informational messages */
    FLUID_DBG,     /**< Debugging messages */
    LAST_LOG_LEVEL /**< @internal This symbol is not part of the public API and ABI
                     stability guarantee and may change at any time! */
};

/* This typedef comes from FluidSynth */

/**
 * Log function handler callback type used by fluid_set_log_function().
 *
 * @param level Log level (#fluid_log_level)
 * @param message Log message text
 * @param data User data pointer supplied to fluid_set_log_function().
 */
typedef void (*fluid_log_function_t)(int level, const char *message, void *data);

/**
 * FluidSynth dynamic library handle
 */
static dynlib_handle fsynth_lib = {};

/* The following function pointers will be set to their corresponding symbols in the FluidSynth library */

// clang-format off

void (*delete_fluid_settings)(fluid_settings_t*) = nullptr;
void (*delete_fluid_synth)   (fluid_synth_t*)    = nullptr;

void (*fluid_version)(int *major, int *minor, int *micro) = nullptr;

fluid_settings_t* (*new_fluid_settings)(void)                       = nullptr;
fluid_synth_t*       (*new_fluid_synth)(fluid_settings_t *settings) = nullptr;

fluid_log_function_t (*fluid_set_log_function)(int level, fluid_log_function_t fun, void *data)      = nullptr;

int  (*fluid_settings_setnum)(fluid_settings_t *settings, const char *name, double val)              = nullptr;

int               (*fluid_synth_chorus_on)(fluid_synth_t *synth, int fx_group, int on)               = nullptr;
int     (*fluid_synth_set_chorus_group_nr)(fluid_synth_t *synth, int fx_group, int nr)               = nullptr;
int  (*fluid_synth_set_chorus_group_level)(fluid_synth_t *synth, int fx_group, double level)         = nullptr;
int  (*fluid_synth_set_chorus_group_speed)(fluid_synth_t *synth, int fx_group, double speed)         = nullptr;
int  (*fluid_synth_set_chorus_group_depth)(fluid_synth_t *synth, int fx_group, double depth_ms)      = nullptr;
int   (*fluid_synth_set_chorus_group_type)(fluid_synth_t *synth, int fx_group, int type)             = nullptr;

int                  (*fluid_synth_reverb_on)(fluid_synth_t *synth, int fx_group, int on)            = nullptr;
int  (*fluid_synth_set_reverb_group_roomsize)(fluid_synth_t *synth, int fx_group, double roomsize)   = nullptr;
int      (*fluid_synth_set_reverb_group_damp)(fluid_synth_t *synth, int fx_group, double damping)    = nullptr;
int     (*fluid_synth_set_reverb_group_width)(fluid_synth_t *synth, int fx_group, double width)      = nullptr;
int     (*fluid_synth_set_reverb_group_level)(fluid_synth_t *synth, int fx_group, double level)      = nullptr;

int            (*fluid_synth_sfcount)(fluid_synth_t *synth)                                          = nullptr;
int             (*fluid_synth_sfload)(fluid_synth_t *synth, const char *filename, int reset_presets) = nullptr;
void          (*fluid_synth_set_gain)(fluid_synth_t *synth, float gain)                              = nullptr;
int  (*fluid_synth_set_interp_method)(fluid_synth_t *synth, int chan, int interp_method)             = nullptr;
int            (*fluid_synth_noteoff)(fluid_synth_t *synth, int chan, int key)                       = nullptr;
int             (*fluid_synth_noteon)(fluid_synth_t *synth, int chan, int key, int vel)              = nullptr;
int       (*fluid_synth_key_pressure)(fluid_synth_t *synth, int chan, int key, int val)              = nullptr;
int                 (*fluid_synth_cc)(fluid_synth_t *synth, int chan, int ctrl, int val)             = nullptr;
int     (*fluid_synth_program_change)(fluid_synth_t *synth, int chan, int program)                   = nullptr;
int   (*fluid_synth_channel_pressure)(fluid_synth_t *synth, int chan, int val)                       = nullptr;
int         (*fluid_synth_pitch_bend)(fluid_synth_t *synth, int chan, int val)                       = nullptr;

int        (*fluid_synth_sysex)(fluid_synth_t *synth, 
                                const char *data, 
                                int len,
                                char *response, 
                                int *response_len, 
                                int *handled, 
                                int dryrun) = nullptr;
                                
int  (*fluid_synth_write_float)(fluid_synth_t *synth, 
                                int len,
                                void *lout, 
                                int loff, 
                                int lincr,
                                void *rout, 
                                int roff, 
                                int rincr) = nullptr;

// clang-format on

} // namespace fsynth

/**
 * A filthy macro to make resolving library symbols a little less verbose, 
 * while still including robust error checking.
 */
#define DB_GET_FSYNTH_SYM(sym) sym = (decltype(sym))dynlib_get_symbol(fsynth_lib, #sym); \
	if (!sym) { dynlib_close(fsynth_lib); return DynLibResult::ResolveSymErr; }

/**
 * Load the FluidSynth library and resolve all required symbols.
 * 
 * If the library is already loaded, does nothing.
 * 
 * IMPORTANT: If adding a new symbol above, remember to resolve 
 * the symbol in this function, otherwise Dosbox is likely to segfault.
 */
static DynLibResult load_fsynth_dynlib()
{
	using namespace fsynth;
	if (!fsynth_lib) {
		fsynth_lib = dynlib_open(fsynth_dynlib_file);
		if (!fsynth_lib) {
			return DynLibResult::LibOpenErr;
		}
		DB_GET_FSYNTH_SYM(fluid_version);
        DB_GET_FSYNTH_SYM(fluid_set_log_function);

		DB_GET_FSYNTH_SYM(new_fluid_settings);
		DB_GET_FSYNTH_SYM(new_fluid_synth);
		
		DB_GET_FSYNTH_SYM(delete_fluid_settings);
		DB_GET_FSYNTH_SYM(delete_fluid_synth);
		DB_GET_FSYNTH_SYM(fluid_settings_setnum);

		DB_GET_FSYNTH_SYM(fluid_synth_chorus_on);
		DB_GET_FSYNTH_SYM(fluid_synth_set_chorus_group_nr);
		DB_GET_FSYNTH_SYM(fluid_synth_set_chorus_group_level);
		DB_GET_FSYNTH_SYM(fluid_synth_set_chorus_group_speed);
		DB_GET_FSYNTH_SYM(fluid_synth_set_chorus_group_depth);
		DB_GET_FSYNTH_SYM(fluid_synth_set_chorus_group_type);

		DB_GET_FSYNTH_SYM(fluid_synth_reverb_on);
		DB_GET_FSYNTH_SYM(fluid_synth_set_reverb_group_roomsize);
		DB_GET_FSYNTH_SYM(fluid_synth_set_reverb_group_damp);
		DB_GET_FSYNTH_SYM(fluid_synth_set_reverb_group_width);
		DB_GET_FSYNTH_SYM(fluid_synth_set_reverb_group_level);

		DB_GET_FSYNTH_SYM(fluid_synth_sfcount);
		DB_GET_FSYNTH_SYM(fluid_synth_sfload);
		DB_GET_FSYNTH_SYM(fluid_synth_set_gain);
		DB_GET_FSYNTH_SYM(fluid_synth_set_interp_method);
		DB_GET_FSYNTH_SYM(fluid_synth_noteoff);
		DB_GET_FSYNTH_SYM(fluid_synth_noteon);
		DB_GET_FSYNTH_SYM(fluid_synth_key_pressure);
		DB_GET_FSYNTH_SYM(fluid_synth_cc);
		DB_GET_FSYNTH_SYM(fluid_synth_program_change);
		DB_GET_FSYNTH_SYM(fluid_synth_channel_pressure);
		DB_GET_FSYNTH_SYM(fluid_synth_pitch_bend);
		DB_GET_FSYNTH_SYM(fluid_synth_sysex);
		DB_GET_FSYNTH_SYM(fluid_synth_write_float);

        /* Keep ERR and PANIC logging only */
        for (auto level : {FLUID_DBG, FLUID_INFO, FLUID_WARN}) {
            fluid_set_log_function(level, nullptr, nullptr);
        }
	}
	return DynLibResult::Success;
}

static void init_fluidsynth_dosbox_settings(Section_prop& secprop)
{
	constexpr auto WhenIdle = Property::Changeable::WhenIdle;

	// Name 'default.sf2' picks the default SoundFont if it's installed
	// in the OS (usually "Fluid_R3").
	auto str_prop = secprop.Add_string("soundfont", WhenIdle, "default.sf2");
	str_prop->Set_help(
	        "Name or path of SoundFont file to use ('default.sf2' by default).\n"
	        "The SoundFont will be looked up in the following locations in order:\n"
	        "  - The user-defined SoundFont directory (see 'soundfont_dir').\n"
	        "  - The 'soundfonts' directory in your DOSBox configuration directory.\n"
	        "  - Other common system locations.\n"
	        "The '.sf2' extension can be omitted. You can use paths relative to the above\n"
	        "locations or absolute paths as well.\n"
	        "Note: Run `MIXER /LISTMIDI` to see the list of available SoundFonts.");

	str_prop = secprop.Add_string("soundfont_dir", WhenIdle, "");
	str_prop->Set_help(
	        "Extra user-defined SoundFont directory (unset by default).\n"
	        "If this is set, SoundFonts are looked up in this directory first, then in the\n"
	        "the standard system locations.");

	constexpr auto DefaultVolume = 100;
	constexpr auto MinVolume     = 1;
	constexpr auto MaxVolume     = 800;

	auto int_prop = secprop.Add_int("soundfont_volume", WhenIdle, DefaultVolume);
	int_prop->SetMinMax(MinVolume, MaxVolume);
	int_prop->Set_help(
	        format_str("Set the SoundFont's volume as a percentage (%d by default).\n"
	                   "This is useful for normalising the volume of different SoundFonts.\n"
	                   "The percentage value can range from %d to %d.",
	                   DefaultVolume,
	                   MinVolume,
	                   MaxVolume));

	str_prop = secprop.Add_string("fsynth_chorus", WhenIdle, "auto");
	str_prop->Set_help(
	        "Configure the FluidSynth chorus. Possible values:\n"
	        "  auto:      Enable chorus, except for known problematic SoundFonts (default).\n"
	        "  on:        Always enable chorus.\n"
	        "  off:       Disable chorus.\n"
	        "  <custom>:  Custom setting via five space-separated values:\n"
	        "               - voice-count:      Integer from 0 to 99\n"
	        "               - level:            Decimal from 0.0 to 10.0\n"
	        "               - speed:            Decimal from 0.1 to 5.0 (in Hz)\n"
	        "               - depth:            Decimal from 0.0 to 21.0\n"
	        "               - modulation-wave:  'sine' or 'triangle'\n"
	        "             For example: 'fsynth_chorus = 3 1.2 0.3 8.0 sine'\n"
	        "Note: You can disable the FluidSynth chorus and enable the mixer-level chorus\n"
	        "      on the FluidSynth channel instead, or enable both chorus effects at the\n"
	        "      same time. Whether this sounds good depends on the SoundFont and the\n"
	        "      chorus settings being used.");

	str_prop = secprop.Add_string("fsynth_reverb", WhenIdle, "auto");
	str_prop->Set_help(
	        "Configure the FluidSynth reverb. Possible values:\n"
	        "  auto:      Enable reverb (default).\n"
	        "  on:        Enable reverb.\n"
	        "  off:       Disable reverb.\n"
	        "  <custom>:  Custom setting via four space-separated values:\n"
	        "               - room-size:  Decimal from 0.0 to 1.0\n"
	        "               - damping:    Decimal from 0.0 to 1.0\n"
	        "               - width is:   Decimal from 0.0 to 100.0\n"
	        "               - level is:   Decimal from 0.0 to 1.0\n"
	        "             For example: 'fsynth_reverb = 0.61 0.23 0.76 0.56'\n"
	        "Note: You can disable the FluidSynth reverb and enable the mixer-level reverb\n"
	        "      on the FluidSynth channel instead, or enable both reverb effects at the\n"
	        "      same time. Whether this sounds good depends on the SoundFont and the\n"
	        "      reverb settings being used.");

	str_prop = secprop.Add_string("fsynth_filter", WhenIdle, "off");
	assert(str_prop);
	str_prop->Set_help(
	        "Filter for the FluidSynth audio output:\n"
	        "  off:       Don't filter the output (default).\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

#if defined(WIN32)

static std::vector<std_fs::path> get_platform_data_dirs()
{
	return {
	        GetConfigDir() / DefaultSoundfontsDir,

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
	        GetConfigDir() / DefaultSoundfontsDir,
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
	dirs.emplace_back(GetConfigDir() / DefaultSoundfontsDir);

	return dirs;
}

#endif

static Section_prop* get_fluidsynth_section()
{
	assert(control);

	auto sec = static_cast<Section_prop*>(control->GetSection("fluidsynth"));
	assert(sec);

	return sec;
}

static std::vector<std_fs::path> get_data_dirs()
{
	auto dirs = get_platform_data_dirs();

	auto sf_dir = get_fluidsynth_section()->Get_string("soundfont_dir");
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
			LOG_WARNING(
			        "FSYNTH: Invalid `soundfont_dir` setting, "
			        "cannot open directory '%s'; using ''",
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
				const auto canonical_path =
				        std_fs::canonical(sf, err);

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

// Checks if the passed value is within valid range and returns the
// default if it's not
static float validate_setting(const char* name, const std::string& str_val,
                              const double def_val, const double min_val,
                              const double max_val)
{
	// Convert the string to a double
	const auto val = atof(str_val.c_str());

	if (val < min_val || val > max_val) {
		LOG_WARNING(
		        "FSYNTH: Invalid %s setting (%s), needs to be between "
		        "%.2f and %.2f: using default (%.2f)",
		        name,
		        str_val.c_str(),
		        min_val,
		        max_val,
		        def_val);
		return def_val;
	}
	return val;
}

static void setup_chorus(fsynth::fluid_synth_t* synth, const std_fs::path& sf_path)
{
	using namespace fsynth;

	assert(synth);

	const auto section = get_fluidsynth_section();

	// Get the user's chorus settings
	const auto chorus = split(section->Get_string("fsynth_chorus"));

	bool chorus_enabled = !chorus.empty() && chorus[0] != "off";

	// Does the SoundFont have known-issues with chorus?
	const auto is_problematic_font =
	        find_in_case_insensitive("FluidR3", sf_path.string()) ||
	        find_in_case_insensitive("zdoom", sf_path.string());

	if (chorus_enabled && chorus[0] == "auto" && is_problematic_font) {
		chorus_enabled = false;
		LOG_INFO("FSYNTH: Chorus auto-disabled due to known issues with the '%s' soundfont",
		         section->Get_string("soundfont").c_str());
	}

	// Default chorus settings
	auto chorus_voice_count_f = 3.0;
	auto chorus_level         = 1.2;
	auto chorus_speed         = 0.3;
	auto chorus_depth         = 8.0;
	auto chorus_mod_wave      = fluid_chorus_mod::FLUID_CHORUS_MOD_SINE;

	// Apply custom chorus settings if provided
	if (chorus_enabled && chorus.size() > 1) {
		if (chorus.size() == 5) {
			chorus_voice_count_f = validate_setting("chorus voice-count",
			                                        chorus[0],
			                                        chorus_voice_count_f,
			                                        0,
			                                        99);

			chorus_level = validate_setting("chorus level",
			                                chorus[1],
			                                chorus_level,
			                                0.0,
			                                10.0);

			chorus_speed = validate_setting(
			        "chorus speed", chorus[2], chorus_speed, 0.1, 5.0);

			chorus_depth = validate_setting("chorus depth",
			                                chorus[3],
			                                chorus_depth,
			                                0.0,
			                                21.0);

			if (chorus[4] == "triangle") {
				chorus_mod_wave = fluid_chorus_mod::FLUID_CHORUS_MOD_TRIANGLE;

			} else if (chorus[4] != "sine") { // default is sine
				LOG_WARNING(
				        "FSYNTH: Invalid chorus modulation wave type ('%s'), "
				        "needs to be 'sine' or 'triangle'",
				        chorus[4].c_str());
			}

		} else {
			LOG_WARNING(
			        "FSYNTH: Invalid number of custom chorus settings (%d), "
			        "should be five",
			        static_cast<int>(chorus.size()));
		}
	}

	// API accept an integer voice-count
	const auto chorus_voice_count = static_cast<int>(round(chorus_voice_count_f));

	// Applies setting to all groups
	constexpr int FxGroup = -1;

// Current API calls as of 2.2
	fluid_synth_chorus_on(synth, FxGroup, chorus_enabled);
	fluid_synth_set_chorus_group_nr(synth, FxGroup, chorus_voice_count);
	fluid_synth_set_chorus_group_level(synth, FxGroup, chorus_level);
	fluid_synth_set_chorus_group_speed(synth, FxGroup, chorus_speed);
	fluid_synth_set_chorus_group_depth(synth, FxGroup, chorus_depth);

	fluid_synth_set_chorus_group_type(synth,
	                                  FxGroup,
	                                  static_cast<int>(chorus_mod_wave));

	if (chorus_enabled) {
		LOG_MSG("FSYNTH: Chorus enabled with %d voices at level %.2f, "
		        "%.2f Hz speed, %.2f depth, and %s-wave modulation",
		        chorus_voice_count,
		        chorus_level,
		        chorus_speed,
		        chorus_depth,
		        chorus_mod_wave == fluid_chorus_mod::FLUID_CHORUS_MOD_SINE
		                ? "sine"
		                : "triangle");
	}
}

static void setup_reverb(fsynth::fluid_synth_t* synth)
{
	using namespace fsynth;

	assert(synth);

	// Get the user's reverb settings
	const auto reverb = split(
	        get_fluidsynth_section()->Get_string("fsynth_reverb"));

	const bool reverb_enabled = !reverb.empty() && reverb[0] != "off";

	// Default reverb settings
	auto reverb_room_size = 0.61;
	auto reverb_damping   = 0.23;
	auto reverb_width     = 0.76;
	auto reverb_level     = 0.56;

	// Apply custom reverb settings if provided
	if (reverb_enabled && reverb.size() > 1) {
		if (reverb.size() == 4) {
			reverb_room_size = validate_setting("reverb room-size",
			                                    reverb[0],
			                                    reverb_room_size,
			                                    0.0,
			                                    1.0);

			reverb_damping = validate_setting("reverb damping",
			                                  reverb[1],
			                                  reverb_damping,
			                                  0.0,
			                                  1.0);

			reverb_width = validate_setting("reverb width",
			                                reverb[2],
			                                reverb_width,
			                                0.0,
			                                100.0);

			reverb_level = validate_setting(
			        "reverb level", reverb[3], reverb_level, 0.0, 1.0);
		} else {
			LOG_WARNING(
			        "FSYNTH: Invalid number of custom reverb settings (%d), "
			        "should be four",
			        static_cast<int>(reverb.size()));
		}
	}

	// Applies setting to all groups
	constexpr int FxGroup = -1;

// Current API calls as of 2.2
	fluid_synth_reverb_on(synth, FxGroup, reverb_enabled);
	fluid_synth_set_reverb_group_roomsize(synth, FxGroup, reverb_room_size);

	fluid_synth_set_reverb_group_damp(synth, FxGroup, reverb_damping);
	fluid_synth_set_reverb_group_width(synth, FxGroup, reverb_width);
	fluid_synth_set_reverb_group_level(synth, FxGroup, reverb_level);


	if (reverb_enabled) {
		LOG_MSG("FSYNTH: Reverb enabled with a %.2f room size, "
		        "%.2f damping, %.2f width, and level %.2f",
		        reverb_room_size,
		        reverb_damping,
		        reverb_width,
		        reverb_level);
	}
}

MidiDeviceFluidSynth::MidiDeviceFluidSynth()
{
	using namespace fsynth;

	DynLibResult res = load_fsynth_dynlib();
	switch (res) {
		case DynLibResult::Success:
			break;
		case DynLibResult::LibOpenErr: {
			const auto sym_msg = "FSYNTH: Failed to load FluidSynth library";
			LOG_ERR("%s", sym_msg);
			throw std::runtime_error(sym_msg);
			break;
		}
		case DynLibResult::ResolveSymErr: {
			const auto sym_msg = "FSYNTH: Failed to resolve one or more FluidSynth symbols";
			LOG_ERR("%s", sym_msg);
			throw std::runtime_error(sym_msg);
			break;
		}
	}

	FSynthVersion vers = {};
	fluid_version(&vers.major, &vers.minor, &vers.micro);
	if (vers < min_fsynth_version || vers >= max_fsynth_version_exclusive) {
		const auto msg = "FSYNTH: FluidSynth version must be at least 2.2.3 and less than 3.0.0";
		LOG_ERR("%s. Version loaded is %d.%d.%d", msg, vers.major, vers.minor, vers.micro);
		throw std::runtime_error(msg);
	} else {
		LOG_MSG("FSYNTH: Successfully loaded FluidSynth %d.%d.%d", vers.major, vers.minor, vers.micro);
	}
	
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
	// settings used to instantiate the synth, so we use the mixer's native
	// rate to configure FluidSynth.
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
	const auto sf_name = section->Get_string("soundfont");
	const auto sf_path = find_sf_file(sf_name);

	if (!sf_path.empty() && fluid_synth_sfcount(fluid_synth.get()) == 0) {
		constexpr auto ResetPresets = true;
		fluid_synth_sfload(fluid_synth.get(),
		                   sf_path.string().c_str(),
		                   ResetPresets);
	}

	if (fluid_synth_sfcount(fluid_synth.get()) == 0) {
		const auto msg = format_str("FSYNTH: Error loading SoundFont '%s'",
		                            sf_name.c_str());

		LOG_ERR("%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	auto sf_volume_percent = section->Get_int("soundfont_volume");
	fluid_synth_set_gain(fluid_synth.get(),
	                     static_cast<float>(sf_volume_percent) / 100.0f);

	// Let the user know that the SoundFont was loaded
	if (sf_volume_percent == 100) {
		LOG_MSG("FSYNTH: Using SoundFont '%s'", sf_path.string().c_str());
	} else {
		LOG_MSG("FSYNTH: Using SoundFont '%s' with volume scaled to %d%%",
		        sf_path.string().c_str(),
		        sf_volume_percent);
	}

	// Applies setting to all groups
	constexpr int FxGroup = -1;

	// Use a 7th-order (highest) polynomial to generate MIDI channel waveforms
	fluid_synth_set_interp_method(fluid_synth.get(), FxGroup, FLUID_INTERP_HIGHEST);

	// Use reasonable chorus and reverb settings matching ScummVM's defaults

	setup_chorus(fluid_synth.get(), sf_path);
	setup_reverb(fluid_synth.get());

	MIXER_LockMixerThread();

	// Set up the mixer callback
	const auto mixer_callback = std::bind(&MidiDeviceFluidSynth::MixerCallback,
	                                      this,
	                                      std::placeholders::_1);

	auto fluidsynth_channel = MIXER_AddChannel(mixer_callback,
	                                           sample_rate_hz,
	                                           ChannelName::FluidSynth,
	                                           {ChannelFeature::Sleep,
	                                            ChannelFeature::Stereo,
	                                            ChannelFeature::ReverbSend,
	                                            ChannelFeature::ChorusSend,
	                                            ChannelFeature::Synthesizer});

	// FluidSynth renders float audio frames between -1.0f and +1.0f, so we
	// ask the channel to scale all the samples up to its 0db level.
	fluidsynth_channel->Set0dbScalar(Max16BitSampleValue);

	const std::string filter_prefs = section->Get_string("fsynth_filter");

	if (!fluidsynth_channel->TryParseAndSetCustomFilter(filter_prefs)) {
		if (filter_prefs != "off") {
			LOG_WARNING("FSYNTH: Invalid 'fsynth_filter' value: '%s', using 'off'",
			            filter_prefs.c_str());
		}

		fluidsynth_channel->SetHighPassFilter(FilterState::Off);
		fluidsynth_channel->SetLowPassFilter(FilterState::Off);

		set_section_property_value("fluidsynth", "fsynth_filter", "off");
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

	// If we haven't failed yet, then we're ready to begin so move the local
	// objects into the member variables.
	settings      = std::move(fluid_settings);
	synth         = std::move(fluid_synth);
	mixer_channel = std::move(fluidsynth_channel);

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
	using namespace fsynth;

	const auto status_byte = msg[0];
	const auto status      = get_midi_status(status_byte);
	const auto channel     = get_midi_channel(status_byte);

	// clang-format off
	switch (status) {
	case MidiStatus::NoteOff:         fluid_synth_noteoff(     synth.get(), channel, msg[1]);         break;
	case MidiStatus::NoteOn:          fluid_synth_noteon(      synth.get(), channel, msg[1], msg[2]); break;
	case MidiStatus::PolyKeyPressure: fluid_synth_key_pressure(synth.get(), channel, msg[1], msg[2]); break;

	case MidiStatus::ControlChange: {
		const auto controller = msg[1];
		const auto value = msg[2];

		if (controller == MidiController::Portamento ||
			controller == MidiController::PortamentoTime ||
			controller == MidiController::PortamentoControl) {

			// The Roland SC-55 and its clones (Yamaha MU80 or Roland's own
			// later modules that emulate the SC-55) handle portamento (pitch
			// glides between consecutive notes on the same channel) in a very
			// specific and unique way, just like most synthesisers.
			//
			// The SC-55 accepts only 7-bit Portamento Time values via MIDI
			// CC5, where the min value of 0 sets the fastest portamento time
			// (effectively turns it off), and the max value of 127 the
			// slowest (up to 8 minutes!). There is an exponential mapping
			// between the CC values and the duration of the portamento (pitch
			// slides/glides); this custom curve is apparently approximated by
			// multiple linear segments. Moreover, the distance between the
			// source and destination notes also affect the portamento time,
			// making portamento dynamic and highly dependent on the notes
			// being played.
			//
			// FluidSynth, on the other hand, implements a very different
			// portamento model. Portament Time values are set via 14-bit CC
			// messages (via MIDI CC5 (coarse) and CC37 (fine)), and there is
			// a linear mapping between CC values and the portamento time as
			// per the following formula:
			//
			//   (CC5 * 127 ms) + (CC37 ms)
			//
			// Because of these fundamental differences, emulating Roland
			// SC-55 style portamento on FluidSynth is practically not
			// possible. Music written for the SC-55 that use portamento
			// sounds weirdly out of tune on FluidSynth (e.g. the Level 8
			// music of Descent), and "mapping" SC-55 portamento behaviour to
			// the FluidSynth range is not possible due to dynamic nature of
			// the SC-55 portamento handling. All in all, it's for the best to
			// ignore portamento altogether. This is not a great loss as it's
			// used rarely and usually only to add some subtle flair to the
			// start of the notes in synth-oriented soundtracks.
		} else {
			fluid_synth_cc(synth.get(), channel, controller, value);
		}
	} break;

	case MidiStatus::ProgramChange:   fluid_synth_program_change(  synth.get(), channel, msg[1]);                 break;
	case MidiStatus::ChannelPressure: fluid_synth_channel_pressure(synth.get(), channel, msg[1]);                 break;
	case MidiStatus::PitchBend:       fluid_synth_pitch_bend(      synth.get(), channel, msg[1] + (msg[2] << 7)); break;
	default: log_unknown_midi_message(msg); break;
	}
	// clang-format on
}

// Apply the sysex message to the service
void MidiDeviceFluidSynth::ApplySysExMessage(const std::vector<uint8_t>& msg)
{
	using namespace fsynth;

	const char* data = reinterpret_cast<const char*>(msg.data());
	const auto n     = static_cast<int>(msg.size());

	fluid_synth_sysex(synth.get(), data, n, nullptr, nullptr, nullptr, false);
}

// The callback operates at the audio frame-level, steadily adding samples to
// the mixer until the requested numbers of audio frames is met.
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
	using namespace fsynth;

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

	// Formatted line did not fill the whole buffer - no further formatting
	// is necessary.
	if (line.size() + 1 < width) {
		return line;
	}

	// The description was too long and got trimmed; place three dots in
	// the end to make it clear to the user.
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

			caller->WriteOut(convert_ansi_markup(output).c_str());
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
				// Problem with entry, move onto the next one
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
		caller->WriteOut("%s%s\n", Indent, MSG_Get("FLUIDSYNTH_NO_SOUNDFONTS"));
	} else {
		for (const auto& path : sf_files) {
			write_line(path);
		}
	}

	caller->WriteOut("\n");
}

static void fluidsynth_init([[maybe_unused]] Section* sec)
{
	if (const auto device = MIDI_GetCurrentDevice();
	    device && device->GetName() == MidiDeviceName::FluidSynth) {
		MIDI_Init();
	}
}

static void register_fluidsynth_text_messages()
{
	MSG_Add("FLUIDSYNTH_NO_SOUNDFONTS", "No available SoundFonts");
}

void FSYNTH_AddConfigSection(const ConfigPtr& conf)
{
	constexpr auto ChangeableAtRuntime = true;

	assert(conf);
	Section_prop* sec = conf->AddSection_prop("fluidsynth",
	                                          &fluidsynth_init,
	                                          ChangeableAtRuntime);
	assert(sec);
	init_fluidsynth_dosbox_settings(*sec);

	register_fluidsynth_text_messages();
}
