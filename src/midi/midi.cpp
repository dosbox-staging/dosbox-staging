/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "midi.h"

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <string>
#include <algorithm>

#include <SDL.h>

#include "ansi_code_markup.h"
#include "control.h"
#include "cross.h"
#include "hardware.h"
#include "mapper.h"
#include "midi_handler.h"
#include "pic.h"
#include "programs.h"
#include "setup.h"
#include "string_utils.h"
#include "timer.h"

#define RAWBUF	1024

uint8_t MIDI_evt_len[256] = {
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x10
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x30
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x40
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x70

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x80
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x90
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xa0
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xb0

  2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xc0
  2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xd0

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xe0

  0,2,3,2, 0,0,1,0, 1,0,1,1, 1,0,1,0   // 0xf0
};

MidiHandler * handler_list = 0;

MidiHandler::MidiHandler() : next(handler_list)
{
	handler_list = this;
}

MidiHandler Midi_none;

/* Include different midi drivers, lowest ones get checked first for default.
   Each header provides an independent midi interface. */

#include "midi_fluidsynth.h"
#include "midi_mt32.h"

#if defined(MACOSX)

#include "midi_coremidi.h"
#include "midi_coreaudio.h"

#elif defined(WIN32)

#include "midi_win32.h"

#else

#include "midi_oss.h"

MidiHandler_oss Midi_oss;

#endif

#include "midi_alsa.h"

#if C_ALSA
MidiHandler_alsa Midi_alsa;
#endif

struct Midi {
	uint8_t status     = 0;
	size_t cmd_len     = 0;
	size_t cmd_pos     = 0;
	uint8_t cmd_buf[8] = {};
	uint8_t rt_buf[8]  = {};
	struct {
		uint8_t buf[MIDI_SYSEX_SIZE] = {};
		size_t used                  = 0;
		int delay                    = 0; // ms
		int64_t start                = 0; // ms
	} sysex              = {};
	bool available       = false;
	MidiHandler *handler = nullptr;
};

static Midi midi            = {};
static Midi disengaged_midi = {};

/* When using a physical Roland MT-32 rev. 0 as MIDI output device,
 * some games may require a delay in order to prevent buffer overflow
 * issues.
 *
 * Explanation for this formula can be found in discussion under patch
 * that introduced it: https://sourceforge.net/p/dosbox/patches/241/
 */
int delay_in_ms(size_t sysex_bytes_num)
{
	constexpr double midi_baud_rate = 3.125; // bytes per ms
	const auto delay = (sysex_bytes_num * 1.25) / midi_baud_rate;
	return static_cast<int>(delay) + 2;
}

