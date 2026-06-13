// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "printer.h"

#include <algorithm>
#include <string>

#include "utils/rgb.h"

#include "misc/cross.h"
#include "misc/logging.h"
#include "misc/support.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace VirtualPrinter {

void Printer::FillPalette(const uint8_t red_max, const uint8_t green_max,
                          const uint8_t blue_max, const uint8_t color_id)
{
	PagePixel pixel{};
	pixel.color_id           = color_id;
	const uint8_t color_mask = pixel.data;

	// Coverage = i/31 is accumulated in linear light: 0 = paper, 1 =
	// fully inked. Per-channel residual linear luminance is 1 -
	// coverage * (channel_max/255). We sRGB-encode for display so a
	// 50% coverage shows as perceptually mid-grey rather than the
	// linearly-encoded L=128 which looks too dark and produces
	// visible moire on sub-pixel-aligned dither patterns.
	for (int i = 0; i < 32; ++i) {
		auto& c = page.palette[i + color_mask];
		const float coverage = static_cast<float>(i) / 31.0f;
		c.r = linear_to_srgb8_lut(1.0f - coverage * (red_max / 255.0f));
		c.g = linear_to_srgb8_lut(1.0f - coverage * (green_max / 255.0f));
		c.b = linear_to_srgb8_lut(1.0f - coverage * (blue_max / 255.0f));
	}
}

void Printer::UpdateFont()
{
	if (cur_font != nullptr) {
		FT_Done_Face(cur_font);
	}
	if (mono_box_font != nullptr) {
		FT_Done_Face(mono_box_font);
		mono_box_font = nullptr;
	}

	// Map the Epson typeface enum to a font filename. We ship
	// Liberation Serif / Sans / Mono renamed to these slot names
	// in resources/fonts/. Users can override by dropping their
	// own TTF with the same filename into <config-dir>/fonts/.
	const char* font_filename = "roman.ttf";

	switch (lq_typeface) {
	case Typeface::Roman: font_filename = "roman.ttf"; break;
	case Typeface::SansSerif: font_filename = "sansserif.ttf"; break;
	case Typeface::Courier: font_filename = "courier.ttf"; break;
	case Typeface::Script: font_filename = "script.ttf"; break;
	case Typeface::OcrA:
	case Typeface::OcrB: font_filename = "ocra.ttf"; break;
	default: font_filename = "roman.ttf"; break;
	}

	// get_resource_path walks the standard hierarchy (bundled
	// resources first, the user's config dir last as an override).
	const auto font_path = get_resource_path("fonts", font_filename);

	if (font_path.empty() ||
	    FT_New_Face(ft_lib, font_path.string().c_str(), 0, &cur_font)) {
		LOG_WARNING("PRINTER: Unable to load font %s", font_filename);
		cur_font = nullptr;
	}

	// Always load the monospace fallback. PrintChar swaps to it for
	// box-drawing / block-element chars when cur_font isn't itself
	// monospace and we're in fixed-pitch mode -- proportional fonts
	// (Roman / Sans Serif / Script) have variable advance widths even
	// for box chars and so misalign in a grid.
	const auto mono_path = get_resource_path("fonts", "courier.ttf");
	if (mono_path.empty() ||
	    FT_New_Face(ft_lib, mono_path.string().c_str(), 0, &mono_box_font)) {
		mono_box_font = nullptr;
	}

	double horizPoints = 10.5;
	double vertPoints  = 10.5;

	if (!multipoint) {
		act_cpi = cpi;

		if (!(style.condensed)) {
			horizPoints *= 10.0 / cpi;
			vertPoints *= 10.0 / cpi;
		}

		if (!(style.prop)) {
			if ((cpi == 10.0) && (style.condensed)) {
				act_cpi = 17.14;
				horizPoints *= 10.0 / 17.14;
			}
			if ((cpi == 12.0) && (style.condensed)) {
				act_cpi = 20.0;
				horizPoints *= 10.0 / 20.0;
				vertPoints *= 10.0 / 12.0;
			}
		} else if (style.condensed) {
			horizPoints /= 2.0;
		}

		if ((style.doublewidth) || (style.doublewidth_oneline)) {
			act_cpi /= 2.0;
			horizPoints *= 2.0;
		}
	} else {
		// Multipoint (scalable) mode.
		act_cpi     = multi_cpi;
		horizPoints = vertPoints = multi_point_size;
	}

	// Capture the natural-size ascender BEFORE the double-height and
	// sub/super scalings that follow. This is the per-line baseline
	// anchor, used in PrintChar so that every glyph on the line shares
	// the same baseline regardless of which size it ends up rendered at.
	// Taller glyphs (double-height) extend upward from the baseline;
	// shorter glyphs (sub/super) sit on it.
	FT_Set_Char_Size(cur_font,
	                 points_to_26_6(horizPoints),
	                 points_to_26_6(vertPoints),
	                 dpi,
	                 dpi);
	line_baseline_anchor_px = ft26_6_to_pixels(cur_font->size->metrics.ascender);

	if (!multipoint && style.doubleheight) {
		vertPoints *= 2.0;
	}

	if ((style.superscript) || (style.subscript)) {
		// Vertical shift = point_size / 3 (PDF baseline units),
		// converted to pixels. With baseline anchoring above, the shift
		// is the rise expressed in pixels -- no per-font delta
		// correction is needed because the baseline is now constant
		// across font-size changes.
		const auto rise_px = static_cast<int>(vertPoints) * dpi / 72 / 3;
		subscript_shift_px   = rise_px;
		superscript_shift_px = rise_px;

		horizPoints *= 2.0 / 3.0;
		vertPoints *= 2.0 / 3.0;
		act_cpi /= 2.0 / 3.0;

	} else {
		subscript_shift_px   = 0;
		superscript_shift_px = 0;
	}

	FT_Set_Char_Size(cur_font,
	                 points_to_26_6(horizPoints),
	                 points_to_26_6(vertPoints),
	                 dpi,
	                 dpi);

	cur_horiz_points = horizPoints;
	cur_vert_points  = vertPoints;

	// Size the mono fallback to the same dimensions so PrintChar can
	// swap to it for box-drawing chars in fixed-pitch mode without re-
	// sizing per glyph.
	if (mono_box_font != nullptr) {
		FT_Set_Char_Size(mono_box_font,
		                 points_to_26_6(horizPoints),
		                 points_to_26_6(vertPoints),
		                 dpi,
		                 dpi);
	}

	// Pick the face that PrintChar will actually render box chars on:
	// the mono fallback in fixed-pitch mode (so proportional-typeface
	// docs still get cell-aligned box-drawing chars, matching real
	// Epson which used a typeface-agnostic box-drawing bitmap set);
	// otherwise the active typeface itself.
	FT_Face box_face = (!style.prop && mono_box_font != nullptr) ? mono_box_font
	                                                             : cur_font;
	natural_em_height_px = ft26_6_to_pixels(box_face->size->metrics.height);

	// Compute the horizontal point size that stretches box_face's em
	// to exactly fill the 1/cpi cell; the bitmap overhang baked into
	// each box-drawing glyph then bridges the cell seam. Engages only
	// when box_face is monospace and its natural advance is narrower
	// than the cell at the configured cpi. PrintChar does the matching
	// vertical stretch lazily because it depends on line_spacing.
	box_fill_horiz_points = 0.0;
	box_fill_em_px        = 0;

	if (!style.prop && act_cpi > 0.0 && FT_IS_FIXED_WIDTH(box_face)) {
		const auto natural_advance_px = ft26_6_to_pixels(
		        box_face->size->metrics.max_advance);

		const auto cell_px = static_cast<int>(dpi / act_cpi);

		if (natural_advance_px > 0 && cell_px > natural_advance_px) {
			box_fill_horiz_points = horizPoints *
			                        BoxFillOvershootHorizontal *
			                        static_cast<double>(cell_px) /
			                        static_cast<double>(natural_advance_px);

			// Read em-width at the stretched size; restore the
			// natural-size font afterwards. Use the raw cast (no
			// truncation) so this em-width matches what PrintChar
			// will compute when it sets the same fractional point
			// size for the actual render.
			FT_Set_Char_Size(box_face,
			                 static_cast<FT_F26Dot6>(box_fill_horiz_points *
			                                         Ft26Dot6Unit),
			                 static_cast<FT_F26Dot6>(vertPoints *
			                                         Ft26Dot6Unit),
			                 dpi,
			                 dpi);

			box_fill_em_px = ft26_6_to_pixels(
			        box_face->size->metrics.max_advance);

			FT_Set_Char_Size(box_face,
			                 points_to_26_6(horizPoints),
			                 points_to_26_6(vertPoints),
			                 dpi,
			                 dpi);
		}
	}

	if (style.italics || char_tables[cur_char_table] == 0) {
		FT_Matrix matrix;

		matrix.xx = 0x10000L;
		matrix.xy = (FT_Fixed)(0.20 * 0x10000L);
		matrix.yx = 0;
		matrix.yy = 0x10000L;

		FT_Set_Transform(cur_font, &matrix, 0);
	}
}

