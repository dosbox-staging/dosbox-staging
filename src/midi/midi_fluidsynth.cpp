/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2002-2011  The DOSBox Team
 *  Copyright (C) 2020       Nikos Chantziaras <realnc@gmail.com>
 *  Copyright (C) 2020       The dosbox-staging team
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
#include <cstdlib>
#include <string>

#include "control.h"
#include "cross.h"

MidiHandlerFluidsynth instance;

static void init_fluid_dosbox_settings(Section_prop &secprop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto *multi_prop = secprop.Add_multiremain("soundfont", when_idle, " ");

	multi_prop->Set_help(
	        "Path to a MIDI SoundFont file in .sf2 format to use with FluidSynth.\n"
	        "An optional second parameter, in percent from 1 to 500, scales this SF2's volume.\n"
	        "For example, soundfont = /path/to/my.sf2 50 will attenuate its volume by 50%\n.");
	multi_prop->GetSection()->Add_string("soundfont_path", when_idle, "");
	multi_prop->GetSection()->Add_string("soundfont_scale", when_idle, "100");
	multi_prop->SetValue("");

	// TODO Handle storing soundfonts in specific directory and update
	// the documentation; right now users need to specify full path or
	// fall on undocumented FluidSynth internal algorithm for picking
	// sf2 files.

	auto *int_prop = secprop.Add_int("synth_threads", when_idle, 1);
	int_prop->SetMinMax(1, 256);
	int_prop->Set_help(
	        "If set to a value greater than 1, then additional synthesis\n"
	        "threads will be created to take advantage of many CPU cores.\n"
	        "(min 1, max 256)");
}

// SetMixerLevel is a callback that's given the user-desired mixer level,
// which is a floating point multiplier that we apply internally as
// FluidSynth's gain value. We then read-back the gain, and use that to
// derive a pre-scale level.
void MidiHandlerFluidsynth::SetMixerLevel(const AudioFrame &desired_level) noexcept
{
	prescale_level.left = INT16_MAX * desired_level.left;
	prescale_level.right = INT16_MAX * desired_level.right;
}

bool MidiHandlerFluidsynth::Open(MAYBE_UNUSED const char *conf)
{
	Close();

	fluid_settings_ptr_t fluid_settings(new_fluid_settings(),
	                                    delete_fluid_settings);
	if (!fluid_settings) {
		LOG_MSG("MIDI: new_fluid_settings failed");
		return false;
	}

	auto *section = static_cast<Section_prop *>(control->GetSection("fluidsynth"));

	// Detailed explanation of all available FluidSynth settings:
	// http://www.fluidsynth.org/api/fluidsettings.xml

	const int cpu_cores = section->Get_int("synth_threads");
	fluid_settings_setint(fluid_settings.get(), "synth.cpu-cores", cpu_cores);

	const auto mixer_callback = std::bind(&MidiHandlerFluidsynth::MixerCallBack,
	                                      this, std::placeholders::_1);
	mixer_channel_ptr_t mixer_channel(MIXER_AddChannel(mixer_callback, 0, "FSYNTH"),
	                                  MIXER_DelChannel);

	// Per the FluidSynth API, the sample-rate should be part of the settings used to
	// instantiate the synth, so we create the mixer channel first and use its native
	// rate to configure FluidSynth.
	fluid_settings_setnum(fluid_settings.get(), "synth.sample-rate",
	                      mixer_channel->GetSampleRate());

	fsynth_ptr_t fluid_synth(new_fluid_synth(fluid_settings.get()),
	                         delete_fluid_synth);
	if (!fluid_synth) {
		LOG_MSG("MIDI: Failed to create the FluidSynth synthesizer");
		return false;
	}

	// Load the requested SoundFont or quit if none provided
	const auto *multi_prop = section->Get_multival("soundfont");
	std::string soundfont = multi_prop->GetSection()->Get_string("soundfont_path");
	Cross::ResolveHomedir(soundfont);
	if (!soundfont.empty() && fluid_synth_sfcount(fluid_synth.get()) == 0) {
		fluid_synth_sfload(fluid_synth.get(), soundfont.data(), true);
	}
	if (fluid_synth_sfcount(fluid_synth.get()) == 0) {
		LOG_MSG("MIDI: FluidSynth failed to load %s, check the path.",
		        soundfont.c_str());
		return false;
	}

	// Adjust the SoundFont's sample amplitudes by an optional scaling percent
	CommandLine cmd(0, multi_prop->GetSection()->Get_string("soundfont_scale"));
	std::string scale_as_string;
	const bool scalar_provided = cmd.FindCommand(1, scale_as_string);
	int scale_by_percent = scalar_provided ? atoi(scale_as_string.c_str()) : 100;
	if (scale_by_percent < 1 || scale_by_percent > 500) {
		LOG_MSG("MIDI: FluidSynth invalid scaling of %d%% provided; resetting to 100%%",
		        scale_by_percent);
		scale_by_percent = 100;
	}
	fluid_synth_set_gain(fluid_synth.get(),
	                     static_cast<float>(scale_by_percent) / 100.0f);

	// Let the user know that the SoundFont was loaded
	if (scale_by_percent == 100)
		LOG_MSG("MIDI: FluidSynth loaded %s.", soundfont.c_str());
	else
		LOG_MSG("MIDI: FluidSynth loaded %s with levels %s by %d%%.",
		        soundfont.c_str(),
		        scale_by_percent > 100 ? "amplified" : "attenuated",
		        scale_by_percent);

	// Use a 7th-order (highest) polynomial to generate MIDI channel waveforms
	constexpr int all_channels = -1;
	fluid_synth_set_interp_method(fluid_synth.get(), all_channels,
	                              FLUID_INTERP_HIGHEST);

	// Apply reasonable chorus and reverb settings matching ScummVM's defaults
	constexpr int chorus_number = 3;
	constexpr double chorus_level = 1.2;
	constexpr double chorus_speed = 0.3;
	constexpr double chorus_depth = 8.0;
	fluid_synth_set_chorus_on(fluid_synth.get(), 1);
	fluid_synth_set_chorus(fluid_synth.get(), chorus_number, chorus_level,
	                       chorus_speed, chorus_depth, FLUID_CHORUS_MOD_SINE);

	constexpr double reverb_room_size = 0.61;
	constexpr double reverb_damping = 0.23;
	constexpr double reverb_width = 0.76;
	constexpr double reverb_level = 0.56;
	fluid_synth_set_reverb_on(fluid_synth.get(), 1);
	fluid_synth_set_reverb(fluid_synth.get(), reverb_room_size,
	                       reverb_damping, reverb_width, reverb_level);

	// Let the mixer command adjust our internal level
	const auto set_mixer_level = std::bind(&MidiHandlerFluidsynth::SetMixerLevel,
	                                       this, std::placeholders::_1);
	mixer_channel->RegisterLevelCallBack(set_mixer_level);
	mixer_channel->Enable(true);

	settings = std::move(fluid_settings);
	synth = std::move(fluid_synth);
	channel = std::move(mixer_channel);
	is_open = true;
	return true;
}

