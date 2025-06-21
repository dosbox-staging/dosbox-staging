// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "midi_coremidi.h"

#if C_COREMIDI

#include <sstream>

#include "programs.h"
#include "string_utils.h"

MidiDeviceCoreMidi::MidiDeviceCoreMidi(const char* conf)
        : MidiDevice(),
          m_port(0),
          m_client(0),
          m_endpoint(0),
          m_pCurPacket(nullptr)
{
	// Get the MIDIEndPoint
	m_endpoint = 0;

	Bitu numDests = MIDIGetNumberOfDestinations();
	Bitu destId   = numDests;

	if (conf && *conf) {
		std::string strconf(conf);
		std::istringstream configmidi(strconf);

		configmidi >> destId;

		if (configmidi.fail() && numDests) {
			lowcase(strconf);

			for (Bitu i = 0; i < numDests; i++) {
				MIDIEndpointRef dummy = MIDIGetDestination(i);
				if (!dummy) {
					continue;
				}

				CFStringRef midiname = nullptr;

				if (MIDIObjectGetStringProperty(dummy,
				                                kMIDIPropertyDisplayName,
				                                &midiname) == noErr) {

					const char* s = CFStringGetCStringPtr(
					        midiname, kCFStringEncodingMacRoman);

					if (s) {
						std::string devname(s);
						lowcase(devname);

						if (devname.find(strconf) !=
						    std::string::npos) {
							destId = i;
							break;
						}
					}
				}
			}
		}
	}

	if (destId >= numDests) {
		destId = 0;
	}
	if (destId < numDests) {
		m_endpoint = MIDIGetDestination(destId);
	}

	// Create a MIDI client and port
	MIDIClientCreate(CFSTR("MyClient"), nullptr, nullptr, &m_client);

	if (!m_client) {
		const auto msg = "MIDI:COREMIDI: No client created";
		LOG_WARNING("%s", msg);
		throw std::runtime_error(msg);
	}

	MIDIOutputPortCreate(m_client, CFSTR("MyOutPort"), &m_port);

	if (!m_port) {
		const auto msg = "MIDI:COREMIDI: No port created";
		LOG_WARNING("%s", msg);
		throw std::runtime_error(msg);
	}
}

MidiDeviceCoreMidi::~MidiDeviceCoreMidi()
{
	if (m_port && m_client) {
		MIDI_Reset(this);
	}

	// Dispose the port
	MIDIPortDispose(m_port);

	// Dispose the client
	MIDIClientDispose(m_client);

	// Dispose the endpoint
	// Not, as it is for Endpoints created by us
	//		MIDIEndpointDispose(m_endpoint);
}

void MidiDeviceCoreMidi::SendMidiMessage(const MidiMessage& msg)
{
	// Acquire a MIDIPacketList
	Byte packetBuf[128];
	MIDIPacketList* packetList = (MIDIPacketList*)packetBuf;

	m_pCurPacket = MIDIPacketListInit(packetList);

	const auto len = MIDI_message_len_by_status[msg.status()];

	// Add msg to the MIDIPacketList
	MIDIPacketListAdd(packetList,
	                  (ByteCount)sizeof(packetBuf),
	                  m_pCurPacket,
	                  (MIDITimeStamp)0,
	                  len,
	                  msg.data.data());

	// Send the MIDIPacketList
	MIDISend(m_port, m_endpoint, packetList);
}

void MidiDeviceCoreMidi::SendSysExMessage(uint8_t* sysex, size_t len)
{
	// Acquire a MIDIPacketList
	Byte packetBuf[MaxMidiSysExBytes * 4];
	MIDIPacketList* packetList = (MIDIPacketList*)packetBuf;

	m_pCurPacket = MIDIPacketListInit(packetList);

	// Add msg to the MIDIPacketList
	MIDIPacketListAdd(packetList,
	                  (ByteCount)sizeof(packetBuf),
	                  m_pCurPacket,
	                  (MIDITimeStamp)0,
	                  len,
	                  sysex);

	// Send the MIDIPacketList
	MIDISend(m_port, m_endpoint, packetList);
}

void COREMIDI_ListDevices([[maybe_unused]] MidiDeviceCoreMidi* device, Program* caller)
{
	constexpr auto Indent = "  ";

	auto num_dests   = MIDIGetNumberOfDestinations();
	auto num_devices = 0;

	for (size_t i = 0; i < num_dests; ++i) {
		MIDIEndpointRef dest = MIDIGetDestination(i);
		if (!dest) {
			continue;
		}

		CFStringRef midi_name = nullptr;

		if (MIDIObjectGetStringProperty(dest,
		                                kMIDIPropertyDisplayName,
		                                &midi_name) == noErr) {

			const char* s = CFStringGetCStringPtr(midi_name,
			                                      kCFStringEncodingMacRoman);
			if (s) {
				caller->WriteOut("%s%02d - %s\n", Indent, i, s);
				++num_devices;
			}
		}
		// This is for EndPoints created by us.
		// MIDIEndpointDispose(dest);
	}

	if (num_devices == 0) {
		caller->WriteOut("%s%s\n", Indent, MSG_Get("MIDI_DEVICE_NO_PORTS"));
	}

	caller->WriteOut("\n");
}

#endif // C_COREMIDI
