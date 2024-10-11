/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
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

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <list>
#include <memory>
#include <string>

#include <SDL.h>

#include "../capture/capture.h"
#include "ansi_code_markup.h"
#include "control.h"
#include "cross.h"
#include "mapper.h"
#include "midi_device.h"
#include "mpu401.h"
#include "pic.h"
#include "programs.h"
#include "setup.h"
#include "string_utils.h"
#include "timer.h"

// #define DEBUG_MIDI

// clang-format off
uint8_t MIDI_message_len_by_status[256] = {
  // Data bytes (dummy zero values)
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x10
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x30
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x40
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x70

  // Status bytes
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x80 -- Note Off
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x90 -- Note On
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xa0 -- Poly Key Pressure
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xb0 -- Control Change

  2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xc0 -- Program Change
  2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xd0 -- Channel Pressure

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xe0 -- Pitch Bend

  0,2,3,2, 0,0,1,0, 1,0,1,1, 1,0,1,0   // 0xf0 -- System Exclusive
};
// clang-format on

#include "midi_fluidsynth.h"
#include "midi_mt32.h"

#if defined(MACOSX)
#include "midi_coreaudio.h"
#include "midi_coremidi.h"

#elif defined(WIN32)
#include "midi_win32.h"
#else
#include "midi_oss.h"
#endif
#if C_ALSA
#include "midi_alsa.h"
#endif

static std::unique_ptr<MidiDevice> create_device(const std::string& name,
                                                 const std::string& config)
{
	using namespace MidiDeviceName;

	// Internal MIDI synths
#if C_FLUIDSYNTH
	if (name == FluidSynth) {
		return std::make_unique<MidiDeviceFluidSynth>();
	}
#endif
#if C_MT32EMU
	if (name == Mt32) {
		return std::make_unique<MidiDeviceMt32>();
	}
#endif

	// External MIDI devices
#if C_COREMIDI
	if (name == CoreMidi) {
		return std::make_unique<MidiDeviceCoreMidi>(config.c_str());
	}
#endif
#if C_COREAUDIO
	if (name == CoreAudio) {
		return std::make_unique<MidiDeviceCoreAudio>(config.c_str());
	}
#endif
#if defined(WIN32)
	if (name == Win32) {
		return std::make_unique<MidiDeviceWin32>(config.c_str());
	}
#endif
#if C_ALSA
	if (name == Alsa) {
		return std::make_unique<MidiDeviceAlsa>(config.c_str());
	}
#endif
#if defined(LINUX)
	if (name == Oss) {
		return std::make_unique<MidiDeviceOss>(config.c_str());
	}
#endif

	// Device not found
	return {};
}

static std::vector<const char*> auto_device_candidates = {MidiDeviceName::Alsa,
                                                          MidiDeviceName::Oss,
                                                          MidiDeviceName::CoreAudio,
                                                          MidiDeviceName::CoreMidi,
                                                          MidiDeviceName::Win32};

static std::unique_ptr<MidiDevice> try_create_auto_device(const std::string& config)
{
	for (const auto device_name : auto_device_candidates) {
		try {
			if (auto device = create_device(device_name, config); device) {
				return device;
			}
			// nullptr means the device is not supported on
			// the current platform; try the next one

		} catch (const std::runtime_error& ex) {
			// error opening device; try the next one
			continue;
		}
	}
	return {};
}

struct Midi {
	uint8_t status = 0;

	struct {
		MidiMessage msg = {};

		size_t len = 0;
		size_t pos = 0;
	} message = {};

	MidiMessage realtime_message = {};

	struct {
		uint8_t buf[MaxMidiSysExSize] = {};

		size_t pos       = 0;
		int64_t delay_ms = 0;
		int64_t start_ms = 0;
	} sysex = {};

	bool is_muted = false;

	std::unique_ptr<MidiDevice> device = nullptr;
};

static Midi midi                    = {};
static bool raw_midi_output_enabled = {};

constexpr auto MaxChannelVolume = 127;

// Keep track of the state of the MIDI device (e.g. channel volumes and which
// notes are currently active on each channel).
class MidiState {
public:
	MidiState()
	{
		Reset();
	}

	void Reset()
	{
		note_on_tracker.fill(false);
		channel_volume_tracker.fill(MaxChannelVolume);
	}

