// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "postscript_passthrough.h"

#include <cstdio>

#include "capture/capture.h"
#include "misc/logging.h"
#include "misc/support.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace VirtualPrinter {

namespace {

constexpr uint8_t EotMarker = 0x04;

// Single file handle that survives across Write() calls until ^D or
// explicit Close().
FILE_unique_ptr current_file = {};

} // namespace

PostScriptPassthrough::~PostScriptPassthrough()
{
	Close();
}

void PostScriptPassthrough::Write(const uint8_t byte)
{
	if (!current_file) {
		current_file = FILE_unique_ptr{
		        CAPTURE_CreateFile(CaptureType::PrinterPostScript)};
		if (!current_file) {
			return;
		}
	}

	// `^D` (EOT) is the PostScript end-of-job marker. Strip it
	// and close the file — the next byte starts a new job.
	if (byte == EotMarker) {
		Close();
		return;
	}

	fputc(byte, current_file.get());
}

void PostScriptPassthrough::Close()
{
	if (current_file) {
		LOG_MSG("PRINTER: PostScript job closed");
		current_file.reset();
	}
}

} // namespace VirtualPrinter
