// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2018-2021 kcgen <kcgen@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MP3_SEEK_TABLE_H
#define DOSBOX_MP3_SEEK_TABLE_H

/* DOSBox MP3 Seek Table Handler
 * -----------------------------
 * See mp3_seek_table.cpp for more documentation.
 *
 * The seek table handler makes use of the following single-header
 * public libraries:
 *   - dr_mp3: http://mackron.github.io/dr_mp3.html, by David Reid
 */

#include "dosbox_config.h"

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