void MIDI_RawOutByte(uint8_t data)
{
	if (!midi.available)
		return;

	if (midi.sysex.start) {
		const auto passed_ticks = GetTicksSince(midi.sysex.start);
		if (passed_ticks < midi.sysex.delay)
			Delay(midi.sysex.delay - passed_ticks);
	}

	/* Test for a realtime MIDI message */
	if (data>=0xf8) {
		midi.rt_buf[0]=data;
		midi.handler->PlayMsg(midi.rt_buf);
		return;
	}
	/* Test for a active sysex tranfer */
	if (midi.status==0xf0) {
		if (!(data&0x80)) {
			if (midi.sysex.used < (MIDI_SYSEX_SIZE - 1))
				midi.sysex.buf[midi.sysex.used++] = data;
			return;
		} else {
			midi.sysex.buf[midi.sysex.used++] = 0xf7;

			if ((midi.sysex.start) && (midi.sysex.used >= 4) && (midi.sysex.used <= 9) && (midi.sysex.buf[1] == 0x41) && (midi.sysex.buf[3] == 0x16)) {
				LOG(LOG_ALL,LOG_ERROR)("MIDI:Skipping invalid MT-32 SysEx midi message (too short to contain a checksum)");
			} else {
//				LOG(LOG_ALL,LOG_NORMAL)("Play sysex; address:%02X %02X %02X, length:%4d, delay:%3d", midi.sysex.buf[5], midi.sysex.buf[6], midi.sysex.buf[7], midi.sysex.used, midi.sysex.delay);
				midi.handler->PlaySysex(midi.sysex.buf, midi.sysex.used);
				if (midi.sysex.start) {
					if (midi.sysex.buf[5] == 0x7F) {
						midi.sysex.delay = 290; // All Parameters reset
					} else if (midi.sysex.buf[5] == 0x10 && midi.sysex.buf[6] == 0x00 && midi.sysex.buf[7] == 0x04) {
						midi.sysex.delay = 145; // Viking Child
					} else if (midi.sysex.buf[5] == 0x10 && midi.sysex.buf[6] == 0x00 && midi.sysex.buf[7] == 0x01) {
						midi.sysex.delay = 30; // Dark Sun 1
					} else {
						midi.sysex.delay = delay_in_ms(midi.sysex.used);
					}
					midi.sysex.start = GetTicks();
				}
			}

			LOG(LOG_ALL,LOG_NORMAL)("Sysex message size %d", static_cast<int>(midi.sysex.used));
			if (CaptureState & CAPTURE_MIDI) {
				CAPTURE_AddMidi( true, midi.sysex.used-1, &midi.sysex.buf[1]);
			}
		}
	}
	if (data&0x80) {
		midi.status=data;
		midi.cmd_pos=0;
		midi.cmd_len=MIDI_evt_len[data];
		if (midi.status==0xf0) {
			midi.sysex.buf[0]=0xf0;
			midi.sysex.used=1;
		}
	}
	if (midi.cmd_len) {
		midi.cmd_buf[midi.cmd_pos++]=data;
		if (midi.cmd_pos >= midi.cmd_len) {
			if (CaptureState & CAPTURE_MIDI) {
				CAPTURE_AddMidi(false, midi.cmd_len, midi.cmd_buf);
			}
			midi.handler->PlayMsg(midi.cmd_buf);
			midi.cmd_pos=1;		//Use Running status
		}
	}
}

void MidiHandler::HaltSequence()
{
	uint8_t message[4] = {}; // see MIDI_evt_len for length lookup-table
	constexpr uint8_t all_notes_off = 0x7b;
	constexpr uint8_t all_controllers_off = 0x79;
	constexpr uint8_t controller = 0xb0;

	// from the first to last channel
	for (uint8_t channel = 0x0; channel <= 0xf; ++channel) {
		message[0] = controller | channel; // 0xb0 through 0xbf

		message[1] = all_notes_off;
		PlayMsg(message);

		message[1] = all_controllers_off;
		PlayMsg(message);
	}
}

void MidiHandler::ResumeSequence()
{
	constexpr uint8_t controller    = 0xb0;
	constexpr uint8_t first_channel = 0x00;
	constexpr uint8_t last_channel  = 0x0f;
	constexpr uint8_t omni_on_cmd   = 0x7d;

	// see MIDI_evt_len for length lookup-table
	uint8_t message[4] = {controller, omni_on_cmd};

	// from the first to last channel
	for (auto channel = first_channel; channel <= last_channel; ++channel) {
		message[0] = controller | channel; // 0xb0 through 0xbf
		PlayMsg(message);
	}
}

void MIDI_HaltSequence()
{
	if (midi.handler)
		midi.handler->HaltSequence();
}

void MIDI_ResumeSequence()
{
	if (midi.handler)
		midi.handler->ResumeSequence();
}

void MIDI_Disengage()
{
	// nothing to disengage, so do nothing
	if (!midi.handler)
		return;

	MIDI_HaltSequence();
	std::swap(midi, disengaged_midi);
	assert(midi.handler == nullptr);
}

void MIDI_Engage()
{
	// nothing to re-engage, so do nothing
	if (!disengaged_midi.handler)
		return;

	std::swap(disengaged_midi, midi);
	assert(midi.handler);
	MIDI_ResumeSequence();
}

bool MIDI_Available()
{
	return midi.available;
}

