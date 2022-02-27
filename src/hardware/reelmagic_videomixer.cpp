/*
 *  Copyright (C) 2022 Jon Dennis
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

//
// This file contains the VGA/RENDER_ interception code...
//

#include "reelmagic.h"
#include "setup.h"
#include "render.h"
#include "../gui/render_scalers.h" //SCALER_MAXWIDTH SCALER_MAXHEIGHT

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <exception>
#include <string>

namespace {
  struct RMException : ::std::exception { //XXX currently duplicating this in realmagic_*.cpp files to avoid header pollution... TDB if this is a good idea...
    std::string _msg;
    RMException(const char *fmt = "General ReelMagic Exception", ...) {
      va_list vl;
      va_start(vl, fmt); _msg.resize(vsnprintf(&_msg[0], 0, fmt, vl) + 1);   va_end(vl);
      va_start(vl, fmt); vsnprintf(&_msg[0], _msg.size(), fmt, vl);          va_end(vl);
      LOG(LOG_REELMAGIC, LOG_ERROR)("%s", _msg.c_str());
    }
    virtual ~RMException() throw() {}
    virtual const char* what() const throw() {return _msg.c_str();}
  };
};



//explicit definition of the different pixel types...
//being a bit redundant here as an attempt to keep this 
//already complicated logic somewhat readable
namespace {
struct RenderOutputPixel {
  Bit8u blue;
  Bit8u green;
  Bit8u red;
  Bit8u alpha;
};

struct VGA16bppPixel {
  /* ??? */
  template <typename T> inline void CopyRGBTo(T& out) { }
  inline bool IsTransparent() const { return false; }
};

struct VGA32bppPixel {
  Bit8u blue;
  Bit8u green;
  Bit8u red;
  Bit8u alpha;
  template <typename T> inline void CopyRGBTo(T& out) const {out.red=red; out.green=green; out.blue=blue;}
};
struct VGAUnder32bppPixel : VGA32bppPixel { inline bool IsTransparent() const { return true; } };
struct VGAOver32bppPixel  : VGA32bppPixel { inline bool IsTransparent() const { return (red|green|blue) == 0; } };

struct VGAPalettePixel {
  static VGA32bppPixel _vgaPalette[256];
  Bit8u index;
  template <typename T> inline void CopyRGBTo(T& out) const { _vgaPalette[index].CopyRGBTo(out); }
};
struct VGAUnderPalettePixel : VGAPalettePixel { inline bool IsTransparent() const { return true; } };
struct VGAOverPalettePixel  : VGAPalettePixel {
  static Bit8u _alphaChannelIndex;
  inline bool IsTransparent() const { return index == _alphaChannelIndex; }
};

struct PlayerPicturePixel {
  Bit8u red;
  Bit8u green;
  Bit8u blue;
  template <typename T> inline void CopyRGBTo(T& out) const {out.red=red; out.green=green; out.blue=blue;}
  inline bool IsTransparent() const {return false;}
};
}


//
// The running state of the video mixer:
// WARNING: things will blow sky high if the ReelMagic_Init()
//          is NOT called before VGA stuff happens!
//
static bool                     _videoMixerEnabled        = false; //everything is passthrough if this is false
static bool                     _mpegDictatesOutputSize   = false; //false if VGA dimensions set the output size, true if MPEG picture dimensions are the output size
static bool                     _vgaDup5Enabled           = false;

//state captured from VGA
VGA32bppPixel                   VGAPalettePixel::_vgaPalette[256];
Bit8u                           VGAOverPalettePixel::_alphaChannelIndex = 0;
static Bitu                     _vgaWidth                 = 0;
static Bitu                     _vgaHeight                = 0;
static Bitu                     _vgaBitsPerPixel          = 0; // != 0 on this variable means we have collected the first call
static float                    _vgaFramesPerSecond       = 0.0f;
static double                   _vgaRatio                 = 0.0;
static bool                     _vgaDoubleWidth           = false;
static bool                     _vgaDoubleHeight          = false;

