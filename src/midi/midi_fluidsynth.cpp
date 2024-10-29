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

#if C_FLUIDSYNTH

#include <bitset>
#include <cassert>
#include <deque>
#include <numeric>
#include <string>
#include <tuple>

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

MidiHandlerFluidsynth instance;

constexpr auto SoundFontExtension = ".sf2";

static void init_fluid_dosbox_settings(Section_prop& secprop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	// Name 'default.sf2' picks the default SoundFont if it's installed
	// in the OS. Usually it's Fluid_R3.
	auto* str_prop = secprop.Add_string("soundfont", when_idle, "default.sf2");
	str_prop->Set_help(
	        "Path to a SoundFont file in .sf2 format ('default.sf2' by default).\n"
	        "You can use an absolute or relative path, or the name of an .sf2 inside the\n"
	        "'soundfonts' directory within your DOSBox configuration directory.\n"
	        "An optional percentage value after the name will scale the SoundFont's volume.\n"
	        "This is useful for normalising the volume of different SoundFonts.\n"
	        "E.g. 'my_soundfont.sf2 50' will attenuate the volume by 50%%.\n"
	        "The percentage value can range from 1 to 800.");

	str_prop = secprop.Add_string("fsynth_chorus", when_idle, "auto");
	str_prop->Set_help(
	        "Chorus effect: 'auto' (default), 'on', 'off', or custom values.\n"
	        "When using custom values:\n"
	        "  All five must be provided in-order and space-separated.\n"
	        "  They are: voice-count level speed depth modulation-wave, where:\n"
	        "    - voice-count is an integer from 0 to 99\n"
	        "    - level is a decimal from 0.0 to 10.0\n"
	        "    - speed is a decimal, measured in Hz, from 0.1 to 5.0\n"
	        "    - depth is a decimal from 0.0 to 21.0\n"
	        "    - modulation-wave is either 'sine' or 'triangle'\n"
	        "  For example: chorus = 3 1.2 0.3 8.0 sine\n"
	        "Note: You can disable the FluidSynth chorus and enable the mixer-level chorus\n"
	        "      on the FluidSynth channel instead, or enable both chorus effects at the\n"
	        "      same time. Whether this sounds good depends on the SoundFont and the\n"
	        "      chorus settings being used.");

	str_prop = secprop.Add_string("fsynth_reverb", when_idle, "auto");
	str_prop->Set_help(
	        "Reverb effect: 'auto' (default), 'on', 'off', or custom values.\n"
	        "When using custom values:\n"
	        "  All four must be provided in-order and space-separated.\n"
	        "  They are: room-size damping width level, where:\n"
	        "    - room-size is a decimal from 0.0 to 1.0\n"
	        "    - damping is a decimal from 0.0 to 1.0\n"
	        "    - width is a decimal from 0.0 to 100.0\n"
	        "    - level is a decimal from 0.0 to 1.0\n"
	        "  For example: reverb = 0.61 0.23 0.76 0.56\n"
	        "Note: You can disable the FluidSynth reverb and enable the mixer-level reverb\n"
	        "      on the FluidSynth channel instead, or enable both reverb effects at the\n"
	        "      same time. Whether this sounds good depends on the SoundFont and the\n"
	        "      reverb settings being used.");

	str_prop = secprop.Add_string("fsynth_filter", when_idle, "off");
	assert(str_prop);
	str_prop->Set_help(
	        "Filter for the FluidSynth audio output:\n"
	        "  off:       Don't filter the output (default).\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

// Parses the 'soundfont' setting which has the 'FILENAME [SCALE]' format.
// The function returns the filename and the optional scale percentage as a
// tuple. If the scale percentage is not provided, a default value of 100 is
// used.
std::tuple<std::string, int> parse_soundfont_pref(const std::string& line)
{
	constexpr auto default_percent = 100;

	if (line.empty()) {
		return {line, default_percent};
	}

	// Look for a space in the last 5 characters of the string
	const auto len      = line.length();
	const auto from_pos = len < 5 ? 0 : len - 5;
	auto last_space_pos = line.substr(from_pos).find_last_of(' ');

	if (last_space_pos == std::string::npos) {
		return {line, default_percent};
	}

	// Ensure the position is relative to the start of the entire string
	last_space_pos += from_pos;

	// Is the stuff after the last space convertable to a number?
	int percent = 0;
	try {
		percent = stoi(line.substr(last_space_pos + 1));
	} catch (...) {
		return {line, default_percent};
	}

	// A number was provided, so split it from the line
	std::string filename = line.substr(0, last_space_pos);
	// Drop any extra whitespace prior to the number
	trim(filename);

	return {filename, percent};
}

#if defined(WIN32)

static std::deque<std_fs::path> get_data_dirs()
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

static std::deque<std_fs::path> get_data_dirs()
{
	return {
	        GetConfigDir() / DefaultSoundfontsDir,
	        resolve_home("~/Library/Audio/Sounds/Banks"),
	};
}

#else

static std::deque<std_fs::path> get_data_dirs()
{
	// First priority is user-specific data location
	const auto xdg_data_home = get_xdg_data_home();

	std::deque<std_fs::path> dirs = {
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

static std_fs::path find_sf_file(const std::string& name)
{
	const std_fs::path sf_path = resolve_home(name);
	if (path_exists(sf_path)) {
		return sf_path;
	}
	for (const auto& dir : get_data_dirs()) {
		for (const auto& sf :
		     {dir / name, dir / (name + SoundFontExtension)}) {
#if 0
			LOG_MSG("FSYNTH: FluidSynth checking if '%s' exists", sf.c_str());
#endif
			if (path_exists(sf)) {
				// Parts of the path come from the `soundfont` setting, and
				// `soundfont = FluidR3_GM.sf2` and `soundfont =
				// fluidr3_gm.sf2` refer to the same file on case-preserving
				// filesystems on Windows and macOS.
				//
				// `std_fs::canonical` returns the absolute path and matches
				// its casing to that of the actual physical file. This
				// prevents certain subtle bugs downstream when we use this
				// path in comparisons.
				//
				return std_fs::canonical(sf).c_str();
			}
		}
	}
	return {};
}

static void log_unknown_midi_message(const std::vector<uint8_t>& msg)
{
	auto append_as_hex = [](const std::string& str, const uint8_t val) {
		constexpr char hex_chars[] = "0123456789ABCDEF";
		std::string hex_str;

		hex_str.reserve(2);
		hex_str += hex_chars[val >> 4];
		hex_str += hex_chars[val & 0x0F];

		return str + (str.empty() ? "" : ", ") + hex_str;
	};

	const auto hex_values = std::accumulate(msg.begin(),
	                                        msg.end(),
	                                        std::string(),
	                                        append_as_hex);

	LOG_WARNING("FSYNTH: Unknown MIDI message sequence (hex): %s",
	            hex_values.c_str());
}

static Section_prop* get_fluidsynth_section()
{
	assert(control);

	auto sec = static_cast<Section_prop*>(control->GetSection("fluidsynth"));
	assert(sec);

	return sec;
}

bool MidiHandlerFluidsynth::Open([[maybe_unused]] const char* conf)
{
	Close();

	FluidSynthSettingsPtr fluid_settings(new_fluid_settings(),
	                                     delete_fluid_settings);
	if (!fluid_settings) {
		LOG_WARNING("FSYNTH: new_fluid_settings failed");
		return false;
	}

	auto* section = get_fluidsynth_section();

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
		LOG_WARNING("FSYNTH: Failed to create the FluidSynth synthesizer.");
		return false;
	}

	// Load the requested SoundFont or quit if none provided
	auto [sf_filename, scale_by_percent] = parse_soundfont_pref(
	        section->Get_string("soundfont"));

	const auto soundfont_path = find_sf_file(sf_filename);

	if (!soundfont_path.empty() && fluid_synth_sfcount(fluid_synth.get()) == 0) {
		constexpr auto reset_presets = true;
		fluid_synth_sfload(fluid_synth.get(),
		                   soundfont_path.string().c_str(),
		                   reset_presets);
	}

	if (fluid_synth_sfcount(fluid_synth.get()) == 0) {
		LOG_WARNING("FSYNTH: FluidSynth failed to load '%s', check the path.",
		            sf_filename.c_str());
		return false;
	}

	if (scale_by_percent < 1 || scale_by_percent > 800) {
		LOG_WARNING(
		        "FSYNTH: Invalid volume scaling percentage: %d; "
		        "must be between 1 and 800, defaulting to 100%%",
		        scale_by_percent);
		scale_by_percent = 100;
	}
	fluid_synth_set_gain(fluid_synth.get(),
	                     static_cast<float>(scale_by_percent) / 100.0f);

	// Let the user know that the SoundFont was loaded
	if (scale_by_percent == 100) {
		LOG_MSG("FSYNTH: Using SoundFont '%s'",
		        soundfont_path.string().c_str());
	} else {
		LOG_MSG("FSYNTH: Using SoundFont '%s' with volume scaled to %d%%",
		        soundfont_path.string().c_str(),
		        scale_by_percent);
	}

	constexpr int fx_group = -1; // applies setting to all groups

	// Use a 7th-order (highest) polynomial to generate MIDI channel waveforms
	fluid_synth_set_interp_method(fluid_synth.get(), fx_group, FLUID_INTERP_HIGHEST);

	// Use reasonable chorus and reverb settings matching ScummVM's defaults

	// Checks if the passed value is within valid range and returns the
	// default if it's not
	auto validate_setting = [=](const char* name,
	                            const std::string& str_val,
	                            const double def_val,
	                            const double min_val,
	                            const double max_val) {
		// convert the string to a double
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
	};

	// Get the user's chorus settings
	const auto chorus   = split(section->Get_string("fsynth_chorus"));
	bool chorus_enabled = !chorus.empty() && chorus[0] != "off";

	// Does the SoundFont have known-issues with chorus?
	const auto is_problematic_font =
	        find_in_case_insensitive("FluidR3", soundfont_path.string()) ||
	        find_in_case_insensitive("zdoom", soundfont_path.string());

	if (chorus_enabled && chorus[0] == "auto" && is_problematic_font) {
		chorus_enabled = false;
		LOG_INFO("FSYNTH: Chorus auto-disabled due to known issues with the '%s' soundfont",
		         soundfont_path.filename().string().c_str());
	}

	// default chorus settings
	auto chorus_voice_count_f = 3.0;
	auto chorus_level         = 1.2;
	auto chorus_speed         = 0.3;
	auto chorus_depth         = 8.0;
	auto chorus_mod_wave      = fluid_chorus_mod::FLUID_CHORUS_MOD_SINE;

	// apply custom chorus settings if provided
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

	// Get the user's reverb settings
	const auto reverb         = split(section->Get_string("fsynth_reverb"));
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

// Current API calls as of 2.2
#if FLUIDSYNTH_VERSION_MINOR >= 2
	fluid_synth_chorus_on(fluid_synth.get(), fx_group, chorus_enabled);
	fluid_synth_set_chorus_group_nr(fluid_synth.get(), fx_group, chorus_voice_count);
	fluid_synth_set_chorus_group_level(fluid_synth.get(), fx_group, chorus_level);
	fluid_synth_set_chorus_group_speed(fluid_synth.get(), fx_group, chorus_speed);
	fluid_synth_set_chorus_group_depth(fluid_synth.get(), fx_group, chorus_depth);
	fluid_synth_set_chorus_group_type(fluid_synth.get(),
	                                  fx_group,
	                                  static_cast<int>(chorus_mod_wave));

	fluid_synth_reverb_on(fluid_synth.get(), fx_group, reverb_enabled);
	fluid_synth_set_reverb_group_roomsize(fluid_synth.get(),
	                                      fx_group,
	                                      reverb_room_size);
	fluid_synth_set_reverb_group_damp(fluid_synth.get(), fx_group, reverb_damping);
	fluid_synth_set_reverb_group_width(fluid_synth.get(), fx_group, reverb_width);
	fluid_synth_set_reverb_group_level(fluid_synth.get(), fx_group, reverb_level);

// deprecated API calls prior to 2.2
#else
	fluid_synth_set_chorus_on(fluid_synth.get(), chorus_enabled);
	fluid_synth_set_chorus(fluid_synth.get(),
	                       chorus_voice_count,
	                       chorus_level,
	                       chorus_speed,
	                       chorus_depth,
	                       chorus_mod_wave);

	fluid_synth_set_reverb_on(fluid_synth.get(), reverb_enabled);
	fluid_synth_set_reverb(fluid_synth.get(),
	                       reverb_room_size,
	                       reverb_damping,
	                       reverb_width,
	                       reverb_level);
#endif

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

	if (reverb_enabled) {
		LOG_MSG("FSYNTH: Reverb enabled with a %.2f room size, "
		        "%.2f damping, %.2f width, and level %.2f",
		        reverb_room_size,
		        reverb_damping,
		        reverb_width,
		        reverb_level);
	}

	MIXER_LockMixerThread();

	// Setup the mixer callback
	const auto mixer_callback = std::bind(&MidiHandlerFluidsynth::MixerCallBack,
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
	// bursty. The Mixer's default of ~20 ms becomes 40 ms here, which gives
	// slower systems a better to keep up (and prevent their audio frame
	// FIFO from running dry).
	const auto render_ahead_ms = MIXER_GetPreBufferMs() * 2;

	// Size the out-bound audio frame FIFO
	assertm(sample_rate_hz >= 8000, "Sample rate must be at least 8 kHz");

	const auto audio_frames_per_ms = iround(sample_rate_hz / MillisInSecond);
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
	settings      = std::move(fluid_settings);
	synth         = std::move(fluid_synth);
	mixer_channel = std::move(fluidsynth_channel);

	current_sf2_path = soundfont_path;

	// Start rendering audio
	const auto render = std::bind(&MidiHandlerFluidsynth::Render, this);
	renderer          = std::thread(render);
	set_thread_name(renderer, "dosbox:fsynth");

	// Start playback
	is_open = true;
	MIXER_UnlockMixerThread();
	return true;
}

MidiHandlerFluidsynth::~MidiHandlerFluidsynth()
{
	Close();
}

void MidiHandlerFluidsynth::Close()
{
	if (!is_open) {
		return;
	}

	LOG_MSG("FSYNTH: Shutting down");

	MIXER_LockMixerThread();

	if (had_underruns) {
		LOG_WARNING(
		        "FSYNTH: Fix underruns by lowering CPU load, increasing "
		        "your conf's prebuffer, or using a simpler SoundFont");
		had_underruns = false;
	}

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

	// Reset the members
	synth.reset();
	settings.reset();

	current_sf2_path = {};

	// Deregister the mixer channel and remove it
	assert(mixer_channel);
	MIXER_DeregisterChannel(mixer_channel);
	mixer_channel.reset();

	last_rendered_ms   = 0.0;
	ms_per_audio_frame = 0.0;

	is_open = false;
	MIXER_UnlockMixerThread();
}

int MidiHandlerFluidsynth::GetNumPendingAudioFrames()
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
void MidiHandlerFluidsynth::PlayMsg(const MidiMessage& msg)
{
	std::vector<uint8_t> message(msg.data.begin(), msg.data.end());

	MidiWork work{std::move(message),
	              GetNumPendingAudioFrames(),
	              MessageType::Channel};

	work_fifo.Enqueue(std::move(work));
}

// The request to play the sysex message is placed in the MIDI work FIFO
void MidiHandlerFluidsynth::PlaySysex(uint8_t* sysex, size_t len)
{
	std::vector<uint8_t> message(sysex, sysex + len);
	MidiWork work{std::move(message), GetNumPendingAudioFrames(), MessageType::SysEx};
	work_fifo.Enqueue(std::move(work));
}

void MidiHandlerFluidsynth::ApplyChannelMessage(const std::vector<uint8_t>& msg)
{
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
void MidiHandlerFluidsynth::ApplySysexMessage(const std::vector<uint8_t>& msg)
{
	const char* data = reinterpret_cast<const char*>(msg.data());
	const auto n     = static_cast<int>(msg.size());

	fluid_synth_sysex(synth.get(), data, n, nullptr, nullptr, nullptr, false);
}

// The callback operates at the audio frame-level, steadily adding samples to
// the mixer until the requested numbers of audio frames is met.
void MidiHandlerFluidsynth::MixerCallBack(const int requested_audio_frames)
{
	assert(mixer_channel);

	// Report buffer underruns
	constexpr auto warning_percent = 5.0f;

	if (const auto percent_full = audio_frame_fifo.GetPercentFull();
	    percent_full < warning_percent) {
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
		last_rendered_ms = PIC_FullIndex();
	} else {
		assert(!audio_frame_fifo.IsRunning());
		mixer_channel->AddSilence();
	}
}

void MidiHandlerFluidsynth::RenderAudioFramesToFifo(const int num_audio_frames)
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

void MidiHandlerFluidsynth::ProcessWorkFromFifo()
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
		ApplyChannelMessage(work->message);
	} else {
		assert(work->message_type == MessageType::SysEx);
		ApplySysexMessage(work->message);
	}
}

// Keep the fifo populated with freshly rendered buffers
void MidiHandlerFluidsynth::Render()
{
	while (work_fifo.IsRunning()) {
		work_fifo.IsEmpty() ? RenderAudioFramesToFifo()
		                    : ProcessWorkFromFifo();
	}
}

std::string format_sf2_line(size_t width, const std_fs::path& sf2_path)
{
	assert(width > 0);
	std::vector<char> line_buf(width);

	const auto& name = sf2_path.filename().string();
	const auto& path = simplify_path(sf2_path).string();

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

MIDI_RC MidiHandlerFluidsynth::ListAll(Program* caller)
{
	const size_t term_width = INT10_GetTextColumns();

	auto write_line = [&](const std_fs::path& sf2_path) {
		const auto line = format_sf2_line(term_width - 2, sf2_path);

		const auto path_matches = current_sf2_path &&
		                          (*current_sf2_path == sf2_path);

		const bool do_highlight = is_open && path_matches;

		if (do_highlight) {
			constexpr auto Green = "[color=light-green]";
			constexpr auto Reset = "[reset]";

			const auto output = format_str("%s* %s%s\n",
			                               Green,
			                               line.c_str(),
			                               Reset);

			caller->WriteOut(convert_ansi_markup(output).c_str());
		} else {
			caller->WriteOut("  %s\n", line.c_str());
		}
	};

	// Print SoundFont found from user config.
	std::error_code err = {};

	std::vector<std_fs::path> sf2_files = {};

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

			const auto& sf2_path = entry.path();

			// Is it an .sf2 file?
			auto ext = sf2_path.extension().string();
			lowcase(ext);
			if (ext != SoundFontExtension) {
				continue;
			}

			sf2_files.emplace_back(sf2_path);
		}
	}

	std::sort(sf2_files.begin(), sf2_files.end());

	for (const auto& path : sf2_files) {
		write_line(path);
	}

	return MIDI_RC::OK;
}

static void fluid_init([[maybe_unused]] Section* sec) {}

void FLUID_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);
	Section_prop* sec = conf->AddSection_prop("fluidsynth", &fluid_init);
	assert(sec);
	init_fluid_dosbox_settings(*sec);
}

#endif // C_FLUIDSYNTH