class MIDI final : public Module_base {
public:
	MIDI(Section *configuration) : Module_base(configuration)
	{
		using namespace std::string_literals;

		Section_prop * section=static_cast<Section_prop *>(configuration);
		std::string dev = section->Get_string("mididevice");
		lowcase(dev);

		std::string fullconf=section->Get_string("midiconfig");
		MidiHandler * handler;
		midi.sysex.delay = 0;
		midi.sysex.start = 0;
		if (fullconf.find("delaysysex") != std::string::npos) {
			midi.sysex.start = GetTicks();
			fullconf.erase(fullconf.find("delaysysex"));
			LOG_MSG("MIDI: Using delayed SysEx processing");
		}
		trim(fullconf);
		const char * conf = fullconf.c_str();
		midi.status=0x00;
		midi.cmd_pos=0;
		midi.cmd_len=0;
		// Value "default" exists for backwards-compatibility.
		// TODO: Rewrite this logic without using goto
		if (dev == "auto" || dev == "default")
			goto getdefault;
		handler=handler_list;
		while (handler) {
			if (dev == handler->GetName()) {
				if (!handler->Open(conf)) {
					LOG_WARNING("MIDI: Can't open device: %s with config: '%s'",
					        dev.c_str(), conf);
					goto getdefault;
				}
				midi.handler=handler;
				midi.available=true;
				LOG_MSG("MIDI: Opened device: %s",
				        handler->GetName());
				return;
			}
			handler=handler->next;
		}
		LOG_MSG("MIDI: Can't find device: %s, using default handler.",
		        dev.c_str());
getdefault:
		for (handler = handler_list; handler; handler = handler->next) {
			const std::string name = handler->GetName();
			if (name == "fluidsynth") {
				// Never select fluidsynth automatically.
				// Users needs to opt-in, otherwise
				// fluidsynth will slow down emulator
				// startup for all games.
				continue;
			}
			if (name == "mt32") {
				// Never select mt32 automatically.
				// Users needs to opt-in.
				continue;
			}
			if (handler->Open(conf)) {
				midi.available=true;
				midi.handler=handler;
				LOG_MSG("MIDI: Opened device: %s", name.c_str());
				return;
			}
		}
		assert((handler != nullptr) && (handler->GetName() == "none"s));
	}

	~MIDI()
	{
		if (!midi.available) {
			assert(!midi.handler);
			return;
		}

		assert(midi.handler);
		midi.handler->Close();
		midi.handler = {};
		midi.available = false;
	}
};

void MIDI_ListAll(Program *caller)
{
	constexpr auto msg_indent = "  ";

	for (auto *handler = handler_list; handler; handler = handler->next) {
		const std::string name = handler->GetName();
		if (name == "none")
			continue;

		std::string name_format = msg_indent;
		name_format.append(convert_ansi_markup("[color=white]%s:[reset]\n"));
		caller->WriteOut(name_format.c_str(), name.c_str());

		const auto err = handler->ListAll(caller);
		if (err == MIDI_RC::ERR_DEVICE_NOT_CONFIGURED)
			caller->WriteOut("%s%s\n",
			                 msg_indent,
			                 MSG_Get("MIDI_DEVICE_NOT_CONFIGURED"));
		if (err == MIDI_RC::ERR_DEVICE_LIST_NOT_SUPPORTED)
			caller->WriteOut("%s%s\n",
			                 msg_indent,
			                 MSG_Get("MIDI_DEVICE_LIST_NOT_SUPPORTED"));

		caller->WriteOut("\n"); // additional newline to separate devices
	}
}

static void register_midi_text_messages()
{
	MSG_Add("MIDI_DEVICE_LIST_NOT_SUPPORTED", "listing not supported");
	MSG_Add("MIDI_DEVICE_NOT_CONFIGURED", "device not configured");
}

static MIDI *test;
void MIDI_Destroy(Section * /*sec*/)
{
	delete test;
}

void MIDI_Init(Section* sec)
{
	assert(sec);

	test = new MIDI(sec);

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&MIDI_Destroy, changeable_at_runtime);

	register_midi_text_messages();
}

