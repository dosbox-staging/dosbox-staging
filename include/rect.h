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

#include <algorithm>
#include <cassert>

namespace DosBox {

// Struct to represent rectangles and sizes.
//
// As a general practice, it should be encoded in the variable/argument name
// what we're dealing with (e.g. `viewport_rect`, `desktop_size`).
//
struct Rect {
	constexpr Rect() = default;

	constexpr Rect(const float x_pos, const float y_pos, const float width,
	               const float height)
	        : x(x_pos),
	          y(y_pos),
	          w(width),
	          h(height)
	{
		assert(w >= 0.0f);
		assert(h >= 0.0f);
	}

	constexpr Rect(const float width, const float height)
	        : Rect(0.0f, 0.0f, width, height)
	{}

	constexpr Rect(const int x_pos, const int y_pos, const int width,
	               const int height)
	        : Rect(static_cast<float>(x_pos), static_cast<float>(y_pos),
	               static_cast<float>(width), static_cast<float>(height))
	{}

	constexpr Rect(const int width, const int height)
	        : Rect(0, 0, width, height)
	{}

	constexpr bool operator==(const Rect& that) const
	{
		return (x == that.x && y == that.y && w == that.w && h == that.h);
	}

	constexpr bool operator!=(const Rect& that) const
	{
		return !operator==(that);
	}

	constexpr float x1() const
	{
		return x;
	}

	constexpr float y1() const
	{
		return y;
	}

	constexpr float x2() const
	{
		return x + w;
	}

	constexpr float y2() const
	{
		return y + h;
	}

	constexpr float cx() const
	{
		return x + w / 2;
	}

	constexpr float cy() const
	{
		return y + h / 2;
	}

	Rect Copy() const
	{
		return *this;
	}

	Rect& Scale(const float s)
	{
		x *= s;
		y *= s;
		w *= s;
		h *= s;
		return *this;
	}

	Rect& ScaleSize(const float s)
	{
		w *= s;
		h *= s;
		return *this;
	}

	Rect& ScaleWidth(const float s)
	{
		w *= s;
		return *this;
	}

	Rect& ScaleHeight(const float s)
	{
		h *= s;
		return *this;
	}

	Rect& Translate(const float dx, const float dy)
	{
		x += dx; 
		y += dy;
		return *this;
	}

	Rect& CenterTo(const float cx, const float cy)
	{
		x = cx - (w / 2);
		y = cy - (h / 2);
		return *this;
	}

	constexpr bool Contains(const Rect& r) const
	{
		return (r.x1() >= x1() && r.x2() <= x2()) &&
		       (r.y1() >= y1() && r.y2() <= y2());
	}

	constexpr bool Overlaps(const Rect& r) const
	{
		const auto ix1 = std::max(x1(), r.x1());
		const auto ix2 = std::min(x2(), r.x2());

		if (ix1 < ix2) {
			const auto iy1 = std::max(y1(), r.y1());
			const auto iy2 = std::min(y2(), r.y2());

			if (iy1 < iy2) {
				return true;
			}
		}

		return false;
	}

	Rect& Intersect(const Rect& r)
	{
		const auto ix1 = std::max(x1(), r.x1());
		const auto ix2 = std::min(x2(), r.x2());

		if (ix1 < ix2) {
			const auto iy1 = std::max(y1(), r.y1());
			const auto iy2 = std::min(y2(), r.y2());

			if (iy1 < iy2) {
				x = ix1;
				y = iy1;
				w = ix2 - ix1;
				h = iy2 - iy1;

				return *this;
			}
		}

		// No intersection
		w = 0.0f;
		h = 0.0f;

		return *this;
	}

	Rect& ScaleSizeToFit(const Rect& dest)
	{
		const auto s = std::min(dest.w / w, dest.h / h);
		return ScaleSize(s);
	}

	float x = 0;
	float y = 0;
	float w = 0;
	float h = 0;
};

} // namespace DosBox

#endif
