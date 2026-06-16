// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "postscript_passthrough.h"

#include <cstdio>

#include "misc/logging.h"
#include "misc/support.h"
#include "printer_internal.h"
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
		const auto out_path = find_next_indexed_path("doc", ".ps");
		if (!out_path) {
			return;
		}
		current_file = FILE_unique_ptr{
		        fopen(out_path->string().c_str(), "wb")};

		if (!current_file) {
			LOG_ERR("PRINTER: Can't open %s for PostScript output",
			        out_path->string().c_str());
			return;
		}
		LOG_MSG("PRINTER: Print job started, writing to %s",
		        out_path->string().c_str());
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
