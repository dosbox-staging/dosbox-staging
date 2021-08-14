/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_MAPPER_H
#define DOSBOX_MAPPER_H

#include "dosbox.h"

#include <string>
#include <vector>

#include <SDL.h>

constexpr uint32_t MMOD1 = 0x1;
#define MMOD1_NAME "Ctrl"
constexpr uint32_t MMOD2 = 0x2;
constexpr uint32_t MMOD3 = 0x4;

// Linux, Windows, BSD, etc.
#if !defined(MACOSX)
#define MMOD2_NAME       "Alt"
#define MMOD3_NAME       "GUI"
#define PRIMARY_MOD      MMOD1
#define PRIMARY_MOD_PAD  ""
#define PRIMARY_MOD_NAME MMOD1_NAME
// macOS
#else
#define MMOD2_NAME       "Opt"
#define MMOD3_NAME       "Cmd"
#define PRIMARY_MOD      MMOD3
#define PRIMARY_MOD_PAD  " "
#define PRIMARY_MOD_NAME MMOD3_NAME
#endif

typedef void(MAPPER_Handler)(bool pressed);

/* Associate function handler with a key combination
 *
 * handler     - function to be triggered
 * key         - SDL scancode for key triggering the event.
 *               Use SDL_SCANCODE_UNKNOWN to skip adding default key binding.
 * mods        - key modifier events used for this action (according to user
 *               preferences); a bitmask of MMOD1, MMOD2, MMOD3 values.
 * event_name  - event name used for serialization in mapper file
 * button_name - descriptive event name visible in the mapper GUI"
 */
void MAPPER_AddHandler(MAPPER_Handler *handler,
                       SDL_Scancode key,
                       uint32_t mods,
                       const char *event_name,
                       const char *button_name);

void MAPPER_BindKeys(Section *sec);
void MAPPER_StartUp(Section *sec);
void MAPPER_Run(bool pressed);
void MAPPER_DisplayUI();
void MAPPER_LosingFocus();
bool MAPPER_IsUsingJoysticks();
std::vector<std::string> MAPPER_GetEventNames(const std::string &prefix);
void MAPPER_AutoType(std::vector<std::string> &sequence,
                     const uint32_t wait_ms,
                     const uint32_t pacing_ms);
void MAPPER_CheckEvent(SDL_Event *event);

#endif
