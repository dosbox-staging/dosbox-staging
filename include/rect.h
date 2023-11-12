/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#ifndef DOSBOX_RECTANGLE_H
#define DOSBOX_RECTANGLE_H

#include "math_utils.h"

namespace DosBox {

// Struct to represent rectangles.
struct Rect {
	constexpr Rect() = default;

	constexpr Rect(const float _w, const float _h)
	        : x(0.0f),
	          y(0.0f),
	          w(_w),
	          h(_h)
	{}

	constexpr Rect(const int _w, const int _h)
	        : x(static_cast<float>(0.0f)),
	          y(static_cast<float>(0.0f)),
	          w(static_cast<float>(_w)),
	          h(static_cast<float>(_h))
	{}
	constexpr Rect(const float _x, const float _y, const float _w, const float _h)
	        : x(_x),
	          y(_y),
	          w(_w),
	          h(_h)
	{}

	constexpr Rect(const int _x, const int _y, const int _w, const int _h)
	        : x(static_cast<float>(_x)),
	          y(static_cast<float>(_y)),
	          w(static_cast<float>(_w)),
	          h(static_cast<float>(_h))
	{}

	float x = 0;
	float y = 0;
	float w = 0;
	float h = 0;
};

} // namespace DosBox

#endif
