// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CAPTURE_MIDI_H
#define DOSBOX_CAPTURE_MIDI_H

void capture_midi_add_data(const bool sysex, const size_t len, const uint8_t* data);

void capture_midi_finalise();

#endif
