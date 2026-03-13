// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/win32.h"

#ifdef WIN32

void MIDI_WIN32_ListDevices([[maybe_unused]] MidiDeviceWin32* device,
                            MoreOutputStrings& output)
{
	unsigned int total = midiOutGetNumDevs();

	for (unsigned int i = 0; i < total; i++) {
		MIDIOUTCAPS mididev;
		midiOutGetDevCaps(i, &mididev, sizeof(MIDIOUTCAPS));

		output.AddString("  %2d - \"%s\"\n", i, mididev.szPname);
	}

	output.AddString("\n");
}

#endif // WIN32
