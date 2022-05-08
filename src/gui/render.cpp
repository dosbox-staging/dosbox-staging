/*
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

#include "dosbox.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>

#include <sys/types.h>

#include "video.h"
#include "render.h"
#include "setup.h"
#include "control.h"
#include "mapper.h"
#include "cross.h"
#include "hardware.h"
#include "support.h"
#include "shell.h"
#include "string_utils.h"
#include "vga.h"

#include "render_crt_glsl.h"
#include "render_glsl.h"
#include "render_scalers.h"

Render_t render;
ScalerLineHandler_t RENDER_DrawLine;

static void RENDER_CallBack( GFX_CallBackFunctions_t function );

static void Check_Palette(void) {
	/* Clean up any previous changed palette data */
	if (render.pal.changed) {
		memset(render.pal.modified, 0, sizeof(render.pal.modified));
		render.pal.changed = false;
	}
	if (render.pal.first>render.pal.last) 
		return;
	Bitu i;
	switch (render.scale.outMode) {
	case scalerMode8:
		break;
	case scalerMode15:
	case scalerMode16:
		for (i=render.pal.first;i<=render.pal.last;i++) {
			uint8_t r=render.pal.rgb[i].red;
			uint8_t g=render.pal.rgb[i].green;
			uint8_t b=render.pal.rgb[i].blue;
			uint16_t newPal = GFX_GetRGB(r,g,b);
			if (newPal != render.pal.lut.b16[i]) {
				render.pal.changed = true;
				render.pal.modified[i] = 1;
				render.pal.lut.b16[i] = newPal;
			}
		}
		break;
	case scalerMode32:
	default:
		for (i=render.pal.first;i<=render.pal.last;i++) {
			uint8_t r=render.pal.rgb[i].red;
			uint8_t g=render.pal.rgb[i].green;
			uint8_t b=render.pal.rgb[i].blue;
			uint32_t newPal = GFX_GetRGB(r,g,b);
			if (newPal != render.pal.lut.b32[i]) {
				render.pal.changed = true;
				render.pal.modified[i] = 1;
				render.pal.lut.b32[i] = newPal;
			}
		}
		break;
	}
	/* Setup pal index to startup values */
	render.pal.first=256;
	render.pal.last=0;
}

void RENDER_SetPal(uint8_t entry,uint8_t red,uint8_t green,uint8_t blue) {
	render.pal.rgb[entry].red=red;
	render.pal.rgb[entry].green=green;
	render.pal.rgb[entry].blue=blue;
	if (render.pal.first>entry) render.pal.first=entry;
	if (render.pal.last<entry) render.pal.last=entry;
}

static void RENDER_EmptyLineHandler(const void *)
{}

static void RENDER_StartLineHandler(const void * s) {
	if (s) {
		const Bitu *src = (Bitu*)s;
		Bitu *cache = (Bitu*)(render.scale.cacheRead);
		for (Bits x=render.src.start;x>0;) {
			const auto src_ptr = reinterpret_cast<const uint8_t *>(src);
			const auto src_val = read_unaligned_size_t(src_ptr);
			if (GCC_UNLIKELY(src_val != cache[0])) {
				if (!GFX_StartUpdate(render.scale.outWrite, render.scale.outPitch)) {
					RENDER_DrawLine = RENDER_EmptyLineHandler;
					return;
				}
				render.scale.outWrite += render.scale.outPitch * Scaler_ChangedLines[0];
				RENDER_DrawLine = render.scale.lineHandler;
				RENDER_DrawLine( s );
				return;
			}
			x--; src++; cache++;
		}
	}
	render.scale.cacheRead += render.scale.cachePitch;
	Scaler_ChangedLines[0] += Scaler_Aspect[ render.scale.inLine ];
	render.scale.inLine++;
	render.scale.outLine++;
}