//state captured from current/active MPEG player
static PlayerPicturePixel       _mpegPictureBuffer[SCALER_MAXWIDTH*SCALER_MAXHEIGHT];
static PlayerPicturePixel      *_mpegPictureBufferPtr     = _mpegPictureBuffer;
static Bitu                     _mpegPictureWidth         = 0;
static Bitu                     _mpegPictureHeight        = 0;

static const Bitu               VIDEOMIXER_BITSPERPIXEL = 32;  //video mixer is exclusively 32bpp on the RENDER... VGA color palette mapping is re-done here...

//current RENDER state
static void RMR_DrawLine_Passthrough(const void *src);
ReelMagic_ScalerLineHandler_t                             ReelMagic_RENDER_DrawLine = RMR_DrawLine_Passthrough; //default things to off
static ReelMagic_VideoMixerMPEGProvider                  *_requestedMpegProvider = NULL;
static ReelMagic_VideoMixerMPEGProvider                  *_activeMpegProvider = NULL;
static RenderOutputPixel                                  _finalMixedRenderLineBuffer[SCALER_MAXWIDTH];
static Bitu                                               _currentRenderLineNumber = 0;
static Bitu                                               _renderWidth             = 0;
static Bitu                                               _renderHeight            = 0;


//
//pixel mixing / underlay / overlay functions...
//
//TODO: understand the right way to detect transparency... for now, i'm taking the approach of using
//      VGA pallet index #0 if VGA mode is 8bpp and using "pure black" if VGA mode is 32bpp...
//      The upper-left RTZ menu will have transparancy around some of the buttons if the
//      "pure black" approach is taken, but not if pallet index #0 is used... I have no idea
//      how the game originally was as I don't have the hardware, but this seems like a reasonable
//      workaround to make this look nice and clean for the mean time....
//      also, as i'm carrying around an alpha channel, i should probably put that to good use...
//
template <typename VGAPixelT, typename MPEGPixelT>
static inline void MixPixel(RenderOutputPixel& out, const VGAPixelT& vga, const MPEGPixelT& mpeg) {
  if (vga.IsTransparent())
    mpeg.CopyRGBTo(out);
  else
    vga.CopyRGBTo(out);
  out.alpha = 0;
}

template <typename VGAPixelT>
static inline void MixPixel(RenderOutputPixel& out, const VGAPixelT& vga) {
  vga.CopyRGBTo(out);
  out.alpha = 0;
}

static void ClearMpegPictureBuffer(const PlayerPicturePixel& p) {
  for (Bitu i = 0; i < (sizeof(_mpegPictureBuffer) / sizeof(_mpegPictureBuffer[0])); ++i)
    _mpegPictureBuffer[i] = p;
}

static void ClearMpegPictureBuffer() {
  PlayerPicturePixel p; p.red = 0; p.green = 0; p.blue = 0;
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
static void RMR_DrawLine_Passthrough(const void *src) {
  RENDER_DrawLine(src);
}

static void RMR_DrawLine_MixerError(const void *src) {
  if (++_currentRenderLineNumber >= _renderHeight) return;
  for (Bitu i = 0; i < (sizeof(_finalMixedRenderLineBuffer) / sizeof(_finalMixedRenderLineBuffer[0])); ++i) {
    RenderOutputPixel& p = _finalMixedRenderLineBuffer[i];
    p.red = 0xFF; p.green = 0x00; p.blue = 0x00; p.alpha = 0x00;
  }
  RENDER_DrawLine(_finalMixedRenderLineBuffer);
}

//its not real enterprise C++ until yer mixing macros and templates...
#define CREATE_RMR_VGA_TYPED_FUNCTIONS(DRAWLINE_FUNC_NAME)                                                           \
  static void DRAWLINE_FUNC_NAME##_VGAO8(const void *src)  {DRAWLINE_FUNC_NAME((const VGAOverPalettePixel *)src);}   \
  static void DRAWLINE_FUNC_NAME##_VGAO32(const void *src) {DRAWLINE_FUNC_NAME((const VGAOver32bppPixel*)src);}      \
  static void DRAWLINE_FUNC_NAME##_VGAU8(const void *src)  {DRAWLINE_FUNC_NAME((const VGAUnderPalettePixel *)src);}  \
  static void DRAWLINE_FUNC_NAME##_VGAU32(const void *src) {DRAWLINE_FUNC_NAME((const VGAUnder32bppPixel*)src);}     \

#define ASSIGN_RMR_DRAWLINE_FUNCTION(DRAWLINE_FUNC_NAME, VGA_BPP, VGA_OVER) { \
  if (VGA_OVER) switch(VGA_BPP) {                                             \
  case 8:  ReelMagic_RENDER_DrawLine = &DRAWLINE_FUNC_NAME##_VGAO8;  break;   \
  case 32: ReelMagic_RENDER_DrawLine = &DRAWLINE_FUNC_NAME##_VGAO32; break;   \
  default: ReelMagic_RENDER_DrawLine = &RMR_DrawLine_MixerError;     break;   \
  }                                                                           \
  else switch(VGA_BPP) {                                                      \
  case 8:  ReelMagic_RENDER_DrawLine = &DRAWLINE_FUNC_NAME##_VGAU8;  break;   \
  case 32: ReelMagic_RENDER_DrawLine = &DRAWLINE_FUNC_NAME##_VGAU32; break;   \
  default: ReelMagic_RENDER_DrawLine = &RMR_DrawLine_MixerError;     break;   \
  }                                                                           \
}

