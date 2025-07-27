// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2022-2022  Jon Dennis
// SPDX-License-Identifier: GPL-2.0-or-later

//
// This file contains the VGA/RENDER_ interception code...
//

#include "reelmagic.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include "../../gui/render/scaler/scalers.h" //SCALER_MAXWIDTH SCALER_MAXHEIGHT
#include "rgb565.h"
#include "setup.h"
#include "video.h"

namespace {
// XXX currently duplicating this in realmagic_*.cpp files to avoid header pollution... TDB if this
// is a good idea...
struct RMException : ::std::exception {
	std::string _msg = {};
	RMException(const char* fmt = "General ReelMagic Exception", ...)
	{
		va_list vl;
		va_start(vl, fmt);
		_msg.resize(vsnprintf(&_msg[0], 0, fmt, vl) + 1);
		va_end(vl);
		va_start(vl, fmt);
		vsnprintf(&_msg[0], _msg.size(), fmt, vl);
		va_end(vl);
		LOG(LOG_REELMAGIC, LOG_ERROR)("%s", _msg.c_str());
	}
	~RMException() noexcept override = default;
	const char* what() const noexcept override
	{
		return _msg.c_str();
	}
};
} // namespace

// explicit definition of the different pixel types...
// being a bit redundant here as an attempt to keep this
// already complicated logic somewhat readable
namespace {
struct RenderOutputPixel {
	uint8_t blue;
	uint8_t green;
	uint8_t red;
	uint8_t alpha;
};

struct VGA16bppPixel {
	Rgb565 pixel = {};
	template <typename T>
	constexpr void CopyRGBTo(T& out) const
	{
		pixel.ToRgb888(out.red, out.green, out.blue);
	}

	constexpr void FromRgb888(const uint8_t r8, const uint8_t g8, const uint8_t b8)
	{
		pixel = Rgb565::FromRgb888(Rgb888(r8, g8, b8));
	}
};

struct VGA32bppPixel {
	uint8_t blue;
	uint8_t green;
	uint8_t red;
	uint8_t alpha;
	template <typename T>
	inline void CopyRGBTo(T& out) const
	{
		out.red   = red;
		out.green = green;
		out.blue  = blue;
	}
};
struct VGAUnder32bppPixel : VGA32bppPixel {
	inline bool IsTransparent() const
	{
		return true;
	}
};
struct VGAOver32bppPixel : VGA32bppPixel {
	inline bool IsTransparent() const
	{
		return (red | green | blue) == 0;
	}
};

struct VGAPalettePixel {
	static VGA32bppPixel _vgaPalette32bpp[256];
	static VGA16bppPixel _vgaPalette16bpp[256];
	uint8_t index;
	template <typename T>
	inline void CopyRGBTo(T& out) const
	{
		_vgaPalette32bpp[index].CopyRGBTo(out);
	}
};

struct VGAUnder16bppPixel : VGA16bppPixel {
	constexpr bool IsTransparent() const
	{
		return true;
	}
};

struct VGAOver16bppPixel : VGA16bppPixel {
	// Like 8-bit VGA, use the first color in the palette as the transparent
	// color. This lets the RTZ intro show the house animation and properly shows
	// the crow's body on the sign-post (where as using a zero-black as
	// transparent doesn't show the house animation and only shows parts of the
	// crow's wings).
	constexpr bool IsTransparent() const
	{
		return pixel == VGAPalettePixel::_vgaPalette16bpp[0].pixel;
	}
};

struct VGAUnderPalettePixel : VGAPalettePixel {
	inline bool IsTransparent() const
	{
		return true;
	}
};
struct VGAOverPalettePixel : VGAPalettePixel {
	static uint8_t _alphaChannelIndex;
	inline bool IsTransparent() const
	{
		return index == _alphaChannelIndex;
	}
};

struct PlayerPicturePixel {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	template <typename T>
	inline void CopyRGBTo(T& out) const
	{
		out.red   = red;
		out.green = green;
		out.blue  = blue;
	}
	inline bool IsTransparent() const
	{
		return false;
	}
};
} // namespace

//
// The running state of the video mixer:
// WARNING: things will blow sky high if the ReelMagic_Init()
//          is NOT called before VGA stuff happens!
//

// everything is passthrough if this is false
static bool _videoMixerEnabled = false;