void Printer::BlitGlyph(const FT_Bitmap bitmap, const uint16_t destx,
                        const uint16_t desty, const bool add)
{
	for (uint64_t y = 0; y < bitmap.rows; y++) {
		for (uint64_t x = 0; x < bitmap.width; x++) {
			// Read pixel from glyph bitmap.
			uint8_t source = *(bitmap.buffer + x + y * bitmap.pitch);

			// Ignore background and don't go over the border.
			if (source > 0 && (static_cast<int>(destx + x) < page.width) &&
			    (static_cast<int>(desty + y) < page.height)) {
				auto& pixel = *reinterpret_cast<PagePixel*>(
				        page.pixels.data() + (x + destx) +
				        (y + desty) * page.pitch);
				source >>= 3;

				if (add) {
					const int sum = pixel.intensity + source;
					pixel.intensity = static_cast<uint8_t>(
					        std::min(MaxIntensity, sum));
					pixel.color_id = static_cast<uint8_t>(
					        pixel.color_id | (color >> 5));
				} else {
					pixel.data = source | color;
				}
			}
		}
	}
}

void Printer::DrawLine(const uint64_t fromx, const uint64_t tox,
                       const uint64_t y, const bool broken)
{
	const uint64_t breakmod = dpi / 15;
	const uint64_t gapstart = (breakmod * 4) / 5;

	// Draw anti-aliased line.
	for (uint64_t x = fromx; x <= tox; x++) {
		// Skip parts if broken line or going over the border.
		if ((!broken || (x % breakmod <= gapstart)) &&
		    (static_cast<int>(x) < page.width)) {
			if (y > 0 && static_cast<int>(y - 1) < page.height) {
				page.pixels[x + (y - 1) * page.pitch] = 240;
			}
			if (static_cast<int>(y) < page.height) {
				page.pixels[x + y * page.pitch] = !broken ? 255 : 240;
			}
			if (static_cast<int>(y + 1) < page.height) {
				page.pixels[x + (y + 1) * page.pitch] = 240;
			}
		}
	}
}

} // namespace VirtualPrinter
