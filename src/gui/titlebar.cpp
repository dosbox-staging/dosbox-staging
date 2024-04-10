/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2024  The DOSBox Staging Team
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

#include "titlebar.h"

#include "checks.h"
#include "control.h"
#include "cpu.h"
#include "dosbox.h"
#include "mapper.h"
#include "sdlmain.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"
#include "video.h"

#include <SDL.h>
#include <map>
#include <vector>

CHECK_NARROWING();

// ***************************************************************************
// Datat types and storage
// ***************************************************************************

static struct TitlebarConfig {
	enum class Setting { Animation, Program, Dosbox, Version, Cycles, Mouse };

	enum class ProgramDisplay   { None, Name, Path, Segment, Custom };
	enum class VersionDisplay   { None, Simple, Detailed };
	enum class MouseHintDisplay { None, Short, Full };

	bool animated_record_mark = true;
	bool show_cycles          = true;
	bool show_dosbox_always   = false;

	ProgramDisplay program  = ProgramDisplay::Name;
	VersionDisplay version  = VersionDisplay::None;
	MouseHintDisplay mouse  = MouseHintDisplay::Full;
	std::string custom_name = {};

	std::map<Setting, std::string> substrings = {};

} config = {};

static const std::vector<TitlebarConfig::Setting> settings_order = {
	TitlebarConfig::Setting::Animation,
	TitlebarConfig::Setting::Program,
	TitlebarConfig::Setting::Dosbox,
	TitlebarConfig::Setting::Version,
	TitlebarConfig::Setting::Cycles,
	TitlebarConfig::Setting::Mouse,
};

static const std::map<TitlebarConfig::Setting, std::string> settings_strings = {
	{ TitlebarConfig::Setting::Animation, "animation" },
	{ TitlebarConfig::Setting::Program,   "program"   },
	{ TitlebarConfig::Setting::Dosbox,    "dosbox"    },
	{ TitlebarConfig::Setting::Version,   "version"   },
	{ TitlebarConfig::Setting::Cycles,    "cycles"    },
	{ TitlebarConfig::Setting::Mouse,     "mouse"     }
};

static struct {
	bool is_capturing_audio = false;
	bool is_capturing_video = false;
	bool is_audio_muted     = false;
	bool is_guest_os_booted = false;

	MouseHint mouse_hint_id    = {};
	std::string segment_name   = {};
	std::string canonical_name = {}; // path + name + extension

	int num_cycles = 0;

	std::string title_no_tags = {};

	SDL_TimerID timer_id           = {};
	bool animation_phase_alternate = false;
} state = {};

// ***************************************************************************
// Constant strings
// ***************************************************************************

static constexpr bool is_debug_build()
{
#if !defined(NDEBUG)
	return true;
#else
	return false;
#endif
}

// The U+23FA (Black Circle For Record) symbol would be more suitable, but with
// some fonts it is larger than Latin alphabet symbols - this (at least on KDE)
// leads to unpleasant effect when it suddenly appears in the titlebar.
// Similarly, we do not use U+1F507 (Speaker With Cancellation Stroke) symbol.
static const std::string MediumWhiteCircle = "\xe2\x9a\xaa"; // U+26AA
static const std::string MediumBlackCircle = "\xe2\x9a\xab"; // U+26AB

static const std::string Separator = " - ";
static const std::string BeginTag  = "[";
static const std::string EndTag    = "] ";

static const std::string RecordingMarkText = "REC";
static const std::string RecordingMarkFrame1 = MediumBlackCircle + RecordingMarkText;
static const std::string RecordingMarkFrame2 = MediumWhiteCircle + RecordingMarkText;

// ***************************************************************************
// Titlebar rendering
// ***************************************************************************

// Time each animation 'frame' lasts, in milliseconds. Lower = faster blinking.
constexpr uint32_t FrameTimeMs = 750;

static bool is_animation_running()
{
	return state.timer_id != 0;
}

static uint32_t animation_tick(uint32_t /* interval */, void* /* name */)
{
	SDL_Event event = {};
	event.user.type = enum_val(SDL_DosBoxEvents::RefreshAnimatedTitle);
	event.user.type += sdl.start_event_id;

	// We are outside of the main thread; we can't update the window title
	// here, SDL does not like it - we have to go through the event queue
	SDL_PushEvent(&event);
	return FrameTimeMs;
}

