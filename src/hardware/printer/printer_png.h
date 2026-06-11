// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRINTER_PNG_H
#define DOSBOX_PRINTER_PNG_H

#include "dosbox_config.h"

#if C_PRINTER

#include "misc/std_filesystem.h"
#include "printer.h"

namespace VirtualPrinter {

// Encode the given 8-bit indexed page bitmap as a single-page PNG at
// out_path. Returns false on open / library failure (a warning is logged).
// 'page' is non-const because libpng's png_set_rows takes a non-const
// pointer; the function does not modify the page.
bool write_png_page(PageBitmap& page, const std_fs::path& out_path);

} // namespace VirtualPrinter

#endif // C_PRINTER

#endif
