/*
 *  Copyright (C) 2023  The DOSBox Team
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

#ifndef DOSBOX_CAPTURE_H
#define DOSBOX_CAPTURE_H

#include "dosbox.h"

#include <string>

constexpr auto CAPTURE_WAVE  = 0x01;
constexpr auto CAPTURE_OPL   = 0x02;
constexpr auto CAPTURE_MIDI  = 0x04;
constexpr auto CAPTURE_IMAGE = 0x08;
constexpr auto CAPTURE_VIDEO = 0x10;

extern uint8_t CaptureState;

std::string capture_generate_filename(const char* type, const char* ext);

FILE* CAPTURE_CreateFile(const char* type, const char* ext);

void CAPTURE_AddWave(const uint32_t freq, const uint32_t len, const int16_t* data);

constexpr auto CAPTURE_FLAG_DBLW = 0x1;
constexpr auto CAPTURE_FLAG_DBLH = 0x2;

void CAPTURE_AddImage(const uint16_t width, const uint16_t height,
                      const uint8_t bits_per_pixel, const uint16_t pitch,
                      const uint8_t capture_flags, const float frames_per_second,
                      const uint8_t* image_data, const uint8_t* palette_data);

void CAPTURE_AddMidi(const bool sysex, const size_t len, const uint8_t* data);
void CAPTURE_VideoStart();
void CAPTURE_VideoStop();

#endif