static void maybe_start_animation()
{
	if (!is_animation_running()) {
		state.animation_phase_alternate = false;
		state.timer_id = SDL_AddTimer(FrameTimeMs / 2, animation_tick, nullptr);
		if (state.timer_id == 0) {
			LOG_ERR("SDL: Could not start timer: %s", SDL_GetError());
		}
	}
}

static void maybe_stop_animation()
{
	if (is_animation_running()) {
		SDL_RemoveTimer(state.timer_id);
		state.timer_id = 0;
	}
}

static void strip_path(std::string& name)
{
	const auto position = name.rfind('\\');
	if (position != std::string::npos) {
		name = name.substr(position + 1);
	}
}

static std::string get_running_program_name()
{
	std::string result = {};

	if (state.is_guest_os_booted &&
	    config.program != TitlebarConfig::ProgramDisplay::Custom) {
		return result;
	}

	switch (config.program) {
	case TitlebarConfig::ProgramDisplay::None:
		return result;
	case TitlebarConfig::ProgramDisplay::Name:
		result = state.canonical_name;
		strip_path(result);
		break;
	case TitlebarConfig::ProgramDisplay::Path:
		result = state.canonical_name;
		break;
	case TitlebarConfig::ProgramDisplay::Segment: return state.segment_name;
	case TitlebarConfig::ProgramDisplay::Custom: return config.custom_name;
	default: assert(false); return result;
	}

	if (result.empty() && !state.segment_name.empty()) {
		// Most likely due to Windows 3.1x running in enhanced mode
		return state.segment_name;
	}

	return result;
}

static std::string get_dosbox_version()
{
	std::string result = {};
	switch (config.version) {
	case TitlebarConfig::VersionDisplay::None:
		return result;
	case TitlebarConfig::VersionDisplay::Simple:
		result += DOSBOX_GetVersion();
		break;
	case TitlebarConfig::VersionDisplay::Detailed:
		result += DOSBOX_GetDetailedVersion();
		break;
	default: assert(false); return result;
	}

	if (is_debug_build()) {
		return result + " (debug build)";
	} else {
		return result;
	}
}

static std::string get_cycles_display()
{
	std::string cycles = {};

	if (!config.show_cycles) {
		return cycles;
	}

	if (!CPU_CycleAutoAdjust) {
		cycles = format_str("%d", state.num_cycles);
	} else if (CPU_CycleLimit > 0) {
		cycles = format_str("max %d%% limit %d",
		                    state.num_cycles,
		                    CPU_CycleLimit);
	} else {
		cycles = format_str("max %d%%", state.num_cycles);
	}

	return cycles + " " + MSG_GetRaw("TITLEBAR_CYCLES_MS");
}

static std::string get_mouse_hint_simple()
{
	// We are using 'MSG_GetRaw' here as we want messages to stay as UTF-8

	if (state.mouse_hint_id == MouseHint::CapturedHotkey ||
	    state.mouse_hint_id == MouseHint::CapturedHotkeyMiddle) {
		// 'MSG_GetRaw' because we want messages to stay as UTF-8
		return MSG_GetRaw("TITLEBAR_HINT_CAPTURED");
	} else {
		return {};
	}
}

static std::string get_mouse_hint_full()
{
	char hint_buffer[200] = {0};

	auto create_hint_str = [&](const char* requested_name) {
		// 'MSG_GetRaw' because we want messages to stay as UTF-8
		safe_sprintf(hint_buffer, MSG_GetRaw(requested_name), PRIMARY_MOD_NAME);
		return hint_buffer;
	};

	switch (state.mouse_hint_id) {
	case MouseHint::None:
		break;
	case MouseHint::CapturedHotkey:
		return create_hint_str("TITLEBAR_HINT_CAPTURED_HOTKEY");
	case MouseHint::CapturedHotkeyMiddle:
		return create_hint_str("TITLEBAR_HINT_CAPTURED_HOTKEY_MIDDLE");
	case MouseHint::ReleasedHotkey:
		return create_hint_str("TITLEBAR_HINT_RELEASED_HOTKEY");
	case MouseHint::ReleasedHotkeyMiddle:
		return create_hint_str("TITLEBAR_HINT_RELEASED_HOTKEY_MIDDLE");
	case MouseHint::ReleasedHotkeyAnyButton:
		return create_hint_str("TITLEBAR_HINT_RELEASED_HOTKEY_ANY_BUTTON");
	case MouseHint::SeamlessHotkey:
		return create_hint_str("TITLEBAR_HINT_SEAMLESS_HOTKEY");
	case MouseHint::SeamlessHotkeyMiddle:
		return create_hint_str("TITLEBAR_HINT_SEAMLESS_HOTKEY_MIDDLE");
	default: assert(false); break;
	}

	return {};
}