// state captured from VGA
VGA32bppPixel VGAPalettePixel::_vgaPalette32bpp[256] = {};
VGA16bppPixel VGAPalettePixel::_vgaPalette16bpp[256] = {};
uint8_t VGAOverPalettePixel::_alphaChannelIndex      = 0;

static ImageInfo _vgaImageInfo    = {};
static double _vgaFramesPerSecond = 0.0;

// state captured from current/active MPEG player
static PlayerPicturePixel _mpegPictureBuffer[SCALER_MAXWIDTH * SCALER_MAXHEIGHT];
static PlayerPicturePixel* _mpegPictureBufferPtr = _mpegPictureBuffer;

static uint32_t _mpegPictureWidth  = 0;
static uint32_t _mpegPictureHeight = 0;

// video mixer is exclusively 32bpp on the RENDER... VGA color palette mapping is re-done here...
static const auto VideoMixerPixelFormat = PixelFormat::BGRX32_ByteArray;

// current RENDER state
static void RMR_DrawLine_Passthrough(const void* src);

// default things to off
ReelMagic_ScalerLineHandler_t ReelMagic_RENDER_DrawLine = RMR_DrawLine_Passthrough;

static ReelMagic_VideoMixerMPEGProvider* _requestedMpegProvider = nullptr;
static ReelMagic_VideoMixerMPEGProvider* _activeMpegProvider    = nullptr;

static RenderOutputPixel _finalMixedRenderLineBuffer[SCALER_MAXWIDTH];
static Bitu _currentRenderLineNumber = 0;
static uint32_t _renderWidth         = 0;
static uint32_t _renderHeight        = 0;

//
// pixel mixing / underlay / overlay functions...
//
// TODO: understand the right way to detect transparency... for now, i'm taking the approach of using
//      VGA pallet index #0 if VGA mode is 8bpp and using "pure black" if VGA mode is 32bpp...
//      The upper-left RTZ menu will have transparancy around some of the buttons if the
//      "pure black" approach is taken, but not if pallet index #0 is used... I have no idea
//      how the game originally was as I don't have the hardware, but this seems like a reasonable
//      workaround to make this look nice and clean for the mean time....
//      also, as i'm carrying around an alpha channel, i should probably put that to good use...
//
template <typename VGAPixelT, typename MPEGPixelT>
static inline void MixPixel(RenderOutputPixel& out, const VGAPixelT& vga_pixel,
                            const MPEGPixelT& mpeg)
{
	if (vga_pixel.IsTransparent()) {
		mpeg.CopyRGBTo(out);
	} else {
		vga_pixel.CopyRGBTo(out);
	}
	out.alpha = 0;
}

template <typename VGAPixelT>
static inline void MixPixel(RenderOutputPixel& out, const VGAPixelT& vga_pixel)
{
	vga_pixel.CopyRGBTo(out);
	out.alpha = 0;
}

static void ClearMpegPictureBuffer(const PlayerPicturePixel p)
{
	for (Bitu i = 0; i < (sizeof(_mpegPictureBuffer) / sizeof(_mpegPictureBuffer[0])); ++i)
		_mpegPictureBuffer[i] = p;
}

static void ClearMpegPictureBuffer()
{
	PlayerPicturePixel p;
	p.red   = 0;
	p.green = 0;
	p.blue  = 0;
	ClearMpegPictureBuffer(p);
}

//
// Line renderers and all their variations... Taking a similiar
// architectural approach to that used for RENDER_DrawLine()
//
// there are all sorts of variations of these functions because
// they are called at a high frequency... these functions are
// responsible for both mixing pixels and scaling the VGA and
// MPEG pictures...
//
// TODO: Go through and profile these and test things like
//       using a lookup table vs computing values on the fly...
//       is the general purpose function just as fast
//       the specicific purpose ones?
//       For the first go at this, the specific purpose functions
//       were indeed faster, but so much has changed since then
//       that this may no longer be the case
//
static void RMR_DrawLine_Passthrough(const void* src)
{
	RENDER_DrawLine(src);
}

static void RMR_DrawLine_MixerError([[maybe_unused]] const void* src)
{
	if (++_currentRenderLineNumber >= _renderHeight)
		return;
	for (Bitu i = 0;
	     i < (sizeof(_finalMixedRenderLineBuffer) / sizeof(_finalMixedRenderLineBuffer[0]));
	     ++i) {
		RenderOutputPixel& p = _finalMixedRenderLineBuffer[i];
		p.red                = 0xFF;
		p.green              = 0x00;
		p.blue               = 0x00;
		p.alpha              = 0x00;
	}
	RENDER_DrawLine(_finalMixedRenderLineBuffer);
}

