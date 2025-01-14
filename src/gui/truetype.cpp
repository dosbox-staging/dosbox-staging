/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2025-2025  The DOSBox Staging Team
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

#include "truetype.h"

#include <array>
#include <map>
#include <set>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BBOX_H
#include FT_GLYPH_H

#include FT_OUTLINE_H
#include FT_BITMAP_H

#include "checks.h"
#include "dosbox.h"
#include "dos_inc.h"
#include "logging.h"
#include "math_utils.h"
#include "render.h"
#include "support.h"
#include "unicode.h"

CHECK_NARROWING();

// Default font file and resource directory
static const std::string DefaultFont = "Flexi_IBM_VGA_True.ttf";
static const std::string ResourceDir = "fonts-console";

// #define DEBUG_TTF_NO_CALLIBRATION
// #define DEBUG_TTF_NO_BORDER_SHARPENING
// #define DEBUG_TTF_NO_INVERSE_RENDERING
// #define DEBUG_TTF_NO_ASPECT_CORRECTION

// ***************************************************************************
// XXX name the section
// ***************************************************************************

enum class Category
{
	// Character not supported by the current font
	Unsupported,
	// Letter, number, punctuation symbol, etc.
	Regular,
	// A space character
	Space,
	// GUI drawing shape or symbol
	Symbol,
	// GUI drawing shape - up down arrow
	SymbolUpDownArrow,
	// Table or box drawing character
	Drawing,
	// Shaded full-cell blocks
	Shade,
	// Symbols for drawing integrals
	Integral,
	// Ligature to be rendered from two 'Category::Regular' characters
	Ligature,
	// Ligature as above - characters should not overlap
	LigatureNoOverlap,
};

// XXX namespace for the callibration code points

// List of code points from 'Box Drawing' and 'Block Elements' Unicode blocks
// which should touch all the borders - to be used for renderer callibration,
// ordered in the order of preferrence
const std::vector<UnicodeCodePoint> callibration_code_points_drawing = {
	// Typical drawing characters available in code page 437
	0x2588, // FULL BLOCK
	0x253c, // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
	0x256c, // BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
	0x256b, // BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE
	0x256a, // BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE
	// Other characters - 'Block Elements'
	0x259a, // QUADRANT UPPER LEFT AND LOWER RIGHT
	0x259e, // QUADRANT UPPER RIGHT AND LOWER LEFT
	0x2599, // QUADRANT UPPER LEFT AND LOWER LEFT AND LOWER RIGHT
	0x259b, // QUADRANT UPPER LEFT AND UPPER RIGHT AND LOWER LEFT
	0x259c, // QUADRANT UPPER LEFT AND UPPER RIGHT AND LOWER RIGHT
	0x259f, // QUADRANT UPPER RIGHT AND LOWER LEFT AND LOWER RIGHT
	// Other characters - 'Box Drawing'
	0x253d, // BOX DRAWINGS LEFT HEAVY AND RIGHT VERTICAL LIGHT
	0x253e, // BOX DRAWINGS RIGHT HEAVY AND LEFT VERTICAL LIGHT
	0x253f, // BOX DRAWINGS VERTICAL LIGHT AND HORIZONTAL LIGHT
	0x2540, // BOX DRAWINGS UP HEAVY AND DOWN HORIZONTAL LIGHT
	0x2541, // BOX DRAWINGS DOWN HEAVY AND UP HORIZONTAL LIGHT
	0x2542, // BOX DRAWINGS VERTICAL HEAVY AND HORIZONTAL LIGHT
	0x2543, // BOX DRAWINGS LEFT UP HEAVY AND RIGHT DOWN LIGHT
	0x2544, // BOX DRAWINGS RIGHT UP HEAVY AND LEFT DOWN LIGHT
	0x2545, // BOX DRAWINGS LEFT DOWN HEAVY AND RIGHT UP LIGHT
	0x2546, // BOX DRAWINGS RIGHT DOWN HEAVY AND LEFT UP LIGHT
	0x2547, // BOX DRAWINGS DOWN LIGHT AND UP HORIZONTAL HEAVY
	0x2548, // BOX DRAWINGS UP LIGHT AND DOWN HORIZONTAL HEAVY
	0x2549, // BOX DRAWINGS RIGHT LIGHT AND LEFT VERTICAL HEAVY
	0x254a, // BOX DRAWINGS LEFT LIGHT AND RIGHT VERTICAL HEAVY
	0x254b, // BOX DRAWINGS HEAVY VERTICAL AND HORIZONTAL
	// Last resort characters - 'Box Drawing'
	0x2571, // BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO LOWER LEFT
	0x2572, // BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO LOWER RIGHT
	0x2573, // BOX DRAWINGS LIGHT DIAGONAL CROSS
};

const std::vector<UnicodeCodePoint> callibration_code_points_shade = {
	0x2593, // DARK SHADE
	0x2592, // MEDIUM SHADE
	0x2591, // LIGHT SHADE
};


const std::vector<UnicodeCodePoint> callibration_code_points_ligature = {
	0x006d, // LATIN SMALL LETTER M
	0x004d, // LATIN CAPITAL LETTER M
	0x0077, // LATIN SMALL LETTER W
	0x0057, // LATIN CAPITAL LETTER W
	0x00e6, // LATIN SMALL LIGATURE AE
	0x00c6, // LATIN CAPITAL LIGATURE AE
	0x0152, // LATIN CAPITAL LIGATURE OE
	0x0153, // LATIN SMALL LIGATURE OE
};

const std::vector<UnicodeCodePoint> callibration_code_points_aspect_ratio = {
	0x25cb, // WHITE CIRCLE
	0x2022, // BULLET
	0x25a0, // BLACK SQUARE
};

// XXX comment
constexpr UnicodeCodePoint callibration_code_point_integral_top    = 0x2321;
constexpr UnicodeCodePoint callibration_code_point_integral_bottom = 0x2320;
constexpr UnicodeCodePoint callibration_code_point_up_down_arrow   = 0x2195;

// All he code points which we can render as a space
const std::set<UnicodeCodePoint> space_code_points = {
	0x0000, // NULL
	0x0020, // SPACE
	0x00a0, // NO-BREAK SPACE
	0x1680, // OGHAM SPACE MARK
	0x2000, // EN QUAD
	0x2001, // EM QUAD
	0x2002, // EN SPACE
	0x2003, // EM SPACE
	0x2004, // THREE-PER-EM SPACE
	0x2005, // FOUR-PER-EM SPACE
	0x2006, // SIX-PER-EM SPACE
	0x2007, // FIGURE SPACE
	0x2008, // PUNCTUATION SPACE
	0x2009, // THIN SPACE
	0x200a, // HAIR SPACE
	0x202f, // NARROW NO-BREAK SPACE
	0x205f, // MEDIUM MATHEMATICAL SPACE
	0x3000, // IDEOGRAPGHIC SPACE
};

const std::unordered_map<UnicodeCodePoint, UnicodeCodePoint> render_as_inverse = {
#ifndef DEBUG_TTF_NO_INVERSE_RENDERING
	{ 0x25d8, 0x2022 },
	{ 0x25d9, 0x25cb },
	// { 0x2593, 0x2591 } XXX evaluate if this is a good idea
#endif
};

// XXX use this
// XXX add comment to ASCII.TXT
const std::unordered_map<UnicodeCodePoint, std::pair<UnicodeCodePoint, UnicodeCodePoint>> supported_ligatures = {
	// Standard Unicode ligatures
	{ 0x00e6, { 0x0061, 0x0065 } }, // LATIN SMALL LIGATURE AE
	{ 0x0132, { 0x0049, 0x004a } }, // LATIN CAPITAL LIGATURE IJ
	{ 0x0133, { 0x0069, 0x006a } }, // LATIN SMALL LIGATURE IJ
	{ 0x0152, { 0x004f, 0x0045 } }, // LATIN CAPITAL LIGATURE OE
	{ 0x0153, { 0x006e, 0x0065 } }, // LATIN SMALL LIGATURE OE
	{ 0x04a4, { 0x041d, 0x0413 } }, // CYRILLIC CAPITAL LIGATURE EN GHE
	{ 0x04a5, { 0x043d, 0x0433 } }, // CYRILLIC SMALL LIGATURE EN GHE
	{ 0x04d5, { 0x0430, 0x0435 } }, // CYRILLIC SMALL LIGATURE A IE
	// DOSBox private ligatures
	{ 0xedb0, { 0x007a, 0x0142 } }, // PRIVATE DOSBOX PLN SYMBOL
	{ 0xedb2, { 0x0423, 0x041e } }, // PRIVATE DOSBOX CYRILLIC CAPITAL LIGATURE UO
	{ 0xedb3, { 0x0443, 0x043e } }, // PRIVATE DOSBOX CYRILLIC SMALL LIGATURE UO

	// Current ligature fallback support code can only render in a sane way
	// some of the ligatures. It can't work for the following ones:
	// - U+00C6 - LATIN CAPITAL LIGATURE AE
	// - U+04B4 - CYRILLIC CAPITAL LIGATURE TE TSE
	// - U+04B5 - CYRILLIC SMALL LIGATURE TE TSE
	// - U+04D4 - CYRILLIC CAPITAL LIGATURE A IE
	// - U+0587 - ARMENIAN SMALL LIGATURE ECH YIWN
	// - U+05F0 - HEBREW LIGATURE YIDDISH DOUBLE VAV
	// - U+05F1 - HEBREW LIGATURE YIDDISH VAV YOD
	// - U+05F2 - HEBREW LIGATURE YIDDISH DOUBLE YOD
};

