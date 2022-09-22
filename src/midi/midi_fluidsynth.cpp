/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
 *  Copyright (C) 2020-2020  Nikos Chantziaras <realnc@gmail.com>
 *  Copyright (C) 2002-2011  The DOSBox Team
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

#if C_FLUIDSYNTH

#include <cassert>
#include <deque>
#include <string>
#include <tuple>

#include "control.h"
#include "cross.h"
#include "fs_utils.h"
#include "mixer.h"
#include "programs.h"
#include "string_utils.h"
#include "support.h"
#include "../ints/int10.h"
#include "string_utils.h"

static constexpr int FRAMES_PER_BUFFER = 48; // synth granularity

MidiHandlerFluidsynth instance;

static void init_fluid_dosbox_settings(Section_prop &secprop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	// Name 'default.sf2' picks the default soundfont if it's installed
	// in the OS. Usually it's Fluid_R3.
	auto *str_prop = secprop.Add_string("soundfont", when_idle, "default.sf2");
	str_prop->Set_help(
	        "Path to a SoundFont file in .sf2 format. You can use an\n"
	        "absolute or relative path, or the name of an .sf2 inside\n"
	        "the 'soundfonts' directory within your DOSBox configuration\n"
	        "directory.\n"
	        "Note: The optional volume scaling percentage after the filename\n"
	        "has been deprecated. Please use a mixer command instead to\n"
	        "change the FluidSynth audio channel's volume, e.g.:\n"
	        "  MIXER FSYNTH 200");

	str_prop = secprop.Add_string("fsynth_chorus", when_idle, "auto");
	str_prop->Set_help(
	        "Chorus effect: 'auto', 'on', 'off', or custom values.\n"
	        "When using custom values:\n"
	        "  All five must be provided in-order and space-separated.\n"
	        "  They are: voice-count level speed depth modulation-wave, where:\n"
	        "  - voice-count is an integer from 0 to 99.\n"
	        "  - level is a decimal from 0.0 to 10.0\n"
	        "  - speed is a decimal, measured in Hz, from 0.1 to 5.0\n"
	        "  - depth is a decimal from 0.0 to 21.0\n"
	        "  - modulation-wave is either 'sine' or 'triangle'\n"
	        "  For example: chorus = 3 1.2 0.3 8.0 sine\n"
	        "Note: You can disable the FluidSynth chorus and enable the\n"
	        "mixer-level chorus on the FluidSynth channel instead, or\n"
	        "enable both chorus effects at the same time. Whether this\n"
	        "sounds good depends on the SoundFont and the chorus settings\n"
	        "being used.");

	str_prop = secprop.Add_string("fsynth_reverb", when_idle, "auto");
	str_prop->Set_help(
	        "Reverb effect: 'auto', 'on', 'off', or custom values.\n"
	        "When using custom values:\n"
	        "  All four must be provided in-order and space-separated.\n"
	        "  They are: room-size damping width level, where:\n"
	        "  - room-size is a decimal from 0.0 to 1.0\n"
	        "  - damping is a decimal from 0.0 to 1.0\n"
	        "  - width is a decimal from 0.0 to 100.0\n"
	        "  - level is a decimal from 0.0 to 1.0\n"
	        "  For example: reverb = 0.61 0.23 0.76 0.56\n"
	        "Note: You can disable the FluidSynth reverb and enable the\n"
	        "mixer-level reverb on the FluidSynth channel instead, or\n"
	        "enable both reverb effects at the same time. Whether this\n"
	        "sounds good depends on the SoundFont and the reverb settings\n"
	        "being used.");

	str_prop = secprop.Add_string("fsynth_filter", when_idle, "off");
	assert(str_prop);
	str_prop->Set_help(
	        "Filter for the FluidSynth audio output:\n"
	        "  off:       Don't filter the output (default).\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

// Takes in the user's SoundFont configuration value consisting of the SF2
// filename followed by an optional scaling percentage. The scaling
// functionality has been deprecated; we're only parsing it here so we can
// raise a deprecation warning if it's present.
std::tuple<std::string, int> parse_sf_pref(const std::string &line,
                                           const int default_percent = -1)
{
	if (line.empty())
		return std::make_tuple(line, default_percent);

	// Look for a space in the last 4 characters of the string
	const auto len = line.length();
	const auto from_pos = len < 4 ? 0 : len - 4;
	auto last_space_pos = line.substr(from_pos).find_last_of(' ');
	if (last_space_pos == std::string::npos)
		return std::make_tuple(line, default_percent);

	// Ensure the position is relative to the start of the entire string
	last_space_pos += from_pos;

	// Is the stuff after the last space convertable to a number?
	int percent = 0;
	try {
		percent = stoi(line.substr(last_space_pos + 1));
	} catch (...) {
		return std::make_tuple(line, default_percent);
	}
	// A number was provided, so split it from the line
	std::string filename = line.substr(0, last_space_pos);
	trim(filename); // drop any extra whitespace prior to the number

	return std::make_tuple(filename, percent);
}

#if defined(WIN32)

static std::deque<std::string> get_data_dirs()
{
	return {
	        CROSS_GetPlatformConfigDir() + "soundfonts\\",
	        "C:\\soundfonts\\",
	};
}

#elif defined(MACOSX)

static std::deque<std::string> get_data_dirs()
{
	return {
	        CROSS_GetPlatformConfigDir() + "soundfonts/",
	        CROSS_ResolveHome("~/Library/Audio/Sounds/Banks/"),
	        // TODO: check /usr/local/share/soundfonts
	        // TODO: check /usr/share/soundfonts
	};
}

#else

static std::deque<std::string> get_data_dirs()
{
	// First priority is $XDG_DATA_HOME
	const char *xdg_data_home_env = getenv("XDG_DATA_HOME");
	const auto xdg_data_home = CROSS_ResolveHome(
	        xdg_data_home_env ? xdg_data_home_env : "~/.local/share");

	std::deque<std::string> dirs = {
	        xdg_data_home + "/dosbox/soundfonts/",
	        xdg_data_home + "/soundfonts/",
	        xdg_data_home + "/sounds/sf2/",
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
		dirs.emplace_back(resolved_dir + "/soundfonts/");
		dirs.emplace_back(resolved_dir + "/sounds/sf2/");
	}

	// Third priority is $XDG_CONF_HOME, for convenience
	dirs.emplace_back(CROSS_GetPlatformConfigDir() + "soundfonts/");

	return dirs;
}

#endif

static std::string find_sf_file(const std::string &name)
{
	const std::string sf_path = CROSS_ResolveHome(name);
	if (path_exists(sf_path))
		return sf_path;
	for (const auto &dir : get_data_dirs()) {
		for (const auto &sf : {dir + name, dir + name + ".sf2"}) {
			// DEBUG_LOG_MSG("FSYNTH: FluidSynth checking if '%s' exists", sf.c_str());
			if (path_exists(sf))
				return sf;
		}
	}
	return "";
}

MidiHandlerFluidsynth::MidiHandlerFluidsynth() : keep_rendering(false) {}

bool MidiHandlerFluidsynth::Open([[maybe_unused]] const char *conf)
{
	Close();

	fluid_settings_ptr_t fluid_settings(new_fluid_settings(),
	                                    delete_fluid_settings);
	if (!fluid_settings) {
		LOG_MSG("FSYNTH: new_fluid_settings failed");
		return false;
	}

	// Setup the mixer callback
	const auto mixer_callback = std::bind(&MidiHandlerFluidsynth::MixerCallBack,
	                                      this, std::placeholders::_1);

	auto mixer_channel = MIXER_AddChannel(mixer_callback,
	                                      use_mixer_rate,
	                                      "FSYNTH",
	                                      {ChannelFeature::Sleep,
	                                       ChannelFeature::Stereo,
	                                       ChannelFeature::ReverbSend,
	                                       ChannelFeature::ChorusSend,
	                                       ChannelFeature::Synthesizer});

	auto *section = static_cast<Section_prop *>(control->GetSection("fluidsynth"));
	assert(section);

	const std::string filter_prefs = section->Get_string("fsynth_filter");

	if (!mixer_channel->TryParseAndSetCustomFilter(filter_prefs)) {
		if (filter_prefs != "off")
			LOG_WARNING("FSYNTH: Invalid 'fsynth_filter' value: '%s', using 'off'",
			            filter_prefs.c_str());

		mixer_channel->SetHighPassFilter(FilterState::Off);
		mixer_channel->SetLowPassFilter(FilterState::Off);
	}

	// Detailed explanation of all available FluidSynth settings:
	// http://www.fluidsynth.org/api/fluidsettings.xml

	// Per the FluidSynth API, the sample-rate should be part of the
	// settings used to instantiate the synth, so we create the mixer
	// channel first and use its native rate to configure FluidSynth.
	fluid_settings_setnum(fluid_settings.get(),
	                      "synth.sample-rate",
	                      mixer_channel->GetSampleRate());

	fsynth_ptr_t fluid_synth(new_fluid_synth(fluid_settings.get()),
	                         delete_fluid_synth);
	if (!fluid_synth) {
		LOG_MSG("FSYNTH: Failed to create the FluidSynth synthesizer.");
		return false;
	}

	// Load the requested SoundFont or quit if none provided
	const char *sf_file = section->Get_string("soundfont");
	const auto sf_spec = parse_sf_pref(sf_file);
	const auto soundfont = find_sf_file(std::get<std::string>(sf_spec));
	auto scale_by_percent = std::get<int>(sf_spec);

	if (!soundfont.empty() && fluid_synth_sfcount(fluid_synth.get()) == 0) {
		fluid_synth_sfload(fluid_synth.get(), soundfont.data(), true);
	}
	if (fluid_synth_sfcount(fluid_synth.get()) == 0) {
		LOG_WARNING("FSYNTH: FluidSynth failed to load '%s', check the path.",
		        sf_file);
		return false;
	}

	// Let the user know that the SoundFont was loaded
	LOG_MSG("FSYNTH: Using SoundFont '%s'", soundfont.c_str());

	if (scale_by_percent >= 0)
		LOG_WARNING("FSYNTH: SoundFont volume scaling has been deprecated. "
		            "Please use the MIXER command to set the volume of the "
		            "FluidSynth audio channel instead: MIXER FSYNTH %d",
		            scale_by_percent);

	// Set unity gain
	fluid_synth_set_gain(fluid_synth.get(), 1.0f);

	constexpr int fx_group = -1; // applies setting to all groups

	// Use a 7th-order (highest) polynomial to generate MIDI channel waveforms
	fluid_synth_set_interp_method(fluid_synth.get(), fx_group,
	                              FLUID_INTERP_HIGHEST);

	// Use reasonable chorus and reverb settings matching ScummVM's defaults

	auto apply_setting = [=](const char *name,
	                         const std::string &str_val,
	                         const double def_val,
	                         const double min_val,
	                         const double max_val) {
		// convert the string to a double
		const auto val = atof(str_val.c_str());
		if (val < min_val || val > max_val) {
			LOG_WARNING("FSYNTH: Invalid %s setting (%s), needs to be between %.2f and %.2f: using default (%.2f)",
			            name,
			            str_val.c_str(),
			            min_val,
			            max_val,
			            def_val);
			return def_val;
		}
		return val;
	};

	// get the users chorus settings
	const auto chorus = split(section->Get_string("fsynth_chorus"));
	bool chorus_enabled = !chorus.empty() && chorus[0] != "off";

	// does the soundfont have known-issues with chorus?
	const auto is_problematic_font = find_in_case_insensitive("FluidR3", soundfont) ||
	                                 find_in_case_insensitive("zdoom", soundfont);
	if (chorus_enabled && chorus[0] == "auto" && is_problematic_font) {
		chorus_enabled = false;
		LOG_INFO("FSYNTH: Chorus auto-disabled due to known issues with the %s soundfont",
		         soundfont.c_str());
	}

	// default chorus settings
	auto chorus_voice_count_f = 3.0;
	auto chorus_level = 1.2;
	auto chorus_speed = 0.3;
	auto chorus_depth = 8.0;
	auto chorus_mod_wave = fluid_chorus_mod::FLUID_CHORUS_MOD_SINE;

	// apply custom chorus settings if provided
	if (chorus_enabled && chorus.size() > 1) {
		if (chorus.size() == 5) {
			apply_setting("chorus voice-count",
			              chorus[0],
			              chorus_voice_count_f,
			              0,
			              99);

			apply_setting("chorus level", chorus[1], chorus_level, 0.0, 10.0);
			apply_setting("chorus speed", chorus[2], chorus_speed, 0.1, 5.0);
			apply_setting("chorus depth", chorus[3], chorus_depth, 0.0, 21.0);

			if (chorus[4] == "triange")
				chorus_mod_wave = fluid_chorus_mod::FLUID_CHORUS_MOD_TRIANGLE;
			else if (chorus[4] != "sine") // default is sine
				LOG_WARNING("FSYNTH: Invalid chorus modulation wave type ('%s'), needs to be 'sine' or 'triangle'",
				            chorus[4].c_str());

		} else {
			LOG_WARNING("FSYNTH: Invalid number of custom chorus settings (%d), should be five",
			            static_cast<int>(chorus.size()));
		}
	}
	// API accept an integer voice-count
	const auto chorus_voice_count = static_cast<int>(round(chorus_voice_count_f));

	// get the users reverb settings
	const auto reverb = split(section->Get_string("fsynth_reverb"));
	const bool reverb_enabled = !reverb.empty() && reverb[0] != "off";

	// default reverb settings
	auto reverb_room_size = 0.61;
	auto reverb_damping = 0.23;
	auto reverb_width = 0.76;
	auto reverb_level = 0.56;

	// apply custom reverb settings if provided
	if (reverb_enabled && reverb.size() > 1) {
		if (reverb.size() == 4) {
			apply_setting("reverb room-size",
			              reverb[0],
			              reverb_room_size,
			              0.0,
			              1.0);

			apply_setting("reverb damping", reverb[1], reverb_damping, 0.0, 1.0);
			apply_setting("reverb width", reverb[2], reverb_width, 0.0, 100.0);
			apply_setting("reverb level", reverb[3], reverb_level, 0.0, 1.0);
		} else {
			LOG_WARNING("FSYNTH: Invalid number of custom reverb settings (%d), should be four",
			            static_cast<int>(reverb.size()));
		}
	}

// current API calls as of 2.2
#if FLUIDSYNTH_VERSION_MINOR >= 2
	fluid_synth_chorus_on(fluid_synth.get(), fx_group, chorus_enabled);
	fluid_synth_set_chorus_group_nr(fluid_synth.get(), fx_group, chorus_voice_count);
	fluid_synth_set_chorus_group_level(fluid_synth.get(), fx_group, chorus_level);
	fluid_synth_set_chorus_group_speed(fluid_synth.get(), fx_group, chorus_speed);
	fluid_synth_set_chorus_group_depth(fluid_synth.get(), fx_group, chorus_depth);
	fluid_synth_set_chorus_group_type(fluid_synth.get(), fx_group, static_cast<int>(chorus_mod_wave));

	fluid_synth_reverb_on(fluid_synth.get(), fx_group, reverb_enabled);
	fluid_synth_set_reverb_group_roomsize(fluid_synth.get(), fx_group, reverb_room_size);
	fluid_synth_set_reverb_group_damp(fluid_synth.get(), fx_group, reverb_damping);
	fluid_synth_set_reverb_group_width(fluid_synth.get(), fx_group, reverb_width);
	fluid_synth_set_reverb_group_level(fluid_synth.get(), fx_group, reverb_level);

// deprecated API calls prior to 2.2
#else
	fluid_synth_set_chorus_on(fluid_synth.get(), chorus_enabled);
	fluid_synth_set_chorus(fluid_synth.get(), chorus_voice_count, chorus_level, chorus_speed,
	                       chorus_depth, chorus_mod_wave);

	fluid_synth_set_reverb_on(fluid_synth.get(), reverb_enabled);
	fluid_synth_set_reverb(fluid_synth.get(), reverb_room_size,
	                       reverb_damping, reverb_width, reverb_level);
#endif

	if (chorus_enabled)
		LOG_MSG("FSYNTH: Chorus enabled with %d voices at level %.2f, %.2f Hz speed, %.2f depth, and %s-wave modulation",
		        chorus_voice_count,
		        chorus_level,
		        chorus_speed,
		        chorus_depth,
		        chorus_mod_wave == fluid_chorus_mod::FLUID_CHORUS_MOD_SINE
		                ? "sine"
		                : "triangle");

	if (reverb_enabled)
		LOG_MSG("FSYNTH: Reverb enabled with a %.2f room size, %.2f damping, %.2f width, and level %.2f",
		        reverb_room_size,
		        reverb_damping,
		        reverb_width,
		        reverb_level);

	settings = std::move(fluid_settings);
	synth = std::move(fluid_synth);
	channel = std::move(mixer_channel);
	selected_font = soundfont;

	// Start rendering audio
	keep_rendering = true;
	const auto render = std::bind(&MidiHandlerFluidsynth::Render, this);
	renderer = std::thread(render);
	set_thread_name(renderer, "dosbox:fsynth");
	play_buffer = playable.Dequeue(); // populate the first play buffer

	// Start playback
	is_open = true;
	return true;
}

MidiHandlerFluidsynth::~MidiHandlerFluidsynth()
{
	Close();
}

void MidiHandlerFluidsynth::Close()
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

	// Reset the members
	channel.reset();
	synth.reset();
	settings.reset();
	last_played_frame = 0;
	selected_font.clear();

	is_open = false;
}

void MidiHandlerFluidsynth::PlayMsg(const uint8_t *msg)
{
	assert(channel);
	channel->WakeUp();

	const int chanID = msg[0] & 0b1111;

	switch (msg[0] & 0b1111'0000) {
	case 0b1000'0000:
		fluid_synth_noteoff(synth.get(), chanID, msg[1]);
		break;
	case 0b1001'0000:
		fluid_synth_noteon(synth.get(), chanID, msg[1], msg[2]);
		break;
	case 0b1010'0000:
		fluid_synth_key_pressure(synth.get(), chanID, msg[1], msg[2]);
		break;
	case 0b1011'0000:
		fluid_synth_cc(synth.get(), chanID, msg[1], msg[2]);
		break;
	case 0b1100'0000:
		fluid_synth_program_change(synth.get(), chanID, msg[1]);
		break;
	case 0b1101'0000:
		fluid_synth_channel_pressure(synth.get(), chanID, msg[1]);
		break;
	case 0b1110'0000:
		fluid_synth_pitch_bend(synth.get(), chanID, msg[1] + (msg[2] << 7));
		break;
	default: {
		uint64_t tmp;
		memcpy(&tmp, msg, sizeof(tmp));
		LOG_MSG("FSYNTH: unknown MIDI command: %0" PRIx64, tmp);
		break;
	}
	}
}

void MidiHandlerFluidsynth::PlaySysex(uint8_t *sysex, size_t len)
{
	assert(channel);
	channel->WakeUp();

	const char *data = reinterpret_cast<const char *>(sysex);
	const auto n = static_cast<int>(len);
	fluid_synth_sysex(synth.get(), data, n, nullptr, nullptr, nullptr, false);
}

void MidiHandlerFluidsynth::MixerCallBack(uint16_t requested_frames)
{
	while (requested_frames) {
		const auto frames_to_be_played = std::min(GetRemainingFrames(),
		                                          requested_frames);
		const auto sample_offset_in_buffer = play_buffer.data() +
		                                     last_played_frame * 2;

		assert(frames_to_be_played <= play_buffer.size());
		channel->AddSamples_sfloat(frames_to_be_played,
		                           sample_offset_in_buffer);

		requested_frames -= frames_to_be_played;
		last_played_frame += frames_to_be_played;
	}
}

// Returns the number of frames left to play in the buffer.
uint16_t MidiHandlerFluidsynth::GetRemainingFrames()
{
	// If the current buffer has some frames left, then return those ...
	if (last_played_frame < FRAMES_PER_BUFFER)
		return FRAMES_PER_BUFFER - last_played_frame;

	// Otherwise put the spent buffer in backstock and get the next buffer
	backstock.Enqueue(std::move(play_buffer));
	play_buffer = playable.Dequeue();
	last_played_frame = 0; // reset the frame counter to the beginning

	return FRAMES_PER_BUFFER;
}

// Populates the playable queue with freshly rendered buffers
void MidiHandlerFluidsynth::Render()
{
	// Allocate our buffers once and reuse for the duration.
	constexpr auto SAMPLES_PER_BUFFER = FRAMES_PER_BUFFER * 2; // L & R
	std::vector<float> render_buffer(SAMPLES_PER_BUFFER);
	std::vector<float> playable_buffer(SAMPLES_PER_BUFFER);

	// Populate the backstock using copies of the current buffer.
	while (backstock.Size() < backstock.MaxCapacity() - 1)
		backstock.Enqueue(playable_buffer);

	backstock.Enqueue(std::move(playable_buffer));
	assert(backstock.Size() == backstock.MaxCapacity());

	while (keep_rendering.load()) {
		fluid_synth_write_float(synth.get(), FRAMES_PER_BUFFER,
		                        render_buffer.data(), 0, 2,
		                        render_buffer.data(), 1, 2);

		// Grab the next buffer from backstock and populate it ...
		playable_buffer = backstock.Dequeue();

		// Swap buffers & scale
		std::swap(render_buffer, playable_buffer);
		for (auto &s : playable_buffer)
			s *= INT16_MAX;

		// and then move it into the playable queue
		playable.Enqueue(std::move(playable_buffer));
	}
}

std::string format_sf2_line(size_t width, const std_fs::path &sf2_path)
{
	assert(width > 0);
	std::vector<char> line_buf(width);

	const auto &name = sf2_path.filename().string();
	const auto &path = simplify_path(sf2_path).string();

	snprintf(line_buf.data(), width, "%-16s - %s", name.c_str(), path.c_str());
	std::string line = line_buf.data();

	// Formatted line did not fill the whole buffer - no further formatting
	// is necessary.
	if (line.size() + 1 < width)
		return line;

	// The description was too long and got trimmed; place three dots in
	// the end to make it clear to the user.
	const std::string cutoff = "...";
	assert(line.size() > cutoff.size());
	const auto start = line.end() - static_cast<int>(cutoff.size());
	line.replace(start, line.end(), cutoff);
	return line;
}

MIDI_RC MidiHandlerFluidsynth::ListAll(Program *caller)
{
	auto *section = static_cast<Section_prop *>(control->GetSection("fluidsynth"));
	const auto sf_spec = parse_sf_pref(section->Get_string("soundfont"));
	const auto sf_name = std::get<std::string>(sf_spec);
	const size_t term_width = INT10_GetTextColumns();

	auto write_line = [caller](bool highlight, const std::string &line) {
		const char color[] = "\033[32;1m";
		const char nocolor[] = "\033[0m";
		if (highlight)
			caller->WriteOut("* %s%s%s\n", color, line.c_str(), nocolor);
		else
			caller->WriteOut("  %s\n", line.c_str());
	};

	// If selected soundfont exists in the current working directory,
	// then print it.
	const std::string sf_path = CROSS_ResolveHome(sf_name);
	if (path_exists(sf_path)) {
		write_line((sf_path == selected_font), sf_name);
	}

	// Go through all soundfont directories and list all .sf2 files.
	for (const auto &dir_path : get_data_dirs()) {
		std::error_code ec = {};
		for (const auto &entry : std_fs::directory_iterator(dir_path, ec)) {
			if (ec)
				break; // problem iterating, so skip the directory

			// Is it a file?
			if (!std_fs::is_regular_file(entry))
				continue;

			// Is it an .sf2 file?
			auto ext = entry.path().extension().string();
			lowcase(ext);
			if (ext != ".sf2")
				continue;

			const auto &sf2_path = entry.path();
			const auto line = format_sf2_line(term_width - 2, sf2_path);
			const bool highlight = is_open && (selected_font ==
			                                   sf2_path.string());

			write_line(highlight, line);
		}
	}

	return MIDI_RC::OK;
}

static void fluid_init([[maybe_unused]] Section *sec)
{}

void FLUID_AddConfigSection(const config_ptr_t &conf)
{
	assert(conf);
	Section_prop *sec = conf->AddSection_prop("fluidsynth", &fluid_init);
	assert(sec);
	init_fluid_dosbox_settings(*sec);
}

#endif // C_FLUIDSYNTH
