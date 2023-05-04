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

#include <stdio.h>
#include <string>

class Section;

#define CAPTURE_WAVE  0x01
#define CAPTURE_OPL   0x02
#define CAPTURE_MIDI  0x04
#define CAPTURE_IMAGE 0x08
#define CAPTURE_VIDEO 0x10

extern Bitu CaptureState;

std::string CAPTURE_GetScreenshotFilename(const char* type, const char* ext);
FILE* CAPTURE_OpenFile(const char* type, const char* ext);

void CAPTURE_AddWave(uint32_t freq, uint32_t len, int16_t* data);

#define CAPTURE_FLAG_DBLW 0x1
#define CAPTURE_FLAG_DBLH 0x2

void CAPTURE_AddImage(const uint16_t width, const uint16_t height,
                      const uint8_t bpp, const uint16_t pitch, uint8_t flags,
                      float fps, uint8_t* data, uint8_t* pal);

void CAPTURE_AddMidi(bool sysex, Bitu len, uint8_t* data);
void CAPTURE_VideoStart();
void CAPTURE_VideoStop();

#endif