static std::string get_mouse_hint()
{
	switch (config.mouse) {
	case TitlebarConfig::MouseHintDisplay::None:
		return {};
	case TitlebarConfig::MouseHintDisplay::Short:
		return get_mouse_hint_simple();
	case TitlebarConfig::MouseHintDisplay::Full:
		return get_mouse_hint_full();
	default: assert(false); return {};
	}
}

static void maybe_add_muted_mark(std::string& title_str)
{
	// Do not add 'mute' tag if emulator is paused
	if (state.is_audio_muted && !sdl.is_paused) {
		title_str = BeginTag + MSG_GetRaw("TITLEBAR_MUTED") + EndTag +
		            title_str;
	}
}

static void maybe_add_recording_pause_mark(std::string& title_str)
{
	if (sdl.is_paused) {
		title_str = BeginTag + MSG_GetRaw("TITLEBAR_PAUSED") + EndTag +
		            title_str;
		return;
	}

	if (!state.is_capturing_audio && !state.is_capturing_video) {
		return;
	}

	std::string tag = {};
	if ( config.animated_record_mark) {
		if (state.animation_phase_alternate) {
			tag = BeginTag + RecordingMarkFrame1 + EndTag;
		} else {
			tag = BeginTag + RecordingMarkFrame2 + EndTag;
		}
	} else {
		tag = BeginTag + RecordingMarkText + EndTag;
	}

	title_str = tag + title_str;
}

static void set_window_title()
{
	auto title_str = state.title_no_tags;
	maybe_add_muted_mark(title_str);
	maybe_add_recording_pause_mark(title_str);
	SDL_SetWindowTitle(sdl.window, title_str.c_str());
}

void GFX_RefreshAnimatedTitle()
{
	if (!is_animation_running()) {
		return;
	}

	state.animation_phase_alternate = !state.animation_phase_alternate;
	set_window_title();
}

void GFX_RefreshTitle()
{
	// Running program name
	state.title_no_tags = get_running_program_name();
	trim(state.title_no_tags);
	const bool is_program_empty = state.title_no_tags.empty();

	// DOSBox name and version
	const bool show_version = config.version != TitlebarConfig::VersionDisplay::None;
	bool show_dosbox        = config.show_dosbox_always || is_program_empty;
	
	if (!is_program_empty && (show_dosbox || show_version)) {
		state.title_no_tags += Separator;
	}

	if (show_dosbox) {
		state.title_no_tags += DOSBOX_NAME;
		if (show_version) {
			state.title_no_tags += " ";
		}
	}
	if (show_version) {
		state.title_no_tags += get_dosbox_version();
	}

	// Cycles, mouse hint, pause/recording mark
	const auto cycles_str = get_cycles_display();
	if (!cycles_str.empty()) {
		state.title_no_tags += Separator + cycles_str;
	}
	const auto hint_str = get_mouse_hint();
	if (!hint_str.empty()) {
		state.title_no_tags += Separator + hint_str;
	}

	// Start/stop animation if needed
	const bool is_capturing = state.is_capturing_audio || state.is_capturing_video;
	if (config.animated_record_mark && !sdl.is_paused && is_capturing) {
		maybe_start_animation();
	} else {
		maybe_stop_animation();
	}

	// Title update
	set_window_title();
}

// ***************************************************************************
// External notifications and setter functions
// ***************************************************************************

void GFX_NotifyBooting()
{
	state.is_guest_os_booted = true;
	GFX_RefreshTitle();
}

void GFX_NotifyAudioCaptureStatus(const bool is_capturing)
{
	if (state.is_capturing_audio != is_capturing) {
		state.is_capturing_audio = is_capturing;
		GFX_RefreshTitle();
	}
}

void GFX_NotifyVideoCaptureStatus(const bool is_capturing)
{
	if (state.is_capturing_video != is_capturing) {
		state.is_capturing_video = is_capturing;
		GFX_RefreshTitle();
	}
}

void GFX_NotifyAudioMutedStatus(const bool is_muted)
{
	if (state.is_audio_muted != is_muted) {
		state.is_audio_muted = is_muted;
		GFX_RefreshTitle();
	}
}

