// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_VIDEO_H
#define DOSBOX_VIDEO_H

#include <string>

#include "utils/fraction.h"
#include "utils/rect.h"
#include "config/setup.h"
#include "misc/types.h"

// Pixels and logical units
// ========================
//
// As high-DPI displays are becoming increasingly the norm, understanding the
// difference between screen dimensions expressed as *logical units* versus
// *pixels* is essential. We fully support high-DPI in DOSBox Staging, so a
// good grasp of this topic essential when dealing with anything rendering
// related.
// 
// The idea behind logical units is that a rectangle of say 200x300 *logical
// units* in size should have the same physical dimensions when measured with
// a ruler on a 1080p, a 4k, and an 8k screen (assuming that the physical
// dimensions of the three screens are the same). When mapping these 200x300
// logic units actual physical pixels the monitor is capable of displaying,
// we'll get 200x300, 400x600, and 800x1200 pixel dimensions on 1080p, 4k, and
// 8k screens, respectively. The *logical size* of the rectangle hasn't
// changed, only its *resolution* expressed in raw native pixels has.
//
// OSes and frameworks like SDL usually report windowing system related
// coordinates and dimensions in logical units (e.g., window sizes, total
// desktop size, mouse position, etc.). But OpenGL only deals with pixels,
// never logical units, and in the core emulation layers we're only dealing
// with "raw emulated pixels" too. Consequently, in `sdlmain.cpp` we'll always
// be dealing with a mixture of logical units and pixels, so it's essential to
// make the distinction between them clear:
//
// - In `sdlmain.cpp`, we postfix every variable that holds a pixel dimension
//   with `_px` (e.g., `render_size_px`, `width_px`). Logical units get no
//   postfix (e.g., `window_size`, `mouse_pos`).
//
// - Functions and methods that return pixel dimensions are postfixed with
//   `_in_pixels` and `InPixels`, respectively (e.g., `GFX_GetViewportSizeInPixels()`).
//
// - We're always dealing with pixels in the core emulation layers (e.g., VGA
//   code), so pixel postfixes are not necessary there in general. The exception
//   is when a core layer interfaces with the top host-side rendering layers,
//   e.g., by calling `GFX_*` methods that interact with SDL -- the use of pixel
//   postfixes is highly recommended in such cases to remove ambiguity.
//

enum class RenderingBackend {
	Texture,
	OpenGl
};

typedef enum {
	GFX_CallbackReset,
	GFX_CallbackStop,
	GFX_CallbackRedraw
} GFX_CallbackFunctions_t;

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

// Graphics standards ordered by time of introduction (and roughly by
// their capabilities)
enum class GraphicsStandard { Hercules, Cga, Pcjr, Tga, Ega, Vga, Svga, Vesa };

const char* to_string(const GraphicsStandard g);

enum class ColorDepth {
	Monochrome,
	Composite,
	IndexedColor2,
	IndexedColor4,
	IndexedColor16,
	IndexedColor256,
	HighColor15Bit,
	HighColor16Bit,
	TrueColor24Bit
};

const char* to_string(const ColorDepth c);

struct VideoMode {
	// Only reliable for non-custom BIOS modes; for custom modes, it's the
	// mode used as a starting point to set up the tweaked mode, so it can
	// be literally anything.
	uint16_t bios_mode_number = 0;

	// True for graphics modes, false for text modes
	bool is_graphics_mode = false;

	// True for tweaked non-standard modes (e.g., Mode X on VGA).
	bool is_custom_mode = false;

	// Dimensions of the video mode. Note that for VGA adapters this does
	// *not* always match the actual physical output at the signal level but
	// represents the pixel-dimensions of the mode in the video memory.
	// E.g., the 320x200 13h VGA mode takes up 64,000 bytes in the video
	// memory, but is width and height doubled by the VGA hardware to
	// 640x400 at the signal level. Similarly, all 200-line CGA and EGA
	// modes are effectively emulated on VGA adapters and are output width
	// and height doubled.
	uint16_t width  = 0;
	uint16_t height = 0;

	// The intended pixel aspect ratio of the video mode. Note this is not
	// simply calculated by stretching 'width x height' to a 4:3 aspect
	// ratio rectangle; it can be literally anything.
	Fraction pixel_aspect_ratio = {};