// its not real enterprise C++ until yer mixing macros and templates...
#define CREATE_RMR_VGA_TYPED_FUNCTIONS(DRAWLINE_FUNC_NAME) \
	static void DRAWLINE_FUNC_NAME##_VGAO8(const void* src) \
	{ \
		DRAWLINE_FUNC_NAME((const VGAOverPalettePixel*)src); \
	} \
	static void DRAWLINE_FUNC_NAME##_VGAO16(const void* src) \
	{ \
		DRAWLINE_FUNC_NAME((const VGAOver16bppPixel*)src); \
	} \
	static void DRAWLINE_FUNC_NAME##_VGAO32(const void* src) \
	{ \
		DRAWLINE_FUNC_NAME((const VGAOver32bppPixel*)src); \
	} \
	static void DRAWLINE_FUNC_NAME##_VGAU8(const void* src) \
	{ \
		DRAWLINE_FUNC_NAME((const VGAUnderPalettePixel*)src); \
	} \
	static void DRAWLINE_FUNC_NAME##_VGAU16(const void* src) \
	{ \
		DRAWLINE_FUNC_NAME((const VGAUnder16bppPixel*)src); \
	} \
	static void DRAWLINE_FUNC_NAME##_VGAU32(const void* src) \
	{ \
		DRAWLINE_FUNC_NAME((const VGAUnder32bppPixel*)src); \
	}

#define ASSIGN_RMR_DRAWLINE_FUNCTION(DRAWLINE_FUNC_NAME, VGA_PF, VGA_OVER) \
	{ \
		if (VGA_OVER) \
			switch (VGA_PF) { \
			case PixelFormat::Indexed8: \
				ReelMagic_RENDER_DrawLine = &DRAWLINE_FUNC_NAME##_VGAO8; \
				break; \
			case PixelFormat::RGB565_Packed16: \
				ReelMagic_RENDER_DrawLine = &DRAWLINE_FUNC_NAME##_VGAO16; \
				break; \
			case PixelFormat::BGRX32_ByteArray: \
				ReelMagic_RENDER_DrawLine = &DRAWLINE_FUNC_NAME##_VGAO32; \
				break; \
			default: \
				ReelMagic_RENDER_DrawLine = &RMR_DrawLine_MixerError; \
				break; \
			} \
		else \
			switch (VGA_PF) { \
			case PixelFormat::Indexed8: \
				ReelMagic_RENDER_DrawLine = &DRAWLINE_FUNC_NAME##_VGAU8; \
				break; \
			case PixelFormat::RGB565_Packed16: \
				ReelMagic_RENDER_DrawLine = &DRAWLINE_FUNC_NAME##_VGAU16; \
				break; \
			case PixelFormat::BGRX32_ByteArray: \
				ReelMagic_RENDER_DrawLine = &DRAWLINE_FUNC_NAME##_VGAU32; \
				break; \
			default: \
				ReelMagic_RENDER_DrawLine = &RMR_DrawLine_MixerError; \
				break; \
			} \
	}

template <typename T>
static inline void RMR_DrawLine_VGAOnly(const T* src)
{
	const Bitu lineWidth = _vgaImageInfo.width;

	RenderOutputPixel* const out = _finalMixedRenderLineBuffer;

	for (Bitu i = 0; i < lineWidth; ++i) {
		MixPixel(out[i], src[i]);
	}

	RENDER_DrawLine(_finalMixedRenderLineBuffer);
}
CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VGAOnly)

template <typename T>
static inline void RMR_DrawLine_VGAMPEGSameSize(const T* src)
{
	const Bitu lineWidth = _vgaImageInfo.width;

	RenderOutputPixel* const out = _finalMixedRenderLineBuffer;

	for (Bitu i = 0; i < lineWidth; ++i) {
		MixPixel(out[i], src[i], _mpegPictureBufferPtr[i]);
	}

	_mpegPictureBufferPtr += _mpegPictureWidth;

	RENDER_DrawLine(_finalMixedRenderLineBuffer);
}
CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VGAMPEGSameSize)

