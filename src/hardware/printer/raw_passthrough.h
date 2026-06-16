// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRINTER_RAW_PASSTHROUGH_H
#define DOSBOX_PRINTER_RAW_PASSTHROUGH_H

#include "dosbox_config.h"

#include <cstdint>

#include "misc/support.h"

namespace VirtualPrinter {

// Raw byte-stream sink for non-PostScript printer languages (PCL,
// HP-GL, Canon BJL, ESC/P captured for offline processing, etc.).
// Bytes go to the .prn file verbatim with no interpretation or
// filtering — splitting on `0x0C` would corrupt PCL / ESC/P
// captures because length-prefixed bit-image commands can carry
// `0x0C` as raster data.
//
// File lifecycle:
//   - lazy-open on first byte
//   - closed by Close() (idle timeout / Ctrl+F2 eject) or destruction
//   - next byte after close starts a new file
class RawPassthrough {
public:
	RawPassthrough() = default;
	~RawPassthrough();

	RawPassthrough(const RawPassthrough&)            = delete;
	RawPassthrough& operator=(const RawPassthrough&) = delete;

	// Append one byte to the current job. Lazy-opens the file on
	// the first byte.
	void Write(uint8_t byte);

	// Close the current job (if any). Subsequent Write() starts a
	// new file.
	void Close();

private:
	FILE_unique_ptr current_file = {};
};

} // namespace VirtualPrinter

#endif
