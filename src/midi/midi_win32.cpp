// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

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
