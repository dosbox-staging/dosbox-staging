// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/virtual_printer.h"

#include <algorithm>
#include <string>

#include "utils/rgb.h"

#include "misc/cross.h"
#include "misc/logging.h"
#include "misc/support.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace VirtualPrinter {

namespace {

// Each sub-palette is one entry per intensity level (0..MaxIntensity).
constexpr int SubPaletteSize = MaxIntensity + 1;

// 8-bit unsigned channel range.
constexpr float ChannelMax = 255.0f;

// Default font size used at power-on / after ESC @ (escp2ref.pdf C-69).
constexpr double DefaultPointSize = 10.5;

// FreeType FT_Fixed is signed 16.16; 0x10000 is the identity (1.0).
constexpr long FtFixedOne = 0x10000L;

// Italic obliqueness expressed as the FT_Matrix xy shear ratio.
constexpr double ItalicSlantRatio = 0.20;

// Conventional typographic resolution: 1 inch = 72 points.
constexpr int PointsPerInch = 72;

// Sub/superscript: 2/3 of the natural size, raised/lowered by 1/3 of
// the point size. Both factors come from the PDF baseline convention
// (cf. escp2ref.pdf C-129 leaves the offset to the printer).
constexpr double SubSuperscriptScale        = 2.0 / 3.0;
constexpr int SubSuperscriptRiseDenominator = 3;

// Anti-aliased line intensities on the page bitmap (palette index =
// max-intensity black). The centre row sits at full saturation when the
// line is solid; the row above and below paint a softer edge.
constexpr uint8_t LineCenterIntensity = 255;
constexpr uint8_t LineEdgeIntensity   = 240;

// Broken-line geometry: one gap every BrokenLinePeriod pixels, with the
// ink-on segment running for BrokenLineInkFraction of that period.
constexpr int BrokenLinePeriodPerInch  = 15;
constexpr int BrokenLineInkNumerator   = 4;
constexpr int BrokenLineInkDenominator = 5;

// FreeType returns 0..255 alpha; the page-pixel intensity field is 5
// bits (0..MaxIntensity). The 3-bit right-shift maps 256 levels to 32.
constexpr int GlyphAlphaToIntensityShift = 3;

} // namespace