static void RENDER_FinishLineHandler(const void * s) {
	if (s) {
		const Bitu *src = (Bitu*)s;
		Bitu *cache = (Bitu*)(render.scale.cacheRead);
		for (Bits x=render.src.start;x>0;) {
			cache[0] = src[0];
			x--; src++; cache++;
		}
	}
	render.scale.cacheRead += render.scale.cachePitch;
}


static void RENDER_ClearCacheHandler(const void * src) {
	Bitu x, width;
	uint32_t *srcLine, *cacheLine;
	srcLine = (uint32_t *)src;
	cacheLine = (uint32_t *)render.scale.cacheRead;
	width = render.scale.cachePitch / 4;
	for (x=0;x<width;x++)
		cacheLine[x] = ~srcLine[x];
	render.scale.lineHandler( src );
}

bool RENDER_StartUpdate(void) {
	if (GCC_UNLIKELY(render.updating))
		return false;
	if (GCC_UNLIKELY(!render.active))
		return false;
	if (GCC_UNLIKELY(render.frameskip.count<render.frameskip.max)) {
		render.frameskip.count++;
		return false;
	}
	render.frameskip.count=0;
	if (render.scale.inMode == scalerMode8) {
		Check_Palette();
	}
	render.scale.inLine = 0;
	render.scale.outLine = 0;
	render.scale.cacheRead = (uint8_t*)&scalerSourceCache;
	render.scale.outWrite = 0;
	render.scale.outPitch = 0;
	Scaler_ChangedLines[0] = 0;
	Scaler_ChangedLineIndex = 0;
	/* Clearing the cache will first process the line to make sure it's never the same */
	if (GCC_UNLIKELY( render.scale.clearCache) ) {
//		LOG_MSG("Clearing cache");
		//Will always have to update the screen with this one anyway, so let's update already
		if (GCC_UNLIKELY(!GFX_StartUpdate( render.scale.outWrite, render.scale.outPitch )))
			return false;
		render.fullFrame = true;
		render.scale.clearCache = false;
		RENDER_DrawLine = RENDER_ClearCacheHandler;
	} else {
		if (render.pal.changed) {
			/* Assume pal changes always do a full screen update anyway */
			if (GCC_UNLIKELY(!GFX_StartUpdate( render.scale.outWrite, render.scale.outPitch )))
				return false;
			RENDER_DrawLine = render.scale.linePalHandler;
			render.fullFrame = true;
		} else {
			RENDER_DrawLine = RENDER_StartLineHandler;
			if (GCC_UNLIKELY(CaptureState & (CAPTURE_IMAGE|CAPTURE_VIDEO))) 
				render.fullFrame = true;
			else
				render.fullFrame = false;
		}
	}
	render.updating = true;
	return true;
}

static void RENDER_Halt( void ) {
	RENDER_DrawLine = RENDER_EmptyLineHandler;
	GFX_EndUpdate( 0 );
	render.updating=false;
	render.active=false;
}

extern uint32_t PIC_Ticks;
void RENDER_EndUpdate( bool abort ) {
	if (GCC_UNLIKELY(!render.updating))
		return;
	RENDER_DrawLine = RENDER_EmptyLineHandler;
	if (GCC_UNLIKELY(CaptureState & (CAPTURE_IMAGE|CAPTURE_VIDEO))) {
		Bitu pitch, flags;
		flags = 0;
		if (render.src.dblw != render.src.dblh) {
			if (render.src.dblw) flags|=CAPTURE_FLAG_DBLW;
			if (render.src.dblh) flags|=CAPTURE_FLAG_DBLH;
		}
		auto fps = render.src.fps;
		pitch = render.scale.cachePitch;
		if (render.frameskip.max) {
			const double fps_skip = 1 + render.frameskip.max;
			fps /= fps_skip;
		}
		CAPTURE_AddImage(render.src.width, render.src.height, render.src.bpp,
		                 pitch, flags, static_cast<float>(fps), (uint8_t *)&scalerSourceCache,
		                 (uint8_t *)&render.pal.rgb);
	}
	if ( render.scale.outWrite ) {
		GFX_EndUpdate( abort? NULL : Scaler_ChangedLines );
		render.frameskip.hadSkip[render.frameskip.index] = 0;
	} else {
#if 0
		Bitu total = 0, i;
		render.frameskip.hadSkip[render.frameskip.index] = 1;
		for (i = 0;i<RENDER_SKIP_CACHE;i++) 
			total += render.frameskip.hadSkip[i];
		LOG_MSG( "Skipped frame %d %d", PIC_Ticks, (total * 100) / RENDER_SKIP_CACHE );
#endif
		// If we made it here, then there's nothing new to render.
		GFX_EndUpdate(nullptr);
	}
	render.frameskip.index = (render.frameskip.index + 1) & (RENDER_SKIP_CACHE - 1);
	render.updating=false;
}