const std::set<UnicodeCodePoint> no_overlap_ligatures = {
	0xedb0 // PRIVATE DOSBOX PLN SYMBOL
};

// XXX add custom code to render 0xedb1 NNN #PRIVATE DOSBOX DOUBLE TILDE

// XXX add comments
static bool is_dosbox_private(const UnicodeCodePoint code_point)
{
	if (code_point == 0xf20d || code_point == 0xf8ff) {
		return false;
	}

	if (code_point >= 0xe000 && code_point <= 0xf8ff) {
		return true;
	}

	return false;
}

static Category get_default_category(const UnicodeCodePoint code_point,
                                     const uint8_t dos_code_point)
{
	if (space_code_points.contains(code_point)) {
		return Category::Space;
	}

#ifdef DEBUG_TTF_NO_POSTPROCESSING
	return Category::Regular;
#endif

	if (dos_code_point < ' ') {

		// U+2195 - UP DOWN ARROW
		// U+21A8 - UP DOWN ARROW WITH BASE
		if (code_point == 0x2195 || code_point == 0x21a8) {
			return Category::SymbolUpDownArrow;
		}

		// U+2022 - BULLET
		if (code_point == 0x2022) {
			return Category::Symbol;
		}

		// 'Arrows' Unicode block
		if (code_point >= 0x2190 && code_point <= 0x21ff) {
			return Category::Symbol;
		}
	}

/* XXX reconsider these; all notes: 0x2669-0x266f
	// U+2640 - FEMALE SIGN
	// U+2642 - MALE SIGN
	// U+266A - EIGHTH NOTE
	// U+266B - BEAMED EIGHTH NOTES
	if (code_point == 0x2640 || code_point == 0x2642 ||
	    code_point == 0x266a || code_point == 0x266b) {
	    	// Although these are symbols, they are not GUI elements
	    	return Category::Regular;
	}
*/

	// U+2591 - LIGHT SHADE
	// U+2592 - MEDIUM SHADE
	// U+2593 - DARK SHADE
	if (code_point >= 0x2591 && code_point <= 0x2593) {
		return Category::Shade;
	}

	// U+2320 - TOP HALF INTEGRAL
	// U+2321 - BOTTOM HALF INTEGRAL
	if (code_point == 0x2320 || code_point == 0x2321) {
		return Category::Integral;
	}

	// 'Box Drawing' and 'Block Elements' Unicode blocks
	if (code_point >= 0x2500 && code_point <= 0x259f) {
		return Category::Drawing;
	}

	// 'Geometric Shapes' and 'Miscellaneous Symbols' Unicode blocks
	if (code_point >= 0x25a0 && code_point <= 0x26ff) {
		return Category::Symbol;
	}

	return Category::Regular;
}

static bool needs_aspect_ratio_correction(const UnicodeCodePoint code_point) // XXX use it
{
#ifdef DEBUG_TTF_NO_ASPECT_CORRECTION
	return false;
#endif

	// XXX notes - reconsider these
	if (code_point >= 0x2669 && code_point <= 0x266f) {
		return false;
	}

	// U+2022 - BULLET
	if (code_point == 0x2022) {
		return true;
	}

	// 'Geometric Shapes' and 'Miscellaneous Symbols' Unicode blocks
	if (code_point >= 0x25a0 && code_point <= 0x26ff) {
		return true;
	}

	return false;
}

// XXX add comments
static bool needs_sharpening_all_borders(const UnicodeCodePoint code_point)
{
#ifdef DEBUG_TTF_NO_BORDER_SHARPENING
	return false;
#endif
	if (get_default_category(code_point, 0) != Category::Drawing) { // XXX this '0' is ugly
		return false;
	}

	if (code_point >= 0x2504 && code_point <= 0x250b) {
		return false;
	}

	if (code_point >= 0x254c && code_point <= 0x254f) {
		return false;
	}

	if (code_point >= 0x2571 && code_point <= 0x2573) {
		return false;
	}

	if (code_point >= 0x2591 && code_point <= 0x2593) {
		return false;
	}

	return true;
}

// XXX add comments
static bool needs_sharpening_only_top(const UnicodeCodePoint code_point)
{
#ifdef DEBUG_TTF_NO_BORDER_SHARPENING
	return false;
#endif
	return (code_point == 0x2321);
}

// XXX add comments
static bool needs_sharpening_only_bottom(const UnicodeCodePoint code_point)
{
#ifdef DEBUG_TTF_NO_BORDER_SHARPENING
	return false;
#endif
	return (code_point == 0x2320);
}

// ***************************************************************************
// XXX name the section
// ***************************************************************************

struct Color
{
	Color(const uint8_t r, const uint8_t g, const uint8_t b) : r(r), g(g), b(b) {}

	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
};

class Cell
{
public:
	Cell() = delete;
	Cell(const size_t x, const size_t y);

	size_t GetSizeX() const { return size_x; }
	size_t GetSizeY() const { return size_y; }

	uint8_t GetPixel(const size_t x, const size_t y) const;
	void SetPixel(const size_t x, const size_t y, const uint8_t value);

	void Invert();

	// Render one line of the cell into the output buffer
	void RenderLine(const std::vector<uint8_t>::iterator &destination,
			const Color foreground,
			const Color background,
	                const size_t y) const;

	// Functions to check if the content touches the border, do not take
	// pixel brightness into account
	bool IsTouchingLeft() const;
	bool IsTouchingRight() const;
	bool IsTouchingTop() const;
	bool IsTouchingBottom() const;

	// Calculates the glyph distance from the border, take the antialiased
	// pixel brightness into account
	float GetDistanceLeft() const;
	float GetDistanceRight() const;
	float GetDistanceTop() const;
	float GetDistanceBottom() const;

	float GetContentWidth() const;
	float GetContentHeight() const;

	// Blend other cell with the current one
	void Blend(const Cell& other);

	// Functions to remove antialiasing from the given borders, to fix the
	// view when two drawing characters are placed next to each other
	void SharpenAllBorders();
	void SharpenOnlyTop();
	void SharpenOnlyBottom();

private:
	// Helper functions for border sharpening (de-antialiasing)
	size_t GetSharpenDepth(const size_t size) const;
	void SharpenTop(const size_t depth_y);
	void SharpenBottom(const size_t depth_y);
	void SharpenLeft(const size_t depth_x);
	void SharpenRight(const size_t depth_x);

	size_t size_x = 0;
	size_t size_y = 0;

	std::vector<uint8_t> data = {};
};

Cell::Cell(const size_t x, const size_t y) :
	size_x(x), size_y(y)
{
	assert(x > 0 && x < UINT16_MAX);
	assert(y > 0 && y < UINT16_MAX);

	data.resize(x * y);
	data.shrink_to_fit();
}

uint8_t Cell::GetPixel(const size_t x, const size_t y) const
{
	return data.at(x + y * size_x);
}

void Cell::SetPixel(const size_t x, const size_t y, const uint8_t value)
{
	data[x + y * size_x] = value;
}

void Cell::Invert()
{
	for (auto &pixel : data) {
		pixel = UINT8_MAX - pixel;
	}
}

void Cell::RenderLine(const std::vector<uint8_t>::iterator &destination,
                      const Color foreground,
                      const Color background,
                      const size_t y) const
{
	auto iter_source      = data.begin() + y * size_x;
	auto iter_destination = destination;
	
	for (size_t x = 0; x < size_x; ++x) {
		const auto value_foreground = *(iter_source++);
		const auto value_background = UINT8_MAX - value_foreground;

		auto mix = [&](const uint8_t foreground, const uint8_t background) {
			const auto value = value_foreground * foreground +
			                   value_background * background;

			const auto value_float = static_cast<float>(value);
			return clamp_to_uint8(std::lround(value_float / UINT8_MAX));
		};

		*(iter_destination++) = mix(foreground.r, background.r);
		*(iter_destination++) = mix(foreground.g, background.g);
		*(iter_destination++) = mix(foreground.b, background.b);
		*(iter_destination++) = 0;
	}
}