template <typename T> static inline void RMR_DrawLine_VGAOnly(const T *src) {
  const Bitu lineWidth          = _vgaWidth;
  RenderOutputPixel * const out = _finalMixedRenderLineBuffer;
  for (Bitu i = 0; i < lineWidth; ++i)
    MixPixel(out[i], src[i]);
  RENDER_DrawLine(_finalMixedRenderLineBuffer);
} CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VGAOnly)

template <typename T> static inline void RMR_DrawLine_VGAMPEGSameSize(const T *src) {
  const Bitu lineWidth          = _vgaWidth;
  RenderOutputPixel * const out = _finalMixedRenderLineBuffer;
  for (Bitu i = 0; i < lineWidth; ++i)
    MixPixel(out[i], src[i], _mpegPictureBufferPtr[i]);
  _mpegPictureBufferPtr += _mpegPictureWidth;
  RENDER_DrawLine(_finalMixedRenderLineBuffer);
} CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VGAMPEGSameSize)

//VGA Sized Output (RENDER) functions...
template <typename T> static inline void RMR_DrawLine_VSO_MPEGDoubleVGASize(const T *src) {
  const Bitu lineWidth          = _vgaWidth;
  RenderOutputPixel * const out = _finalMixedRenderLineBuffer;
  _mpegPictureBufferPtr -= _mpegPictureWidth * (_currentRenderLineNumber++ & 1);
  for (Bitu i = 0; i < lineWidth; ++i) {
    MixPixel(out[i], src[i], _mpegPictureBufferPtr[i >> 1]);
  }
  _mpegPictureBufferPtr += _mpegPictureWidth;
  RENDER_DrawLine(_finalMixedRenderLineBuffer);
} CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VSO_MPEGDoubleVGASize)

template <typename T> static inline void RMR_DrawLine_VSO_VGAMPEGSameWidthSkip6Vertical(const T *src) {
  const Bitu lineWidth          = _vgaWidth;
  RenderOutputPixel * const out = _finalMixedRenderLineBuffer;
  for (Bitu i = 0; i < lineWidth; ++i)
    MixPixel(out[i], src[i], _mpegPictureBufferPtr[i]);
  _mpegPictureBufferPtr += _mpegPictureWidth;
  if (++_currentRenderLineNumber >= 6) {
    _currentRenderLineNumber = 0;
    _mpegPictureBufferPtr += _mpegPictureWidth;
  }
  RENDER_DrawLine(_finalMixedRenderLineBuffer);
} CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VSO_VGAMPEGSameWidthSkip6Vertical)

