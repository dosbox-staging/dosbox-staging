// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_GUI_PRIVATE_COMMON_H
#define DOSBOX_GUI_PRIVATE_COMMON_H

#include "dosbox_config.h"
#include "misc/video.h"
#include "utils/fraction.h"
#include "utils/rect.h"

// must be included after dosbox_config.h
#include "SDL.h"

constexpr uint8_t GFX_CAN_8      = 1 << 0;
constexpr uint8_t GFX_CAN_15     = 1 << 1;
constexpr uint8_t GFX_CAN_16     = 1 << 2;
constexpr uint8_t GFX_CAN_32     = 1 << 3;

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

enum class RenderBackendType { OpenGl, Sdl };

RenderBackendType GFX_GetRenderBackendType();

// forward declaration
class RenderBackend;

RenderBackend* GFX_GetRenderer();

enum class TextureFilterMode { NearestNeighbour, Bilinear };

// Might return nullptr if the window doesn't exist yet
SDL_Window* GFX_GetWindow();

uint32_t GFX_MakePixel(const uint8_t red, const uint8_t green, const uint8_t blue);

TextureFilterMode GFX_GetTextureFilterMode();

void GFX_SetSize(const int render_width_px, const int render_height_px,
                 const Fraction& render_pixel_aspect_ratio,
                 const bool double_width, const bool double_height,
                 const VideoMode& video_mode, GFX_Callback_t callback);

void GFX_ResetScreen();

void GFX_Start();
void GFX_Stop();

// Called at the start of every unique frame (when there have been changes to
// the framebuffer).
bool GFX_StartUpdate(uint32_t*& pixels, int& pitch);

// Called at the end of every frame, regardless of whether there have been
// changes to the framebuffer or not.
void GFX_EndUpdate();

void GFX_CaptureRenderedImage();

DosBox::Rect GFX_GetDesktopSize();

float GFX_GetDpiScaleFactor();

DosBox::Rect GFX_CalcDrawRectInPixels(const DosBox::Rect& canvas_size_px);

DosBox::Rect to_rect(const SDL_Rect r);
SDL_Rect to_sdl_rect(const DosBox::Rect& r);

enum class DosBoxSdlEvent {
	RefreshAnimatedTitle,
	NumEvents // dummy, keep last, do not use
};

int GFX_GetUserSdlEventId(DosBoxSdlEvent event);

bool GFX_IsPaused();

#endif // DOSBOX_GUI_COMMON_H