void GFX_NotifyProgramName(const std::string& segment_name,
                           const std::string& canonical_name)
{
	std::string segment_name_dos = segment_name;

	// Segment name might contain just about any character - adapt it
	for (size_t i = 0; i < segment_name_dos.size(); ++i) {
		if (!is_extended_printable_ascii(segment_name_dos[i])) {
			segment_name_dos[i] = '?';
		}
	}
	trim(segment_name_dos);

	// Store new names as UTF-8, refresh titlebar
	dos_to_utf8(segment_name_dos, state.segment_name);
	dos_to_utf8(canonical_name, state.canonical_name);
	GFX_RefreshTitle();
}

void GFX_NotifyCyclesChanged(const int32_t cycles)
{
	if (cycles >= 0 && state.num_cycles != cycles) {
		state.num_cycles = cycles;
		GFX_RefreshTitle();
	}
}

void GFX_SetMouseHint(const MouseHint hint_id)
{
	if (hint_id != state.mouse_hint_id) {
		state.mouse_hint_id = hint_id;
		GFX_RefreshTitle();
	}
}

// ***************************************************************************
// Lifecycle and config string parsing
// ***************************************************************************

static void check_double_value(const TitlebarConfig::Setting setting,
                               const std::string& setting_str,
	                       std::map<TitlebarConfig::Setting, bool>& is_already_warned)
{
	if (contains(config.substrings, setting) &&
	    !contains(is_already_warned, setting)) {
                LOG_WARNING("SDL: Invalid 'window_titlebar' setting '%s', "
                	    "it can only be specified once; using the last one",
		            settings_strings.at(setting).c_str());
		is_already_warned[setting] = true;
	}

	config.substrings[setting] = setting_str;
}

static void extract_custom_program_name(std::string& setting_str,
                                        std::map<TitlebarConfig::Setting, bool>& is_already_warned,
                                        bool& config_needs_sync)
{
	const std::vector<std::pair<char, char>> delimiters_list = {
	        {'(', ')'},
	        {'<', '>'},
	        {'[', ']'},
	        {'"', '"'},
	        {'\'', '\''},
	};

	bool is_already_warned_empty = false;
	auto warn_empty_name = [&]() {
                if (!is_already_warned_empty) {
                        LOG_WARNING("SDL: Invalid 'window_titlebar' setting "
                                    "'program', contains an empty name");
                        is_already_warned_empty = true;
                        config_needs_sync       = true;
                }
	};

	auto cut_away = [](std::string& string,
	                   const size_t start_position,
	                   const size_t end_position) {
		string = string.substr(0, start_position) +
		         string.substr(end_position + 1);
	};

	bool should_terminate = false;
	while (!should_terminate) {
		auto lowcase_str = setting_str;
		lowcase(lowcase_str);
		should_terminate = true;

		for (const auto& delimiters : delimiters_list) {
			const std::string start_str = std::string("program=") +
			                              delimiters.first;

			const auto start_position = lowcase_str.find(start_str);
			if (start_position == std::string::npos) {
				continue;
			}

			// We have something that looks like a beginning of the
			// custom program name
			const auto name_position = start_position +
			                           start_str.length();
			const auto end_position = lowcase_str.find(delimiters.second,
			                                           name_position);
			if (end_position == std::string::npos) {
				continue;
			}

			// Check for required spacing around 'program="name"'
			if (start_position > 0 &&
			    !std::isspace(setting_str[start_position - 1])) {
				continue;
			}
			if (end_position + 1 < setting_str.length() &&
			    !std::isspace(setting_str[end_position + 1])) {
				continue;
			}

			// Warn about cempty program name
			const auto name_length = end_position - name_position;
			if (name_length == 0) {
				warn_empty_name();
				cut_away(setting_str, start_position, end_position);
				continue;
			}

			// We have found a valid 'program="name"' type string -
			// extract the custom program name
			config.program = TitlebarConfig::ProgramDisplay::Custom;
			config.custom_name = setting_str.substr(name_position,
			                                        name_length);

			const auto setting_length = start_str.length() + 2 + name_length;
			check_double_value(TitlebarConfig::Setting::Program,
				           setting_str.substr(start_position, setting_length),
			                   is_already_warned);
			cut_away(setting_str, start_position, end_position);

			// Continue searching, there might be duplicate settings
			should_terminate = false;
			break;
		}
	}
}

