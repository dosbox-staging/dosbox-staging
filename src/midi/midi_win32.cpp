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

#include "midi_win32.h"

#ifdef WIN32

void MIDI_WIN32_ListDevices([[maybe_unused]] MidiDeviceWin32* device, Program* caller)
{
	unsigned int total = midiOutGetNumDevs();

	for (unsigned int i = 0; i < total; i++) {
		MIDIOUTCAPS mididev;
		midiOutGetDevCaps(i, &mididev, sizeof(MIDIOUTCAPS));

		caller->WriteOut("  %2d - \"%s\"\n", i, mididev.szPname);
	}

	caller->WriteOut("\n");
}

#endif // WIN32
