// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "raw_passthrough.h"

#include <cstdio>

#include "misc/logging.h"
#include "misc/support.h"
#include "printer_internal.h"
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
		const auto out_path = find_next_indexed_path("doc", ".prn");
		if (!out_path) {
			return;
		}
		current_file = FILE_unique_ptr{
		        fopen(out_path->string().c_str(), "wb")};

		if (!current_file) {
			LOG_ERR("PRINTER: Can't open %s for raw output",
			        out_path->string().c_str());
			return;
		}
		LOG_MSG("PRINTER: Print job started, writing to %s",
		        out_path->string().c_str());
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