// VGA Sized Output (RENDER) functions...
template <typename T>
static inline void RMR_DrawLine_VSO_MPEGDoubleVGASize(const T* src)
{
	const Bitu lineWidth         = _vgaImageInfo.width;
	RenderOutputPixel* const out = _finalMixedRenderLineBuffer;
	_mpegPictureBufferPtr -= _mpegPictureWidth * (_currentRenderLineNumber++ & 1);
	for (Bitu i = 0; i < lineWidth; ++i) {
		MixPixel(out[i], src[i], _mpegPictureBufferPtr[i >> 1]);
	}
	_mpegPictureBufferPtr += _mpegPictureWidth;
	RENDER_DrawLine(_finalMixedRenderLineBuffer);
}
CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VSO_MPEGDoubleVGASize)

template <typename T>
static inline void RMR_DrawLine_VSO_VGAMPEGSameWidthSkip6Vertical(const T* src)
{
	const Bitu lineWidth         = _vgaImageInfo.width;
	RenderOutputPixel* const out = _finalMixedRenderLineBuffer;
	for (Bitu i = 0; i < lineWidth; ++i)
		MixPixel(out[i], src[i], _mpegPictureBufferPtr[i]);
	_mpegPictureBufferPtr += _mpegPictureWidth;
	if (++_currentRenderLineNumber >= 6) {
		_currentRenderLineNumber = 0;
		_mpegPictureBufferPtr += _mpegPictureWidth;
	}
	RENDER_DrawLine(_finalMixedRenderLineBuffer);
}
CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VSO_VGAMPEGSameWidthSkip6Vertical)

template <typename T>
static inline void RMR_DrawLine_VSO_VGAMPEGDoubleSameWidthSkip6Vertical(const T* src)
{
	const Bitu lineWidth         = _vgaImageInfo.width;
	RenderOutputPixel* const out = _finalMixedRenderLineBuffer;
	_mpegPictureBufferPtr -= _mpegPictureWidth * (_currentRenderLineNumber & 1);
	for (Bitu i = 0; i < lineWidth; ++i)
		MixPixel(out[i], src[i], _mpegPictureBufferPtr[i >> 1]);
	_mpegPictureBufferPtr += _mpegPictureWidth;
	if (++_currentRenderLineNumber >= 6) {
		_currentRenderLineNumber = 0;
		_mpegPictureBufferPtr += _mpegPictureWidth;
	}
	RENDER_DrawLine(_finalMixedRenderLineBuffer);
}
CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VSO_VGAMPEGDoubleSameWidthSkip6Vertical)

//
// the catch-all un-optimized MPEG scaling function...
//
// WARNING: this ended up being kinda hacky...
//          should we perheps re-think this; possibly using a
//          lookup table computed at mode change time?
//
static Bitu _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_WidthRatio     = 0;
static Bitu _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_HeightRatio    = 0;
static void Initialize_RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_Dimensions()
{
	_RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_WidthRatio = _mpegPictureWidth << 12;
	_RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_WidthRatio /= _renderWidth;
	_RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_HeightRatio = _mpegPictureHeight << 12;
	_RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_HeightRatio /= _renderHeight;
}
template <typename T>
static inline void RMR_DrawLine_VSO_GeneralResizeMPEGToVGA(const T* src)
{
	const Bitu lineWidth         = _vgaImageInfo.width;
	RenderOutputPixel* const out = _finalMixedRenderLineBuffer;
	for (Bitu i = 0; i < lineWidth; ++i)
		MixPixel(out[i],
		         src[i],
		         _mpegPictureBufferPtr[(i * _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_WidthRatio) >> 12]);
	_mpegPictureBufferPtr =
	        &_mpegPictureBuffer[_mpegPictureWidth * ((++_currentRenderLineNumber *
	                                                  _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_HeightRatio) >>
	                                                 12)];
	RENDER_DrawLine(_finalMixedRenderLineBuffer);
}
CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VSO_GeneralResizeMPEGToVGA)