static Bitu MakeAspectTable(Bitu skip,Bitu height,double scaley,Bitu miny) {
	Bitu i;
	double lines=0;
	Bitu linesadded=0;
	for (i=0;i<skip;i++)
		Scaler_Aspect[i] = 0;

	height += skip;
	for (i=skip;i<height;i++) {
		lines += scaley;
		if (lines >= miny) {
			Bitu templines = (Bitu)lines;
			lines -= templines;
			linesadded += templines;
			Scaler_Aspect[i] = templines;
		} else {
			Scaler_Aspect[i] = 0;
		}
	}
	return linesadded;
}


static void RENDER_Reset( void ) {
	Bitu width=render.src.width;
	Bitu height=render.src.height;
	bool dblw=render.src.dblw;
	bool dblh=render.src.dblh;

	double gfx_scalew;
	double gfx_scaleh;
	
	Bitu gfx_flags, xscale, yscale;
	ScalerSimpleBlock_t		*simpleBlock = &ScaleNormal1x;
	ScalerComplexBlock_t	*complexBlock = 0;
	if (render.aspect) {
		if (render.src.ratio>1.0) {
			gfx_scalew = 1;
			gfx_scaleh = render.src.ratio;
		} else {
			gfx_scalew = (1/render.src.ratio);
			gfx_scaleh = 1;
		}
	} else {
		gfx_scalew = 1;
		gfx_scaleh = 1;
	}

	/* Don't do software scaler sizes larger than 4k */
	Bitu maxsize_current_input = SCALER_MAXLINE_WIDTH/width;
	if (render.scale.size > maxsize_current_input) render.scale.size = maxsize_current_input;

	if ((dblh && dblw) || (render.scale.forced && !dblh && !dblw)) {
		/* Initialize always working defaults */
		if (render.scale.size == 2)
			simpleBlock = &ScaleNormal2x;
		else if (render.scale.size == 3)
			simpleBlock = &ScaleNormal3x;
		else
			simpleBlock = &ScaleNormal1x;
		/* Maybe override them */
#if RENDER_USE_ADVANCED_SCALERS>0
		switch (render.scale.op) {
#if RENDER_USE_ADVANCED_SCALERS>2
		case scalerOpAdvInterp:
			if (render.scale.size == 2)
				complexBlock = &ScaleAdvInterp2x;
			else if (render.scale.size == 3)
				complexBlock = &ScaleAdvInterp3x;
			break;
		case scalerOpAdvMame:
			if (render.scale.size == 2)
				complexBlock = &ScaleAdvMame2x;
			else if (render.scale.size == 3)
				complexBlock = &ScaleAdvMame3x;
			break;
		case scalerOpHQ:
			if (render.scale.size == 2)
				complexBlock = &ScaleHQ2x;
			else if (render.scale.size == 3)
				complexBlock = &ScaleHQ3x;
			break;
		case scalerOpSuperSaI:
			if (render.scale.size == 2)
				complexBlock = &ScaleSuper2xSaI;
			break;
		case scalerOpSuperEagle:
			if (render.scale.size == 2)
				complexBlock = &ScaleSuperEagle;
			break;
		case scalerOpSaI:
			if (render.scale.size == 2)
				complexBlock = &Scale2xSaI;
			break;
#endif
		case scalerOpTV:
			if (render.scale.size == 2)
				simpleBlock = &ScaleTV2x;
			else if (render.scale.size == 3)
				simpleBlock = &ScaleTV3x;
			break;
		case scalerOpRGB:
			if (render.scale.size == 2)
				simpleBlock = &ScaleRGB2x;
			else if (render.scale.size == 3)
				simpleBlock = &ScaleRGB3x;
			break;
		case scalerOpScan:
			if (render.scale.size == 2)
				simpleBlock = &ScaleScan2x;
			else if (render.scale.size == 3)
				simpleBlock = &ScaleScan3x;
			break;
		default:
			break;
		}
#endif
	} else if (dblw) {
		simpleBlock = &ScaleNormalDw;
		if (width * simpleBlock->xscale > SCALER_MAXLINE_WIDTH) {
			// This should only happen if you pick really bad values... but might be worth adding selecting a scaler that fits
			simpleBlock = &ScaleNormal1x;
		}
	} else if (dblh) {
		simpleBlock = &ScaleNormalDh;
	} else  {
forcenormal:
		complexBlock = 0;
		simpleBlock = &ScaleNormal1x;
	}
	if (complexBlock) {
#if RENDER_USE_ADVANCED_SCALERS>1
		if ((width >= SCALER_COMPLEXWIDTH - 16) || height >= SCALER_COMPLEXHEIGHT - 16) {
			LOG_MSG("Scaler can't handle this resolution, going back to normal");
			goto forcenormal;
		}
#else
		goto forcenormal;
#endif
		gfx_flags = complexBlock->gfxFlags;
		xscale = complexBlock->xscale;	
		yscale = complexBlock->yscale;
//		LOG_MSG("Scaler:%s",complexBlock->name);
	} else {
		gfx_flags = simpleBlock->gfxFlags;
		xscale = simpleBlock->xscale;	
		yscale = simpleBlock->yscale;
//		LOG_MSG("Scaler:%s",simpleBlock->name);
	}
	switch (render.src.bpp) {
	case 8:
			render.src.start = ( render.src.width * 1) / sizeof(Bitu);
			if (gfx_flags & GFX_CAN_8)
				gfx_flags |= GFX_LOVE_8;
			else
				gfx_flags |= GFX_LOVE_32;
			break;
	case 15:
			render.src.start = ( render.src.width * 2) / sizeof(Bitu);
			gfx_flags |= GFX_LOVE_15;
			gfx_flags = (gfx_flags & ~GFX_CAN_8) | GFX_RGBONLY;
			break;
	case 16:
			render.src.start = ( render.src.width * 2) / sizeof(Bitu);
			gfx_flags |= GFX_LOVE_16;
			gfx_flags = (gfx_flags & ~GFX_CAN_8) | GFX_RGBONLY;
			break;
	case 24:
		render.src.start = (render.src.width * 3) / sizeof(Bitu);
		gfx_flags |= GFX_LOVE_32;
		gfx_flags = (gfx_flags & ~GFX_CAN_8) | GFX_RGBONLY;
		break;
	case 32:
		render.src.start = (render.src.width * 4) / sizeof(Bitu);
		gfx_flags |= GFX_LOVE_32;
		gfx_flags = (gfx_flags & ~GFX_CAN_8) | GFX_RGBONLY;
		break;
	}
	gfx_flags=GFX_GetBestMode(gfx_flags);
	if (!gfx_flags) {
		if (!complexBlock && simpleBlock == &ScaleNormal1x)
			E_Exit("Failed to create a rendering output");
		else
			goto forcenormal;
	}
	width *= xscale;
	Bitu skip = complexBlock ? 1 : 0;
	if (gfx_flags & GFX_SCALING) {
		height = MakeAspectTable(skip, render.src.height, yscale, yscale );
	} else {
		if ((gfx_flags & GFX_CAN_RANDOM) && gfx_scaleh > 1) {
			gfx_scaleh *= yscale;
			height = MakeAspectTable( skip, render.src.height, gfx_scaleh, yscale );
		} else {
			gfx_flags &= ~GFX_CAN_RANDOM;		//Hardware surface when possible
			height = MakeAspectTable( skip, render.src.height, yscale, yscale);
		}
	}

	// Setup the scaler variables

	if (dblh)
		gfx_flags |= GFX_DBL_H;
	if (dblw)
		gfx_flags |= GFX_DBL_W;

#if C_OPENGL
	GFX_SetShader(render.shader_src);
#endif

	// The pixel aspect ratio of the source image, assuming 4:3 screen
	const double real_par = (width / 4.0) / (height / 3.0);
	const double user_par = (render.aspect ? real_par : 1.0);

	gfx_flags = GFX_SetSize(width, height, gfx_flags, gfx_scalew,
	                        gfx_scaleh, &RENDER_CallBack, user_par);

	if (gfx_flags & GFX_CAN_8)
		render.scale.outMode = scalerMode8;
	else if (gfx_flags & GFX_CAN_15)
		render.scale.outMode = scalerMode15;
	else if (gfx_flags & GFX_CAN_16)
		render.scale.outMode = scalerMode16;
	else if (gfx_flags & GFX_CAN_32)
		render.scale.outMode = scalerMode32;
	else
		E_Exit("Failed to create a rendering output");
	ScalerLineBlock_t *lineBlock;
	if (gfx_flags & GFX_HARDWARE) {
#if RENDER_USE_ADVANCED_SCALERS>1
		if (complexBlock) {
			lineBlock = &ScalerCache;
			render.scale.complexHandler = complexBlock->Linear[ render.scale.outMode ];
		} else
#endif
		{
			render.scale.complexHandler = 0;
			lineBlock = &simpleBlock->Linear;
		}
	} else {
#if RENDER_USE_ADVANCED_SCALERS>1
		if (complexBlock) {
			lineBlock = &ScalerCache;
			render.scale.complexHandler = complexBlock->Random[ render.scale.outMode ];
		} else
#endif
		{
			render.scale.complexHandler = 0;
			lineBlock = &simpleBlock->Random;
		}
	}
	switch (render.src.bpp) {
	case 8:
		render.scale.lineHandler = (*lineBlock)[0][render.scale.outMode];
		render.scale.linePalHandler = (*lineBlock)[5][render.scale.outMode];
		render.scale.inMode = scalerMode8;
		render.scale.cachePitch = render.src.width * 1;
		break;
	case 15:
		render.scale.lineHandler = (*lineBlock)[1][render.scale.outMode];
		render.scale.linePalHandler = 0;
		render.scale.inMode = scalerMode15;
		render.scale.cachePitch = render.src.width * 2;
		break;
	case 16:
		render.scale.lineHandler = (*lineBlock)[2][render.scale.outMode];
		render.scale.linePalHandler = 0;
		render.scale.inMode = scalerMode16;
		render.scale.cachePitch = render.src.width * 2;
		break;
	case 24:
		render.scale.lineHandler = (*lineBlock)[3][render.scale.outMode];
		render.scale.linePalHandler = 0;
		render.scale.inMode = scalerMode32;
		render.scale.cachePitch = render.src.width * 3;
		break;
	case 32:
		render.scale.lineHandler = (*lineBlock)[4][render.scale.outMode];
		render.scale.linePalHandler = 0;
		render.scale.inMode = scalerMode32;
		render.scale.cachePitch = render.src.width * 4;
		break;
	default:
		E_Exit("RENDER:Wrong source bpp %u", render.src.bpp);
	}
	render.scale.blocks = render.src.width / SCALER_BLOCKSIZE;
	render.scale.lastBlock = render.src.width % SCALER_BLOCKSIZE;
	render.scale.inHeight = render.src.height;
	/* Reset the palette change detection to it's initial value */
	render.pal.first= 0;
	render.pal.last = 255;
	render.pal.changed = false;
	memset(render.pal.modified, 0, sizeof(render.pal.modified));
	//Finish this frame using a copy only handler
	RENDER_DrawLine = RENDER_FinishLineHandler;
	render.scale.outWrite = 0;
	/* Signal the next frame to first reinit the cache */
	render.scale.clearCache = true;
	render.active=true;
}

