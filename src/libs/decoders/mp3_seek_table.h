/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2025  The DOSBox Staging Team
 *  Copyright (C) 2018-2021  kcgen <kcgen@users.noreply.github.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_MP3_SEEK_TABLE_H
#define DOSBOX_MP3_SEEK_TABLE_H

/* DOSBox MP3 Seek Table Handler
 * -----------------------------
 * See mp3_seek_table.cpp for more documentation.
 *
 * The seek table handler makes use of the following single-header
 * public libraries:
 *   - dr_mp3: http://mackron.github.io/dr_mp3.html, by David Reid
 *   - xxHash: http://cyan4973.github.io/xxHash, by Yann Collet
 */

#include "config.h"

#include <vector>    // provides: vector
#include <SDL.h>     // provides: SDL_RWops

// Ensure we only get the API
#ifdef DR_MP3_IMPLEMENTATION
#  undef DR_MP3_IMPLEMENTATION
#endif
#include "dr_mp3.h" // provides: drmp3

// Our private-decoder structure where we hold:
//   - a pointer to the working dr_mp3 instance
//   - a vector of seek_points
struct mp3_t {
    drmp3* p_dr = nullptr;    // the actual drmp3 instance we open, read, and seek within
    std::vector<drmp3_seek_point> seek_points_vector = {};
};

uint64_t populate_seek_points(mp3_t* p_mp3, bool &result);

#endif
