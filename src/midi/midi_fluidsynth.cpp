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


static constexpr int FRAMES_PER_BUFFER = 512; // synth granularity

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
			// DEBUG_LOG_MSG("MIDI: FluidSynth checking if '%s' exists", sf.c_str());
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
		LOG_MSG("MIDI: new_fluid_settings failed");
		return false;
	}

	// Setup the mixer callback
	const auto mixer_callback = std::bind(&MidiHandlerFluidsynth::MixerCallBack,
	                                      this, std::placeholders::_1);

	const auto mixer_channel = MIXER_AddChannel(mixer_callback,
	                                            0,
	                                            "FSYNTH",
	                                            {ChannelFeature::Sleep,
	                                             ChannelFeature::ReverbSend,
	                                             ChannelFeature::Stereo,
	                                             ChannelFeature::Synthesizer});

	// Detailed explanation of all available FluidSynth settings:
	// http://www.fluidsynth.org/api/fluidsettings.xml

	// Per the FluidSynth API, the sample-rate should be part of the
	// settings used to instantiate the synth, so we create the mixer
	// channel first and use its native rate to configure FluidSynth.
	fluid_settings_setnum(fluid_settings.get(), "synth.sample-rate",
	                      mixer_channel->GetSampleRate());

	fsynth_ptr_t fluid_synth(new_fluid_synth(fluid_settings.get()),
	                         delete_fluid_synth);
	if (!fluid_synth) {
		LOG_MSG("MIDI: Failed to create the FluidSynth synthesizer");
		return false;
	}

	// Load the requested SoundFont or quit if none provided
	auto *section = static_cast<Section_prop *>(control->GetSection("fluidsynth"));
	const auto sf_spec = parse_sf_pref(section->Get_string("soundfont"));
	const auto soundfont = find_sf_file(std::get<std::string>(sf_spec));
	auto scale_by_percent = std::get<int>(sf_spec);

	if (!soundfont.empty() && fluid_synth_sfcount(fluid_synth.get()) == 0) {
		fluid_synth_sfload(fluid_synth.get(), soundfont.data(), true);
	}
	if (fluid_synth_sfcount(fluid_synth.get()) == 0) {
		LOG_MSG("MIDI: FluidSynth failed to load '%s', check the path.",
		        soundfont.c_str());
		return false;
	}

	// Let the user know that the SoundFont was loaded
	LOG_MSG("MIDI: Using SoundFont '%s'", soundfont.c_str());

	if (scale_by_percent >= 0)
		LOG_WARNING("MIDI: SoundFont volume scaling has been deprecated. "
		            "Please use the MIXER command to set the volume of the "
		            "FluidSynth audio channel instead: MIXER FSYNTH %d",
		            scale_by_percent);

	constexpr int fx_group = -1; // applies setting to all groups

	// Use a 7th-order (highest) polynomial to generate MIDI channel waveforms
	fluid_synth_set_interp_method(fluid_synth.get(), fx_group,
	                              FLUID_INTERP_HIGHEST);

	// Disable FluidSynth's reverb and chorus in favour of the mixer's, which
	// ensures similar effects are applied to all channels.
	fluid_synth_reverb_on(fluid_synth.get(), fx_group, false);
	fluid_synth_chorus_on(fluid_synth.get(), fx_group, false);

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
	selected_font = "";

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
		LOG_MSG("MIDI: unknown MIDI command: %0" PRIx64, tmp);
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

		// Scale and copy to playable buffer
		auto in_pos  = render_buffer.begin();
		auto out_pos = playable_buffer.begin();

		while (in_pos != render_buffer.end()) {
			AudioFrame frame = {*in_pos++, *in_pos++};

			frame.left *= INT16_MAX;
			frame.right *= INT16_MAX;

			*out_pos++ = frame.left;
			*out_pos++ = frame.right;
		}

		// and then move it into the playable queue
		playable.Enqueue(std::move(playable_buffer));
	}
}

std::string format_sf2_line(size_t width, const std::string &name, const std::string &path)
{
	assert(width > 0);
	std::vector<char> line_buf(width);
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
	char dir_entry_name[CROSS_LEN];
	for (const auto &dir_path : get_data_dirs()) {
		dir_information *dir = open_directory(dir_path.c_str());
		bool is_directory = false;
		if (!dir)
			continue;
		if (!read_directory_first(dir, dir_entry_name, is_directory))
			continue;
		do {
			if (is_directory)
				continue;

			const size_t name_len = safe_strlen(dir_entry_name);
			if (name_len < 4)
				continue;
			const char *ext = dir_entry_name + name_len - 4;
			const bool is_sf2 = (strcasecmp(ext, ".sf2") == 0);
			if (!is_sf2)
				continue;

			const std::string font_path = dir_path + dir_entry_name;

			const auto line = format_sf2_line(term_width - 2,
			                                  dir_entry_name, font_path);
			const bool highlight = is_open &&
			                       (selected_font == font_path);

			write_line(highlight, line);

		} while (read_directory_next(dir, dir_entry_name, is_directory));
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