bool Cell::IsTouchingLeft() const
{
	for (size_t y = 0; y < size_y; ++y) {
		if (GetPixel(0, y) != 0) {
			return true;
		}
	}

	return false;
}

bool Cell::IsTouchingRight() const
{
	for (size_t y = 0; y < size_y; ++y) {
		if (GetPixel(size_x - 1, y) != 0) {
			return true;
		}
	}

	return false;
}

bool Cell::IsTouchingTop() const
{
	for (size_t x = 0; x < size_x; ++x) {
		if (GetPixel(x, 0) != 0) {
			return true;
		}
	}

	return false;
}

bool Cell::IsTouchingBottom() const
{
	for (size_t x = 0; x < size_x; ++x) {
		if (GetPixel(x, size_y - 1) != 0) {
			return true;
		}
	}

	return false;
}

 // XXX deduplicate the 'GetDistance' code
float Cell::GetDistanceLeft() const
{
	auto result = static_cast<float>(size_x);

	for (size_t x = 0; x < size_x; ++x) {
		bool found = false;
		for (size_t y = 0; y < size_y; ++y) {
			const auto value = GetPixel(x, y);
			if (value == 0) {
				continue;
			}

			found = true;

			auto distance = static_cast<float>(x);
			distance += (UINT8_MAX - value) / static_cast<float>(UINT8_MAX);

			result = std::min(result, distance);
		}

		if (found) {
			return result;
		}
	}

	return 0.0f;
}

float Cell::GetDistanceRight() const
{
	auto result = static_cast<float>(size_x);

	for (size_t x = 0; x < size_x; ++x) {
		bool found = false;
		for (size_t y = 0; y < size_y; ++y) {
			const auto value = GetPixel(size_x - x - 1, y);
			if (value == 0) {
				continue;
			}

			found = true;

			auto distance = static_cast<float>(x);
			distance += (UINT8_MAX - value) / static_cast<float>(UINT8_MAX);

			result = std::min(result, distance);
		}

		if (found) {
			return result;
		}
	}

	return 0.0f;
}

float Cell::GetDistanceTop() const
{
	auto result = static_cast<float>(size_y);

	for (size_t y = 0; y < size_y; ++y) {
		bool found = false;
		for (size_t x = 0; x < size_x; ++x) {
			const auto value = GetPixel(x, y);
			if (value == 0) {
				continue;
			}

			found = true;

			auto distance = static_cast<float>(y);
			distance += (UINT8_MAX - value) / static_cast<float>(UINT8_MAX);

			result = std::min(result, distance);
		}

		if (found) {
			return result;
		}
	}

	return 0.0f;
}

float Cell::GetDistanceBottom() const
{
	auto result = static_cast<float>(size_y);

	for (size_t y = 0; y < size_y; ++y) {
		bool found = false;
		for (size_t x = 0; x < size_x; ++x) {
			const auto value = GetPixel(x, size_y - y - 1);
			if (value == 0) {
				continue;
			}

			found = true;

			auto distance = static_cast<float>(y);
			distance += (UINT8_MAX - value) / static_cast<float>(UINT8_MAX);

			result = std::min(result, distance);
		}

		if (found) {
			return result;
		}
	}

	return 0.0f;
}

float Cell::GetContentWidth() const
{
	auto value = static_cast<float>(size_x);
	value -= GetDistanceLeft();
	value -= GetDistanceRight();

	return std::max(0.0f, value);
}

float Cell::GetContentHeight() const
{
	auto value = static_cast<float>(size_y);
	value -= GetDistanceTop();
	value -= GetDistanceBottom();

	return std::max(0.0f, value);
}

void Cell::Blend(const Cell& other)
{
	for (size_t x = 0; x < size_x && x < other.size_x; ++x) {
		for (size_t y = 0; y < size_y && y < other.size_y; ++y) {
			const auto other_value = other.GetPixel(x, y);
			SetPixel(x, y, std::max(GetPixel(x, y), other_value));
		}
	}
}

size_t Cell::GetSharpenDepth(const size_t size) const
{
	constexpr size_t MinSizeToProcess = 8;
	constexpr float DepthProportion   = 0.2f;

	static_assert(DepthProportion * MinSizeToProcess >= 1.0f);

	if (size < MinSizeToProcess) {
		return  0;
	}

	const auto depth = std::lround(DepthProportion * static_cast<float>(size));

	constexpr size_t Min = 1;
	return std::max(Min, static_cast<size_t>(depth - 1));
}

void Cell::SharpenTop(const size_t depth_y)
{
	if (depth_y == 0) {
		return;
	}

	for (size_t x = 0; x < size_x; ++x) {
		const size_t y_border = 0;
		const size_t y_limit  = y_border + depth_y;

		auto value = GetPixel(x, y_border);
		for (size_t y = y_border + 1; y <= y_limit; ++y) {
			value = std::max(value, GetPixel(x, y));
		}

		for (size_t y = y_border; y <= y_limit; ++y) {
			SetPixel(x, y, value);
		}
	}
}

void Cell::SharpenBottom(const size_t depth_y)
{
	if (depth_y == 0) {
		return;
	}

	for (size_t x = 0; x < size_x; ++x) {
		const size_t y_border = size_y - 1;
		const size_t y_limit  = y_border - depth_y;

		auto value = GetPixel(x, y_border);
		for (size_t y = y_border - 1; y >= y_limit; --y) {
			value = std::max(value, GetPixel(x, y));
		}

		for (size_t y = y_border; y >= y_limit; --y) {
			SetPixel(x, y, value);
		}
	}
}

void Cell::SharpenLeft(const size_t depth_x)
{
	if (depth_x == 0) {
		return;
	}

	for (size_t y = 0; y < size_y; ++y) {
		const size_t x_border = 0;
		const size_t x_limit  = x_border + depth_x;

		auto value = GetPixel(x_border, y);
		for (size_t x = x_border + 1; x <= x_limit; ++x) {
			value = std::max(value, GetPixel(x, y));
		}

		for (size_t x = x_border; x <= x_limit; ++x) {
			SetPixel(x, y, value);
		}
	}
}

void Cell::SharpenRight(const size_t depth_x)
{
	if (depth_x == 0) {
		return;
	}

	for (size_t y = 0; y < size_y; ++y) {
		const size_t x_border = size_x - 1;
		const size_t x_limit  = x_border - depth_x;

		auto value = GetPixel(x_border, y);
		for (size_t x = x_border - 1; x >= x_limit; --x) {
			value = std::max(value, GetPixel(x, y));
		}

		for (size_t x = x_border; x >= x_limit; --x) {
			SetPixel(x, y, value);
		}
	}
}

void Cell::SharpenAllBorders()
{
	const auto depth_x = GetSharpenDepth(size_x);
	const auto depth_y = GetSharpenDepth(size_y);

	SharpenTop(depth_y);
	SharpenBottom(depth_y);
	SharpenLeft(depth_x);
	SharpenRight(depth_x);
}

void Cell::SharpenOnlyTop()
{
	SharpenTop(GetSharpenDepth(size_y));
}

void Cell::SharpenOnlyBottom()
{
	SharpenBottom(GetSharpenDepth(size_y));
}

// ***************************************************************************
// XXX name the section
// ***************************************************************************

struct RenderRecipe {
	FT_UInt  glyph_index    = 0;
	Category glyph_category = {};

	std::optional<FT_UInt> glyph_index_secondary = 0;

	bool invert = false;

	bool sharpen_all_borders = false;
	bool sharpen_only_top    = false; 
	bool sharpen_only_bottom = false;

	bool needs_aspect_ratio_correction = false;
};

// ***************************************************************************
// XXX name the section
// ***************************************************************************

static FT_Library library = {};

class FontWrap {
public:
	bool Load(const std_fs::path &file_path);
	void Unload();

	bool IsLoaded() const { return is_loaded; }
	// Check if code page is compatible with the current code page
	bool IsCompatible();

	RenderRecipe CreateRecipe(const UnicodeCodePoint code_point,
		                  const uint8_t dos_code_point,
		                  std::set<UnicodeCodePoint>& missing_glyphs);

	void PreRender(const size_t width, const size_t height);
	void RenderToLine(const uint8_t character, const size_t line,
	                  const std::vector<uint8_t>::iterator &destination);

	~FontWrap() { Unload(); }

private:
	struct CallibrationData {
		size_t cell_size_x = 0;
		size_t cell_size_y = 0;

		// Shift the rendering by given number of pixels
		float delta_x = 0.0f;
		float delta_y = 0.0f;

