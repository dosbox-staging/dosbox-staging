// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "midi_alsa.h"

#if C_ALSA

#include <cassert>
#include <cstring>
#include <functional>
#include <regex>
#include <sstream>
#include <string>

#include "logging.h"
#include "programs.h"
#include "string_utils.h"

#define ADDR_DELIM ".:"

using port_action_t = std::function<void(snd_seq_client_info_t* client_info,
                                         snd_seq_port_info_t* port_info)>;

static void for_each_alsa_seq_port(port_action_t action)
{
	// We can't reuse the sequencer from midi handler, as the function might
	// be called before that sequencer is created.
	snd_seq_t* seq = nullptr;

	if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0) != 0) {
		LOG_WARNING("MIDI:ALSA: Can't open MIDI sequencer");
		return;
	}
	assert(seq);

	snd_seq_client_info_t* client_info = nullptr;
	snd_seq_client_info_malloc(&client_info);
	assert(client_info);

	snd_seq_port_info_t* port_info = nullptr;
	snd_seq_port_info_malloc(&port_info);
	assert(port_info);

	snd_seq_client_info_set_client(client_info, -1);

	while (snd_seq_query_next_client(seq, client_info) >= 0) {
		const int client_id = snd_seq_client_info_get_client(client_info);

		snd_seq_port_info_set_client(port_info, client_id);
		snd_seq_port_info_set_port(port_info, -1);

		while (snd_seq_query_next_port(seq, port_info) >= 0) {
			action(client_info, port_info);
		}
	}

	snd_seq_port_info_free(port_info);
	snd_seq_client_info_free(client_info);
	snd_seq_close(seq);
}

static bool port_is_writable(const unsigned int port_caps)
{
	constexpr unsigned int mask = SND_SEQ_PORT_CAP_WRITE |
	                              SND_SEQ_PORT_CAP_SUBS_WRITE;

	return (port_caps & mask) == mask;
}

void MidiDeviceAlsa::send_event(int do_flush)
{
	snd_seq_ev_set_direct(&ev);
	snd_seq_ev_set_source(&ev, output_port);
	snd_seq_ev_set_dest(&ev, seq.client, seq.port);

	snd_seq_event_output(seq_handle, &ev);
	if (do_flush) {
		snd_seq_drain_output(seq_handle);
	}
}

static bool parse_addr(const std::string& in, int* client, int* port)
{
	if (in.empty()) {
		return false;
	}

	if (in[0] == 's' || in[0] == 'S') {
		*client = SND_SEQ_ADDRESS_SUBSCRIBERS;
		*port   = 0;
		return true;
	}

	if (in.find_first_of(ADDR_DELIM) == std::string::npos) {
		return false;
	}

	std::istringstream inp(in);
	int val1;
	int val2;
	char c;

	if (!(inp >> val1)) {
		return false;
	}
	if (!(inp >> c)) {
		return false;
	}
	if (!(inp >> val2)) {
		return false;
	}

	*client = val1;
	*port   = val2;

	return true;
}

void MidiDeviceAlsa::SendSysExMessage(uint8_t* sysex, size_t len)
{
	snd_seq_ev_set_sysex(&ev, len, sysex);
	send_event(1);
}