void Printer::FillPalette(const uint8_t red_max, const uint8_t green_max,
                          const uint8_t blue_max, const uint8_t color_id)
{
	PagePixel pixel{};
	pixel.color_id           = color_id;
	const uint8_t color_mask = pixel.data;

	// Coverage = i/MaxIntensity is accumulated in linear light: 0 =
	// paper, 1 = fully inked. Per-channel residual linear luminance is
	// 1 - coverage * (channel_max/255). We sRGB-encode for display so
	// a 50% coverage shows as perceptually mid-grey rather than the
	// linearly-encoded L=128 which looks too dark and produces
	// visible moire on sub-pixel-aligned dither patterns.
	for (int i = 0; i < SubPaletteSize; ++i) {
		const auto coverage = static_cast<float>(i) /
		                      static_cast<float>(MaxIntensity);

		const auto r = linear_to_srgb8_lut(
		        1.0f - coverage * (red_max / ChannelMax));

		const auto g = linear_to_srgb8_lut(
		        1.0f - coverage * (green_max / ChannelMax));

		const auto b = linear_to_srgb8_lut(
		        1.0f - coverage * (blue_max / ChannelMax));

		page.palette[i + color_mask] = Rgb888{r, g, b};
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

	// Map the Epson typeface enum to a font filename. Most typefaces
	// ship two asset variants: a proportional TTF used when ESC p 1
	// is active, and a monospace TTF used when fixed-pitch (ESC p 0)
	// is active. The monospace variants are drawn to fill a 1/cpi
	// cell, which proportional TTFs can't do without leaving large
	// gaps. OCR-A/B share a single ANSI X3.17-1977 OCR-A asset (its
	// shapes are pitch-independent). Users can override any slot by
	// dropping their own TTF with the same filename into
	// <config-dir>/fonts/.
	const bool prop           = style.prop;
	const char* font_filename = prop ? "roman_prop.ttf" : "roman_fixed.ttf";

	switch (lq_typeface) {
	case Typeface::Roman:
		font_filename = prop ? "roman_prop.ttf" : "roman_fixed.ttf";
		break;
	case Typeface::SansSerif:
		font_filename = prop ? "sansserif_prop.ttf" : "sansserif_fixed.ttf";
		break;
	case Typeface::Courier: font_filename = "courier.ttf"; break;
	case Typeface::Script:
		font_filename = prop ? "script_prop.ttf" : "script_fixed.ttf";
		break;
	case Typeface::OcrA:
	case Typeface::OcrB: font_filename = "ocra.ttf"; break;
	default:
		font_filename = prop ? "roman_prop.ttf" : "roman_fixed.ttf";
		break;
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
	// box-drawing / block-element chars and for the cp437 graphic
	// codepoints (smiley, suits, arrows, ⌂ etc.) that the user-selected
	// typeface usually doesn't cover. Old Timey Mono (also our Courier
	// typeface) covers the full cp437 set, so it does double duty here.
	const auto mono_path = get_resource_path("fonts", "courier.ttf");
	if (mono_path.empty() ||
	    FT_New_Face(ft_lib, mono_path.string().c_str(), 0, &mono_box_font)) {
		mono_box_font = nullptr;
	}

	double horiz_points = DefaultPointSize;
	double vert_points  = DefaultPointSize;

	if (!multipoint) {
		actual_cpi = cpi;

		if (!(style.condensed)) {
			horiz_points *= 10.0 / cpi;
			vert_points *= 10.0 / cpi;
		}

		if (!(style.prop)) {
			if ((cpi == 10.0) && (style.condensed)) {
				actual_cpi = 17.14;
				horiz_points *= 10.0 / 17.14;
			}
			if ((cpi == 12.0) && (style.condensed)) {
				actual_cpi = 20.0;
				horiz_points *= 10.0 / 20.0;
				vert_points *= 10.0 / 12.0;
			}
		} else if (style.condensed) {
			horiz_points /= 2.0;
		}

		if ((style.doublewidth) || (style.doublewidth_oneline)) {
			actual_cpi /= 2.0;
			horiz_points *= 2.0;
		}
	} else {
		// Multipoint (scalable) mode.
		actual_cpi   = multi_cpi;
		horiz_points = vert_points = multi_point_size;
	}

	// Capture the natural-size ascender BEFORE the double-height and
	// sub/super scalings that follow. This is the per-line baseline
	// anchor, used in PrintChar so that every glyph on the line shares
	// the same baseline regardless of which size it ends up rendered at.
	// Taller glyphs (double-height) extend upward from the baseline;
	// shorter glyphs (sub/super) sit on it.
	FT_Set_Char_Size(cur_font,
	                 points_to_26_6(horiz_points),
	                 points_to_26_6(vert_points),
	                 dpi,
	                 dpi);

	line_baseline_anchor_px = ft26_6_to_pixels(cur_font->size->metrics.ascender);

	if (!multipoint && style.doubleheight) {
		vert_points *= 2.0;
	}

	if ((style.superscript) || (style.subscript)) {
		// Vertical shift = point_size / 3, converted to pixels. With
		// baseline anchoring above, the shift is the rise expressed
		// in pixels -- no per-font delta correction is needed because
		// the baseline is now constant across font-size changes.
		const auto rise_px = static_cast<int>(vert_points) * dpi /
		                     PointsPerInch / SubSuperscriptRiseDenominator;
		subscript_shift_px   = rise_px;
		superscript_shift_px = rise_px;

		horiz_points *= SubSuperscriptScale;
		vert_points *= SubSuperscriptScale;
		actual_cpi /= SubSuperscriptScale;

	} else {
		subscript_shift_px   = 0;
		superscript_shift_px = 0;
	}

	FT_Set_Char_Size(cur_font,
	                 points_to_26_6(horiz_points),
	                 points_to_26_6(vert_points),
	                 dpi,
	                 dpi);

	cur_horiz_points = horiz_points;
	cur_vert_points  = vert_points;

	// Size the mono fallback to the same dimensions so PrintChar can
	// swap to it for box-drawing chars in fixed-pitch mode without re-
	// sizing per glyph.
	if (mono_box_font != nullptr) {
		FT_Set_Char_Size(mono_box_font,
		                 points_to_26_6(horiz_points),
		                 points_to_26_6(vert_points),
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

	if (!style.prop && actual_cpi > 0.0 && FT_IS_FIXED_WIDTH(box_face)) {
		const auto natural_advance_px = ft26_6_to_pixels(
		        box_face->size->metrics.max_advance);

		const auto cell_px = static_cast<int>(dpi / actual_cpi);

		if (natural_advance_px > 0 && cell_px > natural_advance_px) {
			box_fill_horiz_points = horiz_points *
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
			                 static_cast<FT_F26Dot6>(vert_points *
			                                         Ft26Dot6Unit),
			                 dpi,
			                 dpi);

			box_fill_em_px = ft26_6_to_pixels(
			        box_face->size->metrics.max_advance);

			FT_Set_Char_Size(box_face,
			                 points_to_26_6(horiz_points),
			                 points_to_26_6(vert_points),
			                 dpi,
			                 dpi);
		}
	}

	if (style.italics || char_tables[cur_char_table] == 0) {
		FT_Matrix matrix;

		matrix.xx = FtFixedOne;
		matrix.xy = static_cast<FT_Fixed>(ItalicSlantRatio * FtFixedOne);
		matrix.yx = 0;
		matrix.yy = FtFixedOne;

		FT_Set_Transform(cur_font, &matrix, 0);
	}
}

void Printer::BlitGlyph(const FT_Bitmap bitmap, const int dest_x,
                        const int dest_y, const bool add)
{
	for (int y = 0; y < static_cast<int>(bitmap.rows); ++y) {
		for (int x = 0; x < static_cast<int>(bitmap.width); ++x) {
			// Read pixel from glyph bitmap
			uint8_t source = *(bitmap.buffer + x + y * bitmap.pitch);

			// Ignore background and don't go over the border
			if (source > 0 && (dest_x + x) < page.width &&
			    (dest_y + y) < page.height) {

				auto& pixel = *reinterpret_cast<PagePixel*>(
				        page.pixels.data() + (x + dest_x) +
				        (y + dest_y) * page.pitch);

				source >>= GlyphAlphaToIntensityShift;

				if (add) {
					const int sum = pixel.intensity + source;

					pixel.intensity = static_cast<uint8_t>(
					        std::min(MaxIntensity, sum));

					pixel.color_id = static_cast<uint8_t>(
					        pixel.color_id | color.color_id);
				} else {
					pixel.data = source | color.data;
				}
			}
		}
	}
}

void Printer::DrawLine(const int from_x, const int to_x, const int y, const bool broken)
{
	const int dash_period_px = dpi / BrokenLinePeriodPerInch;
	const int dash_ink_px    = (dash_period_px * BrokenLineInkNumerator) /
	                        BrokenLineInkDenominator;

	// Draw anti-aliased line
	for (int x = from_x; x <= to_x; ++x) {

		// Skip parts if broken line or going over the border
		if (x >= 0 && x < page.width &&
		    (!broken || (x % dash_period_px <= dash_ink_px))) {

			if (y > 0 && (y - 1) < page.height) {
				page.pixels[x + (y - 1) * page.pitch] = LineEdgeIntensity;
			}

			if (y < page.height) {
				page.pixels[x + y * page.pitch] = !broken ? LineCenterIntensity
				                                          : LineEdgeIntensity;
			}

			if ((y + 1) < page.height) {
				page.pixels[x + (y + 1) * page.pitch] = LineEdgeIntensity;
			}
		}
	}
}

} // namespace VirtualPrinter