		// Stretch to use given number of extra pixels
		float stretch_x = 0.0f;
		float stretch_y = 0.0f;
	};

	struct CallibrationIndexes {
		FT_UInt drawing         = 0;
		FT_UInt shade           = 0;
		FT_UInt integral_top    = 0;
		FT_UInt integral_bottom = 0;
		FT_UInt up_down_arrow   = 0;
		FT_UInt aspect_ratio    = 0;
	};


	Cell GetCell(const FT_Bitmap& bitmap,
                     const FT_Int bitmap_left,
                     const FT_Int bitmap_top,
	             const size_t size_x,
	             const size_t size_y) const;

	Cell RenderCellGeneric(const FT_UInt glyph_index,
	                       const CallibrationData& callibration,
	                       const FT_Render_Mode render_mode);

	Cell RenderCellPreserveAspectRatio(const FT_UInt glyph_index,
	                                   const CallibrationData& callibration,
	                                   const FT_Render_Mode render_mode,
	                                   const bool center = false);

	Cell RenderCellSymbol(const FT_UInt glyph_index,
	                      const bool preserve_aspect,
	                      const bool is_up_down_arrow);

	Cell RenderCellLigature(const FT_UInt glyph_index_1,
	                        const FT_UInt glyph_index_2,
	                        const bool should_overlap = true); // XXX separate category

	Cell RenderCellShade(const FT_UInt glyph_index);

	Cell RenderCell(const RenderRecipe& recipe);

	FT_BBox GetBoundingBox(const FT_UInt glyph_index);
	FT_BBox GetFontBoundingBox();

	CallibrationData TweakTouchLeft(const FT_UInt glyph_index,
                                        const FontWrap::CallibrationData& base,
                                        const int max_steps);

	CallibrationData TweakTouchRight(const FT_UInt glyph_index,
                                         const FontWrap::CallibrationData& base,
                                         const int max_steps);

	CallibrationData TweakTouchTop(const FT_UInt glyph_index,
                                       const FontWrap::CallibrationData& base,
                                       const int max_steps);

	CallibrationData TweakTouchBottom(const FT_UInt glyph_index,
                                          const FontWrap::CallibrationData& base,
                                          const int max_steps);

	CallibrationData TweakCenter(const FT_UInt glyph_index,
                                     const FontWrap::CallibrationData& base,
                                     const bool is_up_down_arrow = false);

	CallibrationData TweakGeneric(const Category existing,
	                              const Category fallback,
	                              const FT_UInt glyph_index);

	CallibrationData TweakIntegral(const Category existing,
	                               const Category fallback,
	                               const FT_UInt glyph_index_top,
	                               const FT_UInt glyph_index_bottom);

	void CallibrateRenderer();

	FT_Face face = {};

	static constexpr auto Identity       = 0x10000L;
	static constexpr auto PointsPerPixel = 64;

	static constexpr auto LigatureOverlap = 0.08f;

	// If the face was loaded succesfully
	bool is_loaded = false;

	// Code page the data below is relevant for
	uint16_t code_page = 0;
	// If the font is compatible with the current code page
	bool is_compatible = false;

	static constexpr float CallibrationStep = 0.5f;
	static constexpr int   MaxStepDelta     = 5;
	static constexpr int   MaxStepStretch   = 5;

	FT_BBox bounding_box = {};

	float bounding_box_x = 0.0f;
	float bounding_box_y = 0.0f;

	CallibrationIndexes  callibration_indexes          = {};
	std::vector<FT_UInt> callibration_indexes_ligature = {};

	// Glyph indexes relevant to the DOS code page
	std::array<RenderRecipe, UINT8_MAX + 1> recipes = {};

	// Pre-rendered font bitmaps
	uint16_t pre_render_code_page  = 0;
	size_t   pre_render_width      = 0; // XXX get rid of these variables
	size_t   pre_render_height     = 0;
	std::vector<Cell> pre_rendered = {};

	float pixel_aspect_ratio = 1.0f;

	std::unordered_map<Category, CallibrationData> callibration = {};
	// XXX comment - for ligature renderer
	float ligature_distance_left  = 0.0f;
	float ligature_distance_right = 0.0f;
	// XXX comment - for symbol renderer
	float up_down_arrow_distance_top    = 0.0f;
	float up_down_arrow_distance_bottom = 0.0f;
	// XXX comment; name is misleading, it depends on resolution too
	float font_aspect_ratio = 0.0f;

	void ResetCallibration();

	void ApplyCallibration(const CallibrationData& callibration);
};

static FontWrap font = {};

void FontWrap::Unload()
{
	if (is_loaded) {
		// XXX assert library is loaded
		FT_Done_Face(face);
		is_loaded = false;

		code_page     = 0;
		is_compatible = false;

		callibration_indexes = CallibrationIndexes();
		callibration_indexes_ligature.clear();

		ligature_distance_left  = 0.0f;
		ligature_distance_right = 0.0f;

		up_down_arrow_distance_top    = 0.0f;
		up_down_arrow_distance_bottom = 0.0f;

		font_aspect_ratio = 0.0f;

		pre_rendered.clear();
	}
}

bool FontWrap::Load(const std_fs::path& file_path)
{
	Unload();

	const auto result = FT_New_Face(library, file_path.c_str(), 0, &face);
	is_loaded = (result == FT_Err_Ok);
	if (!is_loaded) {
		// XXX error message
		return false;
	}

	for (const auto code_point : callibration_code_points_drawing) {
		const auto glyph_index = FT_Get_Char_Index(face, code_point);
		if (glyph_index != 0) {
			callibration_indexes.drawing = glyph_index;
			break;
		}
	}

	for (const auto code_point : callibration_code_points_shade) {
		const auto glyph_index = FT_Get_Char_Index(face, code_point);
		if (glyph_index != 0) {
			callibration_indexes.shade = glyph_index;
			break;
		}
	}

	for (const auto code_point : callibration_code_points_aspect_ratio) {
		const auto glyph_index = FT_Get_Char_Index(face, code_point);
		if (glyph_index != 0) {
			callibration_indexes.aspect_ratio = glyph_index;
			break;
		}
	}

	for (const auto code_point : callibration_code_points_ligature) {
		const auto glyph_index = FT_Get_Char_Index(face, code_point);
		if (glyph_index != 0) {
			callibration_indexes_ligature.push_back(glyph_index);
		}
	}

	callibration_indexes.integral_top    = FT_Get_Char_Index(face, callibration_code_point_integral_top);
	callibration_indexes.integral_bottom = FT_Get_Char_Index(face, callibration_code_point_integral_bottom);
	callibration_indexes.up_down_arrow   = FT_Get_Char_Index(face, callibration_code_point_up_down_arrow);

	return true;
}

RenderRecipe FontWrap::CreateRecipe(const UnicodeCodePoint code_point,
                                    const uint8_t dos_code_point,
                                    std::set<UnicodeCodePoint>& missing_glyphs)
{
	RenderRecipe recipe = {};

	recipe.glyph_category = get_default_category(code_point, dos_code_point);
	
	// For code points which should be rendered as space character (i.e.
	// U+00A0 - NO-BREAK SPACE) do not even bother checking the font
	if (recipe.glyph_category == Category::Space) {
		return recipe;
	}

	const bool is_private = is_dosbox_private(code_point);
	if (!is_private) {
		recipe.glyph_index = FT_Get_Char_Index(face, code_point);

		// Some characters should better be rendered as inverse of others
		if (render_as_inverse.contains(code_point)) {
			const auto inverse_code_point = render_as_inverse.at(code_point);
			const auto inverse_index      = FT_Get_Char_Index(face, inverse_code_point);

			if (inverse_index != 0) {
				recipe.glyph_index = inverse_index;
				recipe.invert      = true;
			}
		}

		// Some characters intended for drawing should have non-antialiased
		// borders, as they can touch other drawing elements to, for example,
		// form a longer lines
		if (needs_sharpening_all_borders(code_point)) {
			recipe.sharpen_all_borders = true;
		} else if (needs_sharpening_only_top(code_point)) {
			recipe.sharpen_only_top    = true;
		} else if (needs_sharpening_only_bottom(code_point)) {
			recipe.sharpen_only_bottom = true;
		}

		if (needs_aspect_ratio_correction(code_point)) {
			recipe.needs_aspect_ratio_correction = true;
		}

		// If we have a valid recipe, no further processing is needed 
		if (recipe.glyph_index != 0) {
			return recipe;
		}
	}

	// Glyph is not directly supported by the font
	recipe.glyph_category = Category::Unsupported;

	// Some ligatures not present in the font or private characters can
	// still be rendered using our image processing code

	// Some ligatures can be rendered by composing two glyphs
	if (supported_ligatures.contains(code_point)) {
		// We have a fallback code capable of rendering this ligature;
		// check if the font contains the ingredients
		const auto code_point_1 = supported_ligatures.at(code_point).first;
		const auto code_point_2 = supported_ligatures.at(code_point).second;

		const auto index_1 = FT_Get_Char_Index(face, code_point_1);
		const auto index_2 = FT_Get_Char_Index(face, code_point_2);

		if (index_1 != 0 && index_2 != 0) {
			recipe.glyph_index           = index_1;
			recipe.glyph_index_secondary = index_2;

			if (no_overlap_ligatures.contains(code_point)) {
				recipe.glyph_category = Category::LigatureNoOverlap;
			} else {
				recipe.glyph_category = Category::Ligature;
			}

			// We can render this ligature with our fallback code
			return recipe;
		}

		// Characters needed by our ligature renderer are not available
		if (is_private) {
			if (index_1 == 0) {
				missing_glyphs.insert(code_point_1);
			}
			if (index_2 == 0) {
				missing_glyphs.insert(code_point_2);
			}
		}
	}

	if (!is_private) {
		missing_glyphs.insert(code_point);
	}
	return recipe;
}

