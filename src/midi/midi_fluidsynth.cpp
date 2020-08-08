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

#include <clocale>
#include <string>

#include "control.h"

// Fluidsynth uses libinstpatch which as of v1.1.3 has the nasty behavior of
// changing the locale under our nose. We remember and restore the current
// locale whenever this could happen.
//
// https://github.com/realnc/dosbox-core/issues/7
// https://github.com/swami/libinstpatch/issues/37
//
class LocaleGuard final {
public:
	~LocaleGuard() { setlocale(LC_ALL, locale_.c_str()); }

private:
	std::string locale_ = setlocale(LC_ALL, nullptr);
};

MidiHandlerFluidsynth MidiHandlerFluidsynth::instance;

void init_fluid_dosbox_settings(Section_prop &secprop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto *str_prop = secprop.Add_string("fluid.soundfont", when_idle, "");
	str_prop->Set_help("Soundfont to use with FluidSynth."
	                   "One must be specified.");

	auto *int_prop = secprop.Add_int("fluid.samplerate", when_idle, 44100);
	int_prop->SetMinMax(8000, 96000);
	int_prop->Set_help("Sample rate (Hz) Fluidsynth will render at. (min "
	                   "8000, max 96000");

	str_prop = secprop.Add_string("fluid.gain", when_idle, "0.4");
	str_prop->Set_help("Fluidsynth volume gain. (min 0.0, max 10.0");

	int_prop = secprop.Add_int("fluid.polyphony", when_idle, 256);
	int_prop->Set_help("Fluidsynth polyphony.");

	int_prop = secprop.Add_int("fluid.cores", when_idle, 1);
	int_prop->SetMinMax(1, 256);
	int_prop->Set_help("Number of CPU cores Fluidsynth will use."
	                   "(min 1, max 256)");

	auto *bool_prop = secprop.Add_bool("fluid.reverb", when_idle, true);
	bool_prop->Set_help("Enable reverb.");

	bool_prop = secprop.Add_bool("fluid.chorus", when_idle, true);
	bool_prop->Set_help("Fluidsynth chorus.");

	str_prop = secprop.Add_string("fluid.reverb.roomsize", when_idle, "0.2");
	str_prop->Set_help("Fluidsynth reverb room size.");

	str_prop = secprop.Add_string("fluid.reverb.damping", when_idle, "0.0");
	str_prop->Set_help("Fluidsynth reverb damping.");

	str_prop = secprop.Add_string("fluid.reverb.width", when_idle, "0.5");
	str_prop->Set_help("Fluidsynth reverb width.");

	str_prop = secprop.Add_string("fluid.reverb.level", when_idle, "0.9");
	str_prop->Set_help("Fluidsynth reverb level.");

	int_prop = secprop.Add_int("fluid.chorus.number", when_idle, 3);
	int_prop->Set_help("Fluidsynth chorus voices");

	str_prop = secprop.Add_string("fluid.chorus.level", when_idle, "2.0");
	str_prop->Set_help("Fluidsynth chorus level.");

	str_prop = secprop.Add_string("fluid.chorus.speed", when_idle, "0.3");
	str_prop->Set_help("Fluidsynth chorus speed.");

	str_prop = secprop.Add_string("fluid.chorus.depth", when_idle, "8.0");
	str_prop->Set_help("Fluidsynth chorus depth.");
}

bool MidiHandlerFluidsynth::Open(MAYBE_UNUSED const char *conf)
{
	LocaleGuard locale_guard;

	Close();

	auto *section = static_cast<Section_prop *>(control->GetSection("midi"));

	fluid_settings_ptr_t settings(new_fluid_settings(), delete_fluid_settings);

	auto get_double = [section](const char *const propname) {
		try {
			return std::stod(section->Get_string(propname));
		} catch (const std::exception &e) {
			/*
			log_cb(RETRO_LOG_WARN,
			       "[dosbox] error reading floating point '%s' "
			       "conf setting: %s\n",
			       e.what());
		       */
			return 0.0;
		}
	};

	fluid_settings_setnum(settings.get(), "synth.sample-rate",
	                      section->Get_int("fluid.samplerate"));
	fluid_settings_setnum(settings.get(), "synth.gain", get_double("fluid.gain"));
	fluid_settings_setint(settings.get(), "synth.polyphony",
	                      section->Get_int("fluid.polyphony"));
	fluid_settings_setint(settings.get(), "synth.cpu-cores",
	                      section->Get_int("fluid.cores"));

	fluid_settings_setint(settings.get(), "synth.reverb.active",
	                      section->Get_bool("fluid.reverb"));
	fluid_settings_setnum(settings.get(), "synth.reverb.room-size",
	                      get_double("fluid.reverb.roomsize"));
	fluid_settings_setnum(settings.get(), "synth.reverb.damp",
	                      get_double("fluid.reverb.damping"));
	fluid_settings_setnum(settings.get(), "synth.reverb.width",
	                      get_double("fluid.reverb.width"));
	fluid_settings_setnum(settings.get(), "synth.reverb.level",
	                      get_double("fluid.reverb.level"));

	fluid_settings_setint(settings.get(), "synth.chorus.active",
	                      section->Get_bool("fluid.chorus"));
	fluid_settings_setint(settings.get(), "synth.chorus.nr",
	                      section->Get_int("fluid.chorus.number"));
	fluid_settings_setnum(settings.get(), "", get_double("fluid.chorus.level"));
	fluid_settings_setnum(settings.get(), "synth.chorus.speed",
	                      get_double("fluid.chorus.speed"));
	fluid_settings_setnum(settings.get(), "synth.chorus.depth",
	                      get_double("fluid.chorus.depth"));

	fsynth_ptr_t fluid_synth(new_fluid_synth(settings.get()), delete_fluid_synth);
	if (!fluid_synth) {
		// log_cb(RETRO_LOG_WARN, "[dosbox] Error creating fluidsynth synthesiser\n");
		return false;
	}

	std::string soundfont = section->Get_string("fluid.soundfont");
	if (!soundfont.empty() && fluid_synth_sfcount(fluid_synth.get()) == 0) {
		fluid_synth_sfload(fluid_synth.get(), soundfont.data(), true);
	}

	mixer_channel_ptr_t mixer_channel(
	        MIXER_AddChannel(mixerCallback,
	                         section->Get_int("fluid.samplerate"), "FSYNTH"),
	        MIXER_DelChannel);
	mixer_channel->Enable(true);

	fluid_settings = std::move(settings);
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
	fluid_settings = nullptr;
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
		// log_cb(RETRO_LOG_WARN, "[dosbox] fluidsynth: unknown MIDI command: %08lx", tmp);
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

// TODO: originally, buffer MixTemp was being used here to receive int16_t data
// from fluid and pass it ot the mixer... but this buffer is uint8_t, thus
// introduces alignment problem; replaced with new local buffer for now.
int16_t data[MIXER_BUFSIZE];

void MidiHandlerFluidsynth::mixerCallback(const Bitu len)
{
	fluid_synth_write_s16(instance.synth.get(), len, data, 0, 2, data, 1, 2);
	instance.channel->AddSamples_s16(len, data);
}

#endif // C_FLUIDSYNTH