void init_midi_dosbox_settings(Section_prop& secprop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto* str_prop = secprop.Add_string("mididevice", when_idle, "auto");
	const char* midi_devices[] =
	{ "auto",
#if defined(MACOSX)
#	if C_COREMIDI
	  "coremidi",
#	endif
#	if C_COREAUDIO
	  "coreaudio",
#	endif
#elif defined(WIN32)
	  "win32",
#else
	  "oss",
#endif
#if C_ALSA
	  "alsa",
#endif
#if C_FLUIDSYNTH
	  "fluidsynth",
#endif
#if C_MT32EMU
	  "mt32",
#endif
	  "none",
	  0 };

	str_prop->Set_values(midi_devices);
	str_prop->Set_help(
	        "Set where MIDI data from the emulated MPU-401 MIDI interface is sent\n"
	        "('auto' by default):\n"
#if defined(MACOSX)
#	if C_COREMIDI
	        "  coremidi:    Any device that has been configured in the macOS\n"
	        "               Audio MIDI Setup.\n"
#	endif
#	if C_COREAUDIO
	        "  coreaudio:   Use the built-in macOS MIDI synthesiser.\n"
#	endif
#elif defined(WIN32)
	        "  win32:       Use the Win32 MIDI playback interface.\n"
#else
	        "  oss:         Use the Linux OSS MIDI playback interface.\n"
#endif
#if C_ALSA
	        "  alsa:        Use the Linux ALSA MIDI playback interface.\n"
#endif
#if C_FLUIDSYNTH
	        "  fluidsynth:  The built-in FluidSynth MIDI synthesizer (SoundFont player).\n"
	        "               See the [fluidsynth] section for detailed configuration.\n"
#endif
#if C_MT32EMU
	        "  mt32:        The built-in Roland MT-32 synthesizer.\n"
	        "               See the [mt32] section for detailed configuration.\n"
#endif
	        "  auto:        Either one of the built-in MIDI synthesisers (if `midiconfig` is\n"
	        "               set to 'fluidsynth' or 'mt32'), or a MIDI device external to\n"
	        "               DOSBox (any other 'midiconfig' value). This might be a software\n"
	        "               synthesizer or physical device. This is the default behaviour.\n"
	        "  none:        Disable MIDI output.");

	str_prop = secprop.Add_string("midiconfig", when_idle, "");
	str_prop->Set_help(
	        "Configuration options for the selected MIDI interface (unset by default).\n"
	        "This is usually the ID or name of the MIDI synthesizer you want\n"
	        "to use (find the ID/name with the DOS command 'MIXER /LISTMIDI').\n"
#if (C_FLUIDSYNTH == 1 || C_MT32EMU == 1)
	        "Notes: - This option has no effect when using the built-in synthesizers\n"
	        "         ('mididevice = fluidsynth' or 'mididevice = mt32').\n"
#endif
#if C_COREAUDIO
	        "       - When using 'coreaudio', you can specify a SoundFont here.\n"
#endif
#if C_ALSA
	        "       - When using ALSA, use the Linux command 'aconnect -l' to list all open\n"
	        "         MIDI ports and select one (e.g. 'midiconfig = 14:0' for sequencer\n"
	        "         client 14, port 0).\n"
#endif
	        "       - If you're using a physical Roland MT-32 with revision 0 PCB, the\n"
	        "         hardware may require a delay in order to prevent its buffer from\n"
	        "         overflowing. In that case, add 'delaysysex', e.g:\n"
	        "         'midiconfig = 2 delaysysex'.");

	str_prop = secprop.Add_string("mpu401", when_idle, "intelligent");
	const char* mputypes[] = {"intelligent", "uart", "none", 0};
	str_prop->Set_values(mputypes);
	str_prop->Set_help("MPU-401 mode to emulate ('intelligent' by default).");
}

void MPU401_Init(Section*);

void MIDI_AddConfigSection(const config_ptr_t &conf)
{
	assert(conf);
	Section_prop *sec = conf->AddSection_prop("midi", &MIDI_Init, true);
	assert(sec);
	sec->AddInitFunction(&MPU401_Init, true);

	init_midi_dosbox_settings(*sec);
}