bool FontWrap::IsCompatible()
{
	if (!is_loaded) {
		return false;
	}

	// Check for the cached result
	if (code_page != 0 && dos.loaded_codepage == code_page) {
		return is_compatible;
	}

	// Re-check compatibility
	is_compatible = false;
	code_page     = dos.loaded_codepage;

	// XXX if custom loaded code page - return not compatible

	if (!is_code_page_supported(code_page)) {
		LOG_WARNING("TTF: Code page %d cannot be displayed by the current font engine",
		            code_page);
		return false;
	}

	// Search for the glyph indexes relevant to the DOS code page
	std::set<UnicodeCodePoint> missing_glyphs = {};
	for (size_t idx = 0; idx < recipes.size(); ++idx) {
		const auto code_points = dos_to_unicode(format_str("%c", idx),
		    DosStringConvertMode::ScreenCodesOnly);
		// We can't display characters which use combining marks,
		// FreeType alone can't render such graphemes, we would need a
		// text shaping engine (TODO: write a simple one?).
		UnicodeCodePoint code_point = {};
		if (code_points.empty()) {
			assert(false);
			return false;
		} else if (code_points.size() == 2 && space_code_points.contains(code_points[0])) {
			code_point = code_points[1];
		} else if (code_points.size() > 1) {
			LOG_WARNING("TTF: Code page %d uses combining marks, "
			            "this is not supported by the current font engine",
			            code_page);
			return false;
		} else {
			code_point = code_points[0];
		}

		recipes[idx] = CreateRecipe(code_point, idx, missing_glyphs);

		if (recipes[idx].glyph_index == 0 && is_dosbox_private(code_point)) {
			LOG_WARNING("TTF: Code page %d uses non-Unicode characters, "
			            "this is not supported by the current font engine",
			            code_page);
			return false;
		}
	}

	if (missing_glyphs.empty()) {
		is_compatible = true;
		return true;
	}

	// Report the missing glyphs in the output log XXX separate function
	std::string message = {};	
	std::string line    = {};

	constexpr size_t LineBreakAt = 100;

	for (const auto code_point : missing_glyphs) {
		const auto code_point_str = format_str("U+%04X", code_point);
		if (line.size() + 3 + code_point_str.size() > LineBreakAt) {
			if (!message.empty()) {
				message += ",\n";
			}
			message += line;
			line.clear();
		}

		if (!line.empty()) {
			line += ", ";
		}
		line += code_point_str;
	}
	if (!line.empty()) {
		if (!message.empty()) {
			message += ",\n";
		}
		message += line;		
	}

	LOG_WARNING("TTF: Code page %d cannot be displayed using the current font, missing glyphs:\n\n%s\n",
	            dos.loaded_codepage, message.c_str());
	return false;
}

Cell FontWrap::GetCell(const FT_Bitmap& bitmap,
                       const FT_Int bitmap_left,
                       const FT_Int bitmap_top,
                       const size_t size_x,
                       const size_t size_y) const
{
	Cell cell(size_x, size_y);

	// Code based on https://freetype.org/freetype2/docs/tutorial/example1.c

	const int x_min = bitmap_left;
	const int y_min = size_y - bitmap_top;
	const int x_max = x_min + bitmap.width;
	const int y_max = y_min + bitmap.rows;
	
	auto get_pixel_grey = [&](const int p, const int q) -> uint8_t {
		const auto index = q * bitmap.pitch + p;
		
		return bitmap.buffer[index];
	};

	auto get_pixel_mono = [&](const int p, const int q) -> uint8_t {
		const auto index = q * bitmap.pitch + p / 8;

		const auto bit_num  = 7 - (p % 8);
		const auto bit_mask = (1 << bit_num);

		return (bitmap.buffer[index] & bit_mask) ? 0xff : 0;
	};

	int p = 0;
	for (int x = x_min; x < x_max; ++x, ++p) {
		if (x < 0 || x >= cell.GetSizeX()) {
			continue;
		}
		int q = 0;
		for (int y = y_min; y < y_max; ++y, ++q) {
			if (y < 0 || y >= cell.GetSizeY()) {
				continue;
			}

			uint8_t pixel_value = 0;
			switch (bitmap.pixel_mode) {
			case FT_PIXEL_MODE_GRAY:
				pixel_value = get_pixel_grey(p, q);
				break;
			case FT_PIXEL_MODE_MONO:
				pixel_value = get_pixel_mono(p, q);
				break;
			default:
				assert(false);
				break;
			}

			cell.SetPixel(static_cast<uint16_t>(x),
			              static_cast<uint16_t>(y),
			              pixel_value);
		}
	}

	return cell;
}

Cell FontWrap::RenderCellGeneric(const FT_UInt glyph_index,
                                 const CallibrationData& callibration,
                                 const FT_Render_Mode render_mode)
{
	ApplyCallibration(callibration);

	// XXX check for errors, for every function call

	FT_Load_Glyph(face, glyph_index, 0);
	FT_Render_Glyph(face->glyph, render_mode);

	return GetCell(face->glyph->bitmap,
	               face->glyph->bitmap_left,
	               face->glyph->bitmap_top,
	               callibration.cell_size_x,
	               callibration.cell_size_y);
}

Cell FontWrap::RenderCellPreserveAspectRatio(const FT_UInt glyph_index,
	                                     const CallibrationData& callibration,
	                                     const FT_Render_Mode render_mode,
	                                     const bool center) // XXX center if needed
{
	const auto cell = RenderCellGeneric(glyph_index, callibration, render_mode);

	// XXX cap the value to prevent going over the cell heigth, check if coefficients are sane
        auto callibration_tweaked = callibration;

        const auto coefficient = font_aspect_ratio / pixel_aspect_ratio - 1.0f;
        const auto diff_pixels = coefficient * cell.GetSizeY();

        callibration_tweaked.stretch_y += diff_pixels;
        
        const auto distance_to_middle = callibration_tweaked.delta_y +
                                        cell.GetDistanceBottom() +
                                        cell.GetContentHeight() / 2;
        callibration_tweaked.delta_y -= coefficient * distance_to_middle / 2;

        if (center) {
        	// Due to rounding errors and imprecisions the aspect ratio
        	// correction above can sometimes move the previously centered
        	// glyph slightly off-center

        	callibration_tweaked = TweakCenter(glyph_index, callibration_tweaked);
        }

	return RenderCellGeneric(glyph_index, callibration_tweaked, render_mode);
}

Cell FontWrap::RenderCellSymbol(const FT_UInt glyph_index,
                                const bool preserve_aspect,
                                const bool is_up_down_arrow)
{
	constexpr auto RenderMode = FT_RENDER_MODE_NORMAL;

	auto callibration_tweaked = TweakCenter(glyph_index, callibration[Category::Symbol], is_up_down_arrow);

	if (preserve_aspect) {
		return RenderCellPreserveAspectRatio(glyph_index, callibration_tweaked, RenderMode, !is_up_down_arrow);
	} else {
		return RenderCellGeneric(glyph_index, callibration_tweaked, RenderMode);
	}
}