static void setup_video_mixer(const bool update_render_mode)
{
	// the "_activeMpegProvider" variable serves a few purposes:
	//  1. Tells the "ReelMagic_RENDER_StartUpdate()" function which player to call
	//  "OnVerticalRefresh()" on
	//  2. Prevents the "ReelMagic_RENDER_StartUpdate()" function from calling
	//  "OnVerticalRefresh()" if:
	//     . We have not yet received a VGA mode/configuration
	//     . The video mixer is in an error state
	_activeMpegProvider = nullptr; // no MPEG activation unless all is good...

	if (!_videoMixerEnabled) {
		// video mixer is disabled... VGA mode dictates RENDER mode just like "normal dosbox"
		ReelMagic_RENDER_DrawLine = &RMR_DrawLine_Passthrough;

		RENDER_SetSize(_vgaImageInfo, _vgaFramesPerSecond);

		LOG(LOG_REELMAGIC, LOG_NORMAL)
		("Video Mixer is Disabled. Passed through VGA RENDER_SetSize()");
		return;
	}

	// cache the current MPEG picture size...
	ReelMagic_VideoMixerMPEGProvider* const mpeg = _requestedMpegProvider;
	if (mpeg) {
		_mpegPictureWidth  = mpeg->GetAttrs().PictureSize.Width;
		_mpegPictureHeight = mpeg->GetAttrs().PictureSize.Height;
	}

	_renderWidth  = _vgaImageInfo.width;
	_renderHeight = _vgaImageInfo.height;

	// check to make sure we have enough horizontal line buffer for the
	// current VGA mode...
	[[maybe_unused]] const Bitu maxRenderWidth =
	        sizeof(_finalMixedRenderLineBuffer) /
	        sizeof(_finalMixedRenderLineBuffer[0]);

	assert(_vgaImageInfo.width <= maxRenderWidth);

	// set the RENDER mode only if requested...
	if (update_render_mode) {
		// Setting the pixelformat in _vgaImageInfo and passing it directly
		// will result in garbled graphics due to the weird and wonderful ways
		// how the video mixer interacts with the VGA renderer. Don't ask...
		ImageInfo info = _vgaImageInfo;
		info.pixel_format = VideoMixerPixelFormat;

		RENDER_SetSize(info, _vgaFramesPerSecond);
	}

	// if no active player, set the VGA only function... the difference between this and
	// "passthrough mode" is that this keeps the video mixer enabled with a RENDER output
	// color depth of 32 bits to eliminate any flickering associated with the RENDER_SetSize()
	// call when starting/stopping a video to give the user that smooth hardware decoder feel :-)
	if ((!mpeg) || (!mpeg->GetConfig().VideoOutputVisible)) {
		ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VGAOnly,
		                             _vgaImageInfo.pixel_format,
		                             true);

		_activeMpegProvider = mpeg;
		LOG(LOG_REELMAGIC, LOG_NORMAL)
		("Video Mixer Mode VGA Only (vga=%ux%u mpeg=off)",
		 _vgaImageInfo.width,
		 _vgaImageInfo.height);
		return;
	}

	// choose a RENDER draw function...
	const bool vgaOver  = mpeg->GetConfig().UnderVga;
	const char* modeStr = "UNKNOWN";

	if ((_vgaImageInfo.width == _mpegPictureWidth) &&
	    (_vgaImageInfo.height == _mpegPictureHeight)) {
		modeStr = "Matching Sized MPEG to VGA Pictures";
		ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VGAMPEGSameSize,
		                             _vgaImageInfo.pixel_format,
		                             vgaOver);

	} else if ((_vgaImageInfo.width == (_mpegPictureWidth * 2)) &&
	           (_vgaImageInfo.height == ((_mpegPictureHeight * 2)))) {
		modeStr = "Double Sized MPEG to VGA Pictures";
		ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VSO_MPEGDoubleVGASize,
		                             _vgaImageInfo.pixel_format,
		                             vgaOver);

	} else if ((_vgaImageInfo.width == _mpegPictureWidth) &&
	           ((_mpegPictureHeight /
	             (_mpegPictureHeight - _vgaImageInfo.height)) == 6)) {
		modeStr = "Matching Sized MPEG to VGA Pictures, skipping every 6th MPEG line";
		ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VSO_VGAMPEGSameWidthSkip6Vertical,
		                             _vgaImageInfo.pixel_format,
		                             vgaOver);

	} else if ((_vgaImageInfo.width == (_mpegPictureWidth * 2)) &&
	           (((_mpegPictureHeight * 2) /
	             ((_mpegPictureHeight * 2) - _vgaImageInfo.height)) == 6)) {
		modeStr = "Double Sized MPEG to VGA Pictures, skipping every 6th MPEG line";
		ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VSO_VGAMPEGDoubleSameWidthSkip6Vertical,
		                             _vgaImageInfo.pixel_format,
		                             vgaOver);

	} else {
		modeStr = "Generic Unoptimized MPEG Resize";
		Initialize_RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_Dimensions();
		ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VSO_GeneralResizeMPEGToVGA,
		                             _vgaImageInfo.pixel_format,
		                             vgaOver);
	}

	// log the mode we are now in
	if (ReelMagic_RENDER_DrawLine == &RMR_DrawLine_MixerError) {
		modeStr = "Error";
	} else {
		_activeMpegProvider = mpeg;
	}

	LOG(LOG_REELMAGIC, LOG_NORMAL)
	("Video Mixer Mode %s (vga=%ux%u mpeg=%ux%u render=%ux%u)",
	 modeStr,
	 _vgaImageInfo.width,
	 _vgaImageInfo.height,
	 _mpegPictureWidth,
	 _mpegPictureHeight,
	 _renderWidth,
	 _renderHeight);
}

