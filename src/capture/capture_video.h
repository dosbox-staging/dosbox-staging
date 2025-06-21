// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CAPTURE_VIDEO_H
#define DOSBOX_CAPTURE_VIDEO_H

#include "render.h"

void capture_video_add_frame(const RenderedImage& image,
                             const float frames_per_second);

void capture_video_add_audio_data(const uint32_t sample_rate,
                                  const uint32_t num_sample_frames,
                                  const int16_t* sample_frames);

void capture_video_finalise();

#endif