template <typename T> static inline void RMR_DrawLine_VSO_VGAMPEGDoubleSameWidthSkip6Vertical(const T *src) {
  const Bitu lineWidth          = _vgaWidth;
  RenderOutputPixel * const out = _finalMixedRenderLineBuffer;
  _mpegPictureBufferPtr -= _mpegPictureWidth * (_currentRenderLineNumber & 1);
  for (Bitu i = 0; i < lineWidth; ++i)
    MixPixel(out[i], src[i], _mpegPictureBufferPtr[i >> 1]);
  _mpegPictureBufferPtr += _mpegPictureWidth;
  if (++_currentRenderLineNumber >= 6) {
    _currentRenderLineNumber = 0;
    _mpegPictureBufferPtr += _mpegPictureWidth;
  }
  RENDER_DrawLine(_finalMixedRenderLineBuffer);
} CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VSO_VGAMPEGDoubleSameWidthSkip6Vertical)



//VGA "Dup 5" functions

template <typename T> static inline void RMR_DrawLine_VGAOnlyDup5Vertical(const T *src) {
  const Bitu lineWidth          = _vgaWidth;
  RenderOutputPixel * const out = _finalMixedRenderLineBuffer;
  for (Bitu i = 0; i < lineWidth; ++i)
    MixPixel(out[i], src[i]);
  if (++_currentRenderLineNumber >= 5) {
    _currentRenderLineNumber = 0;
    RENDER_DrawLine(_finalMixedRenderLineBuffer);
  }
  RENDER_DrawLine(_finalMixedRenderLineBuffer);
} CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VGAOnlyDup5Vertical)

template <typename T> static inline void RMR_DrawLine_VGADup5VerticalMPEGSameSize(const T *src) {
  const Bitu lineWidth          = _vgaWidth;
  RenderOutputPixel * const out = _finalMixedRenderLineBuffer;
  for (Bitu i = 0; i < lineWidth; ++i)
    MixPixel(out[i], src[i], _mpegPictureBufferPtr[i]);
  _mpegPictureBufferPtr += _mpegPictureWidth;
  RENDER_DrawLine(_finalMixedRenderLineBuffer);

  if (++_currentRenderLineNumber >= 5) {
    _currentRenderLineNumber = 0;
    RMR_DrawLine_VGADup5VerticalMPEGSameSize(src);
    _currentRenderLineNumber = 0;
  }
} CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VGADup5VerticalMPEGSameSize)



//
// the catch-all un-optimized MPEG scaling function...
//
// WARNING: this ended up being kinda hacky...
//          should we perheps re-think this; possibly using a
//          lookup table computed at mode change time?
//
static Bitu _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_WidthRatio     = 0;
static Bitu _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_HeightRatio    = 0;
static Bitu _RMR_DrawLine_VSO_GeneralResizeMPEGToVGADup5LineCounter = 0;
static void Initialize_RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_Dimensions() {
  _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_WidthRatio   = _mpegPictureWidth << 12;
  _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_WidthRatio  /= _renderWidth;
  _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_HeightRatio  = _mpegPictureHeight << 12;
  _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_HeightRatio /= _renderHeight;
}
template <typename T> static inline void RMR_DrawLine_VSO_GeneralResizeMPEGToVGA(const T *src) {
  const Bitu lineWidth          = _vgaWidth;
  RenderOutputPixel * const out = _finalMixedRenderLineBuffer;
  for (Bitu i = 0; i < lineWidth; ++i)
    MixPixel(out[i], src[i], _mpegPictureBufferPtr[(i * _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_WidthRatio) >> 12]);
  _mpegPictureBufferPtr =
    &_mpegPictureBuffer[_mpegPictureWidth *
      ((++_currentRenderLineNumber * _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_HeightRatio) >> 12)];
  RENDER_DrawLine(_finalMixedRenderLineBuffer);
} CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VSO_GeneralResizeMPEGToVGA)