static void RENDER_CallBack( GFX_CallBackFunctions_t function ) {
	if (function == GFX_CallBackStop) {
		RENDER_Halt( );	
		return;
	} else if (function == GFX_CallBackRedraw) {
		render.scale.clearCache = true;
		return;
	} else if ( function == GFX_CallBackReset) {
		GFX_EndUpdate( 0 );	
		RENDER_Reset();
	} else {
		E_Exit("Unhandled GFX_CallBackReset %d", function );
	}
}

void RENDER_SetSize(uint32_t width,
                    uint32_t height,
                    unsigned bpp,
                    double fps,
                    double ratio,
                    bool dblw,
                    bool dblh)
{
	RENDER_Halt( );
	if (!width || !height || width > SCALER_MAXWIDTH || height > SCALER_MAXHEIGHT) {
		return;	
	}
	if ( ratio > 1 ) {
		double target = height * ratio + 0.025;
		ratio = target / height;
	} else {
		//This would alter the width of the screen, we don't care about rounding errors here
	}
	render.src.width = width;
	render.src.height = height;
	render.src.bpp = bpp;
	render.src.dblw = dblw;
	render.src.dblh = dblh;
	render.src.fps = fps;
	render.src.ratio = ratio;
	RENDER_Reset( );
}

extern void GFX_SetTitle(int32_t cycles, int frameskip,bool paused);
static void IncreaseFrameSkip(bool pressed) {
	if (!pressed)
		return;
	if (render.frameskip.max<10) render.frameskip.max++;
	LOG_MSG("Frame Skip at %d",render.frameskip.max);
	GFX_SetTitle(-1,render.frameskip.max,false);
}