	void UpdateState(const MidiMessage& msg)
	{
		const auto status  = get_midi_status(msg.status());
		const auto channel = get_midi_channel(msg.status());

		if (status == MidiStatus::NoteOn) {
			const auto note = msg.data1();
			SetNoteActive(channel, note, true);

		} else if (status == MidiStatus::NoteOff) {
			const auto note = msg.data1();
			SetNoteActive(channel, note, false);

		} else if (status == MidiStatus::ControlChange) {
			if (msg.data1() == MidiController::Volume) {
				const auto volume = msg.data2();
				SetChannelVolume(channel, volume);
			}
		}
	}

	inline void SetNoteActive(const uint8_t channel, const uint8_t note,
	                          const bool is_playing)
	{
		note_on_tracker[NoteAddr(channel, note)] = is_playing;
	}

	inline bool IsNoteActive(const uint8_t channel, const uint8_t note)
	{
		return note_on_tracker[NoteAddr(channel, note)];
	}

	inline void SetChannelVolume(const uint8_t channel, const uint8_t volume)
	{
		assert(channel <= NumMidiChannels);
		assert(volume <= MaxChannelVolume);

		channel_volume_tracker[channel] = volume;
	}

	inline uint8_t GetChannelVolume(const uint8_t channel)
	{
		assert(channel <= NumMidiChannels);

		return channel_volume_tracker[channel];
	}

	~MidiState() = default;

	// prevent copying
	MidiState(const MidiState&) = delete;
	// prevent assignment
	MidiState& operator=(const MidiState&) = delete;

private:
	std::array<bool, NumMidiNotes* NumMidiChannels> note_on_tracker = {};
	std::array<uint8_t, NumMidiChannels> channel_volume_tracker     = {};

	inline size_t NoteAddr(const uint8_t channel, const uint8_t note)
	{
		assert(channel <= LastMidiChannel);
		assert(note <= LastMidiNote);
		return channel * NumMidiNotes + note;
	}
};

static MidiState midi_state = {};

void init_midi_state(Section*)
{
	midi_state.Reset();
}

/* When using a physical Roland MT-32 rev. 0 as MIDI output device,
 * some games may require a delay in order to prevent buffer overflow
 * issues.
 *
 * Explanation for this formula can be found in discussion under patch
 * that introduced it: https://sourceforge.net/p/dosbox/patches/241/
 */
static int delay_in_ms(size_t sysex_bytes_num)
{
	constexpr double MidiBaudRate = 3.125; // bytes per ms
	const auto delay_ms           = (sysex_bytes_num * 1.25) / MidiBaudRate;
	return static_cast<int>(delay_ms) + 2;
}

bool is_midi_data_byte(const uint8_t byte)
{
	return byte <= 0x7f;
}

bool is_midi_status_byte(const uint8_t byte)
{
	return !is_midi_data_byte(byte);
}

uint8_t get_midi_status(const uint8_t status_byte)
{
	return status_byte & 0xf0;
}

MessageType get_midi_message_type(const uint8_t status_byte)
{
	return get_midi_status(status_byte) == MidiStatus::SystemMessage
	             ? MessageType::SysEx
	             : MessageType::Channel;
}

uint8_t get_midi_channel(const uint8_t channel_status)
{
	return channel_status & 0x0f;
}

static bool is_external_midi_device()
{
	return midi.device->GetType() == MidiDevice::Type::External;
}

static void output_note_off_for_active_notes(const uint8_t channel)
{
	assert(channel <= LastMidiChannel);

	constexpr auto NoteOffVelocity = 64;
	constexpr auto NoteOffMsgLen   = 3;

	MidiMessage msg = {};
	msg[0]          = MidiStatus::NoteOff | channel;
	msg[2]          = NoteOffVelocity;

	for (auto note = FirstMidiNote; note <= LastMidiNote; ++note) {
		if (midi_state.IsNoteActive(channel, note)) {
			msg[1] = note;

			if (CAPTURE_IsCapturingMidi()) {
				constexpr auto IsSysEx = false;
				CAPTURE_AddMidiData(IsSysEx,
				                    NoteOffMsgLen,
				                    msg.data.data());
			}
			midi.device->SendMidiMessage(msg);
		}
	}
}