Cell FontWrap::RenderCellLigature(const FT_UInt glyph_index_1,
                                  const FT_UInt glyph_index_2,
                                  const bool should_overlap)
{
	constexpr auto  Flags    = FT_RENDER_MODE_NORMAL;
	constexpr float MinWidth = 1.0f;

	const auto cell_width = static_cast<float>(pre_render_width);

	const auto overlap = should_overlap ? cell_width * LigatureOverlap : 0.0f;

	const auto& callibration_base = callibration[Category::Regular];

	auto callibration_1 = callibration_base;
	auto callibration_2 = callibration_base;

	const auto cell_1 = RenderCellGeneric(glyph_index_1, callibration_base, Flags);
	const auto cell_2 = RenderCellGeneric(glyph_index_2, callibration_base, Flags);

	const auto distance_1_left  = cell_1.GetDistanceLeft();
	const auto distance_1_right = cell_1.GetDistanceRight();
	const auto distance_2_left  = cell_2.GetDistanceLeft();
	const auto distance_2_right = cell_2.GetDistanceRight();

	auto width_1 = cell_width - distance_1_left - distance_1_right;
	auto width_2 = cell_width - distance_2_left - distance_2_right;

	width_1 = std::max(MinWidth, width_1);
	width_2 = std::max(MinWidth, width_2);

	auto width_allowed = cell_width - ligature_distance_left - ligature_distance_right;
	width_allowed = std::min(width_allowed, width_1 + width_2);

	auto width_allowed_1 = width_allowed * width_1 / (width_1 + width_2) + overlap;
	auto width_allowed_2 = width_allowed * width_2 / (width_1 + width_2) + overlap;

	width_allowed_1 = std::min(width_allowed_1, width_1);
	width_allowed_2 = std::min(width_allowed_2, width_2);

	callibration_1.stretch_x -= width_1 - width_allowed_1;
	callibration_2.stretch_x -= width_2 - width_allowed_2;

	callibration_1.delta_x -= distance_1_left - ligature_distance_left;
	callibration_2.delta_x -= distance_2_left - ligature_distance_left +
	                          overlap * 2 - width_allowed_1;

	auto cell = RenderCellGeneric(glyph_index_1, callibration_1, Flags);
	cell.Blend(RenderCellGeneric(glyph_index_2, callibration_2, Flags));

	return cell;
}

Cell FontWrap::RenderCellShade(const FT_UInt glyph_index)
{
	ApplyCallibration(callibration.at(Category::Shade));

	FT_Load_Glyph(face, glyph_index, 0);
	FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);

	auto bitmap_left = face->glyph->bitmap_left;
	auto bitmap_top  = face->glyph->bitmap_top;

	const auto glyph_bounding_box = GetBoundingBox(glyph_index);

	const auto glyph_bounding_box_x = bounding_box.xMax - bounding_box.xMin;
	const auto glyph_bounding_box_y = bounding_box.yMax - bounding_box.yMin;


	FT_Load_Glyph(face, glyph_index, 0); 
	// XXX if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
	// XXX check the number of points/contours against maximum
	// XXX check error codes

	FT_Outline grid = {};

	const auto num_points   = face->glyph->outline.n_points;
	const auto num_contours = face->glyph->outline.n_contours;

	FT_Outline_New(library, num_points * 9, num_contours * 9, &grid);
	grid.flags = face->glyph->outline.flags;

	size_t start_point   = 0;
	size_t start_contour = 0;
	for (int x = 0; x < 3; x++) {
		for (int y = 0; y < 3; y++) {
			FT_Outline tmp = {};

			FT_Outline_New(library, num_points, num_contours, &tmp);

			FT_Outline_Translate(&tmp,
			                     x * glyph_bounding_box_x,
			                     y * glyph_bounding_box_y);

			for (size_t i = 0; i < num_points; ++i) {
				*(grid.points + start_point + i) = *(tmp.points + i);
				*(grid.tags   + start_point + i) = *(tmp.tags + i); 
			}
			start_point += num_points;

			for (size_t i = 0; i < num_contours; ++i) {
				*(grid.contours + start_contour + i) = *(tmp.contours + i);
			}
			start_contour += num_contours;

			FT_Outline_Done(library, &tmp);
		}
	}

	FT_Bitmap bitmap = {};
	FT_Bitmap_Init(&bitmap);

	FT_Raster_Params params = {};
	params.target = &bitmap;
	params.flags  = FT_RASTER_FLAG_AA;
	// XXX callibration parameters

 	FT_Outline_Render(library, &grid, &params);

 	const auto cell = GetCell(bitmap,
                                  bitmap_left, // face->glyph->bitmap_left,
                                  bitmap_top, // face->glyph->bitmap_top,
                                  callibration.at(Category::Shade).cell_size_x,
                                  callibration.at(Category::Shade).cell_size_y);





 	// XXX

	FT_Bitmap_Done(library, &bitmap);
	FT_Outline_Done(library, &grid);

	return cell;



	return RenderCellGeneric(glyph_index,
	                         callibration.at(Category::Shade),
	                         FT_RENDER_MODE_NORMAL);
}

Cell FontWrap::RenderCell(const RenderRecipe& recipe)
{
	auto cell = Cell(pre_render_width, pre_render_height);

	const auto category = recipe.glyph_category;

	const bool is_up_down_arrow = (category == Category::SymbolUpDownArrow);
	const bool is_overlapping   = (category == Category::Ligature);
	const bool preserve_aspect  = recipe.needs_aspect_ratio_correction; // XXX naming? XXX non-symbols?

	// Call appropriate base renderer
	switch (category) {
	case Category::Space:
		break;
	case Category::Regular:
	case Category::Drawing:
	case Category::Integral:
	case Category::Shade:
		// TODO: 'Shade' category should have own rendr function, taking
		// care about antialiasing at the borders
		if (preserve_aspect) {
			cell = RenderCellPreserveAspectRatio(recipe.glyph_index,
			                                     callibration.at(category),
                                                             FT_RENDER_MODE_NORMAL);
		} else {
			cell = RenderCellGeneric(recipe.glyph_index,
	                                         callibration.at(category),
	                                         FT_RENDER_MODE_NORMAL);			
		}
		break;
	case Category::Symbol:
	case Category::SymbolUpDownArrow:
		cell = RenderCellSymbol(recipe.glyph_index,
		                        preserve_aspect,
		                        is_up_down_arrow);
		break;
	case Category::Ligature:
	case Category::LigatureNoOverlap:
		if (recipe.glyph_index_secondary) {
			cell = RenderCellLigature(recipe.glyph_index,
			                          *recipe.glyph_index_secondary,
			                          is_overlapping);
		} else {
			assert(false);
		}
		break;
	default:
		assert(false);
		break;
	}

	// Postprocess the rendered cell
	if (recipe.invert) {
		cell.Invert();		
	} else if (recipe.sharpen_all_borders) {
		cell.SharpenAllBorders();
	} else if (recipe.sharpen_only_top) {
		cell.SharpenOnlyTop();
	} else if (recipe.sharpen_only_bottom) {
		cell.SharpenOnlyBottom();
	}

	return cell;
}

FT_BBox FontWrap::GetBoundingBox(const FT_UInt glyph_index)
{
	FT_Load_Glyph(face, glyph_index, 0); 
	FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);

	FT_BBox box    = {};
	FT_Glyph glyph = {};
	FT_Get_Glyph(face->glyph, &glyph);

	// 'FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_UNSCALED, &box);' would be
	// faster, but less precise
	FT_Outline_Get_BBox(&face->glyph->outline, &box);
	FT_Done_Glyph(glyph);

	return box;
}

FT_BBox FontWrap::GetFontBoundingBox()
{
	// XXX this should only be computed when the font is being loaded
	// XXX reconsider this, maybe check also the 'shapes'
	if (callibration_indexes.drawing != 0) {
		return GetBoundingBox(callibration_indexes.drawing);
	}

	auto max = [](const FT_BBox& box1, const FT_BBox& box2) {
		FT_BBox result = {};

		result.xMin = std::min(box1.xMin, box2.xMin);
		result.xMax = std::max(box1.xMax, box2.xMax);
		result.yMin = std::min(box1.yMin, box2.yMin);
		result.yMax = std::max(box1.yMax, box2.yMax);

		return result;
	};

	// We have no good glyph for callibration - so go through all the glyphs
	// in the current code page
	// XXX also check the secondary one
	auto box = GetBoundingBox(0);
	for (size_t idx = 0; idx < recipes.size(); ++idx) {
		box = max(box, GetBoundingBox(recipes[idx].glyph_index));
	}
	/* XXX or maybe go through all the known code points?
	FT_ULong char_code   = 0;
	FT_UInt  glyph_index = 0;
	char_code = FT_Get_First_Char(face, &glyph_index);
	while (glyph_index != 0)
	{
		box = max(box, ...);

		char_code = FT_Get_Next_Char(face, char_code, &glyph_index);
	}
	*/
	// XXX or, there might be some global font metrics available - check

	return box;
}

