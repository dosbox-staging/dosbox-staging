// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/raw_passthrough.h"

#include <cstdio>

#include "capture/capture.h"
#include "misc/logging.h"
#include "misc/support.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace VirtualPrinter {

RawPassthrough::~RawPassthrough()
{
	Close();
}

void RawPassthrough::Write(const uint8_t byte)
{
	if (!current_file) {
		current_file = FILE_unique_ptr{
		        CAPTURE_CreateFile(CaptureType::PrinterRaw)};
		if (!current_file) {
			return;
		}
	}

	fputc(byte, current_file.get());
}

void RawPassthrough::Close()
{
	if (current_file) {
		LOG_MSG("PRINTER: Raw passthrough job closed");
		current_file.reset();
	}
}

} // namespace VirtualPrinter
