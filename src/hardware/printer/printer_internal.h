// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Internal helpers shared between printer.cpp and the output / dispatch
// backends in printer_png.cpp / printer_ps.cpp / etc. Not part of the
// cross-module API exposed by printer_if.h.

#ifndef DOSBOX_PRINTER_INTERNAL_H
#define DOSBOX_PRINTER_INTERNAL_H

#include "dosbox_config.h"

#include <optional>
#include <string>
#include <string_view>

#include "misc/logging.h"
#include "misc/std_filesystem.h"

namespace VirtualPrinter {

// Build the path for the next output file in <document_path>, using
// the 4-digit zero-padded indexed scheme (see
// utils::format_indexed_filename). `basename` and `suffix` are
// passed through to the utility unchanged.
//
// Returns std::nullopt only if document_path is not a readable
// directory.
std::optional<std_fs::path> find_next_indexed_path(std::string_view basename,
                                                   std::string_view suffix);

} // namespace VirtualPrinter

#endif // DOSBOX_PRINTER_INTERNAL_H