// Many MIDI drivers used in games send the "All Notes Off" Channel Mode
// Message to turn off all active notes when switching between songs, instead
// of properly sending Note Off messages for each individual note as required
// by the MIDI specification (all Note On messages *must* be always paired
// with Note Offs; the "All Notes Off" message must not be used as a shortcut
// for that). E.g. all Sierra drivers exhibit this incorrect behaviour, while
// LucasArts games are doing the correct thing and pair all Note On messages
// with Note Offs.
//
// This hack can lead to "infinite notes" (hanging notes) when recording the
// MIDI output into a MIDI sequencer, or when using DOSBox's raw MIDI output
// capture functionality. What's worse, it can also result in multiple Note On
// messages for the same note on the same channel in the recorded MIDI stream,
// followed by a single Note Off only. While playing back the raw MIDI stream
// is interpreted "correctly" on MIDI modules typically used in the 1990s,
// it's up to the individual MIDI sequencer how to resolve this situation when
// dealing with recorded MIDI data. This can lead lead to missing notes, and
// it makes editing long MIDI recordings containing multiple songs very
// difficult and error-prone.
//
// See page 20, 24, 25 and A-4 of the "The Complete MIDI 1.0 Detailed
// Specification" document version 96.1, third edition (1996, MIDI
// Manufacturers Association) for further details
//
// https://archive.org/details/Complete_MIDI_1.0_Detailed_Specification_96-1-3/
//
static void sanitise_midi_stream(const MidiMessage& msg)
{
	const auto status  = get_midi_status(msg.status());
	const auto channel = get_midi_channel(msg.status());

	if (status == MidiStatus::ControlChange) {
		const auto mode = msg.data1();
		if (mode == MidiChannelMode::AllSoundOff ||
		    mode >= MidiChannelMode::AllNotesOff) {
			// Send Note Offs for the currently active notes prior
			// to sending the "All Notes Off" message, as mandated
			// by the MIDI spec
			output_note_off_for_active_notes(channel);

			for (auto note = FirstMidiNote; note <= LastMidiNote; ++note) {
				midi_state.SetNoteActive(channel, note, false);
			}
		}
	}
}