static void sync_config()
{
	std::string setting_str = {};

	for (const auto setting : settings_order ) {
		if (!contains(config.substrings, setting)) {
			if (!setting_str.empty()) {
				setting_str += " ";
			}
			setting_str += config.substrings[setting];
			trim(setting_str);
		}
	}

	assert(control);
	auto section = static_cast<Section_prop*>(control->GetSection("sdl"));
	assert(section);
	const auto string_prop = section->GetStringProp("window_titlebar");
	string_prop->SetValue(setting_str);
}

static void parse_config(const std::string& new_setting_str)
{
	bool config_needs_sync = false;
	std::map<TitlebarConfig::Setting, bool> is_already_warned = {};

	config = TitlebarConfig();

	std::string work_str = new_setting_str;
	extract_custom_program_name(work_str, is_already_warned, config_needs_sync);

	const auto titlebar_content = split(work_str);
	for (const auto& setting_str : titlebar_content) {
		auto check_double_setting = [&](const TitlebarConfig::Setting setting) {
			check_double_value(setting, setting_str, is_already_warned);
		};

		if (iequals(setting_str, "animation=on")) {
			check_double_setting(TitlebarConfig::Setting::Animation);
			config.animated_record_mark = true;
			continue;
		}

		if (iequals(setting_str, "animation=off")) {
			check_double_setting(TitlebarConfig::Setting::Animation);
			config.animated_record_mark = false;
			continue;
		}

		if (iequals(setting_str, "program=none") ||
		    iequals(setting_str, "program=off")) {
			check_double_setting(TitlebarConfig::Setting::Program);
			config.program = TitlebarConfig::ProgramDisplay::None;
			continue;
		}

		if (iequals(setting_str, "program=name")) {
			check_double_setting(TitlebarConfig::Setting::Program);
			config.program = TitlebarConfig::ProgramDisplay::Name;
			continue;
		}

		if (iequals(setting_str, "program=path")) {
			check_double_setting(TitlebarConfig::Setting::Program);
			config.program = TitlebarConfig::ProgramDisplay::Path;
			continue;
		}

		if (iequals(setting_str, "program=segment")) {
			check_double_setting(TitlebarConfig::Setting::Program);
			config.program = TitlebarConfig::ProgramDisplay::Segment;
			continue;
		}

		if (iequals(setting_str, "dosbox=always")) {
			check_double_setting(TitlebarConfig::Setting::Dosbox);
			config.show_dosbox_always = true;
			continue;
		}

		if (iequals(setting_str, "dosbox=auto")) {
			check_double_setting(TitlebarConfig::Setting::Dosbox);
			config.show_dosbox_always = false;
			continue;
		}

		if (iequals(setting_str, "version=none") ||
		    iequals(setting_str, "version=off")) {
			check_double_setting(TitlebarConfig::Setting::Version);
			config.version = TitlebarConfig::VersionDisplay::None;
			continue;
		}

		if (iequals(setting_str, "version=simple")) {
			check_double_setting(TitlebarConfig::Setting::Version);
			config.version = TitlebarConfig::VersionDisplay::Simple;
			continue;
		}

		if (iequals(setting_str, "version=detailed")) {
			check_double_setting(TitlebarConfig::Setting::Version);
			config.version = TitlebarConfig::VersionDisplay::Detailed;
			continue;
		}

		if (iequals(setting_str, "cycles=on")) {
			check_double_setting(TitlebarConfig::Setting::Cycles);
			config.show_cycles = true;
			continue;
		}

		if (iequals(setting_str, "cycles=off")) {
			check_double_setting(TitlebarConfig::Setting::Cycles);
			config.show_cycles = false;
			continue;
		}

		if (iequals(setting_str, "mouse=none") ||
		    iequals(setting_str, "mouse=off")) {
			check_double_setting(TitlebarConfig::Setting::Mouse);
			config.mouse = TitlebarConfig::MouseHintDisplay::None;
			continue;
		}

		if (iequals(setting_str, "mouse=short")) {
			check_double_setting(TitlebarConfig::Setting::Mouse);
			config.mouse = TitlebarConfig::MouseHintDisplay::Short;
			continue;
		}

		if (iequals(setting_str, "mouse=full")) {
			check_double_setting(TitlebarConfig::Setting::Mouse);
			config.mouse = TitlebarConfig::MouseHintDisplay::Full;
			continue;
		}

		LOG_WARNING("SDL: Invalid 'window_titlebar' setting '%s', ignoring",
		            setting_str.c_str());
		config_needs_sync = true;
	}

	config_needs_sync |= !is_already_warned.empty();
	if (config_needs_sync) {
		sync_config();
	}
}