template <typename T> static inline void RMR_DrawLine_VSO_GeneralResizeMPEGToVGADup5(const T *src) {
  const Bitu lineWidth          = _vgaWidth;
  RenderOutputPixel * const out = _finalMixedRenderLineBuffer;
  for (Bitu i = 0; i < lineWidth; ++i)
    MixPixel(out[i], src[i], _mpegPictureBufferPtr[(i * _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_WidthRatio) >> 12]);
  _mpegPictureBufferPtr =
    &_mpegPictureBuffer[_mpegPictureWidth *
      ((++_currentRenderLineNumber * _RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_HeightRatio) >> 12)];
  RENDER_DrawLine(_finalMixedRenderLineBuffer);

  if (++_RMR_DrawLine_VSO_GeneralResizeMPEGToVGADup5LineCounter >= 5) {
    _RMR_DrawLine_VSO_GeneralResizeMPEGToVGADup5LineCounter = 0;
    RMR_DrawLine_VSO_GeneralResizeMPEGToVGADup5(src);
    _RMR_DrawLine_VSO_GeneralResizeMPEGToVGADup5LineCounter = 0;
  }
} CREATE_RMR_VGA_TYPED_FUNCTIONS(RMR_DrawLine_VSO_GeneralResizeMPEGToVGADup5)