void MidiDeviceAlsa::SendMidiMessage(const MidiMessage& msg)
{
	const auto status_byte = msg[0];

	ev.type            = SND_SEQ_EVENT_OSS;
	ev.data.raw32.d[0] = status_byte;
	ev.data.raw32.d[1] = msg[1];
	ev.data.raw32.d[2] = msg[2];

	const auto status  = get_midi_status(status_byte);
	const auto channel = get_midi_channel(status_byte);

	switch (status) {
	case MidiStatus::NoteOff:
		snd_seq_ev_set_noteoff(&ev, channel, msg[1], msg[2]);
		send_event(1);
		break;

	case MidiStatus::NoteOn:
		snd_seq_ev_set_noteon(&ev, channel, msg[1], msg[2]);
		send_event(1);
		break;

	case MidiStatus::PolyKeyPressure:
		snd_seq_ev_set_keypress(&ev, channel, msg[1], msg[2]);
		send_event(1);
		break;

	case MidiStatus::ControlChange:
		snd_seq_ev_set_controller(&ev, channel, msg[1], msg[2]);
		send_event(1);
		break;

	case MidiStatus::ProgramChange:
		snd_seq_ev_set_pgmchange(&ev, channel, msg[1]);
		send_event(0);
		break;

	case MidiStatus::ChannelPressure:
		snd_seq_ev_set_chanpress(&ev, channel, msg[1]);
		send_event(0);
		break;

	case MidiStatus::PitchBend: {
		long theBend = ((long)msg[1] + (long)(msg[2] << 7)) - 0x2000;
		snd_seq_ev_set_pitchbend(&ev, channel, theBend);
		send_event(1);
		break;
	}
	default:
		// Maybe filter out FC as it leads for at least one user to
		// crash, but the entire midi stream has not yet been checked.
		LOG_WARNING("MIDI:ALSA: Unknown MIDI message sequence (hex): %02X %02X %02X",
		            status_byte,
		            msg[1],
		            msg[2]);

		send_event(1);
		break;
	}
}

MidiDeviceAlsa::~MidiDeviceAlsa()
{
	if (seq_handle) {
		MIDI_Reset(this);
		snd_seq_close(seq_handle);
	}
	seq = {};
}

static bool port_name_matches(const std::string& pattern,
                              snd_seq_client_info_t* client_info,
                              snd_seq_port_info_t* port_info)
{
	if (pattern.empty()) {
		return true;
	}

	char port_name[80];
	safe_sprintf(port_name,
	             "%s - %s",
	             snd_seq_client_info_get_name(client_info),
	             snd_seq_port_info_get_name(port_info));

	return (strcasestr(port_name, pattern.c_str()) != nullptr);
}

static AlsaAddress find_seq_input_port(const std::string& pattern)
{
	AlsaAddress seq_addr = {};

	// Modern sequencers like FluidSynth indicate that they
	// are capable of generating sound.
	//
	auto find_synth_port = [&pattern, &seq_addr](auto* client_info,
	                                             auto* port_info) {
		const auto* addr     = snd_seq_port_info_get_addr(port_info);
		const auto port_type = snd_seq_port_info_get_type(port_info);

		const bool match = port_name_matches(pattern, client_info, port_info);

		if (match && (port_type & SND_SEQ_PORT_TYPE_SYNTHESIZER)) {
			seq_addr.client = addr->client;
			seq_addr.port   = addr->port;
		}
	};

	for_each_alsa_seq_port(find_synth_port);

	if (seq_addr.client != -1) {
		return seq_addr;
	}

	// Older sequencers like TiMidity++ only indicate that
	// subscribers can write to them, but so does MIDI-Through port
	// (which is a kernel client sequencer, not a user client one).
	//
	// When a sequencer does not set port type properly, we can't be
	// sure which one is intended for input.  Therefore we consider
	// only the first port for such sequencers.
	//
	// Prevents the problem with TiMidity++, which creates 4 ports
	// but only first two ones generate sound (even though all 4
	// ones are marked as writable).
	//
	auto find_input_port = [&pattern, &seq_addr](auto* client_info,
	                                             auto* port_info) {
		const auto* addr = snd_seq_port_info_get_addr(port_info);
		const auto caps  = snd_seq_port_info_get_capability(port_info);

		const bool is_new_client = (addr->client != seq_addr.client);

		bool is_candidate = false;
		if (pattern.empty()) {
			is_candidate = (snd_seq_client_info_get_type(client_info) ==
			                SND_SEQ_USER_CLIENT);
		} else {
			is_candidate = port_name_matches(pattern, client_info, port_info);
		}

		if (is_new_client && is_candidate && port_is_writable(caps)) {
			seq_addr.client = addr->client;
			seq_addr.port   = addr->port;
		}
	};

	for_each_alsa_seq_port(find_input_port);
	return seq_addr;
}