void TITLEBAR_ReadConfig(const Section_prop& secprop)
{
	parse_config(secprop.Get_string("window_titlebar"));

	GFX_RefreshTitle();
}

void TITLEBAR_AddConfig(Section_prop& secprop)
{
	constexpr auto always = Property::Changeable::Always;

	Prop_string* prop_str = nullptr;

	prop_str = secprop.Add_string("window_titlebar",
	                              always,
	                              "program=name dosbox=auto cycles=on mouse=full");
	prop_str->Set_help(
	        "Space separated list of information to be displayed in the window's titlebar\n"
	        "('program=name dosbox=auto cycles=on mouse=full' by default). If a parameter\n"
	        "is not specified, its default value is used.\n"
	        "Possible information to display are:\n"
	        "  animation=<value>:  If set to 'on' (default), animate the audio/video\n"
	        "                      recording mark. Set to 'off' to disable animation; this\n"
	        "                      is useful if your screen font produces weird results.\n"
	        "  program=<value>:    Display the name of the running program.\n"
	        "                      <value> can be one of:\n"
	        "                        none/off:  Do not display program name.\n"
	        "                        name:      Program name, with file extension (default).\n"
	        "                        path:      Name, extension, and full absolute path.\n"
	        "                        segment:   Display program memory segment name.\n"
	        "                        'Title':   Custom name. Alternatively, you can use\n"
	        "                                   \"Title\", (Title), <Title> or [Title] form.\n"
	        "                      Note: With some software (like Windows 3.1x in enhanced\n"
	        "                      mode) it is impossible to recognize the full program\n"
	        "                      name or path; in such cases 'segment' is used instead.\n"
	        "  dosbox=<value>:     Display 'DOSBox Staging' in the title bar.\n"
	        "                      <value> can be one of:\n"
	        "                        always:   Always display 'DOSBox Staging'.\n"
	        "                        auto:     Only display it if no program is running or\n"
	        "                                  'program=none' is set (default).\n"
	        "  version=<value>:    Display DOSBox version information.\n"
	        "                      <value> can be one of:\n"
	        "                         none/off:  Do not display DOSBox version (default).\n"
	        "                         simple:    Simple version information.\n"
	        "                         detailed:  Include Git hash, if available.\n"
	        "  cycles=<value>:     If set to 'on' (default), show CPU cycles setting.\n"
	        "                      Set to 'off' to disable cycles setting display.\n"
	        "  mouse=<value>:      Mouse capturing hint verbosity level:\n"
	        "                        none/off:  Do not display any mouse hints.\n"
	        "                        short:     Only display if mouse is captured.\n"
	        "                        full:      Display verbose information on how to\n"
	        "                                   capture or release the cursor (default).");
}

void TITLEBAR_AddMessages()
{
	MSG_Add("TITLEBAR_CYCLES_MS", "cycles/ms");
	MSG_Add("TITLEBAR_MUTED", "MUTED");
	MSG_Add("TITLEBAR_PAUSED", "PAUSED");

	MSG_Add("TITLEBAR_HINT_CAPTURED", "mouse captured");
	MSG_Add("TITLEBAR_HINT_CAPTURED_HOTKEY", "mouse captured, %s+F10 to release");
	MSG_Add("TITLEBAR_HINT_CAPTURED_HOTKEY_MIDDLE",
	        "mouse captured, %s+F10 or middle-click to release");
	MSG_Add("TITLEBAR_HINT_RELEASED_HOTKEY", "to capture the mouse press %s+F10");
	MSG_Add("TITLEBAR_HINT_RELEASED_HOTKEY_MIDDLE",
	        "to capture the mouse press %s+F10 or middle-click");
	MSG_Add("TITLEBAR_HINT_RELEASED_HOTKEY_ANY_BUTTON",
	        "to capture the mouse press %s+F10 or click any button");
	MSG_Add("TITLEBAR_HINT_SEAMLESS_HOTKEY", "seamless mouse, %s+F10 to capture");
	MSG_Add("TITLEBAR_HINT_SEAMLESS_HOTKEY_MIDDLE",
	        "seamless mouse, %s+F10 or middle-click to capture");
}