void MIDI_RawOutByte(const uint8_t data)
{
	if (!MIDI_IsAvailable()) {
		return;
	}

	if (midi.sysex.start_ms) {
		const auto passed_ticks = GetTicksSince(midi.sysex.start_ms);
		if (passed_ticks < midi.sysex.delay_ms) {
			Delay(midi.sysex.delay_ms - passed_ticks);
		}
	}

	const auto is_realtime_message = (data >= MidiStatus::TimingClock);
	if (is_realtime_message) {
		midi.realtime_message[0] = data;
		midi.device->SendMidiMessage(midi.realtime_message);
		return;
	}

	if (midi.status == MidiStatus::SystemExclusive) {
		if (is_midi_data_byte(data)) {
			if (midi.sysex.pos < (MaxMidiSysExSize - 1)) {
				midi.sysex.buf[midi.sysex.pos++] = data;
			}
			return;
		} else {
			midi.sysex.buf[midi.sysex.pos++] = MidiStatus::EndOfExclusive;

			if (midi.sysex.start_ms && (midi.sysex.pos >= 4) &&
			    (midi.sysex.pos <= 9) && (midi.sysex.buf[1] == 0x41) &&
			    (midi.sysex.buf[3] == 0x16)) {
#ifdef DEBUG_MIDI
				LOG_DEBUG(
				        "MIDI: Skipping invalid MT-32 SysEx midi message "
				        "(too short to contain a checksum)");
#endif
			} else {
#ifdef DEBUG_MIDI
				LOG_TRACE(
				        "MIDI: Playing SysEx message, "
				        "address: %02X %02X %02X, length: %4d, delay: %3d",
				        midi.sysex.buf[5],
				        midi.sysex.buf[6],
				        midi.sysex.buf[7],
				        midi.sysex.pos,
				        midi.sysex.delay_ms);
#endif
				midi.device->SendSysExMessage(midi.sysex.buf,
				                              midi.sysex.pos);

				if (midi.sysex.start_ms) {
					if (midi.sysex.buf[5] == 0x7f) {
						// Reset All Parameters fix
						midi.sysex.delay_ms = 290;

					} else if (midi.sysex.buf[5] == 0x10 &&
					           midi.sysex.buf[6] == 0x00 &&
					           midi.sysex.buf[7] == 0x04) {
						// Viking Child fix
						midi.sysex.delay_ms = 145;

					} else if (midi.sysex.buf[5] == 0x10 &&
					           midi.sysex.buf[6] == 0x00 &&
					           midi.sysex.buf[7] == 0x01) {
						// Dark Sun 1 fix
						midi.sysex.delay_ms = 30;

					} else {
						midi.sysex.delay_ms = delay_in_ms(
						        midi.sysex.pos);
					}
					midi.sysex.start_ms = GetTicks();
				}
			}

			if (CAPTURE_IsCapturingMidi()) {
				constexpr auto IsSysEx = true;
				CAPTURE_AddMidiData(IsSysEx,
				                    midi.sysex.pos - 1,
				                    &midi.sysex.buf[1]);
			}
		}
	}

	if (is_midi_status_byte(data)) {
		// Start of a new MIDI message
		midi.status      = data;
		midi.message.pos = 0;

		// Total length of the MIDI message, including the status byte
		midi.message.len = MIDI_message_len_by_status[midi.status];

		if (midi.status == MidiStatus::SystemExclusive) {
			midi.sysex.buf[0] = MidiStatus::SystemExclusive;
			midi.sysex.pos    = 1;
		}
	}

	if (midi.message.len > 0) {
		midi.message.msg[midi.message.pos++] = data;

		if (midi.message.pos >= midi.message.len) {
			// 1. Update the MIDI state based on the last non-SysEx
			// message.
			midi_state.UpdateState(midi.message.msg);

			// 2. Sanitise the MIDI stream unless raw output is
			// enabled. Currently, this can result in the emission
			// of extra MIDI Note Off events only, and updating the
			// MIDI state.
			//
			// `sanitise_midi_stream` also captures these extra
			// events if MIDI capture is enabled and sends them to
			// the MIDI device. This is a bit hacky and rather
			// limited design, but it does the job for now... A
			// better solution would be a message queue or stream
			// that we could also alter and filter, plus a
			// centralised capture and send function.
			if (!raw_midi_output_enabled) {
				sanitise_midi_stream(midi.message.msg);
			}

			// 3. Determine whether the message should be sent to
			// the device based on the mute state.
			auto play_msg = true;

			if (midi.is_muted && is_external_midi_device()) {
				const auto& msg = midi.message.msg;
				const auto status = get_midi_status(msg.status());

				// Track Channel Volume change messages in
				// MidiState, but don't send them to external
				// devices when muted.
				if (status == MidiStatus::ControlChange &&
				    msg.data1() == MidiController::Volume) {
					play_msg = false;
				}
			}

			// 4. Always capture the original message if MIDI
			// capture is enabled, regardless of the mute state.
			if (CAPTURE_IsCapturingMidi()) {
				constexpr auto IsSysEx = false;
				CAPTURE_AddMidiData(IsSysEx,
				                    midi.message.len,
				                    midi.message.msg.data.data());
			}

			// 5. Send the MIDI message to the device for playback
			if (play_msg) {
				midi.device->SendMidiMessage(midi.message.msg);
			}

			midi.message.pos = 1; // Use Running Status
		}
	}
}

void MidiDevice::Reset()
{
	MidiMessage msg = {};

	for (auto channel = FirstMidiChannel; channel <= LastMidiChannel; ++channel) {
		msg[0] = MidiStatus::ControlChange | channel;

		msg[1] = MidiChannelMode::AllNotesOff;
		SendMidiMessage(msg);

		msg[1] = MidiChannelMode::ResetAllControllers;
		SendMidiMessage(msg);
	}
}

void MIDI_Reset()
{
	if (MIDI_IsAvailable()) {
		midi.device->Reset();
	}
}

void MIDI_Mute()
{
	if (!MIDI_IsAvailable() || midi.is_muted) {
		return;
	}

	if (is_external_midi_device()) {
		MidiMessage msg = {0, MidiController::Volume, 0};

		for (auto channel = FirstMidiChannel; channel <= LastMidiChannel;
		     ++channel) {

			msg[0] = MidiStatus::ControlChange | channel;
			midi.device->SendMidiMessage(msg);
		}
	}

	midi.is_muted = true;
}

