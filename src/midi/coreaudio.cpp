// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/coreaudio.h"

#include "dos/programs.h"

#if C_COREAUDIO

void COREAUDIO_ListDevices([[maybe_unused]] MidiDeviceCoreAudio* device,
                           [[maybe_unused]] Program* caller)
{
	caller->WriteOut("  ");
	caller->WriteOut(MSG_Get("MIDI_DEVICE_LIST_NOT_SUPPORTED"));
	caller->WriteOut("\n\n");
}

#endif // C_COREAUDIO
