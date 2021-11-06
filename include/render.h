/*
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

#ifndef DOSBOX_RENDER_H
#define DOSBOX_RENDER_H

// 0: complex scalers off, scaler cache off, some simple scalers off, memory requirements reduced
// 1: complex scalers off, scaler cache off, all simple scalers on
// 2: complex scalers off, scaler cache on
// 3: complex scalers on
#define RENDER_USE_ADVANCED_SCALERS 3

#include "../src/gui/render_scalers.h"

#define RENDER_SKIP_CACHE	16
//Enable this for scalers to support 0 input for empty lines
//#define RENDER_NULL_INPUT

struct RenderPal_t {
	struct {
		uint8_t red = 0;
		uint8_t green = 0;
		uint8_t blue = 0;
		uint8_t unused = 0;
	} rgb[256] = {};
	union {
		uint16_t b16[256];
		uint32_t b32[256] = {};
	} lut = {};
	bool changed = false;
	uint8_t modified[256] = {};
	uint32_t first = 0;
	uint32_t last = 0;
};

struct Render_t {
	struct {
		uint32_t width = 0;
		uint32_t start = 0;
		uint32_t height = 0;
		unsigned bpp = 0;
		bool dblw = false;
		bool dblh = false;
		double ratio = 0;
		double fps = 0;
	} src = {};
	struct {
		int count = 0;
		int max = 0;
		uint32_t index = 0;
		uint8_t hadSkip[RENDER_SKIP_CACHE] = {};
	} frameskip = {};
	struct {
		uint32_t size = 0;
		scalerMode_t inMode = {};
		scalerMode_t outMode = {};
		scalerOperation_t op = {};
		bool clearCache = false;
		bool forced = false;
		ScalerLineHandler_t lineHandler = nullptr;
		ScalerLineHandler_t linePalHandler = nullptr;
		ScalerComplexHandler_t complexHandler = nullptr;
		uint32_t blocks = 0;
		uint32_t lastBlock = 0;
		int outPitch = 0;
		uint8_t *outWrite = nullptr;
		uint32_t cachePitch = 0;
		uint8_t *cacheRead = nullptr;
		uint32_t inHeight = 0;
		uint32_t inLine = 0;
		uint32_t outLine = 0;
	} scale = {};
#if C_OPENGL
	char *shader_src = nullptr;
#endif
	RenderPal_t pal = {};
	bool updating = false;
	bool active = false;
	bool aspect = true;
	bool fullFrame = true;
};

extern Render_t render;
extern ScalerLineHandler_t RENDER_DrawLine;
void RENDER_SetSize(uint32_t width,
                    uint32_t height,
                    unsigned bpp,
                    double fps,
                    double ratio,
                    bool dblw,
                    bool dblh);
bool RENDER_StartUpdate(void);
void RENDER_EndUpdate(bool abort);
void RENDER_SetPal(Bit8u entry, Bit8u red, Bit8u green, Bit8u blue);

#endif
