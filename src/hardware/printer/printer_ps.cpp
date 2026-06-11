// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "printer.h"

#if C_PRINTER

#include <cstdio>
#include <optional>
#include <utility>

#include "misc/std_filesystem.h"
#include "utils/checks.h"

#include "printer_internal.h"

CHECK_NARROWING();

namespace VirtualPrinter {

// PostScript output state machine
// -------------------------------
// Each PRINTER_WriteControl strobe that completes a page triggers an
// OutputPage call which lands here.
//
// Single-page mode (multipage_output = false): every page opens a new
// .ps file, writes header + showpage + EOF, and closes it. output_handle
// stays null between calls.
//
// Multi-page mode  (multipage_output = true): the first page opens a
// new .ps file and writes the header; subsequent pages reuse the same
// FILE handle, parked in the output_handle member. The user presses
// Ctrl+F2 (form-feed) -> FinishMultipage runs, writes the EOF marker,
// and closes the handle.
//
// FILE_unique_ptr move semantics: the file handle migrates between the
// local psfile and the member output_handle on each call so the
// destructor closes it automatically on any unexpected return path.
void Printer::OutputPagePostScript()
{
	// If multipage mode is continuing, take ownership back from the
	// member; otherwise open a fresh file.
	FILE_unique_ptr psfile = std::move(output_handle);

	if (!psfile) {
		const auto out_path = find_next_name(multipage_output ? "doc" : "page",
		                                     ".ps");
		if (!out_path) {
			return;
		}

		psfile = FILE_unique_ptr{fopen(out_path->string().c_str(), "wb")};
		if (!psfile) {
			LOG_ERR("PRINTER: Can't open file %s for printer output",
			        out_path->string().c_str());
			return;
		}

		// Print header.
		fprintf(psfile.get(), "%%!PS-Adobe-3.0\n");
		fprintf(psfile.get(), "%%%%Pages: (atend)\n");
		fprintf(psfile.get(),
		        "%%%%BoundingBox: 0 0 %i %i\n",
		        static_cast<uint16_t>(default_page_width * 72),
		        static_cast<uint16_t>(default_page_height * 72));
		fprintf(psfile.get(), "%%%%Creator: DOSBOX Virtual Printer\n");
		fprintf(psfile.get(), "%%%%DocumentData: Clean7Bit\n");
		fprintf(psfile.get(), "%%%%LanguageLevel: 2\n");
		fprintf(psfile.get(), "%%%%EndComments\n");
		multipage_counter = 1;
	}

	fprintf(psfile.get(), "%%%%Page: %i %i\n", multipage_counter, multipage_counter);
	fprintf(psfile.get(),
	        "%i %i scale\n",
	        static_cast<uint16_t>(default_page_width * 72),
	        static_cast<uint16_t>(default_page_height * 72));
	fprintf(psfile.get(),
	        "%i %i 8 [%i 0 0 -%i 0 %i]\n",
	        page->w,
	        page->h,
	        page->w,
	        page->h,
	        page->h);
	fprintf(psfile.get(), "currentfile\n");
	fprintf(psfile.get(), "/ASCII85Decode filter\n");
	fprintf(psfile.get(), "/RunLengthDecode filter\n");
	fprintf(psfile.get(), "image\n");

	SDL_LockSurface(page);

	uint32_t pix          = 0;
	const uint32_t numpix = page->h * page->w;
	ascii85_buffer_pos = ascii85_cur_col = 0;

	while (pix < numpix) {
		// Compress data using RLE.
		if ((pix < numpix - 2) && (GetPixel(pix) == GetPixel(pix + 1)) &&
		    (GetPixel(pix) == GetPixel(pix + 2))) {
			// Three or more pixels with the same colour: RLE run.
			uint8_t sameCount = 3;
			const uint8_t col = GetPixel(pix);
			while (sameCount < 128 && sameCount + pix < numpix &&
			       col == GetPixel(pix + sameCount)) {
				sameCount++;
			}

			FprintAscii85(psfile.get(), 257 - sameCount);
			FprintAscii85(psfile.get(), 255 - col);

			pix += sameCount;
		} else {
			// Find end of heterogeneous area.
			uint8_t diffCount = 1;
			while (diffCount < 128 && diffCount + pix < numpix &&
			       ((diffCount + pix < numpix - 2) ||
			        (GetPixel(pix + diffCount) !=
			         GetPixel(pix + diffCount + 1)) ||
			        (GetPixel(pix + diffCount) !=
			         GetPixel(pix + diffCount + 2)))) {
				diffCount++;
			}

			FprintAscii85(psfile.get(), diffCount - 1);
			for (uint8_t i = 0; i < diffCount; i++) {
				FprintAscii85(psfile.get(), 255 - GetPixel(pix++));
			}
		}
	}

	// Write end-of-data marker for the RLE + ASCII85 filters.
	FprintAscii85(psfile.get(), 128);
	FprintAscii85(psfile.get(), 256);

	SDL_UnlockSurface(page);

	fprintf(psfile.get(), "showpage\n");

	if (multipage_output) {
		multipage_counter++;
		output_handle = std::move(psfile);
	} else {
		fprintf(psfile.get(), "%%%%Pages: 1\n");
		fprintf(psfile.get(), "%%%%EOF\n");
		// psfile closes here via FILE_unique_ptr destructor.
		output_handle.reset();
	}
}

// ASCII85 encoder
// ---------------
// Encodes binary bytes as printable ASCII for embedding in a PostScript
// stream. Four input bytes are packed into a 32-bit value and emitted
// as five base-85 digits (offset by '!' = 33). Special cases:
//
//   byte = 0..255  -> regular input byte. Buffer fills to 4, then flush.
//   byte = 256     -> sentinel: end of stream. Flush partial tuple and
//                     emit the '~>' terminator.
//   byte = 257     -> sentinel: flush partial tuple now (used by the
//                     stream-end path; doesn't itself produce input data).
//
// Lines are wrapped at 79 columns. An all-zero tuple is emitted as the
// single character 'z' (PS-standard shorthand). The encoder also avoids
// starting a line with '%', which PostScript treats as the start of a
// comment.
void Printer::FprintAscii85(FILE* file, uint16_t byte)
{
	if (byte != 256) {
		if (byte < 256) {
			ascii85_buffer[ascii85_buffer_pos++] = static_cast<uint8_t>(
			        byte);
		}

		if (ascii85_buffer_pos == 4 || byte == 257) {
			uint32_t num = static_cast<uint32_t>(ascii85_buffer[0])
			                    << 24 |
			               static_cast<uint32_t>(ascii85_buffer[1])
			                       << 16 |
			               static_cast<uint32_t>(ascii85_buffer[2]) << 8 |
			               static_cast<uint32_t>(ascii85_buffer[3]);

			// Deal with special case.
			if (num == 0 && byte != 257) {
				fprintf(file, "z");
				if (++ascii85_cur_col >= 79) {
					ascii85_cur_col = 0;
					fprintf(file, "\n");
				}
			} else {
				char buffer[5];
				for (int8_t i = 4; i >= 0; i--) {
					buffer[i] = static_cast<uint8_t>(
					        static_cast<uint32_t>(num) %
					        static_cast<uint32_t>(85));
					buffer[i] += 33;
					num /= static_cast<uint32_t>(85);
				}

				// Make sure a line never starts with a % (which
				// may be mistaken as start of a comment).
				if (ascii85_cur_col == 0 && buffer[0] == '%') {
					fprintf(file, " ");
				}

				for (int i = 0;
				     i < ((byte != 257) ? 5 : ascii85_buffer_pos + 1);
				     i++) {
					fprintf(file, "%c", buffer[i]);
					if (++ascii85_cur_col >= 79) {
						ascii85_cur_col = 0;
						fprintf(file, "\n");
					}
				}
			}

			ascii85_buffer_pos = 0;
		}

	} else {
		// Close ASCII85 string. Pad partial tuple with zero bytes and
		// emit '~>' end-of-data marker.
		if (ascii85_buffer_pos > 0) {
			for (uint8_t i = ascii85_buffer_pos; i < 4; i++) {
				ascii85_buffer[i] = 0;
			}

			FprintAscii85(file, 257);
		}

		fprintf(file, "~");
		fprintf(file, ">\n");
	}
}

} // namespace VirtualPrinter

#endif // C_PRINTER