void MIDI_Unmute()
{
	if (!MIDI_IsAvailable() || !midi.is_muted) {
		return;
	}

	if (is_external_midi_device()) {
		MidiMessage msg = {0, MidiController::Volume, 0};

		for (auto channel = FirstMidiChannel; channel <= LastMidiChannel;
		     ++channel) {

			msg[0] = MidiStatus::ControlChange | channel;
			msg[2] = midi_state.GetChannelVolume(channel);

			midi.device->SendMidiMessage(msg);
		}
	}

	midi.is_muted = false;
}

bool MIDI_IsAvailable()
{
	return (midi.device != nullptr);
}

static Section_prop* get_midi_section()
{
	assert(control);

	auto sec = static_cast<Section_prop*>(control->GetSection("midi"));
	assert(sec);

	return sec;
}

static std::string get_mididevice_setting()
{
	return get_midi_section()->Get_string("mididevice");
}

// We'll adapt the RtMidi library, eventually, so hold off any substantial
// rewrites on the MIDI stuff until then to unnecessary work.
class MIDI final {
public:
	MIDI()
	{
		const auto device_pref = get_mididevice_setting();

		midi = {};

		// Has the user disable MIDI?
		if (const auto device_has_bool = parse_bool_setting(device_pref);
		    device_has_bool && *device_has_bool == false) {
			LOG_MSG("MIDI: MIDI device set to 'none'; disabling MIDI output");
			return;
		}

		const auto section      = get_midi_section();
		raw_midi_output_enabled = section->Get_bool("raw_midi_output");

		std::string midiconfig_prefs = section->Get_string("midiconfig");

		if (midiconfig_prefs.find("delaysysex") != std::string::npos) {
			midi.sysex.start_ms = GetTicks();
			midiconfig_prefs.erase(midiconfig_prefs.find("delaysysex"));
			LOG_MSG("MIDI: Using delayed SysEx processing");
		}

		trim(midiconfig_prefs);
		const auto midiconfig_pref = midiconfig_prefs.c_str();

		if (device_pref == "auto") {
			// Use the first working device
			midi.device = try_create_auto_device(midiconfig_pref);

		} else {
			midi.device = create_device(device_pref, midiconfig_pref);
		}

		if (midi.device) {
			LOG_MSG("MIDI: Opened device '%s'",
			        midi.device->GetName().c_str());
		}
	}
};

void MIDI_ListDevices(Program* caller)
{
	auto write_device_name = [&](const std::string& device_name) {
		const auto color = convert_ansi_markup("[color=white]%s:[reset]\n");

		caller->WriteOut(color.c_str(), device_name.c_str());
	};

	auto device_ptr = midi.device.get();

	const std::string device_name = midi.device ? midi.device->GetName() : "";
#if C_FLUIDSYNTH
	write_device_name(MidiDeviceName::FluidSynth);

	FSYNTH_ListDevices((device_name == MidiDeviceName::FluidSynth)
	                           ? dynamic_cast<MidiDeviceFluidSynth*>(device_ptr)
	                           : nullptr,

	                   caller);
#endif
#if C_MT32EMU
	write_device_name(MidiDeviceName::Mt32);

	MT32_ListDevices((device_name == MidiDeviceName::Mt32)
	                         ? dynamic_cast<MidiDeviceMt32*>(device_ptr)
	                         : nullptr,
	                 caller);
#endif
#if C_COREMIDI
	write_device_name(MidiDeviceName::CoreMidi);

	COREMIDI_ListDevices((device_name == MidiDeviceName::CoreMidi)
	                             ? dynamic_cast<MidiDeviceCoreMidi*>(device_ptr)
	                             : nullptr,
	                     caller);
#endif
#if C_COREAUDIO
	write_device_name(MidiDeviceName::CoreAudio);

	COREAUDIO_ListDevices((device_name == MidiDeviceName::CoreAudio)
	                              ? dynamic_cast<MidiDeviceCoreAudio*>(device_ptr)
	                              : nullptr,
	                      caller);
#endif
#if defined(WIN32)
	write_device_name(MidiDeviceName::Win32);

	MIDI_WIN32_ListDevices((device_name == MidiDeviceName::Win32)
	                               ? dynamic_cast<MidiDeviceWin32*>(device_ptr)
	                               : nullptr,
	                       caller);
#endif
#if C_ALSA
	write_device_name(MidiDeviceName::Alsa);

	ALSA_ListDevices((device_name == MidiDeviceName::Alsa)
	                         ? dynamic_cast<MidiDeviceAlsa*>(device_ptr)
	                         : nullptr,
	                 caller);
#endif
#if defined(LINUX)
	write_device_name(MidiDeviceName::Oss);

	MIDI_OSS_ListDevices((device_name == MidiDeviceName::Oss)
	                             ? dynamic_cast<MidiDeviceOss*>(device_ptr)
	                             : nullptr,
	                     caller);
#endif
}