	// - For graphics modes, the first graphics standard the mode was
	//   introduced in, unless there is ambiguity, in which case the emulated
	//   graphics adapter (e.g. in the case of PCjr and Tandy modes).
	// - For text modes, the graphics adapter in use.
	GraphicsStandard graphics_standard = {};

	// Colour depth of the video mode. Note this is *not* the same as the
	// storage bit-depth; e.g., some 24-bit true colour modes actually store
	// pixels at 32-bits with the upper 8-bits unused.
	ColorDepth color_depth = {};

	// True if this is a double-scanned mode on VGA (e.g. 200-line CGA and
	// EGA modes and most sub-400-line (S)VGA & VESA modes)
	bool is_double_scanned_mode = false;

	// True for all (S)VGA and VESA modes, and for EGA modes on emulated VGA
	// adapters that reprogram the default canonical 16-colour CGA palette
	// to custom 18-bit VGA DAC colours.
	//
	// Useful for differentiating "true EGA" modes used for backwards
	// compatibility on VGA (i.e., to run EGA games) from "repurposed" EGA
	// modes (typical used in demos and ports of Amiga action/platformer
	// games; many of these use the planar 320x200 16-colour EGA mode to
	// achieve faster smooth-scrolling, but with custom 18-bit VGA DAC
	// colours).
	bool has_vga_colors = false;

	constexpr bool operator==(const VideoMode& that) const
	{
		return (bios_mode_number == that.bios_mode_number &&
		        is_graphics_mode == that.is_graphics_mode &&
		        is_custom_mode == that.is_custom_mode &&
		        width == that.width && height == that.height &&
		        pixel_aspect_ratio == that.pixel_aspect_ratio &&
		        graphics_standard == that.graphics_standard &&
		        color_depth == that.color_depth &&
		        is_double_scanned_mode == that.is_double_scanned_mode &&
		        has_vga_colors == that.has_vga_colors);
	}

	constexpr bool operator!=(const VideoMode& that) const
	{
		return !operator==(that);
	}
};

std::string to_string(const VideoMode& video_mode);

enum class PixelFormat : uint8_t {
	// Up to 256 colours, paletted;
	// stored as packed uint8 data
	Indexed8 = 8,

	// 32K high colour, 5 bits per red/blue/green component;
	// stored as packed uint16 data with highest bit unused
	//
	// Stored as array of uint16_t in host native endianess.
	// Each uint16_t is layed out as follows:
	// (msb)1X 5R 5G 5B(lsb)
	// Example:
	// uint16_t pixel = (red << 10) | (green << 5) | (blue << 0)
	//
	// SDL Equivalent: SDL_PIXELFORMAT_RGB555
	// FFmpeg Equivalent: AV_PIX_FMT_RGB555
	RGB555_Packed16 = 15,

	// 65K high colour, 5 bits for red/blue, 6 bit for green;
	// stored as packed uint16 data
	//
	// Stored as array of uint16_t in host native endianess.
	// Each uint16_t is layed out as follows:
	// (msb)5R 6G 5B(lsb)
	// Example:
	// uint16_t pixel = (red << 11) | (green << 5) | (blue << 0)
	//
	// SDL Equivalent: SDL_PIXELFORMAT_RGB565
	// FFmpeg Equivalent: AV_PIX_FMT_RGB565
	RGB565_Packed16 = 16,

	// 16.7M (24-bit) true colour, 8 bits per red/blue/green component;
	// stored as a sequence of three packed uint8_t values in BGR byte
	// order, also known as memory order. This format is endian-agnostic.
	//
	// Example:
	// uint8_t *pixels = image.image_data;
	// pixels[0] = blue; pixels[1] = green; pixels[2] = red;
	//
	// SDL Equivalent: SDL_PIXELFORMAT_BGR24
	// FFmpeg Equivalent: AV_PIX_FMT_BGR24
	BGR24_ByteArray = 24,