void FontWrap::ApplyCallibration(const CallibrationData& callibration)
{
	const auto scale_x = (static_cast<float>(callibration.cell_size_x) + callibration.stretch_x) / bounding_box_x;
	const auto scale_y = (static_cast<float>(callibration.cell_size_y) + callibration.stretch_y) / bounding_box_y;

	FT_Matrix matrix = {};
	FT_Vector delta  = {};

	matrix.xx = std::lround(Identity * scale_x);
	matrix.yy = std::lround(Identity * scale_y);

	delta.x = -bounding_box.xMin + std::lround(callibration.delta_x * PointsPerPixel);
	delta.y = -bounding_box.yMin + std::lround(callibration.delta_y * PointsPerPixel);

	FT_Set_Pixel_Sizes(face, callibration.cell_size_x, callibration.cell_size_y);
	FT_Set_Transform(face, &matrix, &delta);
}

void FontWrap::ResetCallibration()
{
	FT_Matrix matrix = {};
	FT_Vector delta  = {};
	matrix.xx = Identity;
	matrix.yy = Identity;

	FT_Set_Pixel_Sizes(face, pre_render_width, pre_render_height);
	FT_Set_Transform(face, &matrix, &delta);
}

FontWrap::CallibrationData FontWrap::TweakTouchLeft(const FT_UInt glyph_index,
                                                    const FontWrap::CallibrationData& base,
                                                    const int max_steps)
{
	constexpr auto Flags = FT_RENDER_MODE_MONO;

	auto result = base;

	result.delta_x = 0.0f;

	const auto cell = RenderCellGeneric(glyph_index, result, Flags);
	if (cell.IsTouchingLeft()) {
		return result;
	}

	auto candidate = result;
	for (int idx = 1; idx <= max_steps; ++idx) {
		candidate.delta_x = -idx * CallibrationStep;
		if (RenderCellGeneric(glyph_index, candidate, Flags).IsTouchingLeft()) {
			return candidate;
		}
	}

	return result;
}

FontWrap::CallibrationData FontWrap::TweakTouchRight(const FT_UInt glyph_index,
                                                     const FontWrap::CallibrationData& base,
                                                     const int max_steps)
{
	constexpr auto Flags = FT_RENDER_MODE_MONO;

	auto result = base;

	result.stretch_x = 0.0f;

	const auto cell = RenderCellGeneric(glyph_index, result, Flags);
	if (cell.IsTouchingRight()) {
		return result;
	}

	auto candidate = result;
	for (int idx = 1; idx <= max_steps; ++idx) {
		candidate.stretch_x = idx * CallibrationStep;
		if (RenderCellGeneric(glyph_index, candidate, Flags).IsTouchingRight()) {
			return candidate;
		}
	}

	return result;
}

FontWrap::CallibrationData FontWrap::TweakTouchTop(const FT_UInt glyph_index,
                                                   const FontWrap::CallibrationData& base,
                                                   const int max_steps)
{
	constexpr auto Flags = FT_RENDER_MODE_MONO;

	auto result = base;

	result.stretch_y = 0.0f;

	const auto cell = RenderCellGeneric(glyph_index, result, Flags);
	if (cell.IsTouchingTop()) {
		return result;
	}

	auto candidate = result;
	for (int idx = 1; idx <= max_steps; ++idx) {
		candidate.stretch_y = idx * CallibrationStep;
		if (RenderCellGeneric(glyph_index, candidate, Flags).IsTouchingTop()) {
			return candidate;
		}
	}

	return result;
}

FontWrap::CallibrationData FontWrap::TweakTouchBottom(const FT_UInt glyph_index,
                                                      const FontWrap::CallibrationData& base,
                                                      const int max_steps)
{
	constexpr auto Flags = FT_RENDER_MODE_MONO;

	auto result = base;

	result.delta_y = 0.0f;

	const auto cell = RenderCellGeneric(glyph_index, result, Flags);
	if (cell.IsTouchingBottom()) {
		return result;
	}

	auto candidate = result;
	for (int idx = 1; idx <= max_steps; ++idx) {
		candidate.delta_y = -idx * CallibrationStep;
		if (RenderCellGeneric(glyph_index, candidate, Flags).IsTouchingBottom()) {
			return candidate;
		}
	}

	return result;
}

FontWrap::CallibrationData FontWrap::TweakCenter(const FT_UInt glyph_index,
                                                 const FontWrap::CallibrationData& base,
                                                 const bool is_up_down_arrow)
{
	constexpr auto RenderMode = FT_RENDER_MODE_NORMAL;

	auto callibration_tweaked = base;

	const auto cell = RenderCellGeneric(glyph_index, callibration_tweaked, RenderMode);

	const auto distance_left   = cell.GetDistanceLeft();
	const auto distance_rigth  = cell.GetDistanceRight();

	const auto distance_top    = is_up_down_arrow ? up_down_arrow_distance_top : cell.GetDistanceTop();
	const auto distance_bottom = is_up_down_arrow ? up_down_arrow_distance_bottom : cell.GetDistanceBottom();

	callibration_tweaked.delta_x -= distance_left;
	callibration_tweaked.delta_x += (distance_left + distance_rigth) / 2;

	callibration_tweaked.delta_y -= distance_bottom;
	callibration_tweaked.delta_y += (distance_top + distance_bottom) / 2;

	return callibration_tweaked;
}

FontWrap::CallibrationData FontWrap::TweakGeneric(const Category existing,
                                                  const Category fallback,
                                                  const FT_UInt glyph_index)
{
	if (glyph_index == 0) {
		return callibration[fallback];
	}

	auto result = callibration[existing];

	result = TweakTouchLeft(glyph_index, result, MaxStepDelta);
	result = TweakTouchBottom(glyph_index, result, MaxStepDelta);	
	
	const int max_stretch_x = -result.delta_x * 2 + MaxStepStretch;
	const int max_stretch_y = -result.delta_y * 2 + MaxStepStretch;

	result = TweakTouchRight(glyph_index, result, max_stretch_x);
	result = TweakTouchTop(glyph_index, result, max_stretch_y);

	return result;
}

FontWrap::CallibrationData FontWrap::TweakIntegral(const Category existing,
                                                   const Category fallback,
                                                   const FT_UInt glyph_index_top,
                                                   const FT_UInt glyph_index_bottom)
{
	if (glyph_index_top == 0 || glyph_index_bottom == 0) {
		return callibration[fallback];
	}

	auto result = callibration[existing];

	result = TweakTouchBottom(glyph_index_bottom, result, MaxStepDelta);

	const int max_stretch_y = result.delta_y * 2 + MaxStepStretch;

	result = TweakTouchTop(glyph_index_top, result, max_stretch_y);

	return result;
}

void FontWrap::CallibrateRenderer()
{
	ResetCallibration();

	// XXX calculate this once, when loading the font
	bounding_box   = GetFontBoundingBox();
	bounding_box_x = static_cast<float>(bounding_box.xMax - bounding_box.xMin) / PointsPerPixel;
	bounding_box_y = static_cast<float>(bounding_box.yMax - bounding_box.yMin) / PointsPerPixel;
	// XXX why bounding_box_y is 0?
	// XXX assertions, sanity checks

	callibration.clear();
	callibration[Category::Regular].cell_size_x = pre_render_width;
	callibration[Category::Regular].cell_size_y = pre_render_height;

	callibration[Category::Drawing]  = TweakGeneric(Category::Regular,
	                                                Category::Regular,
	                                                callibration_indexes.drawing);

	callibration[Category::Shade]    = TweakGeneric(Category::Regular,
	                                                Category::Drawing,
	                                                callibration_indexes.shade);

	callibration[Category::Integral] = TweakIntegral(Category::Drawing,
	                                                 Category::Drawing,
	                                                 callibration_indexes.integral_top,
	                                                 callibration_indexes.integral_bottom);

	callibration[Category::Symbol] = callibration[Category::Drawing];

	constexpr auto Flags = FT_RENDER_MODE_NORMAL;

	// Additional callibration of ligature rendering
	bool first = true;
	for (const auto glyph_index : callibration_indexes_ligature) {
		const auto cell = RenderCellGeneric(glyph_index,
			                            callibration[Category::Regular],
			                            Flags);

		if (first) {
			ligature_distance_left  = cell.GetDistanceLeft();
			ligature_distance_right = cell.GetDistanceRight();

			first = false;
			continue;
		}

		ligature_distance_left  = std::min(ligature_distance_left,
		                                   cell.GetDistanceLeft());
		ligature_distance_right = std::min(ligature_distance_right,
		                                   cell.GetDistanceRight());
	}

	// Additional callibration of up down arrows    XXX separate function
	const auto cell_1 = RenderCellGeneric(callibration_indexes.up_down_arrow,
		                              callibration[Category::Symbol],
		                              Flags);
	up_down_arrow_distance_top    = cell_1.GetDistanceTop();
	up_down_arrow_distance_bottom = cell_1.GetDistanceBottom();

	// Detect font aspect ratio          XXX separate function
	const auto cell_2 = RenderCellGeneric(callibration_indexes.aspect_ratio,
		                              callibration[Category::Symbol],
		                              Flags);

	// XXX check if values are sane
	font_aspect_ratio = cell_2.GetContentWidth() / cell_2.GetContentHeight();
}