static void DecreaseFrameSkip(bool pressed) {
	if (!pressed)
		return;
	if (render.frameskip.max>0) render.frameskip.max--;
	LOG_MSG("Frame Skip at %d",render.frameskip.max);
	GFX_SetTitle(-1,render.frameskip.max,false);
}
/* Disabled as I don't want to waste a keybind for that. Might be used in the future (Qbix)
static void ChangeScaler(bool pressed) {
	if (!pressed)
		return;
	render.scale.op = (scalerOperation)((int)render.scale.op+1);
	if((render.scale.op) >= scalerLast || render.scale.size == 1) {
		render.scale.op = (scalerOperation)0;
		if(++render.scale.size > 3)
			render.scale.size = 1;
	}
	RENDER_CallBack( GFX_CallBackReset );
} */

#if C_OPENGL

// Reads the given shader path into the string
static bool read_shader(const std_fs::path &shader_path, std::string &shader_str)
{
	std::ifstream fshader(shader_path, std::ios_base::binary);
	if (!fshader.is_open())
		return false;

	std::stringstream buf;
	buf << fshader.rdbuf();
	fshader.close();
	if (buf.str().empty())
		return false;

	shader_str = buf.str();
	shader_str += '\n';
	return true;
}

static bool RENDER_GetShader(std::string &shader_path, char *old_src)
{
	// Start with the path as-is and then try from resources
	const auto candidate_paths = {std_fs::path(shader_path),
	                              std_fs::path(shader_path + ".glsl"),
	                              GetResourcePath("glshaders", shader_path),
	                              GetResourcePath("glshaders",
	                                              shader_path + ".glsl")};

	std::string s; // populated with the shader source
	for (const auto &p : candidate_paths)
		if (read_shader(p, s))
			break;

	if (s.empty()) {
		render.shader_src = nullptr;
		return false;
	}

	if (first_shell) {
		std::string pre_defs;
		const size_t count = first_shell->GetEnvCount();
		for (size_t i = 0; i < count; ++i) {
			std::string env;
			if (!first_shell->GetEnvNum(i, env))
				continue;
			if (env.compare(0, 9, "GLSHADER_") == 0) {
				const auto brk = env.find('=');
				if (brk == std::string::npos)
					continue;
				env[brk] = ' ';
				pre_defs += "#define " + env.substr(9) + '\n';
			}
		}
		if (pre_defs.length()) {
			// if "#version" occurs it must be before anything
			// except comments and whitespace
			auto pos = s.find("#version ");

			if (pos != std::string::npos)
				pos = s.find('\n', pos + 9);

			s.insert(pos, pre_defs);
		}
	}
	// keep the same buffer if contents aren't different
	char *src = nullptr;
	if (!old_src || s != old_src) {
		src = strdup(s.c_str());
		if (!src) {
			LOG_ERR("RENDER: Couldn't copy shader source");
		}
	} else {
		src = old_src;
	}
	render.shader_src = src;
	return (render.shader_src != nullptr);
}

