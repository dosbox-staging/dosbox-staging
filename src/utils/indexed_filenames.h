// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_INDEXED_FILENAMES_H
#define DOSBOX_INDEXED_FILENAMES_H

#include "dosbox_config.h"

#include <string>
#include <string_view>

#include "misc/std_filesystem.h"

// Format "<basename>NNNN<suffix>" where NNNN is the decimal `index`
// zero-padded to at least 4 digits. `suffix` is the rest of the
// filename after the digits (extension and any postfix between the
// digits and extension).
//
// Examples:
//   format_indexed_filename("doc",   42, ".ps")        -> "doc0042.ps"
//   format_indexed_filename("image",  1, "-raw.png")   -> "image0001-raw.png"
//   format_indexed_filename("x",  12345, ".y")         -> "x12345.y"
//
std::string format_indexed_filename(std::string_view basename, int index,
                                    std::string_view suffix);

// In `dir`, find the highest integer N such that a regular file named
// "<basename>NNNN<suffix>" exists, where NNNN is one or more decimal
// digits. Matching of `basename` and `suffix` is case-insensitive.
//
// Returns 0 if no match exists or the directory cannot be read.
//
int find_highest_file_index(const std_fs::path& dir, std::string_view basename,
                            std::string_view suffix);

#endif // DOSBOX_INDEXED_FILENAMES_H