void FontWrap::PreRender(const size_t width, const size_t height)
{
	if (!IsCompatible()) {
		return;
	}

	if (width == pre_render_width && height == pre_render_height &&
	    code_page == pre_render_code_page && !pre_rendered.empty()) {
	    	// Current pre-render data is still valid
		return;
	}

	// We need to pre-render font for the new size
	pre_rendered.clear();
	pre_rendered.reserve(width * height * sizeof(uint32_t));
	pre_render_width     = width;
	pre_render_height    = height;
	pre_render_code_page = code_page;

	pixel_aspect_ratio = 4.0f / 3.0f;                            // XXX get the screen proportions, refresh if changed
	pixel_aspect_ratio = (640.0f / 400.0f) / pixel_aspect_ratio; // XXX get the screen resolution, refresh if changed

	CallibrateRenderer();
	for (size_t idx = 0; idx <= UINT8_MAX; ++idx) {
		pre_rendered.push_back(RenderCell(recipes[idx]));
	}
}

void FontWrap::RenderToLine(const uint8_t character, const size_t line,
	                    const std::vector<uint8_t>::iterator &destination)
{
	// XXX these are debug background colors

	Color white(0xff, 0xff, 0xff);
	Color bg1(0xa0, 0x00, 0x00);
	Color bg2(0x00, 0x00, 0xa0);

	const bool select = (character + character / 32) % 2;

	pre_rendered.at(character).RenderLine(destination, white, select ? bg1 : bg2, line % pre_render_height);
}


// XXX code below is mostly temporary

// XXX add support for bitmap fonts





// XXX move to 'cross.cpp'
#if defined(WIN32)
static const std::string OsFontsDir = "C:\\WINDOWS\\Fonts";
#elif C_COREFOUNDATION
static const std::string OsFontsDir = "/Library/Fonts";
// XXX add "~/Library/Fonts/"
#else
static const std::string OsFontsDir = "/usr/share/fonts";
// XXX add "~/.fonts/"
#endif

// If the FreeType library is initialized and functional
static bool is_initialized = false;
// If the TTF renderer was enabled in the configuration
static bool is_ttf_enabled = false;
// Last font file read from the configuration
static std_fs::path font_file = {};

// XXX describe this
enum class SizeStrategy {
	BitmapFont,
	CrtShader,
	HighResolution,
};

static SizeStrategy size_strategy = {};


// XXX this should be a common functionality
static std_fs::path get_user_file_path(const std::string& file_name,
                                       const std::string& default_extension,                               
                                       const std::string& resource_dir,
                                       const std::vector<std::string>& other_dirs = {})
{
	if (file_name.empty()) {
		return {};
	}

	const std::string dot_extension = std::string(".") + default_extension;
	const std::string file_name_extension = file_name + dot_extension;

	const bool try_extension = !default_extension.empty() &&
	                           !file_name.ends_with(dot_extension);

	std::vector<std_fs::path> candidates = {};

	candidates.push_back(file_name);
	if (try_extension) {
		candidates.push_back(file_name_extension);
	}

	if (!resource_dir.empty()) {
		candidates.push_back(get_resource_path(resource_dir, file_name));
		if (try_extension) {
			candidates.push_back(get_resource_path(resource_dir,
			                                       file_name_extension));
		}
	}

	for (const auto& other_dir : other_dirs) {
		if (other_dir.empty()) {
			continue;
		}

		candidates.push_back(std_fs::path(other_dir) / file_name);
		if (try_extension) {
			candidates.push_back(std_fs::path(other_dir) / file_name_extension);			
		}
	}

	for (const auto& candidate : candidates) {
		if (!std_fs::exists(candidate)) {
			continue;
		}
		
		if (std_fs::is_regular_file(candidate) ||
		    std_fs::is_symlink(candidate)) {
			return candidate;
		}
	}

	return {};
}


static std::string find_default_font_file()
{
	static std::optional<std_fs::path> result = {};
	if (result) {
		return result->string();
	}

	result = get_resource_path(ResourceDir, DefaultFont);
	if (result->empty()) {
		LOG_ERR("TTF: Could not find the default font");
	}
	return result->string();
}

static std::string find_font_file(const std::string& font_name)
{
	if (font_name.empty()) {
		return {};
	}

	static const std::string DefaultFontExtension = "ttf";
	return get_user_file_path(font_name,
	                          DefaultFontExtension,
	                          ResourceDir,
	                          { OsFontsDir });
}









static bool should_takeover_rendering()
{
	if (!is_ttf_enabled || !is_initialized || !font.IsCompatible()) {
		return false;
	}

	// XXX we probably need some more generic check
	if (vga.mode != M_TEXT &&
	    vga.mode != M_TANDY_TEXT &&
	    vga.mode != M_CGA_TEXT_COMPOSITE &&
	    vga.mode != M_HERC_TEXT) {
	    	// Not a text mode
		return false;
	}

	// XXX check if enabled
	// XXX check if charset redefined
	// XXX check if custom CPI loaded
	// XXX check if current font can handle the code page

	return true;
}

void TRUETYPE_NotifyNewCodePage()
{
	// XXX do we need something else
	should_takeover_rendering();
}

bool TRUETYPE_Render()
{
	if (!should_takeover_rendering()) {
		return false;
	}

	const size_t cell_width  = 20;
	const size_t cell_height = 40;

	font.PreRender(cell_width, cell_height);

	// XXX raise SCALER_MAXWIDTH and SCALER_MAXHEIGHT?

	RENDER_StartUpdate();
	for (size_t row = 0; row < 8; ++row) {
		for (size_t y = 0; y < cell_height; ++y) {
			if (row * cell_height + y >= 400) {
				break;
			}
			std::vector<uint8_t> line = {};
			line.resize(SCALER_MAXWIDTH * sizeof(uint32_t));

			for (size_t pos = 0; pos < 1280; pos += cell_width) {
				if (pos / cell_width >= 32) {
					break;
				}

				const auto num_char = (pos / cell_width) + row * 32;
				if (num_char > UINT8_MAX) {
					break;
				}

				font.RenderToLine(num_char, y, line.begin() + pos * 4);
			}
			RENDER_DrawLine(line.data());
		}
	}

	RENDER_EndUpdate(false);

	return true;
}

void TRUETYPE_ReadConfig()
{
	if (!is_initialized) {
		return;
	}

	const auto secprop = get_sdl_section();
	assert(secprop);
	if (secprop == nullptr) {
		return;
	}

	is_ttf_enabled = true;

	// Check if TTF output is enabled and which mode is selected
	const auto text_output = secprop->Get_string("text_output");
	if (text_output == "ttf_bitmap") {
		size_strategy = SizeStrategy::BitmapFont;
	} else if (text_output == "ttf_crt") {
		size_strategy = SizeStrategy::CrtShader;
	} else if (text_output == "ttf_hires") {
		size_strategy = SizeStrategy::HighResolution;
	} else {
		is_ttf_enabled = false;
	}

	if (!is_ttf_enabled) {
		// XXX possibly release renderer
		return;
	}

	// Get the font file name and path
	const auto new_font_file = find_font_file(secprop->Get_string("text_font"));
	if (!new_font_file.empty() && font_file == new_font_file) {
		// XXX possibly take over the renderer
		return;
	}
	const auto default_font_file = find_default_font_file();

	// Load the user font
	if (!new_font_file.empty() && new_font_file != default_font_file) {
		const auto result = font.Load(new_font_file);
		if (!font.IsLoaded()) {
			// Unable to load font
			if (result == FT_Err_Unknown_File_Format) {
				// XXX warning
			} else if (result != FT_Err_Ok) {
				// XXX warning
			}			
		} else {
			// XXX possibly take over the renderer
			return;
		}
	}

	// Load default font
	if (default_font_file.empty()) {
		// XXX possibly release the renderer
		return;
	}

	font.Load(default_font_file);
	// XXX possibly take over or release the renderer
	return;
}

void TRUETYPE_Init()
{
	if (is_initialized) {
		return;
	}

	if (FT_Err_Ok != FT_Init_FreeType(&library))
	{
		LOG_ERR("GUI: Error initializing FreeType library");
	}

	is_initialized = true;
	TRUETYPE_ReadConfig();
}

void TRUETYPE_Shutdown()
{
	font.Unload();
	FT_Done_FreeType(library);
	is_initialized = false;
}