//
// The RENDER_*() interceptors begin here...
//
void ReelMagic_RENDER_SetPalette(const uint8_t entry, const uint8_t red,
                                 const uint8_t green, const uint8_t blue)
{
	VGA32bppPixel& p = VGAPalettePixel::_vgaPalette32bpp[entry];
	p.red            = red;
	p.green          = green;
	p.blue           = blue;
	p.alpha          = 0;

	VGAPalettePixel::_vgaPalette16bpp[entry].FromRgb888(red, green, blue);

	RENDER_SetPalette(entry, red, green, blue);
}

void ReelMagic_RENDER_SetSize(const ImageInfo& image_info,
                              const double frames_per_second)
{
	_vgaImageInfo       = image_info;
	_vgaFramesPerSecond = frames_per_second;

	constexpr auto update_render_mode = true;
	setup_video_mixer(update_render_mode);
}

bool ReelMagic_RENDER_StartUpdate(void)
{
	if (_activeMpegProvider) {
		VGAOverPalettePixel::_alphaChannelIndex = _activeMpegProvider->GetConfig().VgaAlphaIndex;
		_activeMpegProvider->OnVerticalRefresh(_mpegPictureBuffer,
		                                       static_cast<float>(_vgaFramesPerSecond));
	}
	_currentRenderLineNumber = 0;
	_mpegPictureBufferPtr    = _mpegPictureBuffer;
	return RENDER_StartUpdate();
}

void ReelMagic_ClearVideoMixer()
{
	_requestedMpegProvider = nullptr;
	ClearMpegPictureBuffer();
}

bool ReelMagic_IsVideoMixerEnabled()
{
	return _videoMixerEnabled;
}

void ReelMagic_SetVideoMixerEnabled(const bool enabled)
{
	if (!enabled)
		ReelMagic_ClearVideoMixer(); // defensive
	if (enabled == _videoMixerEnabled)
		return;
	_videoMixerEnabled = enabled;
	LOG(LOG_REELMAGIC, LOG_NORMAL)("%s Video Mixer", enabled ? "Enabling" : "Disabling");

	constexpr auto update_render_mode = true;
	setup_video_mixer(update_render_mode);
}

ReelMagic_VideoMixerMPEGProvider* ReelMagic_GetVideoMixerMPEGProvider()
{
	return _requestedMpegProvider;
}

void ReelMagic_ClearVideoMixerMPEGProvider()
{
	_requestedMpegProvider = nullptr;

	constexpr auto update_render_mode = false;
	setup_video_mixer(update_render_mode);
}

void ReelMagic_SetVideoMixerMPEGProvider(ReelMagic_VideoMixerMPEGProvider* const provider)
{
	assert(provider);
	// Can our MPEG picture buffer accomodate the provider's picture size?
	const auto dimensions = provider->GetAttrs().PictureSize;
	if (dimensions.Width > SCALER_MAXWIDTH ||
	    dimensions.Height > SCALER_MAXHEIGHT) {
		LOG(LOG_REELMAGIC, LOG_ERROR)
		("Video Mixing Buffers Too Small for MPEG Video Size. Reject Player Push");
		return;
	}

	// clear the MPEG picture buffer when not replacing an existing provider
	if (!_requestedMpegProvider) {
		ClearMpegPictureBuffer();
	}

	// set the new requested provider
	_requestedMpegProvider = provider;

	constexpr auto update_render_mode = false;
	setup_video_mixer(update_render_mode);
}

void ReelMagic_InitVideoMixer([[maybe_unused]] Section* sec)
{
	// empty
}
