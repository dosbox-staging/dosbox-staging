// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRINTER_PNG_H
#define DOSBOX_PRINTER_PNG_H

#include "dosbox_config.h"

#if C_PRINTER

#include "SDL.h"

#include "misc/std_filesystem.h"

// Encode 'page' (an 8-bit indexed SDL surface) as a single-page PNG at
// out_path. Returns false on open / library failure (a warning is logged).
bool write_png_page(SDL_Surface* page, const std_fs::path& out_path);

#endif // C_PRINTER

#endif