static void parse_shader_options(const std::string &shader_src) {
	try {
		const std::regex re("^\\s*#pragma\\s+(\\w+)");
		std::sregex_iterator next(shader_src.begin(), shader_src.end(), re);
		const std::sregex_iterator end;

		while (next != end) {
			std::smatch match = *next;
			auto pragma = match[1].str();
			if (pragma == "use_srgb_texture")
				render.shader_opts.use_srgb_texture = true;
			else if (pragma == "use_srgb_framebuffer")
				render.shader_opts.use_srgb_framebuffer = true;
			++next;
		}
	} catch (std::regex_error &e) {
		LOG_ERR("Regex error while parsing OpenGL shader for pragmas: %d", e.code());
	}
}

bool RENDER_UseSRGBTexture()
{
	return render.shader_opts.use_srgb_texture;
}

bool RENDER_UseSRGBFramebuffer()
{
	return render.shader_opts.use_srgb_framebuffer;
}

#endif

void RENDER_Init(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);

	//For restarting the renderer.
	static bool running = false;
	bool aspect = render.aspect;
	Bitu scalersize = render.scale.size;
	bool scalerforced = render.scale.forced;
	scalerOperation_t scaleOp = render.scale.op;

	render.pal.first=256;
	render.pal.last=0;
	render.aspect=section->Get_bool("aspect");
	render.frameskip.max=section->Get_int("frameskip");
	render.frameskip.count=0;
	VGA_SetMonoPalette(section->Get_string("monochrome_palette"));
	std::string cline;
	std::string scaler;
	//Check for commandline paramters and parse them through the configclass so they get checked against allowed values
	if (control->cmdline->FindString("-scaler",cline,true)) {
		section->HandleInputline(std::string("scaler=") + cline);
	} else if (control->cmdline->FindString("-forcescaler",cline,true)) {
		section->HandleInputline(std::string("scaler=") + cline + " forced");
	}
	   
	Prop_multival* prop = section->Get_multival("scaler");
	scaler = prop->GetSection()->Get_string("type");
	std::string f = prop->GetSection()->Get_string("force");
	render.scale.forced = false;
	if(f == "forced") render.scale.forced = true;
   
	const bool in_pixel_perfect_mode = (GFX_GetBestMode(0) & GFX_UNITY_SCALE);

	if (scaler == "none" || in_pixel_perfect_mode) { render.scale.op = scalerOpNormal;render.scale.size = 1; }
	else if (scaler == "normal2x") { render.scale.op = scalerOpNormal;render.scale.size = 2; }
	else if (scaler == "normal3x") { render.scale.op = scalerOpNormal;render.scale.size = 3; }
