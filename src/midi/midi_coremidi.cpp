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

#include "midi_coremidi.h"

#if C_COREMIDI

void COREMIDI_ListDevices([[maybe_unused]] MidiDeviceCoreMidi* device, Program* caller)
{
	Bitu numDests = MIDIGetNumberOfDestinations();

	for (Bitu i = 0; i < numDests; i++) {
		MIDIEndpointRef dest = MIDIGetDestination(i);
		if (!dest) {
			continue;
		}

		CFStringRef midiname = nullptr;

		if (MIDIObjectGetStringProperty(dest,
		                                kMIDIPropertyDisplayName,
		                                &midiname) == noErr) {

			const char* s = CFStringGetCStringPtr(midiname,
			                                      kCFStringEncodingMacRoman);
			if (s) {
				caller->WriteOut("  %02d - %s\n", i, s);
			}
		}
		// This is for EndPoints created by us.
		// MIDIEndpointDispose(dest);
	}

	caller->WriteOut("\n");
}

#endif // C_COREMIDI