static std::unique_ptr<MIDI> midi_instance = nullptr;

static void midi_init([[maybe_unused]] Section* sec)
{
	// Retry loop
	for (;;) {
		MPU401_Destroy();
		MPU401_Init();

		midi_instance.reset();

		try {
			midi_instance = std::make_unique<MIDI>();
			midi_state.Reset();

			// A MIDI device has been successfully initialised
			return;

		} catch (const std::runtime_error& ex) {
			const auto mididevice_pref = get_mididevice_setting();
			if (mididevice_pref == "auto") {
				LOG_WARNING(
						"MIDI: Error opening device '%s'; "
						"MIDI auto-discovery failed, "
						"using 'mididevice = none' and disabling MIDI output",
						mididevice_pref.c_str());

				set_section_property_value("midi", "mididevice", "none");

				// 'mididevice = auto' didn't work out; we disable the MIDI
				// output and bail out.
				return;

			} else {
				// If 'mididevice' was set to a concrete value and the
				// device could not be initialiased, we'll try 'auto' as a
				// fallback.
				LOG_WARNING("MIDI: Error opening device '%s'; using 'auto'",
							mididevice_pref.c_str());

				set_section_property_value("midi", "mididevice", "auto");
			}
		}
	}
}

void MIDI_Init()
{
	midi_init(get_midi_section());
}

