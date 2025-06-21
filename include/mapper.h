// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MAPPER_H
#define DOSBOX_MAPPER_H

#include "dosbox.h"
#include "version.h"

#include <string>
#include <vector>

#include <SDL.h>

#define MAPPERFILE "mapper-sdl2-" DOSBOX_VERSION ".map"

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

// Screen fits ~89 characters total without clipping. Allocate a few more bytes
// for good measure.
constexpr int MaxBindNameLength = 100;

/**
 * Handle a game controller being connected or disconnected by reinitializing
 * binds. Caller is responsible for any user interface updates, e.g. updating
 * the mapper UI buttons to reflect joystick binds being added / removed.
 *
 * @param event A pointer to an SDL_JoyDeviceEvent whose type must be one of:
 * - SDL_CONTROLLERDEVICEADDED
 * - SDL_CONTROLLERDEVICEREMOVED
 * - SDL_JOYDEVICEADDED
 * - SDL_JOYDEVICEREMOVED
 */
void MAPPER_HandleJoyDeviceEvent(SDL_JoyDeviceEvent* event);

#endif
