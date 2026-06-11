// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "printer.h"

#if C_PRINTER

#include <string>

#include "misc/cross.h"
#include "misc/logging.h"
#include "misc/support.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace VirtualPrinter {

void Printer::FillPalette(const uint8_t red_max, const uint8_t green_max,
                          const uint8_t blue_max, uint8_t color_id)
{
	const float red   = red_max / 30.9f;
	const float green = green_max / 30.9f;
	const float blue  = blue_max / 30.9f;

	const uint8_t color_mask = color_id <<= 5;

	for (int i = 0; i < 32; i++) {
		auto& c = page.palette[i + color_mask];
		c.r = static_cast<uint8_t>(255 - (red * static_cast<float>(i)));
		c.g = static_cast<uint8_t>(255 - (green * static_cast<float>(i)));
		c.b = static_cast<uint8_t>(255 - (blue * static_cast<float>(i)));
	}
}

void Printer::UpdateFont()
{
	if (cur_font != nullptr) {
		FT_Done_Face(cur_font);
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

	Real64 horizPoints = 10.5;
	Real64 vertPoints  = 10.5;

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

		if (style.doubleheight) {
			vertPoints *= 2.0;
		}
	} else {
		// Multipoint (scalable) mode.
		act_cpi     = multi_cpi;
		horizPoints = vertPoints = multi_point_size;
	}

	if ((style.superscript) || (style.subscript)) {
		horizPoints *= 2.0 / 3.0;
		vertPoints *= 2.0 / 3.0;
		act_cpi /= 2.0 / 3.0;
	}

	FT_Set_Char_Size(cur_font,
	                 static_cast<uint16_t>(horizPoints) * 64,
	                 static_cast<uint16_t>(vertPoints) * 64,
	                 dpi,
	                 dpi);

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
				uint8_t* target = page.pixels.data() +
				                  (x + destx) +
				                  (y + desty) * page.pitch;
				source >>= 3;

				if (add) {
					if (((*target) & 0x1f) + source > 31) {
						*target |= (color | 0x1f);
					} else {
						*target += source;
						*target |= color;
					}
				} else {
					*target = source | color;
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

#endif // C_PRINTER