#if RENDER_USE_ADVANCED_SCALERS>2
	else if (scaler == "advmame2x") { render.scale.op = scalerOpAdvMame;render.scale.size = 2; }
	else if (scaler == "advmame3x") { render.scale.op = scalerOpAdvMame;render.scale.size = 3; }
	else if (scaler == "advinterp2x") { render.scale.op = scalerOpAdvInterp;render.scale.size = 2; }
	else if (scaler == "advinterp3x") { render.scale.op = scalerOpAdvInterp;render.scale.size = 3; }
	else if (scaler == "hq2x") { render.scale.op = scalerOpHQ;render.scale.size = 2; }
	else if (scaler == "hq3x") { render.scale.op = scalerOpHQ;render.scale.size = 3; }
	else if (scaler == "2xsai") { render.scale.op = scalerOpSaI;render.scale.size = 2; }
	else if (scaler == "super2xsai") { render.scale.op = scalerOpSuperSaI;render.scale.size = 2; }
	else if (scaler == "supereagle") { render.scale.op = scalerOpSuperEagle;render.scale.size = 2; }
#endif
#if RENDER_USE_ADVANCED_SCALERS>0
	else if (scaler == "tv2x") { render.scale.op = scalerOpTV;render.scale.size = 2; }
	else if (scaler == "tv3x") { render.scale.op = scalerOpTV;render.scale.size = 3; }
	else if (scaler == "rgb2x"){ render.scale.op = scalerOpRGB;render.scale.size = 2; }
	else if (scaler == "rgb3x"){ render.scale.op = scalerOpRGB;render.scale.size = 3; }
	else if (scaler == "scan2x"){ render.scale.op = scalerOpScan;render.scale.size = 2; }
	else if (scaler == "scan3x"){ render.scale.op = scalerOpScan;render.scale.size = 3; }