void MidiHandlerFluidsynth::Close()
{
	if (!is_open)
		return;

	channel->Enable(false);
	channel = nullptr;
	synth = nullptr;
	settings = nullptr;
	is_open = false;
}

void MidiHandlerFluidsynth::PlayMsg(const uint8_t *msg)
{
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
	const char *data = reinterpret_cast<const char *>(sysex);
	const auto n = static_cast<int>(len);
	fluid_synth_sysex(synth.get(), data, n, nullptr, nullptr, nullptr, false);
}

void MidiHandlerFluidsynth::PrintStats()
{
	// Normally prescale is simply a float-multiplier such as 0.5, 1.0, etc.
	// However in the case of FluidSynth, it produces 32-bit floats between
	// -1.0 and +1.0, therefore we scale those up to the 16-bit integer range
	// in addition to the mixer's FSYNTH levels. Before printing statistics,
	// we need to back-out this integer multiplier.
	prescale_level.left /= INT16_MAX;
	prescale_level.right /= INT16_MAX;
	soft_limiter.PrintStats();
}

void MidiHandlerFluidsynth::MixerCallBack(uint16_t frames)
{
	constexpr uint16_t max_samples = expected_max_frames * 2; // two channels per frame
	std::array<float, max_samples> stream;

	while (frames > 0) {
		constexpr uint16_t max_frames = expected_max_frames; // local copy fixes link error
		const uint16_t len = std::min(frames, max_frames);
		fluid_synth_write_float(synth.get(), len, stream.data(), 0, 2,
		                        stream.data(), 1, 2);
		const auto &out_stream = soft_limiter.Apply(stream, len);
		channel->AddSamples_s16(len, out_stream.data());
		frames -= len;
	}
}

static void fluid_destroy(MAYBE_UNUSED Section *sec)
{
	instance.PrintStats();
}

static void fluid_init(Section *sec)
{
	sec->AddDestroyFunction(&fluid_destroy, true);
}

void FLUID_AddConfigSection(Config *conf)
{
	assert(conf);
	Section_prop *sec = conf->AddSection_prop("fluidsynth", &fluid_init);
	assert(sec);
	init_fluid_dosbox_settings(*sec);
}

#endif // C_FLUIDSYNTH
