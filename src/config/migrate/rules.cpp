// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rules.h"

#include <cctype>
#include <cstring>
#include <string>

#include "cycles_parser.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace ConfigMigrate {

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

ApplyResult passthrough(const InputLine& line)
{
	ApplyResult r = {};
	r.emit.push_back({line.key, line.value});
	return r;
}

bool is_decimal_int(const std::string& s)
{
	if (s.empty()) {
		return false;
	}
	for (const auto c : s) {
		if (!std::isdigit(static_cast<unsigned char>(c))) {
			return false;
		}
	}
	return true;
}

bool equals_ignore_case(const std::string& a, const char* b)
{
	const auto n = std::strlen(b);
	if (a.size() != n) {
		return false;
	}
	for (size_t i = 0; i < n; ++i) {
		if (std::tolower(static_cast<unsigned char>(a[i])) !=
		    std::tolower(static_cast<unsigned char>(b[i]))) {
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// Custom handlers — [sdl]
// ---------------------------------------------------------------------------

ApplyResult sdl_fullresolution(const InputLine& line)
{
	ApplyResult r = {};
	if (line.value == "desktop") {
		r.emit.push_back({"fullscreen_mode", "standard"});
	} else if (line.value == "original") {
		r.emit.push_back({"fullscreen_mode", "standard"});
		r.warning =
		        "'[sdl] fullresolution = original' has been removed; "
		        "migrated to 'fullscreen_mode = standard'. Consider "
		        "'forced-borderless' on Windows for an "
		        "exclusive-fullscreen-like effect.";
	} else {
		// Likely a custom resolution like 1920x1080
		r.emit.push_back({"fullscreen_mode", "standard"});
		r.warning = "'[sdl] fullresolution = " + line.value +
		            "' is no longer supported; migrated to "
		            "'fullscreen_mode = standard'. Custom fullscreen "
		            "resolutions are not available in current DOSBox Staging.";
	}
	return r;
}

ApplyResult sdl_host_rate(const InputLine& line)
{
	ApplyResult r = {};
	if (line.value == "auto") {
		// Drop entirely; the new default takes over.
		return r;
	}
	r.target_section = "dosbox";
	r.emit.push_back({"dos_rate", "host"});
	r.warning = "'[sdl] host_rate = " + line.value +
	            "' has been removed. VRR is now auto-handled. Migrated to "
	            "'[dosbox] dos_rate = host'; use 'dos_rate = <Hz>' for a "
	            "custom DOS-side rate.";
	return r;
}

ApplyResult sdl_vsync(const InputLine& line)
{
	ApplyResult r = {};
	const auto& v = line.value;
	if (v == "true" || v == "1") {
		r.emit.push_back({"vsync", "on"});
	} else if (v == "false" || v == "0") {
		r.emit.push_back({"vsync", "off"});
	} else if (v == "auto") {
		r.emit.push_back({"vsync", "off"});
		r.warning =
		        "'[sdl] vsync = auto' has been removed; migrated to "
		        "'off' (the new default). Set 'on' or "
		        "'fullscreen-only' if you need vsync.";
	} else if (v == "adaptive") {
		r.emit.push_back({"vsync", "on"});
		r.warning = "'[sdl] vsync = adaptive' has been removed; migrated to 'on'.";
	} else if (v == "yield") {
		r.emit.push_back({"vsync", "off"});
		r.warning = "'[sdl] vsync = yield' has been removed; migrated to 'off'.";
	} else {
		// on / off / fullscreen-only — pass through
		r.emit.push_back({"vsync", v});
	}
	return r;
}

// ---------------------------------------------------------------------------
// Custom handlers — [render]
// ---------------------------------------------------------------------------

ApplyResult render_glshader(const InputLine& line)
{
	ApplyResult r = {};
	if (line.value == "default") {
		r.emit.push_back({"shader", "crt-auto"});
		r.warning =
		        "'[render] glshader = default' was remapped to 'shader "
		        "= crt-auto'. The old 'default' was a non-CRT "
		        "pass-through; use 'shader = sharp' if you prefer "
		        "the legacy look.";
	} else if (line.value == "none") {
		r.emit.push_back({"shader", "sharp"});
		r.warning =
		        "'[render] glshader = none' was remapped to 'shader = "
		        "sharp'. Pre-shader pass-through is no longer available.";
	} else {
		r.emit.push_back({"shader", line.value});
		r.warning = "Verify that '" + line.value +
		            "' is still a valid shader name; the bundled shader "
		            "set was overhauled in 0.81.0.";
	}
	return r;
}

// ---------------------------------------------------------------------------
// Custom handlers — [voodoo]
// ---------------------------------------------------------------------------

ApplyResult voodoo_multithreading(const InputLine& line)
{
	ApplyResult r = {};
	if (line.value == "true" || line.value == "1") {
		r.emit.push_back({"voodoo_threads", "auto"});
	} else if (line.value == "false" || line.value == "0") {
		r.emit.push_back({"voodoo_threads", "1"});
	} else {
		// Unexpected value: best-effort pass-through
		r.emit.push_back({"voodoo_threads", line.value});
	}
	return r;
}

// ---------------------------------------------------------------------------
// Custom handlers — [cpu]
// ---------------------------------------------------------------------------

ApplyResult cpu_cputype(const InputLine& line)
{
	ApplyResult r = {};
	const auto& v = line.value;
	if (v == "386_slow") {
		r.emit.push_back({"cputype", "386"});
	} else if (v == "486_slow") {
		r.emit.push_back({"cputype", "486"});
	} else if (v == "pentium_slow") {
		r.emit.push_back({"cputype", "pentium"});
	} else if (v == "486_prefetch") {
		r.emit.push_back({"cputype", "486"});
		r.warning =
		        "'[cpu] cputype = 486_prefetch' was migrated to '486'. "
		        "There is no 486-with-prefetch core in current DOSBox "
		        "Staging. Use 'cputype = 386_prefetch' if you need "
		        "prefetch behaviour (which is a 386 core).";
	} else if (v == "386") {
		// Silent semantic remap: pre-0.82 `386` meant today's `386_fast`.
		r.emit.push_back({"cputype", "386_fast"});
		r.warning =
		        "'[cpu] cputype = 386' had different semantics in "
		        "pre-0.82 — it meant today's '386_fast'. Migrated to "
		        "'386_fast'. Use 'cputype = 386' explicitly only if "
		        "you want the slower modern '386' core.";
	} else {
		r.emit.push_back({"cputype", v});
	}
	return r;
}

// ---------------------------------------------------------------------------
// Custom handlers — [mixer]
// ---------------------------------------------------------------------------

ApplyResult mixer_crossfeed(const InputLine& line)
{
	ApplyResult r = {};
	if (!is_decimal_int(line.value)) {
		// Already a preset or unknown — pass through.
		return passthrough(line);
	}
	const auto n       = std::stoi(line.value);
	std::string preset = "off";
	if (n >= 1 && n <= 25) {
		preset = "light";
	} else if (n >= 26 && n <= 50) {
		preset = "normal";
	} else if (n >= 51) {
		preset = "strong";
	}
	r.emit.push_back({"crossfeed", preset});
	r.warning = "'[mixer] crossfeed = " + line.value +
	            "' numeric strength was removed in 0.81.0; mapped to "
	            "'crossfeed = " +
	            preset + "'. Tune manually if needed.";
	return r;
}

// ---------------------------------------------------------------------------
// Custom handlers — [fluidsynth]
// ---------------------------------------------------------------------------

ApplyResult fluidsynth_soundfont(const InputLine& line)
{
	ApplyResult r = {};
	// The legacy form is `soundfont = name N` where N is an optional
	// volume scaling integer 1..800. Split on the last whitespace and
	// check if the trailing token is a positive integer.
	const auto& v      = line.value;
	const auto last_ws = v.find_last_of(" \t");
	if (last_ws == std::string::npos) {
		return passthrough(line);
	}
	const auto trailing = v.substr(last_ws + 1);
	if (!is_decimal_int(trailing)) {
		return passthrough(line);
	}
	const auto head_end = v.find_last_not_of(" \t", last_ws);
	if (head_end == std::string::npos) {
		return passthrough(line);
	}
	const auto name = v.substr(0, head_end + 1);
	r.emit.push_back({"soundfont", name});
	r.emit.push_back({"soundfont_volume", trailing});
	return r;
}

// ---------------------------------------------------------------------------
// Custom handlers — [sblaster]
// ---------------------------------------------------------------------------

ApplyResult sblaster_oplmode(const InputLine& line)
{
	ApplyResult r = {};
	if (line.value == "cms") {
		r.emit.push_back({"oplmode", "auto"});
		r.emit.push_back({"cms", "on"});
	} else {
		r.emit.push_back({"oplmode", line.value});
	}
	return r;
}

// ---------------------------------------------------------------------------
// Custom handlers — [gus]
// ---------------------------------------------------------------------------

ApplyResult gus_gusbase(const InputLine& line)
{
	ApplyResult r = {};
	r.emit.push_back({"gusbase", line.value});
	const auto& v = line.value;
	if (v == "280" || v == "2a0" || v == "2c0" || v == "2e0" || v == "300") {
		r.warning = "'[gus] gusbase = " + v +
		            "' was removed in 0.82.0; check whether this value is "
		            "still valid (accepted: 210, 220, 230, 240, 250, 260).";
	}
	return r;
}

ApplyResult gus_gusirq(const InputLine& line)
{
	ApplyResult r = {};
	r.emit.push_back({"gusirq", line.value});
	if (line.value == "9" || line.value == "10") {
		r.warning = "'[gus] gusirq = " + line.value +
		            "' was removed in 0.82.0; check whether this value is "
		            "still valid (accepted: 2, 3, 5, 7, 11, 12, 15).";
	}
	return r;
}

ApplyResult gus_gusdma(const InputLine& line)
{
	ApplyResult r = {};
	r.emit.push_back({"gusdma", line.value});
	if (line.value == "0") {
		r.warning =
		        "'[gus] gusdma = 0' was removed in 0.82.0 (accepted: "
		        "1, 3, 5, 6, 7).";
	}
	return r;
}

// ---------------------------------------------------------------------------
// Custom handlers — [innovation]
// ---------------------------------------------------------------------------

ApplyResult innovation_sidmodel(const InputLine& line)
{
	ApplyResult r = {};
	if (line.value == "none") {
		r.emit.push_back({"innovation", "off"});
	} else if (line.value == "8580") {
		r.emit.push_back({"innovation", "on"});
		r.warning =
		        "Only the SID 6581 chip is now emulated; check whether "
		        "'[innovation] innovation = on' (replacing 'sidmodel = "
		        "8580') gives the expected result.";
	} else {
		// auto, 6581, or anything else
		r.emit.push_back({"innovation", "on"});
	}
	return r;
}

ApplyResult innovation_sidclock(const InputLine& line)
{
	// `default` is the only "fine" value; anything else gets a warning.
	ApplyResult r          = {};
	r.removed_with_warning = (line.value != "default");
	if (r.removed_with_warning) {
		r.warning = "'[innovation] sidclock = " + line.value +
		            "' was removed in 0.83.0; the original SSI-2001 "
		            "frequency (0.895 MHz) is always used.";
	}
	return r;
}

ApplyResult innovation_sidport(const InputLine& line)
{
	ApplyResult r          = {};
	r.removed_with_warning = (line.value != "280");
	if (r.removed_with_warning) {
		r.warning = "'[innovation] sidport = " + line.value +
		            "' was removed in 0.83.0; the SSI-2001 is now "
		            "hard-wired to port 280h.";
	}
	return r;
}

// ---------------------------------------------------------------------------
// Custom handlers — [speaker]
// ---------------------------------------------------------------------------

ApplyResult speaker_disney(const InputLine& line)
{
	ApplyResult r = {};
	if (line.value == "true" || line.value == "1") {
		r.emit.push_back({"lpt_dac", "disney"});
	}
	// false → drop the line silently (deprecated alias maps to nothing)
	return r;
}

// ---------------------------------------------------------------------------
// Custom handlers — [serial]
// ---------------------------------------------------------------------------

ApplyResult serial_port(const InputLine& line)
{
	ApplyResult r = {};
	// The serialN values are MultiVal: first token is the type.
	const auto first_ws = line.value.find_first_of(" \t");
	const auto type     = (first_ws == std::string::npos)
	                            ? line.value
	                            : line.value.substr(0, first_ws);
	if (equals_ignore_case(type, "direct")) {
		r.emit.push_back({line.key, "disabled"});
		r.warning = "'[serial] " + line.key +
		            " = direct ...' is no longer supported. The legacy "
		            "serial-port passthrough feature was removed in 0.83.0. "
		            "Migrated to 'disabled'.";
	} else {
		r.emit.push_back({line.key, line.value});
	}
	return r;
}

// ---------------------------------------------------------------------------
// Custom handlers — [mouse]
// ---------------------------------------------------------------------------

ApplyResult mouse_dos_driver(const InputLine& line)
{
	ApplyResult r = {};
	if (line.value == "true" || line.value == "1") {
		r.emit.push_back({"builtin_dos_mouse_driver", "on"});
	} else if (line.value == "false" || line.value == "0") {
		r.emit.push_back({"builtin_dos_mouse_driver", "off"});
	} else {
		r.emit.push_back({"builtin_dos_mouse_driver", line.value});
	}
	return r;
}

ApplyResult mouse_dos_immediate(const InputLine& line)
{
	ApplyResult r = {};
	if (line.value == "true" || line.value == "1") {
		r.emit.push_back({"builtin_dos_mouse_driver_options", "immediate"});
		r.warning =
		        "'[mouse] dos_mouse_immediate = true' was migrated "
		        "to 'builtin_dos_mouse_driver_options = immediate'. "
		        "If you have other options set, merge them manually.";
	}
	// false → drop
	return r;
}

// ---------------------------------------------------------------------------
// Custom handlers — [dos]
// ---------------------------------------------------------------------------

ApplyResult dos_country(const InputLine& line)
{
	ApplyResult r = {};
	if (line.value == "0") {
		r.emit.push_back({"country", "auto"});
	} else if (is_decimal_int(line.value)) {
		r.emit.push_back({"country", line.value});
		r.warning =
		        "'[dos] country' was an int in v0.80.0 and a string "
		        "after. Numeric country code '" +
		        line.value +
		        "' preserved; verify it still parses, or replace with "
		        "an ISO country code or 'auto'.";
	} else {
		r.emit.push_back({"country", line.value});
	}
	return r;
}

ApplyResult dos_file_locking(const InputLine& line)
{
	ApplyResult r = {};
	if (line.value == "true" || line.value == "1") {
		r.emit.push_back({"file_locking", "on"});
		r.warning =
		        "'[dos] file_locking = on' preserves your previous "
		        "behaviour. Consider 'auto' (the new default) which "
		        "only enables file locking under Windows 3.1.";
	} else if (line.value == "false" || line.value == "0") {
		r.emit.push_back({"file_locking", "off"});
	} else {
		r.emit.push_back({"file_locking", line.value});
	}
	return r;
}

// ---------------------------------------------------------------------------
// Rule table
// ---------------------------------------------------------------------------

const std::vector<Rule> rules = {
        // [sdl]
        {.from_section = "sdl",
         .from_key     = "fullresolution",
         .action       = Action::Custom,
         .custom       = sdl_fullresolution},
        {.from_section = "sdl",
         .from_key     = "windowresolution",
         .action       = Action::Rename,
         .to_key       = "window_size"},
        {.from_section = "sdl",
         .from_key     = "transparency",
         .action       = Action::Rename,
         .to_key       = "window_transparency"},
        {.from_section = "sdl",
         .from_key     = "viewport_resolution",
         .action       = Action::Move,
         .to_section   = "render",
         .to_key       = "viewport"},
        {.from_section = "sdl",
         .from_key     = "max_resolution",
         .action       = Action::Move,
         .to_section   = "render",
         .to_key       = "viewport"},
        {.from_section = "sdl",
         .from_key     = "host_rate",
         .action       = Action::Custom,
         .custom       = sdl_host_rate},
        {.from_section = "sdl",
         .from_key     = "vsync",
         .action       = Action::Custom,
         .custom       = sdl_vsync},
        {.from_section = "sdl",
         .from_key     = "vsync_skip",
         .action       = Action::Remove,
         .warning      = "'[sdl] vsync_skip' has been removed. For "
                         "similar behaviour set 'presentation_mode = "
                         "host-rate' and 'vsync = off'."},
        {.from_section = "sdl",
         .from_key     = "presentation_mode",
         .action       = Action::ValueMap,
         .value_map    = {{"cfr", "host-rate"}, {"vfr", "dos-rate"}}},
        {.from_section = "sdl",
         .from_key     = "output",
         .action       = Action::ValueMap,
         .value_map    = {{"surface", "opengl"},
                          {"openglpp", "opengl"},
                          {"openglnb", "opengl"},
                          {"texturepp", "texture"}}},
        {.from_section = "sdl",
         .from_key     = "capture_mouse",
         .action       = Action::Move,
         .to_section   = "mouse",
         .to_key       = "mouse_capture"},
        {.from_section = "sdl",
         .from_key     = "sensitivity",
         .action       = Action::Move,
         .to_section   = "mouse",
         .to_key       = "mouse_sensitivity"},
        {.from_section = "sdl",
         .from_key     = "raw_mouse_input",
         .action       = Action::Move,
         .to_section   = "mouse",
         .to_key       = "mouse_raw_input"},
        {.from_section = "sdl",
         .from_key     = "waitonerror",
         .action       = Action::Remove,
         .warning      = "'[sdl] waitonerror' has been removed; the "
                         "setting no longer has any effect."},
        {.from_section = "sdl",
         .from_key     = "priority",
         .action       = Action::Remove,
         .warning      = "'[sdl] priority' has been removed; thread "
                         "priority is now managed by the host OS."},

        // [render]
        {.from_section = "render", .from_key = "frameskip", .action = Action::Remove},
        {.from_section = "render",
         .from_key     = "aspect",
         .action       = Action::ValueMap,
         .value_map    = {{"true", "on"}, {"false", "off"}}},
        {.from_section = "render", .from_key = "scaler", .action = Action::Remove},
        {.from_section = "render",
         .from_key     = "glshader",
         .action       = Action::Custom,
         .custom       = render_glshader},

        // [composite]
        {.from_section = "composite",
         .from_key     = "saturation",
         .action       = Action::Remove,
         .warning      = "'[composite] saturation' has been removed. The new "
                         "'[render] saturation' has a different range "
                         "(-50..50, was 0..360) and meaning (digital RGB "
                         "saturation vs CGA composite). Tune '[render] "
                         "saturation' manually if needed."},
        {.from_section = "composite",
         .from_key     = "contrast",
         .action       = Action::Remove,
         .warning      = "'[composite] contrast' has been removed. The new "
                         "'[render] contrast' has a different range (0..100, "
                         "was 0..360) and meaning. Tune '[render] contrast' "
                         "manually."},
        {.from_section = "composite",
         .from_key     = "brightness",
         .action       = Action::Remove,
         .warning      = "'[composite] brightness' has been removed. The new "
                         "'[render] brightness' has a different range "
                         "(0..100, was -100..100) and meaning. Tune '[render] "
                         "brightness' manually."},

        // [voodoo]
        {.from_section = "voodoo",
         .from_key     = "voodoo_multithreading",
         .action       = Action::Custom,
         .custom       = voodoo_multithreading},

        // [dosbox]
        {.from_section = "dosbox",
         .from_key     = "captures",
         .action       = Action::Move,
         .to_section   = "capture",
         .to_key       = "capture_dir"},
        {.from_section = "dosbox",
         .from_key     = "machine",
         .action       = Action::ValueMap,
         .value_map    = {{"vgaonly", "svga_paradise"}}},

        // [cpu]
        {.from_section = "cpu",
         .from_key     = "cputype",
         .action       = Action::Custom,
         .custom       = cpu_cputype},
        {.from_section = "cpu",
         .from_key     = "cycles",
         .action       = Action::Custom,
         .custom       = MigrateLegacyCycles},

        // [mixer]
        {.from_section = "mixer",
         .from_key     = "crossfeed",
         .action       = Action::Custom,
         .custom       = mixer_crossfeed},

        // [midi]
        {.from_section = "midi",
         .from_key     = "mididevice",
         .action       = Action::ValueMap,
         .value_map    = {{"auto", "port"},
                          {"alsa", "port"},
                          {"coremidi", "port"},
                          {"oss", "port"},
                          {"win32", "port"}}},

        // [fluidsynth]
        {.from_section = "fluidsynth",
         .from_key     = "soundfont",
         .action       = Action::Custom,
         .custom       = fluidsynth_soundfont},

        // [sblaster]
        {.from_section = "sblaster",
         .from_key     = "oplmode",
         .action       = Action::Custom,
         .custom       = sblaster_oplmode},
        {.from_section = "sblaster", .from_key = "oplrate", .action = Action::Remove},
        {.from_section = "sblaster", .from_key = "oplemu", .action = Action::Remove},

        // [gus]
        {.from_section = "gus",
         .from_key     = "gusbase",
         .action       = Action::Custom,
         .custom       = gus_gusbase},
        {.from_section = "gus",
         .from_key     = "gusirq",
         .action       = Action::Custom,
         .custom       = gus_gusirq},
        {.from_section = "gus",
         .from_key     = "gusdma",
         .action       = Action::Custom,
         .custom       = gus_gusdma},

        // [innovation]
        {.from_section = "innovation",
         .from_key     = "sidmodel",
         .action       = Action::Custom,
         .custom       = innovation_sidmodel},
        {.from_section = "innovation",
         .from_key     = "sidclock",
         .action       = Action::Custom,
         .custom       = innovation_sidclock},
        {.from_section = "innovation",
         .from_key     = "sidport",
         .action       = Action::Custom,
         .custom       = innovation_sidport},
        {.from_section = "innovation",
         .from_key     = "6581filter",
         .action       = Action::Rename,
         .to_key       = "innovation_sid_filter"},
        {.from_section = "innovation",
         .from_key     = "8580filter",
         .action       = Action::Remove,
         .warning      = "'[innovation] 8580filter' was removed in 0.83.0 "
                         "(only the 6581 chip is emulated). Use "
                         "'innovation_sid_filter' for 6581 filter strength."},

        // [speaker]
        {.from_section = "speaker",
         .from_key     = "disney",
         .action       = Action::Custom,
         .custom       = speaker_disney},
        {.from_section = "speaker", .from_key = "zero_offset", .action = Action::Remove},

        // [serial]
        {.from_section = "serial",
         .from_key     = "serial1",
         .action       = Action::Custom,
         .custom       = serial_port},
        {.from_section = "serial",
         .from_key     = "serial2",
         .action       = Action::Custom,
         .custom       = serial_port},
        {.from_section = "serial",
         .from_key     = "serial3",
         .action       = Action::Custom,
         .custom       = serial_port},
        {.from_section = "serial",
         .from_key     = "serial4",
         .action       = Action::Custom,
         .custom       = serial_port},

        // [mouse]
        {.from_section = "mouse",
         .from_key     = "dos_mouse_driver",
         .action       = Action::Custom,
         .custom       = mouse_dos_driver},
        {.from_section = "mouse",
         .from_key     = "dos_mouse_immediate",
         .action       = Action::Custom,
         .custom       = mouse_dos_immediate},

        // [dos]
        {.from_section = "dos",
         .from_key     = "ems",
         .action       = Action::ValueMap,
         .value_map    = {{"true", "on"}, {"false", "off"}}},
        {.from_section = "dos",
         .from_key     = "country",
         .action       = Action::Custom,
         .custom       = dos_country},
        {.from_section = "dos",
         .from_key     = "keyboardlayout",
         .action       = Action::Rename,
         .to_key       = "keyboard_layout"},
        {.from_section = "dos",
         .from_key     = "expand_shell_variable",
         .action       = Action::ValueMap,
         .value_map    = {{"true", "on"}, {"false", "off"}}},
        {.from_section = "dos",
         .from_key     = "command_history_file",
         .action       = Action::Rename,
         .to_key       = "shell_history_file"},
        {.from_section = "dos",
         .from_key     = "file_locking",
         .action       = Action::Custom,
         .custom       = dos_file_locking},
};

} // namespace

const Rule* FindRule(std::string_view section, std::string_view key)
{
	for (const auto& r : rules) {
		if (r.from_section == section && r.from_key == key) {
			return &r;
		}
	}
	return nullptr;
}

ApplyResult Apply(const Rule& rule, const InputLine& line)
{
	ApplyResult result = {};
	result.warning     = rule.warning;

	switch (rule.action) {
	case Action::Rename:
		result.emit.push_back({rule.to_key.empty() ? line.key : rule.to_key,
		                       line.value});
		break;

	case Action::Move:
		result.target_section = rule.to_section;
		result.emit.push_back({rule.to_key.empty() ? line.key : rule.to_key,
		                       line.value});
		break;

	case Action::Remove:
		result.removed_with_warning = !rule.warning.empty();
		break;

	case Action::ValueMap: {
		std::string new_value = line.value;
		for (const auto& [old_v, new_v] : rule.value_map) {
			if (old_v == line.value) {
				new_value = new_v;
				break;
			}
		}
		result.emit.push_back({line.key, new_value});
		break;
	}

	case Action::Split:
		result.emit.push_back({rule.to_key.empty() ? line.key : rule.to_key,
		                       line.value});
		result.emit.push_back({rule.split_key, rule.split_value});
		break;

	case Action::Custom:
		if (rule.custom) {
			return rule.custom(line);
		}
		break;
	}
	return result;
}

} // namespace ConfigMigrate
