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

#include "checks.h"
#include "dosbox.h"
#include "dos_inc.h"
#include "logging.h"
#include "render.h"
#include "support.h"
#include "unicode.h"

#include <array>
#include <set>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BBOX_H
#include FT_GLYPH_H

CHECK_NARROWING();

// Default font file and resource directory
static const std::string DefaultFont = "Flexi_IBM_VGA_True.ttf";
static const std::string ResourceDir = "fonts-console";

// List of code points from 'Box Drawing' and 'Block Elements' Unicode blocks
// which should touch all the borders - to be used for renderer callibration,
// ordered in the order of preferrence
const std::vector<uint16_t> callibration_code_points_drawing = {
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

const std::vector<uint16_t> callibration_code_points_shapes = {
	0x25d9,
	0x25d8,
};

// All he code points which we can render as a space
const std::set<uint16_t> space_code_points = {
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

static bool is_drawing_character(const unicode_code_point code_point)
{
	// 'Box Drawing' and 'Block Elements' Unicode blocks
	return (code_point >= 0x2500 && code_point <= 0x259f);
}

static bool is_shape_character(const unicode_code_point code_point)
{
	// 'Geometric Shapes' and 'Miscellaneous Symbols' Unicode blocks
	return (code_point >= 0x25a0 && code_point <= 0x26ff);
}

// XXX add comments
static bool needs_deantialiasing_around(const unicode_code_point code_point)
{
	if (code_point == 0x25d8 || code_point == 0x25d9) {
		return true;
	}

	if (!is_drawing_character(code_point)) {
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
static bool needs_deantialiasing_top_only(const unicode_code_point code_point)
{
	return (code_point == 0x2321);
}

// XXX add comments
static bool needs_deantialiasing_bottom_only(const unicode_code_point code_point)
{
	return (code_point == 0x2320);
}


static FT_Library library = {};

class FontWrap {
public:
	bool Load(const std_fs::path &file_path);
	void Unload();

	bool IsLoaded() const { return is_loaded; }
	// Check if code page is compatible with the current code page
	bool IsCompatible();

	void PreRender(const uint16_t width, const uint16_t height);
	void RenderToLine(const uint8_t character, const uint16_t line,
	                  const std::vector<uint8_t>::iterator &destination);

	~FontWrap() { Unload(); }

private:
	struct CallibrationData {
		FT_BBox box = {};

		uint16_t cell_size_x = 0;
		uint16_t cell_size_y = 0;

		uint8_t delta_x = 0;
		uint8_t delta_y = 0;

		uint8_t stretch_x = 0;
		uint8_t stretch_y = 0;
	};

	struct CallibrationGlyphIndexes {
		FT_UInt drawing = 0;
		FT_UInt shapes  = 0;
	};

	std::vector<uint8_t> GetBitmap(const FT_UInt glyph_index,
	                               const FT_Render_Mode render_mode,
	                               const CallibrationData& callibration);
	
	FT_BBox GetBoundingBox(const FT_UInt glyph_index);
	FT_BBox GetBoundingBox();

	bool IsTouchingBorderLeft(const std::vector<uint8_t>& bitmap) const;
	bool IsTouchingBorderRight(const std::vector<uint8_t>& bitmap) const;
	bool IsTouchingBorderTop(const std::vector<uint8_t>& bitmap) const;
	bool IsTouchingBorderBottom(const std::vector<uint8_t>& bitmap) const;

	CallibrationData TweakCallibration(const CallibrationData& existing,
	                                   const FT_UInt glyph_index);
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

	CallibrationGlyphIndexes callibration_glyph_indexes = {};

	std::array<unicode_code_point, UINT8_MAX + 1> main_code_points = {};
	// Glyph indexes relevant to the DOS code page
	std::array<FT_UInt, UINT8_MAX + 1> glyph_indexes = {};

	// Pre-rendered font bitmap
	uint16_t pre_render_code_page     = 0;
	uint16_t pre_render_width         = 0;
	uint16_t pre_render_height        = 0;
	std::vector<uint8_t> pre_rendered = {};

	CallibrationData callibration_main    = {};
	CallibrationData callibration_drawing = {};
	CallibrationData callibration_shapes  = {};

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

		callibration_glyph_indexes = CallibrationGlyphIndexes();

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
		auto glyph_index = FT_Get_Char_Index(face, code_point);
		if (glyph_index != 0) {
			callibration_glyph_indexes.drawing = glyph_index;
			break;
		}
	}

	for (const auto code_point : callibration_code_points_shapes) {
		auto glyph_index = FT_Get_Char_Index(face, code_point);
		if (glyph_index != 0) {
			callibration_glyph_indexes.shapes = glyph_index;
			break;
		}
	}

	return true;
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
	for (size_t idx = 0; idx < glyph_indexes.size(); ++idx) {
		const auto code_points = dos_to_unicode(format_str("%c", idx),
		    DosStringConvertMode::ScreenCodesOnly);
		// We can't display characters which use combining marks,
		// FreeType alone can't render such graphemes, we would need a
		// text shaping engine (maybe Harfbuzz or Pango?).
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
		// XXX add code for rendering ligatures
		main_code_points[idx] = code_point;
		glyph_indexes[idx]    = FT_Get_Char_Index(face, code_point);
		if (glyph_indexes[idx] == 0 && space_code_points.contains(code_point)) {
			glyph_indexes[idx] = FT_Get_Char_Index(face, ' ');
		}
		if (glyph_indexes[idx] == 0) {
			// The font does not contain the needed glyph
			missing_glyphs.insert(code_points[0]);
		}
	}

	if (missing_glyphs.empty()) {
		is_compatible = true;
		return true;
	}

	// Report the missing glyphs in the output log
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

std::vector<uint8_t> FontWrap::GetBitmap(const FT_UInt glyph_index,
                                         const FT_Render_Mode render_mode,
                                         const CallibrationData& callibration)
{
	ApplyCallibration(callibration);

	// XXX check for errors, for every function call

	FT_Load_Glyph(face, glyph_index, 0);
	FT_Render_Glyph(face->glyph, render_mode);

	const auto& input = face->glyph->bitmap.buffer; // XXX do we need input?

	const auto cell_size_x = callibration.cell_size_x;
	const auto cell_size_y = callibration.cell_size_y;

	std::vector<uint8_t> cell(cell_size_x * cell_size_y);

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
		if (x < 0 || x >= cell_size_x) {
			continue;
		}
		int q = 0;
		for (int y = y_min; y < y_max; ++y, ++q) {
			if (y < 0 || y >= cell_size_y) {
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

			cell[x + y * cell_size_x] = pixel_value;
		}
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
	if (callibration_glyph_indexes.drawing != 0) {
		return GetBoundingBox(callibration_glyph_indexes.drawing);
	}

	// We have no good glyph for callibration - so go through all the glyphs
	// in the current code page
	auto box = GetBoundingBox(glyph_indexes[0]);
	for (uint16_t idx = 1; idx < glyph_indexes.size(); ++idx) {
		const auto current = GetBoundingBox(glyph_indexes[idx]);

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
		const auto current = GetBoundingBox(glyph_indexes[glyph_index]);

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

bool FontWrap::IsTouchingBorderLeft(const std::vector<uint8_t>& bitmap) const
{
	for (uint16_t idx = 0; idx < pre_render_height; ++idx) {
		if (bitmap[idx * pre_render_width] == UINT8_MAX) {
			return true;
		}
	}

	return false;
}

bool FontWrap::IsTouchingBorderRight(const std::vector<uint8_t>& bitmap) const
{
	for (uint16_t idx = 0; idx < pre_render_height; ++idx) {
		if (bitmap[(idx + 1) * pre_render_width - 1] == UINT8_MAX) {
			return true;
		}
	}

	return false;
}

bool FontWrap::IsTouchingBorderTop(const std::vector<uint8_t>& bitmap) const
{
	for (uint16_t idx = 0; idx < pre_render_width; ++idx) {
		if (bitmap[idx] == UINT8_MAX) {
			return true;
		}
	}

	return false;
}

bool FontWrap::IsTouchingBorderBottom(const std::vector<uint8_t>& bitmap) const
{
	const auto cell_size = pre_render_width * pre_render_height;
	for (uint16_t idx = 0; idx < pre_render_width; ++idx) {
		if (bitmap[cell_size - idx - 1] == UINT8_MAX) {
			return true;
		}
	}

	return false;
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

FontWrap::CallibrationData FontWrap::TweakCallibration(const FontWrap::CallibrationData& existing,
                                                       const FT_UInt glyph_index)
{
	if (glyph_index == 0) {
		return existing;
	}

	// XXX these values should possibly depend on target font resolution
	constexpr uint8_t MaxDelta   = 4;
	constexpr uint8_t MaxStretch = 4;

	auto result      = existing;
	auto test_bitmap = GetBitmap(glyph_index, FT_RENDER_MODE_MONO, existing);

	if (!IsTouchingBorderLeft(test_bitmap)) {
		auto candidate = result;
		for (uint8_t idx = 1; idx <= MaxDelta; ++idx) {
			candidate.delta_x = idx;
			if (IsTouchingBorderLeft(GetBitmap(glyph_index,
			                                   FT_RENDER_MODE_MONO,
			                                   candidate))) {
				result = candidate;
				break;
			}
		}
	}

	if (!IsTouchingBorderBottom(test_bitmap)) {
		auto candidate = result;
		for (uint8_t idx = 1; idx <= MaxDelta; ++idx) {
			candidate.delta_y = idx;
			if (IsTouchingBorderBottom(GetBitmap(glyph_index,
			                                     FT_RENDER_MODE_MONO,
			                                     candidate))) {
				result = candidate;
				break;
			}
		}
	}

	const uint8_t max_stretch_x = result.delta_x * 2 + MaxStretch;
	const uint8_t max_stretch_y = result.delta_y * 2 + MaxStretch;

	test_bitmap = GetBitmap(glyph_index, FT_RENDER_MODE_MONO, result);

	if (!IsTouchingBorderRight(test_bitmap)) {
		auto candidate = result;
		for (uint8_t idx = 1; idx <= max_stretch_x; ++idx) {
			candidate.stretch_x = idx;
			if (IsTouchingBorderRight(GetBitmap(glyph_index,
			                                    FT_RENDER_MODE_MONO,
			                                    candidate))) {
				result = candidate;
				break;
			}
		}
	}

	if (!IsTouchingBorderTop(test_bitmap)) {
		auto candidate = result;
		for (uint8_t idx = 1; idx <= max_stretch_y; ++idx) {
			candidate.stretch_y = idx;
			if (IsTouchingBorderTop(GetBitmap(glyph_index,
			                                  FT_RENDER_MODE_MONO,
			                                  candidate))) {
				result = candidate;
				break;
			}
		}
	}

	return result;
}

void FontWrap::CallibrateRendering()
{
	ResetCallibration();

	callibration_main = CallibrationData();
	
	callibration_main.box = GetBoundingBox();
	callibration_main.cell_size_x = pre_render_width;
	callibration_main.cell_size_y = pre_render_height;

	callibration_drawing = TweakCallibration(callibration_main,
	                                         callibration_glyph_indexes.drawing);

	if (callibration_glyph_indexes.shapes != 0) {
		callibration_shapes = TweakCallibration(callibration_main,
	                                                callibration_glyph_indexes.shapes);		
	} else {
		callibration_shapes = callibration_drawing;

	}

	// XXX add code to get rid of antialiasing on borders
	// XXX post-process 0x25d8 and 0x25d9
}

void FontWrap::PreRender(const uint16_t width, const uint16_t height)
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
		const auto code_point = main_code_points[idx];
		std::vector<uint8_t> cell = {};
		if (is_drawing_character(code_point)) {
			cell = GetBitmap(glyph_indexes[idx],
		                            FT_RENDER_MODE_NORMAL,
		                            callibration_drawing);
		} else if (is_shape_character(code_point)) {
			cell = GetBitmap(glyph_indexes[idx],
		                            FT_RENDER_MODE_NORMAL,
		                            callibration_shapes);
		} else {
			cell = GetBitmap(glyph_indexes[idx],
		                            FT_RENDER_MODE_NORMAL,
		                            callibration_main);		
		}

		pre_rendered.insert(pre_rendered.end(), cell.begin(), cell.end());
	}
}

void FontWrap::RenderToLine(const uint8_t character, const uint16_t line,
	                    const std::vector<uint8_t>::iterator &destination)
{
	// XXX add color parameters

	const auto cell_size = pre_render_width * pre_render_height;
	const auto offset = (line % pre_render_height) * pre_render_width;

	const auto source = pre_rendered.begin() + (character * cell_size) + offset;

	for (uint16_t idx = 0; idx < pre_render_width; ++idx) {
		*(destination + idx * 4 + 0) = *(source + idx);
		*(destination + idx * 4 + 1) = *(source + idx);
		*(destination + idx * 4 + 2) = *(source + idx);

		// XXX temporary debug code
		std::vector<uint8_t>::iterator i;
		if (character % 2) {
			i = destination + idx * 4 + 0;
		} else {
			i = destination + idx * 4 + 2;
		}
		*i = std::max(*i, static_cast<uint8_t>(0xa0));
	}
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

	const uint16_t cell_width  = 16;
	const uint16_t cell_height = 32;

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

