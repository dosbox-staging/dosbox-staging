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

// ***************************************************************************
// XXX name the section
// ***************************************************************************

enum class Category
{
	Regular,
	// GUI drawing shape or symbol
	Symbol,
	// Table or box drawing character
	Drawing,
	// Shaded full-cell blocks
	Shade,
	// Symbols for drawing integrals
	Integral,
};

// List of code points from 'Box Drawing' and 'Block Elements' Unicode blocks
// which should touch all the borders - to be used for renderer callibration,
// ordered in the order of preferrence
const std::vector<unicode_code_point> callibration_code_points_drawing = {
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

const std::vector<unicode_code_point> callibration_code_points_shade = {
	0x2593, // DARK SHADE
	0x2592, // MEDIUM SHADE
	0x2591, // LIGHT SHADE
};

// XXX comment, change casing
constexpr unicode_code_point callibration_code_point_integral_top    = 0x2321;
constexpr unicode_code_point callibration_code_point_integral_bottom = 0x2320;

// All he code points which we can render as a space
const std::set<unicode_code_point> space_code_points = {
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

const std::unordered_map<unicode_code_point, unicode_code_point> render_as_inverse = {
	{ 0x25d8, 0x2022 },
	{ 0x25d9, 0x25cb },
	{ 0x2593, 0x2591 }
};

// XXX add comments
static bool is_dosbox_private(const unicode_code_point code_point)
{
	if (code_point == 0xf20d || code_point == 0xf8ff) {
		return false;
	}

	if (code_point >= 0xe000 && code_point <= 0xf8ff) {
		return true;
	}

	return false;
}

static Category category(const unicode_code_point code_point)
{
	// U+2022 - BULLET
	if (code_point == 0x2022) { // XXX only if below DOS code point 0x20?
		return Category::Symbol;
	}

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

// XXX add comments
static bool needs_sharpening_all_borders(const unicode_code_point code_point)
{
	if (category(code_point) != Category::Drawing) {
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
static bool needs_sharpening_only_top(const unicode_code_point code_point)
{
	return (code_point == 0x2321);
}

// XXX add comments
static bool needs_sharpening_only_bottom(const unicode_code_point code_point)
{
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

	// Functions to check if the content touches the border
	bool IsTouchingLeft() const;
	bool IsTouchingRight() const;
	bool IsTouchingTop() const;
	bool IsTouchingBottom() const;

	// Functions to remove antialiasing from the given borders, to fix the
	// view when two drawing characters are placed next to each other
	void SharpenAllBorders();
	void SharpenOnlyTop();
	void SharpenOnlyBottom();

private:
	// Helper functions for deantialiasing
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
	for (size_t idx = 0; idx < size_y; ++idx) {
		if (GetPixel(0, idx) != 0) {
			return true;
		}
	}

	return false;
}

bool Cell::IsTouchingRight() const
{
	for (size_t idx = 0; idx < size_y; ++idx) {
		if (GetPixel(size_x - 1, idx) != 0) {
			return true;
		}
	}

	return false;
}

bool Cell::IsTouchingTop() const
{
	for (size_t idx = 0; idx < size_x; ++idx) {
		if (GetPixel(idx, 0) != 0) {
			return true;
		}
	}

	return false;
}

bool Cell::IsTouchingBottom() const
{
	for (size_t idx = 0; idx < size_x; ++idx) {
		if (GetPixel(idx, size_y - 1) != 0) {
			return true;
		}
	}

	return false;
}

size_t Cell::GetSharpenDepth(const size_t size) const
{
	constexpr size_t MinSizeToProcess = 8;
	constexpr float DepthProportion   = 0.2f;

	static_assert(DepthProportion * MinSizeToProcess >= 1.0f);

	if (size < MinSizeToProcess) {
		return  0;
	}

	const auto depth = std::lround(DepthProportion * size);

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

	std::optional<FT_UInt> right_glyph_index = 0; // XXX use this

	unicode_code_point code_point = 0; // XXX remove this

	bool invert = false;

	bool sharpen_all_borders = false;
	bool sharpen_only_top    = false; 
	bool sharpen_only_bottom = false;
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

	RenderRecipe GetRecipe(const unicode_code_point code_point);

	void PreRender(const size_t width, const size_t height);
	void RenderToLine(const uint8_t character, const size_t line,
	                  const std::vector<uint8_t>::iterator &destination);

	~FontWrap() { Unload(); }

private:
	struct CallibrationData {
		FT_BBox box = {};

		size_t cell_size_x = 0;
		size_t cell_size_y = 0;

		size_t delta_x = 0;
		size_t delta_y = 0;

		size_t stretch_x = 0;
		size_t stretch_y = 0;
	};

	struct CallibrationIndexes {
		FT_UInt drawing         = 0;
		FT_UInt shade           = 0;
		FT_UInt integral_top    = 0;
		FT_UInt integral_bottom = 0;
	};

	Cell GetCell(const FT_UInt glyph_index,
	             const CallibrationData& callibration,
	             const FT_Render_Mode render_mode);

	Cell GetCell(const RenderRecipe& recipe,
	             const FT_Render_Mode render_mode);

	FT_BBox GetBoundingBox(const FT_UInt glyph_index);
	FT_BBox GetBoundingBox();

	CallibrationData TweakTouchLeft(const FT_UInt glyph_index,
                                        const FontWrap::CallibrationData& base,
                                        const size_t max_steps);

	CallibrationData TweakTouchRight(const FT_UInt glyph_index,
                                         const FontWrap::CallibrationData& base,
                                         const size_t max_steps);

	CallibrationData TweakTouchTop(const FT_UInt glyph_index,
                                       const FontWrap::CallibrationData& base,
                                       const size_t max_steps);

	CallibrationData TweakTouchBottom(const FT_UInt glyph_index,
                                          const FontWrap::CallibrationData& base,
                                          const size_t max_steps);

	CallibrationData TweakGeneric(const Category existing,
	                              const Category fallback,
	                              const FT_UInt glyph_index);

	CallibrationData TweakIntegral(const Category existing,
	                               const Category fallback,
	                               const FT_UInt glyph_index_top,
	                               const FT_UInt glyph_index_bottom);

	void CallibrateRendering();

	FT_Face face = {};

	static constexpr auto Identity       = 0x10000L;
	static constexpr auto PointsPerPixel = 64;

	// If the face was loaded succesfully
	bool is_loaded = false;

	// Code page the data below is relevant for
	uint16_t code_page = 0;
	// If the font is compatible with the current code page
	bool is_compatible = false;

	CallibrationIndexes callibration_indexes = {};

	// Glyph indexes relevant to the DOS code page
	std::array<RenderRecipe, UINT8_MAX + 1> recipes = {};

	// Pre-rendered font bitmaps
	uint16_t pre_render_code_page     = 0;
	size_t   pre_render_width         = 0; // XXX get rid of this variable
	size_t   pre_render_height        = 0; // XXX get rid of this variable
	std::vector<Cell> pre_rendered    = {};

	std::unordered_map<Category, CallibrationData> callibration = {};

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

	if (!(face->face_flags & FT_FACE_FLAG_FIXED_WIDTH)) {
		// XXX better error message
		LOG_WARNING("TTF: Font is not fixed-width");
		Unload();
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

	callibration_indexes.integral_top    = FT_Get_Char_Index(face, callibration_code_point_integral_top);
	callibration_indexes.integral_bottom = FT_Get_Char_Index(face, callibration_code_point_integral_bottom);

	return true;
}

RenderRecipe FontWrap::GetRecipe(const unicode_code_point code_point)
{
	RenderRecipe recipe = {};

	recipe.glyph_index    = FT_Get_Char_Index(face, code_point);
	recipe.glyph_category = category(code_point);

	// Some characters should better be rendered as inverse of others
	if (render_as_inverse.contains(code_point)) {
		const auto inverse_code_point = render_as_inverse.at(code_point);
		const auto inverse_index      = FT_Get_Char_Index(face, inverse_code_point);

		if (inverse_index != 0) {
			recipe.glyph_index = inverse_index;
			recipe.invert      = true;
		}
	}

	// Workaround for possible fonts not having a space-like character
	// (for example U+00A0 - NO-BREAK SPACE)
	if (recipe.glyph_index == 0 && space_code_points.contains(code_point)) {
		recipe.glyph_index = FT_Get_Char_Index(face, ' ');
	}


	// XXX if recipe.glyph_index == 0 && is_ligature

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
	std::set<unicode_code_point> missing_glyphs = {};
	for (size_t idx = 0; idx < recipes.size(); ++idx) {
		const auto code_points = dos_to_unicode(format_str("%c", idx),
		    DosStringConvertMode::ScreenCodesOnly);
		// We can't display characters which use combining marks,
		// FreeType alone can't render such graphemes, we would need a
		// text shaping engine (TODO: write a simple one?).
		unicode_code_point code_point = {};
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
		if (is_dosbox_private(code_point)) {
			LOG_WARNING("TTF: Code page %d uses non-Unicode characters, "
			            "this is not supported by the current font engine",
			            code_page);
			return false;
		}

		recipes[idx] = GetRecipe(code_point);
		if (recipes[idx].glyph_index == 0) {
			missing_glyphs.insert(code_point);
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

Cell FontWrap::GetCell(const FT_UInt glyph_index,
                       const CallibrationData& callibration,
                       const FT_Render_Mode render_mode)
{
	ApplyCallibration(callibration);

	// XXX check for errors, for every function call

	FT_Load_Glyph(face, glyph_index, 0);
	FT_Render_Glyph(face->glyph, render_mode);

	const auto& input = face->glyph->bitmap.buffer; // XXX do we need input?

	const auto cell_size_x = callibration.cell_size_x;
	const auto cell_size_y = callibration.cell_size_y;

	Cell cell(cell_size_x, cell_size_y);

	// Code based on https://freetype.org/freetype2/docs/tutorial/example1.c

	const int x_min = face->glyph->bitmap_left;
	const int y_min = cell_size_y - face->glyph->bitmap_top;
	const int x_max = x_min + face->glyph->bitmap.width;
	const int y_max = y_min + face->glyph->bitmap.rows;
	
	auto get_pixel_grey = [&](const int p, const int q) -> uint8_t {
		const auto index = q * face->glyph->bitmap.pitch + p;
		
		return face->glyph->bitmap.buffer[index];
	};

	auto get_pixel_mono = [&](const int p, const int q) -> uint8_t {
		const auto index = q * face->glyph->bitmap.pitch + p / 8;

		const auto bit_num  = 7 - (p % 8);
		const auto bit_mask = (1 << bit_num);

		return (face->glyph->bitmap.buffer[index] & bit_mask) ? 0xff : 0;
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
			switch (face->glyph->bitmap.pixel_mode) {
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

Cell FontWrap::GetCell(const RenderRecipe& recipe,
	               const FT_Render_Mode render_mode)
{
	auto cell = GetCell(recipe.glyph_index,
	                    callibration.at(recipe.glyph_category),
	                    render_mode);

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
	// faster, but it is less precise
	FT_Outline_Get_BBox(&face->glyph->outline, &box);
	FT_Done_Glyph(glyph);

	return box;
}

FT_BBox FontWrap::GetBoundingBox()
{
	// XXX reconsider this, maybe check also the 'shapes'
	if (callibration_indexes.drawing != 0) {
		return GetBoundingBox(callibration_indexes.drawing);
	}

	// We have no good glyph for callibration - so go through all the glyphs
	// in the current code page
	// XXX also check the secondary one
	auto box = GetBoundingBox(0);
	for (size_t idx = 0; idx < recipes.size(); ++idx) {
		const auto current = GetBoundingBox(recipes[idx].glyph_index);

		box.xMin = std::min(box.xMin, current.xMin);
		box.xMax = std::max(box.xMax, current.xMax);
		box.yMin = std::min(box.yMin, current.yMin);
		box.yMax = std::max(box.yMax, current.yMax);
	}
	/* XXX or maybe go through all the known code points?
	FT_ULong char_code   = 0;
	FT_UInt  glyph_index = 0;
	char_code = FT_Get_First_Char(face, &glyph_index);
	while (glyph_index != 0)
	{
		const auto current = GetBoundingBox(recipes[glyph_index].main_glyph_index);

		box.xMin = std::min(box.xMin, current.xMin);
		box.xMax = std::max(box.xMax, current.xMax);
		box.yMin = std::min(box.yMin, current.yMin);
		box.yMax = std::max(box.yMax, current.yMax);

		// get min bbox
		char_code = FT_Get_Next_Char(face, char_code, &glyph_index);
	}
	*/
	// XXX or, there might be some global font metrics available - check

	return box;
}

void FontWrap::ApplyCallibration(const CallibrationData& callibration)
{
	const auto divider_x = callibration.box.xMax - callibration.box.xMin -
	                       callibration.stretch_x * PointsPerPixel / 2;
	const auto divider_y = callibration.box.yMax - callibration.box.yMin -
                               callibration.stretch_y * PointsPerPixel / 2;

	const auto scale_x = PointsPerPixel * callibration.cell_size_x /
                             static_cast<double>(divider_x);
	const auto scale_y = PointsPerPixel * callibration.cell_size_y /
	                     static_cast<double>(divider_y);

	FT_Matrix matrix = {};
	FT_Vector delta  = {};

	matrix.xx = std::lround(Identity * scale_x);
	matrix.yy = std::lround(Identity * scale_y);

	delta.x = -callibration.box.xMin - callibration.delta_x * PointsPerPixel / 2;
	delta.y = -callibration.box.yMin - callibration.delta_y * PointsPerPixel / 2;

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
                                                    const size_t max_steps)
{
	constexpr auto Flags = FT_RENDER_MODE_MONO;

	auto result = base;

	result.delta_x = 0;

	const auto cell = GetCell(glyph_index, result, Flags);
	if (cell.IsTouchingLeft()) {
		return result;
	}

	auto candidate = result;
	for (size_t idx = 1; idx <= max_steps; ++idx) {
		candidate.delta_x = idx;
		if (GetCell(glyph_index, candidate, Flags).IsTouchingLeft()) {
			return candidate;
		}
	}

	return result;
}

FontWrap::CallibrationData FontWrap::TweakTouchRight(const FT_UInt glyph_index,
                                                     const FontWrap::CallibrationData& base,
                                                     const size_t max_steps)
{
	constexpr auto Flags = FT_RENDER_MODE_MONO;

	auto result = base;

	result.stretch_x = 0;

	const auto cell = GetCell(glyph_index, result, Flags);
	if (cell.IsTouchingRight()) {
		return result;
	}

	auto candidate = result;
	for (size_t idx = 1; idx <= max_steps; ++idx) {
		candidate.stretch_x = idx;
		if (GetCell(glyph_index, candidate, Flags).IsTouchingRight()) {
			return candidate;
		}
	}

	return result;
}

FontWrap::CallibrationData FontWrap::TweakTouchTop(const FT_UInt glyph_index,
                                                   const FontWrap::CallibrationData& base,
                                                   const size_t max_steps)
{
	constexpr auto Flags = FT_RENDER_MODE_MONO;

	auto result = base;

	result.stretch_y = 0;

	const auto cell = GetCell(glyph_index, result, Flags);
	if (cell.IsTouchingTop()) {
		return result;
	}

	auto candidate = result;
	for (size_t idx = 1; idx <= max_steps; ++idx) {
		candidate.stretch_y = idx;
		if (GetCell(glyph_index, candidate, Flags).IsTouchingTop()) {
			return candidate;
		}
	}

	return result;
}

FontWrap::CallibrationData FontWrap::TweakTouchBottom(const FT_UInt glyph_index,
                                                      const FontWrap::CallibrationData& base,
                                                      const size_t max_steps)
{
	constexpr auto Flags = FT_RENDER_MODE_MONO;

	auto result = base;

	result.delta_y = 0;

	const auto cell = GetCell(glyph_index, result, Flags);
	if (cell.IsTouchingBottom()) {
		return result;
	}

	auto candidate = result;
	for (size_t idx = 1; idx <= max_steps; ++idx) {
		candidate.delta_y = idx;
		if (GetCell(glyph_index, candidate, Flags).IsTouchingBottom()) {
			return candidate;
		}
	}

	return result;
}


FontWrap::CallibrationData FontWrap::TweakGeneric(const Category existing,
                                                  const Category fallback,
                                                  const FT_UInt glyph_index)
{
	if (glyph_index == 0) {
		return callibration[fallback];
	}

	constexpr size_t MaxDelta   = 4; // XXX these should be static
	constexpr size_t MaxStretch = 4;

	auto result = callibration[existing];

	result = TweakTouchLeft(glyph_index, result, MaxDelta);
	result = TweakTouchBottom(glyph_index, result, MaxDelta);	
	
	const size_t max_stretch_x = result.delta_x * 2 + MaxStretch;
	const size_t max_stretch_y = result.delta_y * 2 + MaxStretch;

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

	constexpr size_t MaxDelta   = 4; // XXX these should be static
	constexpr size_t MaxStretch = 4;

	auto result = callibration[existing];

	result = TweakTouchBottom(glyph_index_bottom, result, MaxDelta);

	const size_t max_stretch_y = result.delta_y * 2 + MaxStretch;

	result = TweakTouchTop(glyph_index_top, result, max_stretch_y);

	return result;
}

void FontWrap::CallibrateRendering()
{
	ResetCallibration();

	callibration.clear();
	callibration[Category::Regular].box         = GetBoundingBox();
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

	// XXX center symbols horizontally/vertically, do not calibrate them
	callibration[Category::Symbol] = callibration[Category::Drawing];
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

	CallibrateRendering();
	for (size_t idx = 0; idx <= UINT8_MAX; ++idx) {
		pre_rendered.push_back(GetCell(recipes[idx], FT_RENDER_MODE_NORMAL));
	}
}

void FontWrap::RenderToLine(const uint8_t character, const size_t line,
	                    const std::vector<uint8_t>::iterator &destination)
{
	Color white(0xff, 0xff, 0xff);
	Color bg1(0xa0, 0x00, 0x00);
	Color bg2(0x00, 0x00, 0xa0);

	pre_rendered.at(character).RenderLine(destination, white, (character % 2) ? bg1 : bg2, line % pre_render_height);
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

	const size_t cell_width  = 16;
	const size_t cell_height = 32;

	font.PreRender(cell_width, cell_height);

	// XXX raise SCALER_MAXWIDTH and SCALER_MAXHEIGHT?

	RENDER_StartUpdate();
	for (size_t row = 0; row < 12; ++row) {
		for (size_t y = 0; y < cell_height; ++y) {
			std::vector<uint8_t> line = {};
			line.resize(SCALER_MAXWIDTH * sizeof(uint32_t));

			for (size_t pos = 0; pos < 640; pos += cell_width) {
				if (pos / cell_width >= 39) {
					break;
				}

				const auto num_char = (pos / cell_width) + row * 39;
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

