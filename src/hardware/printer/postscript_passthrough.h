// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRINTER_POSTSCRIPT_PASSTHROUGH_H
#define DOSBOX_PRINTER_POSTSCRIPT_PASSTHROUGH_H

#include "dosbox_config.h"

#if C_PRINTER

#include <cstdint>

namespace VirtualPrinter {

// PostScript passthrough sink. Receives raw bytes from the DOS app
// (which is talking to a PostScript printer driver like Apple
// LaserWriter) and writes them verbatim to a .ps file. No
// interpretation, no rasterisation — PostScript is self-describing.
//
// File lifecycle:
//   - lazy-open on first byte
//   - closed on `^D` (0x04, the PostScript end-of-job marker),
//     on FormFeed (user pressed Ctrl+F2), or on Close()
//   - next byte after close starts a new file
class PostScriptPassthrough {
public:
	PostScriptPassthrough() = default;
	~PostScriptPassthrough();

	PostScriptPassthrough(const PostScriptPassthrough&)            = delete;
	PostScriptPassthrough& operator=(const PostScriptPassthrough&) = delete;

	// Append one byte to the current job. Lazy-opens the file on
	// the first byte. Closes on ^D (0x04).
	void Write(uint8_t byte);

	// Close the current job (if any). Called by FormFeed and at
	// destruction. Subsequent Write() starts a new file.
	void Close();
};

} // namespace VirtualPrinter

#endif // C_PRINTER

#endif
