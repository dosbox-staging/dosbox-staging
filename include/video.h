/*
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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

#ifndef DOSBOX_VIDEO_H
#define DOSBOX_VIDEO_H

#include <string>

#include "setup.h"
#include "types.h"

#define REDUCE_JOYSTICK_POLLING

typedef enum {
	GFX_CallBackReset,
	GFX_CallBackStop,
	GFX_CallBackRedraw
} GFX_CallBackFunctions_t;

enum class IntegerScalingMode {
	Off,
	Horizontal,
	Vertical,
};

enum class InterpolationMode { Bilinear, NearestNeighbour };

typedef void (*GFX_CallBack_t)(GFX_CallBackFunctions_t function);

constexpr uint8_t GFX_CAN_8      = 1 << 0;
constexpr uint8_t GFX_CAN_15     = 1 << 1;
constexpr uint8_t GFX_CAN_16     = 1 << 2;
constexpr uint8_t GFX_CAN_32     = 1 << 3;
constexpr uint8_t GFX_DBL_H      = 1 << 4; // double-width  flag
constexpr uint8_t GFX_DBL_W      = 1 << 5; // double-height flag
constexpr uint8_t GFX_CAN_RANDOM = 1 << 6; // interface can also do random acces

// return code of:
// - true means event loop can keep running.
// - false means event loop wants to quit.
bool GFX_Events();

// Let the presentation layer safely call no-op functions.
// Useful during output initialization or transitions.
void GFX_DisengageRendering();

Bitu GFX_GetBestMode(Bitu flags);
Bitu GFX_GetRGB(uint8_t red,uint8_t green,uint8_t blue);
void GFX_SetShader(const std::string &source);
void GFX_SetIntegerScalingMode(const std::string& new_mode);
IntegerScalingMode GFX_GetIntegerScalingMode();

struct VideoMode;

Bitu GFX_SetSize(const int width, const int height, const Bitu flags,
                 const double scalex, const double scaley,
                 const VideoMode& video_mode, GFX_CallBack_t callback);

void GFX_ResetScreen(void);
void GFX_RequestExit(const bool requested);
void GFX_Start(void);
void GFX_Stop(void);
void GFX_SwitchFullScreen(void);
bool GFX_StartUpdate(uint8_t * &pixels, int &pitch);
void GFX_EndUpdate( const uint16_t *changedLines );
void GFX_GetSize(int &width, int &height, bool &fullscreen);
void GFX_LosingFocus();
void GFX_RegenerateWindow(Section *sec);

enum class MouseHint {
    None,                    // no hint to display
    NoMouse,                 // no mouse mode
    CapturedHotkey,          // mouse captured, use hotkey to release
    CapturedHotkeyMiddle,    // mouse captured, use hotkey or middle-click to release
    ReleasedHotkey,          // mouse released, use hotkey to capture
    ReleasedHotkeyMiddle,    // mouse released, use hotkey or middle-click to capture
    ReleasedHotkeyAnyButton, // mouse released, use hotkey or any click to capture
    SeamlessHotkey,          // seamless mouse, use hotkey to capture
    SeamlessHotkeyMiddle,    // seamless mouse, use hotkey or middle-click to capture
};

void GFX_CenterMouse();
void GFX_SetMouseHint(const MouseHint requested_hint_id);
void GFX_SetMouseCapture(const bool requested_capture);
void GFX_SetMouseVisibility(const bool requested_visible);
void GFX_SetMouseRawInput(const bool requested_raw_input);

// Detects the presence of a desktop environment or window manager
bool GFX_HaveDesktopEnvironment();

#if defined(REDUCE_JOYSTICK_POLLING)
void MAPPER_UpdateJoysticks(void);
#endif

#endif
