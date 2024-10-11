/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2020  The DOSBox Team
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

#ifndef DOSBOX_MIDI_COREMIDI_H
#define DOSBOX_MIDI_COREMIDI_H

#include "midi_device.h"

#if C_COREMIDI

#include <CoreMIDI/MIDIServices.h>
#include <sstream>
#include <string>

#include "programs.h"
#include "string_utils.h"

class MidiDeviceCoreMidi final : public MidiDevice {
public:
	std::string GetName() const override
	{
		return MidiDeviceName::CoreMidi;
	}

	Type GetType() const override
	{
		return MidiDevice::Type::External;
	}

	MidiDeviceCoreMidi(const char* conf)
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

					if (MIDIObjectGetStringProperty(
					            dummy,
					            kMIDIPropertyDisplayName,
					            &midiname) == noErr) {

						const char* s = CFStringGetCStringPtr(
						        midiname,
						        kCFStringEncodingMacRoman);

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

	~MidiDeviceCoreMidi() override
	{
		if (m_port && m_client) {
			Reset();
		}

		// Dispose the port
		MIDIPortDispose(m_port);

		// Dispose the client
		MIDIClientDispose(m_client);

		// Dispose the endpoint
		// Not, as it is for Endpoints created by us
		//		MIDIEndpointDispose(m_endpoint);
	}

	void SendMidiMessage(const MidiMessage& msg) override
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

	void SendSysExMessage(uint8_t* sysex, size_t len) override
	{
		// Acquire a MIDIPacketList
		Byte packetBuf[MaxMidiSysExSize * 4];
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

private:
	MIDIPortRef m_port         = {};
	MIDIClientRef m_client     = {};
	MIDIEndpointRef m_endpoint = {};
	MIDIPacket* m_pCurPacket   = {};
};

void COREMIDI_ListDevices(MidiDeviceCoreMidi* device, Program* caller);

#endif // C_COREMIDI

#endif