void init_midi_dosbox_settings(Section_prop& secprop)
{
	constexpr auto WhenIdle = Property::Changeable::WhenIdle;

	auto* str_prop = secprop.Add_string("mididevice", WhenIdle, "auto");
	str_prop->Set_help(
	        "Set where MIDI data from the emulated MPU-401 MIDI interface is sent\n"
	        "('auto' by default):");

	str_prop->SetOptionHelp(MidiDeviceName::CoreMidi,
	                        "  coremidi:    Any device that has been configured in the macOS\n"
	                        "               Audio MIDI Setup.");

	str_prop->SetOptionHelp(MidiDeviceName::CoreAudio,
	                        "  coreaudio:   Use the built-in macOS MIDI synthesiser.");

	str_prop->SetOptionHelp(MidiDeviceName::Win32,
	                        "  win32:       Use the Win32 MIDI playback interface.");

	str_prop->SetOptionHelp(MidiDeviceName::Oss,
	                        "  oss:         Use the Linux OSS MIDI playback interface.");

	str_prop->SetOptionHelp(MidiDeviceName::Alsa,
	                        "  alsa:        Use the Linux ALSA MIDI playback interface.");

	str_prop->SetOptionHelp(
	        MidiDeviceName::FluidSynth,
	        "  fluidsynth:  The built-in FluidSynth MIDI synthesizer (SoundFont player).\n"
	        "               See the [fluidsynth] section for detailed configuration.");

	str_prop->SetOptionHelp(
	        MidiDeviceName::Mt32,
	        "  mt32:        The built-in Roland MT-32 synthesizer.\n"
	        "               See the [mt32] section for detailed configuration.");
	str_prop->SetOptionHelp(
	        "auto",
	        "  auto:        Either one of the built-in MIDI synthesisers (if `midiconfig` is\n"
	        "               set to 'fluidsynth' or 'mt32'), or a MIDI device external to\n"
	        "               DOSBox (any other 'midiconfig' value). This might be a software\n"
	        "               synthesizer or physical device. This is the default behaviour.");
	str_prop->SetOptionHelp("none", "  none:        Disable MIDI output.");
	str_prop->Set_values({
		"auto",
#if defined(MACOSX)
#if C_COREMIDI
		        MidiDeviceName::CoreMidi,
#endif
#if C_COREAUDIO
		        MidiDeviceName::CoreAudio,
#endif
#elif defined(WIN32)
		        MidiDeviceName::Win32,
#else
		        MidiDeviceName::Oss,
#endif
#if C_ALSA
		        MidiDeviceName::Alsa,
#endif
#if C_FLUIDSYNTH
		        MidiDeviceName::FluidSynth,
#endif
		        MidiDeviceName::Mt32, "none"
	});

	str_prop = secprop.Add_string("midiconfig", WhenIdle, "");
	str_prop->Set_help(
	        "Configuration options for the selected MIDI interface (unset by default).\n"
	        "This is usually the ID or name of the MIDI synthesizer you want\n"
	        "to use (find the ID/name with the DOS command 'MIXER /LISTMIDI').\n"
	        "Notes:");

	str_prop->SetOptionHelp("fluidsynth_or_mt32emu",
	                        "  - This option has no effect when using the built-in synthesizers\n"
	                        "    ('mididevice = fluidsynth' or 'mididevice = mt32').");

	str_prop->SetOptionHelp("coreaudio",
	                        "  - When using 'coreaudio', you can specify a SoundFont here.");

	str_prop->SetOptionHelp("alsa",
	                        "  - When using ALSA, use the Linux command 'aconnect -l' to list all open\n"
	                        "    MIDI ports and select one (e.g. 'midiconfig = 14:0' for sequencer\n"
	                        "    client 14, port 0).");

	str_prop->SetOptionHelp(
	        "mt32",
	        "  - If you're using a physical Roland MT-32 with revision 0 PCB, the hardware\n"
	        "    may require a delay in order to prevent its buffer from overflowing.\n"
	        "    In that case, add 'delaysysex' (e.g. 'midiconfig = 2 delaysysex').");

	str_prop->SetEnabledOptions({
#if (C_FLUIDSYNTH == 1 || C_MT32EMU == 1)
		"fluidsynth_or_mt32emu",
#endif
#if C_COREAUDIO
		        "coreaudio",
#endif
#if C_ALSA
		        "alsa",
#endif
		        "mt32",
	});

	str_prop = secprop.Add_string("mpu401", WhenIdle, "intelligent");
	str_prop->Set_values({"intelligent", "uart", "none"});
	str_prop->Set_help("MPU-401 mode to emulate ('intelligent' by default).");

	auto* bool_prop = secprop.Add_bool("raw_midi_output", WhenIdle, false);
	bool_prop->Set_help(
	        "Enable raw, unaltered MIDI output (disabled by default).\n"
	        "The MIDI drivers of many games don't fully conform to the MIDI standard,\n"
	        "which makes editing the MIDI recordings of these games very error-prone and\n"
	        "cumbersome in MIDI sequencers, often resulting in hanging or missing notes.\n"
	        "DOSBox corrects the MIDI output of such games by default. This results in no\n"
	        "audible difference whatsoever; it only affects the representation of the MIDI\n"
	        "data. You should only enable 'raw_midi_output' if you really need to capture\n"
	        "the raw, unaltered MIDI output of a program, e.g. when working with music\n"
	        "applications, or when debugging MIDI issues.");
}

static void register_midi_text_messages()
{
	MSG_Add("MIDI_DEVICE_LIST_NOT_SUPPORTED", "Listing not supported");
	MSG_Add("MIDI_DEVICE_NOT_CONFIGURED", "Device not configured");
	MSG_Add("MIDI_DEVICE_NO_SUPPORTED_MODELS", "No supported models present");
	MSG_Add("MIDI_DEVICE_NO_MODEL_ACTIVE", "No model is currently active");
}

void MIDI_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	constexpr auto ChangeableAtRuntime = true;

	Section_prop* sec = conf->AddSection_prop("midi", &midi_init, ChangeableAtRuntime);
	assert(sec);

	init_midi_dosbox_settings(*sec);

	register_midi_text_messages();
}