#endif

#if C_OPENGL
	assert(control);
	const Section *sdl_sec = control->GetSection("sdl");
	assert(sdl_sec);
	const bool using_opengl = starts_with("opengl",
	                                      sdl_sec->GetPropValue("output"));
	char* shader_src = render.shader_src;
	Prop_path *sh = section->Get_path("glshader");
	f = (std::string)sh->GetValue();

	// if 'default' is given, use the sharp shader
	if (f == "default")
		f = "sharp";

	if (f.empty() || f == "none")
		render.shader_src = NULL;
	else if (!RENDER_GetShader(sh->realpath, shader_src) &&
	         (sh->realpath == f || !RENDER_GetShader(f, shader_src))) {
		sh->SetValue("none");
		LOG_WARNING("RENDER: Shader file '%s' not found", f.c_str());
	} else if (using_opengl) {
		LOG_MSG("RENDER: Using GLSL shader '%s'", f.c_str());
		parse_shader_options(std::string(render.shader_src));
	}
	if (shader_src!=render.shader_src) free(shader_src);
#endif

	//If something changed that needs a ReInit
	// Only ReInit when there is a src.bpp (fixes crashes on startup and directly changing the scaler without a screen specified yet)
	if(running && render.src.bpp && ((render.aspect != aspect) || (render.scale.op != scaleOp) || 
				  (render.scale.size != scalersize) || (render.scale.forced != scalerforced) ||
#if C_OPENGL
				  (render.shader_src != shader_src) ||
#endif
				   render.scale.forced))
		RENDER_CallBack( GFX_CallBackReset );

	if(!running) render.updating=true;
	running = true;

	MAPPER_AddHandler(DecreaseFrameSkip, SDL_SCANCODE_UNKNOWN, 0,
	                  "decfskip", "Dec Fskip");
	MAPPER_AddHandler(IncreaseFrameSkip, SDL_SCANCODE_UNKNOWN, 0,
	                  "incfskip", "Inc Fskip");
	GFX_SetTitle(-1,render.frameskip.max,false);
}
