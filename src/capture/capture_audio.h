// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CAPTURE_AUDIO_H
#define DOSBOX_CAPTURE_AUDIO_H

void capture_audio_add_data(uint32_t sample_rate_hz, uint32_t num_sample_frames,
                            const int16_t* sample_frames);

void capture_audio_finalise();

#endif
