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

#include "misc/logging.h"
#include "misc/std_filesystem.h"

// Build the next free path of the form <document_path>/<prefix><N><ext>
// (N counting from 1). Returns std::nullopt if more than 10000 files in
// the series already exist or the docpath is unusable.
std::optional<std_fs::path> find_next_name(const std::string& prefix,
                                           const std::string& ext);

#endif