	// Same as BGR24_ByteArray but padded to 32-bits. 16.7M true colour,
	// 8 bits per red/blue/green/empty component; stored as a sequence of
	// four packed uint8_t values in BGRX byte order, also known as
	// memory order. This format is endian-agnostic.
	//
	// Example:
	// uint8_t *pixels = image.image_data;
	// pixels[0] = blue; pixels[1] = green; pixels[2] = red; pixels[3] = empty;
	//
	// SDL has has no equivalent.
	// FFmpeg Equivalent: BGRX32_ByteArray
	BGRX32_ByteArray = 32,
};

const char* to_string(const PixelFormat pf);

uint8_t get_bits_per_pixel(const PixelFormat pf);

// Extra information about a bitmap image that represents a single frame of
// DOS video output.
//
// E.g. for the 320x200 256-colour 13h VGA mode with double-scanning
// and pixel-doubling enabled:
//
//   - width = 320 (will be pixel-doubled post-render via double_width)
//   - height = 400 (2*200 lines because we're rendering scan-doubled)
//   - pixel_aspect_ratio = 5/6 (1:1.2) (because the PAR is meant for the
//     final image, post the optional width & height doubling)
//   - double_width = true (pixel-doubling)
//   - double_height = false (we're rendering scan-doubled)
//
struct ImageInfo {
	// The image data has this many pixels per image row (so this is the
	// image width prior to optional width-doubling).
	uint16_t width = 0;

	// The image data has this many rows (so this is the image height prior
	// to optional height-doubling).
	uint16_t height = 0;

	// If true, the final image should be doubled horizontally via a scaler
	// before outputting it (e.g. to achieve pixel-doubling).
	bool double_width = false;

	// If true, the final image should be doubled vertically via a scaler
	// before outputting it (e.g. to achieve fake double-scanning).
	bool double_height = false;

	// If true, we're dealing with a double-scanned VGA mode that was
	// force-rendered as single-scanned.
	//
	// We need to store this flag so we can include it in the video mode
	// equality criteria. E.g., the render dimensions of the double-scanned
	// 320x200 VGA mode (mode 13h) and 320x400 (non-VESA Mode X variant) are
	// both 320x400, so they would be considered equal if this flag was not
	// included. This would throw off the adaptive shader switching logic when
	// such video mode transitions happen.
	bool forced_single_scan = false;

	// If true, we're dealing with "baked-in" double scanning, i.e., when
	// 320x200 is rendered as 320x400. This can happen for non-VESA VGA modes
	// and for EGA modes on VGA. Every other double-scanned mode on VGA (all
	// CGA modes and all double-scanned VESA modes) are "fake-double scanned"
	// (doubled post-render by setting `double_height` to true).
	bool rendered_double_scan = false;

	// If true, the image has been rendered doubled horizontally. This is only
	// used to "normalise" the 160x200 16-colour Tandy and PCjr modes to
	// 320-pixel-wide rendered output (it simplifies rendering the host video
	// output downstream, but slightly complicates raw captures).
	bool rendered_pixel_doubling = false;

	// Pixel aspect ratio to be applied to the final image, *after*
	// optional width and height doubling, so it appears as intended.
	// (`video_mode.pixel_aspect_ratio` holds the "nominal" pixel
	// aspect ratio of the source video mode, which can be different).
	Fraction pixel_aspect_ratio = {};

	// Pixel format of the image data.
	PixelFormat pixel_format = {};

	// Details about the source video mode.
	//
	// This is usually different than the details of image data. E.g., for
	// the 320x200 256-colour 13h VGA mode it always contains the following,
	// regardless of whether double-scanning and pixel-doubling is enabled
	// at the rendering level:
	//   - width = 320
	//   - height = 200
	//   - pixel_aspect_ratio = 5/6 (1:1.2)
	VideoMode video_mode = {};

	constexpr bool operator==(const ImageInfo& that) const
	{
		return (width == that.width && height == that.height &&
		        double_width == that.double_width &&
		        double_height == that.double_height &&
		        forced_single_scan == that.forced_single_scan &&
		        pixel_aspect_ratio == that.pixel_aspect_ratio &&
		        pixel_format == that.pixel_format &&
		        video_mode == that.video_mode);
	}

	constexpr bool operator!=(const ImageInfo& that) const
	{
		return !operator==(that);
	}
};