MidiDeviceAlsa::MidiDeviceAlsa(const char* conf)
{
	assert(conf);
	seq = {};

	LOG_DEBUG("MIDI:ALSA: Attempting connection to: '%s'", conf);

	// Try to use port specified in config; if port is not configured,
	// then attempt to connect to the newest capable port.
	//
	const std::string conf_str = conf;
	const bool use_specific_addr = parse_addr(conf_str, &seq.client, &seq.port);

	if (!use_specific_addr) {
		const auto found_addr = find_seq_input_port(conf_str);
		if (found_addr.client > 0) {
			seq = found_addr;
		}
	}

	if (seq.client == -1) {
		const auto msg = "MIDI:ALSA: No available MIDI devices found";
		LOG_WARNING("%s", msg);
		throw std::runtime_error(msg);
	}

	if (snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_OUTPUT, 0) != 0) {
		const auto msg = "MIDI:ALSA: Can't open sequencer";
		LOG_WARNING("%s", msg);
		throw std::runtime_error(msg);
	}

	snd_seq_set_client_name(seq_handle, "DOSBox Staging");

	unsigned int caps = SND_SEQ_PORT_CAP_READ;
	if (seq.client == SND_SEQ_ADDRESS_SUBSCRIBERS) {
		caps = ~SND_SEQ_PORT_CAP_SUBS_READ;
	}

	output_port = snd_seq_create_simple_port(seq_handle,
	                                         "Virtual MPU-401 output",
	                                         caps,
	                                         SND_SEQ_PORT_TYPE_MIDI_GENERIC |
	                                                 SND_SEQ_PORT_TYPE_APPLICATION);

	if (output_port < 0) {
		snd_seq_close(seq_handle);

		const auto msg = "MIDI:ALSA: Can't create ALSA port";
		LOG_WARNING("%s", msg);
		throw std::runtime_error(msg);
	}

	if (seq.client != SND_SEQ_ADDRESS_SUBSCRIBERS) {
		if (snd_seq_connect_to(seq_handle, output_port, seq.client, seq.port) ==
		    0) {

			snd_seq_client_info_t* info = nullptr;
			snd_seq_client_info_malloc(&info);
			assert(info);

			snd_seq_get_any_client_info(seq_handle, seq.client, info);

			LOG_MSG("MIDI:ALSA: Connected to MIDI port %d:%d - %s",
			        seq.client,
			        seq.port,
			        snd_seq_client_info_get_name(info));

			snd_seq_client_info_free(info);
			return;
		}
	}

	snd_seq_close(seq_handle);

	const auto msg = format_str("MIDI:ALSA: Can't connect to MIDI port %d:%d",
	                            seq.client,
	                            seq.port);
	LOG_WARNING("%s", msg.c_str());
	throw std::runtime_error(msg);
}

AlsaAddress MidiDeviceAlsa::GetInputPortAddress()
{
	return seq;
}

void ALSA_ListDevices(MidiDeviceAlsa* device, Program* caller)
{
	const auto input_port = [&]() -> AlsaAddress {
		if (device) {
			return device->GetInputPortAddress();
		}
		return {};
	}();

	auto print_port = [&](auto* client_info, auto* port_info) {
		const auto* addr = snd_seq_port_info_get_addr(port_info);

		const unsigned int type = snd_seq_port_info_get_type(port_info);
		const unsigned int caps = snd_seq_port_info_get_capability(port_info);

		if ((type & SND_SEQ_PORT_TYPE_SYNTHESIZER) || port_is_writable(caps)) {
			const bool selected = (addr->client == input_port.client &&
			                       addr->port == input_port.port);

			const char esc_color[]   = "\033[32;1m";
			const char esc_nocolor[] = "\033[0m";

			caller->WriteOut("%c %s%3d:%d - %s - %s%s\n",
			                 (selected ? '*' : ' '),
			                 (selected ? esc_color : ""),
			                 addr->client,
			                 addr->port,
			                 snd_seq_client_info_get_name(client_info),
			                 snd_seq_port_info_get_name(port_info),
			                 (selected ? esc_nocolor : ""));
		}
	};

	for_each_alsa_seq_port(print_port);

	caller->WriteOut("\n");
}

#endif // C_ALSA