static void SetupVideoMixer(const bool updateRenderMode) {
  //the "_activeMpegProvider" variable serves a few purposes:
  // 1. Tells the "ReelMagic_RENDER_StartUpdate()" function which player to call "OnVerticalRefresh()" on
  // 2. Prevents the "ReelMagic_RENDER_StartUpdate()" function from calling "OnVerticalRefresh()" if:
  //    . We have not yet received a VGA mode/configuration
  //    . The video mixer is in an error state
  _activeMpegProvider = NULL; //no MPEG activation unless all is good...

  //need at least one call from VGA before we can do this...
  if (_vgaBitsPerPixel == 0) return;

  if (!_videoMixerEnabled) {
    //video mixer is disabled... VGA mode dictates RENDER mode just like "normal dosbox"
    ReelMagic_RENDER_DrawLine = &RMR_DrawLine_Passthrough;
    RENDER_SetSize(_vgaWidth, _vgaHeight, _vgaBitsPerPixel, _vgaFramesPerSecond, _vgaRatio, _vgaDoubleWidth, _vgaDoubleHeight);
    LOG(LOG_REELMAGIC, LOG_NORMAL)("Video Mixer is Disabled. Passed through VGA RENDER_SetSize()");
    return;
  }

  //cache the current MPEG picture size...
  ReelMagic_VideoMixerMPEGProvider * const mpeg = _requestedMpegProvider;
  if (mpeg != NULL) {
    _mpegPictureWidth  = mpeg->GetAttrs().PictureSize.Width;
    _mpegPictureHeight = mpeg->GetAttrs().PictureSize.Height;
  }

  //video mixer is enabled... figure out the operational mode of this thing based on
  //a miserable combination of variables...
  if (_mpegDictatesOutputSize && mpeg) {
    _renderWidth = _mpegPictureWidth;
    _renderHeight = _mpegPictureWidth;
  }
  else if (_vgaDup5Enabled) {
    _renderWidth  = _vgaWidth;
    _renderHeight = (_vgaHeight / 5) * 6;
  }
  else {
    _renderWidth  = _vgaWidth;
    _renderHeight = _vgaHeight;
  }

  //check to make sure we have enough horizontal line buffer for the current VGA mode...
  const Bitu maxRenderWidth = sizeof(_finalMixedRenderLineBuffer) / sizeof(_finalMixedRenderLineBuffer[0]);
  if (_renderWidth > maxRenderWidth) {
    LOG(LOG_REELMAGIC, LOG_ERROR)("Video Mixing Buffers Too Small for VGA Mode -- Can't output video!");
    ReelMagic_RENDER_DrawLine = &RMR_DrawLine_MixerError;
    _renderWidth  = 320;
    _renderHeight = 240;
    RENDER_SetSize(_renderWidth, _renderHeight, VIDEOMIXER_BITSPERPIXEL, _vgaFramesPerSecond, _vgaRatio, _vgaDoubleWidth, _vgaDoubleHeight);
    return;
  }

  //set the RENDER mode only if requested...
  if (updateRenderMode)
    RENDER_SetSize(_renderWidth, _renderHeight, VIDEOMIXER_BITSPERPIXEL, _vgaFramesPerSecond, _vgaRatio, _vgaDoubleWidth, _vgaDoubleHeight);

  // if no active player, set the VGA only function... the difference between this and
  // "passthrough mode" is that this keeps the video mixer enabled with a RENDER output
  // color depth of 32 bits to eliminate any flickering associated with the RENDER_SetSize()
  // call when starting/stopping a video to give the user that smooth hardware decoder feel :-)
  if ((!mpeg) || (!mpeg->GetConfig().VideoOutputVisible)) {
    if (_vgaDup5Enabled) {
      ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VGAOnlyDup5Vertical, _vgaBitsPerPixel, true);
    }
    else {
      ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VGAOnly, _vgaBitsPerPixel, true);
    }
    _activeMpegProvider = mpeg;
    LOG(LOG_REELMAGIC, LOG_NORMAL)("Video Mixer Mode VGA Only (vga=%ux%u mpeg=off render=%ux%u)", (unsigned)_vgaWidth, (unsigned)_vgaHeight, (unsigned)_renderWidth, (unsigned)_renderHeight);
    return;
  }


  //choose a RENDER draw function...
  const bool vgaOver = mpeg->GetConfig().UnderVga;
  const char * modeStr = "UNKNOWN";
  if (_mpegDictatesOutputSize) {
    E_Exit("MPEG output size not yet implemented!");
  }
  else {
    if (_vgaDup5Enabled) {
      if ((_renderWidth != _mpegPictureWidth) || (_renderHeight != _mpegPictureHeight)) {
        modeStr = "Generic Unoptimized MPEG Resize to DUP5 VGA Pictures";
        Initialize_RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_Dimensions();
        ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VSO_GeneralResizeMPEGToVGADup5, _vgaBitsPerPixel, vgaOver);
      }
      else {
        modeStr = "Matching Sized MPEG to DUP5 VGA Pictures";
        ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VGADup5VerticalMPEGSameSize, _vgaBitsPerPixel, vgaOver);
      }
    }
    else if ((_vgaWidth == _mpegPictureWidth) && (_vgaHeight == _mpegPictureHeight)) {
      modeStr = "Matching Sized MPEG to VGA Pictures";
      ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VGAMPEGSameSize, _vgaBitsPerPixel, vgaOver);
    }
    else if ((_vgaWidth == (_mpegPictureWidth*2)) && (_vgaHeight == ((_mpegPictureHeight*2)))) {
      modeStr = "Double Sized MPEG to VGA Pictures";
      ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VSO_MPEGDoubleVGASize, _vgaBitsPerPixel, vgaOver);
    }
    else if ((_vgaWidth == _mpegPictureWidth) && ((_mpegPictureHeight / (_mpegPictureHeight - _vgaHeight)) == 6)) {
      modeStr = "Matching Sized MPEG to VGA Pictures, skipping every 6th MPEG line";
      ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VSO_VGAMPEGSameWidthSkip6Vertical, _vgaBitsPerPixel, vgaOver);
    }
    else if ((_vgaWidth == (_mpegPictureWidth*2)) && (((_mpegPictureHeight*2) / ((_mpegPictureHeight*2) - _vgaHeight)) == 6)) {
      modeStr = "Double Sized MPEG to VGA Pictures, skipping every 6th MPEG line";
      ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VSO_VGAMPEGDoubleSameWidthSkip6Vertical, _vgaBitsPerPixel, vgaOver);
    }
    else {
      modeStr = "Generic Unoptimized MPEG Resize";
      Initialize_RMR_DrawLine_VSO_GeneralResizeMPEGToVGA_Dimensions();
      ASSIGN_RMR_DRAWLINE_FUNCTION(RMR_DrawLine_VSO_GeneralResizeMPEGToVGA, _vgaBitsPerPixel, vgaOver);
    }
  }

  //log the mode we are now in
  if (ReelMagic_RENDER_DrawLine == &RMR_DrawLine_MixerError) modeStr = "Error";
  else _activeMpegProvider = mpeg;
  LOG(LOG_REELMAGIC, LOG_NORMAL)("Video Mixer Mode %s (vga=%ux%u mpeg=%ux%u render=%ux%u)", modeStr, (unsigned)_vgaWidth, (unsigned)_vgaHeight, (unsigned)_mpegPictureWidth, (unsigned)_mpegPictureHeight, (unsigned)_renderWidth, (unsigned)_renderHeight);
}





