// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RECTANGLE_H
#define DOSBOX_RECTANGLE_H

#include <algorithm>
#include <cassert>
#include <string>

#include "utils/string_utils.h"

namespace DosBox {

// General purpose rectangle class.
//
// A rectangle of zero width and height is allowed and is considered empty
// (see `IsEmpty()`).
//
// Negative width and height values are allowed as such "non-existant"
// rectangles can be useful as intermediate results in certain algorithms (see
// `HasPositiveSize()`, `HasNegativeSize()`, `IsExistant()`, and
// `Normalise()`).
//
// Many of the transform methods assume existant or non-empty rectangles. They
// raise assertions when these assumptions are not met in debug builds, and
// return a fixed default value or perform a no-op in release builds. See the
// individual method descriptions for further details.
//
// The rectangle class can also be used to represent sizes or dimensions only
// (e.g., the size of a window); in such use-cases the starting point is
// usually set to zero. In general, it should be encoded in the variable's
// name what we're dealing with (e.g., `viewport_rect`, `window_size`).
//
// Operations mutate the current instance; use `Copy()` if you wish to create
// a new instance. This is handy in chained const instantiations that involve
// transforms:
//
// ```
// const DosBox::Rect a = {4, 5};
// const DosBox::Rect b = a.Copy().ScaleSize(2.0f).Translate(-1.5f, 3.0f);
// ```
//
struct Rect {
	constexpr Rect() = default;

	constexpr Rect(const float x_pos, const float y_pos, const float width,
	               const float height)
	        : x(x_pos),
	          y(y_pos),
	          w(width),
	          h(height)
	{}

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

	// Returns the X center coordinate.
	constexpr float cx() const
	{
		return x + w / 2;
	}

	// Returns the Y center coordinate.
	constexpr float cy() const
	{
		return y + h / 2;
	}

	// Returns true if the width or the height is exactly zero.
	constexpr bool IsEmpty() const
	{
		return (w == 0.0f || h == 0.0f);
	}

	// Returns true if both the width and the height are positive non-zero
	// numbers.
	constexpr bool HasPositiveSize() const
	{
		return (w > 0.0f && h > 0.0f);
	}

	// Returns true if either the width or the height is a negative non-zero
	// number.
	constexpr bool HasNegativeSize() const
	{
		return !IsEmpty() && !HasPositiveSize();
	}

	// Returns true if the rectangle is existant. Rectangles with no size
	// (both width & height zero) or positive width & height are considered
	// existant. If either the width or the height is negative, the rectangle
	// is considered non-existant. Rectangle operations such as `Contains()`,
	// `Overlaps()`, `Intersect()`, etc. assert that both rectangle operands
	// are existant.
	constexpr bool IsExistant() const
	{
		return IsEmpty() || HasPositiveSize();
	}

	Rect Copy() const
	{
		return *this;
	}

	// Normalise the rectangle so the width and height are positive. This
	// may involve changing the coordinates of the starting point.
	Rect& Normalise()
	{
		if (w < 0.0f) {
			x += w;
			w = -w;
		}
		if (h < 0.0f) {
			y += h;
			h = -h;
		}

		return *this;
	}

	// Scales both the position and the size of the rectangle.
	Rect& Scale(const float s)
	{
		x *= s;
		y *= s;
		w *= s;
		h *= s;

		return *this;
	}

	// Scales the size but leaves the position intact.
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

	// Centers rectangle to the point `(cx, cy)` if the rectangle has a
	// positive size, otherwise a no-op.
	Rect& CenterTo(const float cx, const float cy)
	{
		assert(HasPositiveSize());

		x = cx - (w / 2);
		y = cy - (h / 2);

		return *this;
	}

	// Returns true if this rectangle contains the other rectangle.
	//
	// Returns false if this rectangle is empty.
	//
	// Returns true if the other rectangle is empty but its position is
	// contained in this rectangle.
	//
	// Results in assertion failure if either rectangle is non-existant in
	// debug builds, and false in release builds.
	//
	constexpr bool Contains(const Rect& r) const
	{
		assert(IsExistant());
		assert(r.IsExistant());

		if (!IsExistant() || !r.IsExistant()) {
			return false;
		}
		if (IsEmpty()) {
			return false;
		}

		return (r.x1() >= x1() && r.x2() <= x2()) &&
			   (r.y1() >= y1() && r.y2() <= y2());
	}

	// Returns true if this rectangle and the other rectangle are overlapping.
	//
	// Returns false if the two rectangles are not overlapping, are touching
	// but not overlapping, or if either is empty.
	//
	// Results in assertion failure if either rectangle is non-existant in
	// debug builds, and false in release builds.
	//
	constexpr bool Overlaps(const Rect& r) const
	{
		assert(IsExistant());
		assert(r.IsExistant());

		if (!IsExistant() || !r.IsExistant()) {
			return false;
		}

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

	// Intersects this rectangle with the other rectangle.
	//
	// Returns in an empty rectangle if the two rectangles are not
	// overlapping, are touching but not overlapping, or if either is empty.
	//
	// Results in assertion failure if either rectangle is non-existant in
	// debug builds, and a no-op in release builds.
	//
	Rect& Intersect(const Rect& r)
	{
		assert(IsExistant());
		assert(r.IsExistant());

		if (!IsExistant() || !r.IsExistant()) {
			// no-op
			return *this;
		}

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
		x = 0.0f;
		y = 0.0f;
		w = 0.0f;
		h = 0.0f;

		return *this;
	}

	// Scales this rectangle to fit into the other destination rectangle.
	//
	// Results in assertion failure if either rectangle is empty or
	// non-existant in debug builds, and a no-op in release builds.
	//
	Rect& ScaleSizeToFit(const Rect& dest)
	{
		assert(HasPositiveSize());
		assert(dest.HasPositiveSize());

		if (!HasPositiveSize() || !dest.HasPositiveSize()) {
			// no-op
			return *this;
		}

		const auto s = std::min(dest.w / w, dest.h / h);
		return ScaleSize(s);
	}

	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;

	// Return a string representation of the rectangle in
	// `{x: 0, y: -3, w: 5.5, h: 1.57143}` format.
	std::string ToString() const
	{
		return format_str("{x: %g, y: %g, w: %g, h: %g}", x, y, w, h);
	}
};

} // namespace DosBox

#endif
