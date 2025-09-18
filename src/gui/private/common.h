// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_GUI_PRIVATE_COMMON_H
#define DOSBOX_GUI_PRIVATE_COMMON_H

#include "SDL.h"

#include "hardware/video/video.h"
#include "utils/fraction.h"
#include "utils/rect.h"

constexpr uint8_t GFX_CAN_8      = 1 << 0;
constexpr uint8_t GFX_CAN_15     = 1 << 1;
constexpr uint8_t GFX_CAN_16     = 1 << 2;
constexpr uint8_t GFX_CAN_32     = 1 << 3;
constexpr uint8_t GFX_DBL_H      = 1 << 4; // double-width  flag
constexpr uint8_t GFX_DBL_W      = 1 << 5; // double-height flag
constexpr uint8_t GFX_CAN_RANDOM = 1 << 6; // interface can also do random acces

typedef enum {
	GFX_CallbackReset,
	GFX_CallbackStop,
	GFX_CallbackRedraw
} GFX_CallbackFunctions_t;

typedef void (*GFX_Callback_t)(GFX_CallbackFunctions_t function);

// TODO We should improve this interface over time (e.g., remove the GFX_
// prefix, move the functions to more appropriate places, like the renderer,
// etc.)

enum class PresentationMode {
	// In DOS rate presentation mode, the video frames are presented at the
	// emulated DOS refresh rate, irrespective of the host operating system's
	// display refresh rate (e.g., ~70 Hz for the common 320x200 VGA mode). In
	// other words, the DOS rate and only that determines the presentation
	// rate.
	//
	// The best use-case for presenting at the DOS rate is variable refresh
	// rate (VRR) monitors; in this case, our present rate dictates the
	// refresh rate of the monitor, so to speak, so we can handle any weird
	// DOS refresh rate without tearing. Another common use case is presenting
	// on a fixed refresh rate monitor without vsync.
	DosRate,

	// In host rate presentation mode, the video frames are presented at the
	// refresh rate of the host monitor (the refresh rate set at the host
	// operating system level), irrespective of the emulated DOS video mode's
	// refresh rate. This effectively means we present the most recently
	// rendered frame at regularly spaced intervals determined by the host
	// rate.
	//
	// Host rate only really makes sense with vsync enabled on fixed refresh
	// rate monitors. Without vsync, we aren't better off than simply
	// presenting at the DOS rate (there would be a lot of tearing in both
	// cases; it doesn't matter how exactly the tearing happens). But with
	// vsync enabled, we're effectively "sampling" the stream of emulated
	// video frames at the host refresh rate and display them vsynced without
	// tearing. This means that some frames might be presented twice and some
	// might be skipped due to the mismatch between the DOS and the host rate.
	//
	// The most common use case for vsynced host rate presentation is
	// displaying ~70 Hz 320x200 VGA content on a fixed 60 Hz refresh rate
	// monitor.
	HostRate
};

enum class InterpolationMode { Bilinear, NearestNeighbour };

enum class RenderingBackend { Texture, OpenGl };

RenderingBackend GFX_GetRenderingBackend();

SDL_Window* GFX_GetSDLWindow();

// forward declaration
struct ShaderInfo;

void GFX_SetShader(const ShaderInfo& shader_info, const std::string& shader_source);

uint32_t GFX_GetRGB(const uint8_t red, const uint8_t green, const uint8_t blue);

InterpolationMode GFX_GetTextureInterpolationMode();

uint8_t GFX_SetSize(const int render_width_px, const int render_height_px,
                    const Fraction& render_pixel_aspect_ratio, const uint8_t flags,
                    const VideoMode& video_mode, GFX_Callback_t callback);

void GFX_ResetScreen();

void GFX_Start();

// Called at the start of every unique frame (when there have been changes to
// the framebuffer).
bool GFX_StartUpdate(uint8_t*& pixels, int& pitch);

// Called at the end of every frame, regardless of whether there have been
// changes to the framebuffer or not.
void GFX_EndUpdate(const uint16_t* changed_lines);

// Let the presentation layer safely call no-op functions.
// Useful during output initialization or transitions.
void GFX_DisengageRendering();

void GFX_RegenerateWindow(Section* sec);

DosBox::Rect GFX_GetDesktopSize();

float GFX_GetDpiScaleFactor();

#endif // DOSBOX_GUI_COMMON_H