//
// The RENDER_*() interceptors begin here...
//
void ReelMagic_RENDER_SetPal(Bit8u entry,Bit8u red,Bit8u green,Bit8u blue) {
  VGA32bppPixel& p = VGAPalettePixel::_vgaPalette[entry];
  p.red    = red;
  p.green  = green;
  p.blue   = blue;
  p.alpha  = 0;
  RENDER_SetPal(entry, red, green, blue);
}

void ReelMagic_RENDER_SetSize(Bitu width,Bitu height,Bitu bpp,float fps,double ratio,bool dblw,bool dblh) {
  _vgaWidth             = width;
  _vgaHeight            = height;
  _vgaBitsPerPixel      = bpp;
  _vgaFramesPerSecond   = fps;
  _vgaRatio             = ratio;
  _vgaDoubleWidth       = dblw;
  _vgaDoubleHeight      = dblh;

  SetupVideoMixer(!_mpegDictatesOutputSize);
}

bool ReelMagic_RENDER_StartUpdate(void) {
  if (_activeMpegProvider) {
    VGAOverPalettePixel::_alphaChannelIndex = _activeMpegProvider->GetConfig().VgaAlphaIndex;
    _activeMpegProvider->OnVerticalRefresh(_mpegPictureBuffer, _vgaFramesPerSecond);
  }
  _currentRenderLineNumber = 0;
  _mpegPictureBufferPtr = _mpegPictureBuffer;
  return RENDER_StartUpdate();
}

void ReelMagic_ResetVideoMixer() {
  _requestedMpegProvider = NULL;
  ClearMpegPictureBuffer();
}

void ReelMagic_SetVideoMixerEnabled(const bool enabled) {
  if (!enabled) ReelMagic_ResetVideoMixer(); //defensive
  if (enabled == _videoMixerEnabled) return;
  _videoMixerEnabled = enabled;
  LOG(LOG_REELMAGIC, LOG_NORMAL)("%s Video Mixer", enabled ? "Enabling" : "Disabling");
  SetupVideoMixer(true);
}

ReelMagic_VideoMixerMPEGProvider *ReelMagic_GetVideoMixerMPEGProvider() {
  return _requestedMpegProvider;
}

void ReelMagic_SetVideoMixerMPEGProvider(ReelMagic_VideoMixerMPEGProvider * const provider) {
  if (provider == NULL) {
    _requestedMpegProvider = NULL;
    SetupVideoMixer(_mpegDictatesOutputSize);
    return;
  }

  //check to make sure that our MPEG picture buffer is big enough for the provider's MPEG picture size
  const Bitu mpegPictureSize     = provider->GetAttrs().PictureSize.Width * provider->GetAttrs().PictureSize.Height;
  const Bitu maxMpegPictureSize  = sizeof(_mpegPictureBuffer) / sizeof(_mpegPictureBuffer[0]);
  if (mpegPictureSize > maxMpegPictureSize) {
    LOG(LOG_REELMAGIC, LOG_ERROR)("Video Mixing Buffers Too Small for MPEG Video Size. Reject Player Push");
    return;
  }

  //clear the MPEG picture buffer when not replacing an existing provider
  if (_requestedMpegProvider == NULL)
    ClearMpegPictureBuffer();

  //set the new requested provider
  _requestedMpegProvider = provider;
  
  //update the video rendering mode if necessary...
  SetupVideoMixer(_mpegDictatesOutputSize);
}

void ReelMagic_InitVideoMixer(Section* sec) {
  Section_prop * section=static_cast<Section_prop *>(sec);
  //
  _vgaDup5Enabled = section->Get_bool("vgadup5hack");
}