enum class InterpolationMode { Bilinear, NearestNeighbour };

typedef void (*GFX_Callback_t)(GFX_CallbackFunctions_t function);

constexpr uint8_t GFX_CAN_8      = 1 << 0;
constexpr uint8_t GFX_CAN_15     = 1 << 1;
constexpr uint8_t GFX_CAN_16     = 1 << 2;
constexpr uint8_t GFX_CAN_32     = 1 << 3;
constexpr uint8_t GFX_DBL_H      = 1 << 4; // double-width  flag
constexpr uint8_t GFX_DBL_W      = 1 << 5; // double-height flag
constexpr uint8_t GFX_CAN_RANDOM = 1 << 6; // interface can also do random acces

// TODO this hardly belongs to `video.h`...
bool DOSBOX_PollAndHandleEvents();

// Let the presentation layer safely call no-op functions.
// Useful during output initialization or transitions.
void GFX_DisengageRendering();

uint32_t GFX_GetRGB(const uint8_t red, const uint8_t green, const uint8_t blue);

struct ShaderInfo;

void GFX_SetShader(const ShaderInfo& shader_info, const std::string& shader_source);

InterpolationMode GFX_GetTextureInterpolationMode();

struct VideoMode;
class Fraction;

uint8_t GFX_SetSize(const int render_width_px, const int render_height_px,
                    const Fraction& render_pixel_aspect_ratio, const uint8_t flags,
                    const VideoMode& video_mode, GFX_Callback_t callback);

void GFX_ResetScreen(void);
void GFX_RequestExit(const bool requested);
void GFX_Start(void);
void GFX_Stop(void);

// Called at the start of every unique frame (when there have been changes to
// the framebuffer).
bool GFX_StartUpdate(uint8_t * &pixels, int &pitch);

// Called at the end of every frame, regardless of whether there have been
// changes to the framebuffer or not.
void GFX_EndUpdate(const uint16_t *changed_lines);

void GFX_LosingFocus();
void GFX_RegenerateWindow(Section *sec);

void GFX_RefreshTitle();
void GFX_RefreshAnimatedTitle();
void GFX_NotifyBooting();
void GFX_NotifyAudioCaptureStatus(const bool is_capturing);
void GFX_NotifyVideoCaptureStatus(const bool is_capturing);
void GFX_NotifyAudioMutedStatus(const bool is_muted);
void GFX_NotifyProgramName(const std::string& segment_name,
                           const std::string& canonical_name);
void GFX_NotifyCyclesChanged();

enum class MouseHint {
	None,                    // no hint to display
	CapturedHotkey,          // captured, hotkey to release
	CapturedHotkeyMiddle,    // captured, hotkey or middle-click release
	ReleasedHotkey,          // released, hotkey to capture
	ReleasedHotkeyMiddle,    // released, hotkey or middle-click to capture
	ReleasedHotkeyAnyButton, // released, hotkey or any click to capture
	SeamlessHotkey,          // seamless, hotkey to capture
	SeamlessHotkeyMiddle,    // seamless, hotkey or middle-click to capture
};

void GFX_CenterMouse();
void GFX_SetMouseHint(const MouseHint requested_hint_id);
void GFX_SetMouseCapture(const bool requested_capture);
void GFX_SetMouseVisibility(const bool requested_visible);
void GFX_SetMouseRawInput(const bool requested_raw_input);

// Detects the presence of a desktop environment or window manager
bool GFX_HaveDesktopEnvironment();

void MAPPER_UpdateJoysticks(void);

DosBox::Rect GFX_CalcDrawRectInPixels(const DosBox::Rect& canvas_size_px,
                                      const DosBox::Rect& render_size_px,
                                      const Fraction& render_pixel_aspect_ratio);

DosBox::Rect GFX_GetCanvasSizeInPixels();
DosBox::Rect GFX_GetViewportSizeInPixels();
DosBox::Rect GFX_GetDesktopSize();

float GFX_GetDpiScaleFactor();

RenderingBackend GFX_GetRenderingBackend();

double GFX_GetHostRefreshRate();

PresentationMode GFX_GetPresentationMode();
void GFX_MaybePresentFrame();

#endif
