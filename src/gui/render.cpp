/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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

/* $Id: render.cpp,v 1.39 2006-02-01 07:22:45 harekiet Exp $ */

#include <sys/types.h>
#include <dirent.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include "video.h"
#include "render.h"
#include "setup.h"
#include "mapper.h"
#include "cross.h"
#include "hardware.h"
#include "support.h"

#include "render_scalers.h"

Render_t render;

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
		GFX_SetPalette(render.pal.first,render.pal.last-render.pal.first+1,(GFX_PalEntry *)&render.pal.rgb[render.pal.first]);
		break;
	case scalerMode15:
	case scalerMode16:
		for (i=render.pal.first;i<=render.pal.last;i++) {
			Bit8u r=render.pal.rgb[i].red;
			Bit8u g=render.pal.rgb[i].green;
			Bit8u b=render.pal.rgb[i].blue;
			Bit16u newPal = GFX_GetRGB(r,g,b);
			if (newPal != render.pal.lut.b16[i]) {
				render.pal.changed = true;
				render.pal.lut.b16[i] = newPal;
			}
		}
		break;
	case scalerMode32:
	default:
		for (i=render.pal.first;i<=render.pal.last;i++) {
			Bit8u r=render.pal.rgb[i].red;
			Bit8u g=render.pal.rgb[i].green;
			Bit8u b=render.pal.rgb[i].blue;
			Bit32u newPal = GFX_GetRGB(r,g,b);
			if (newPal != render.pal.lut.b32[i]) {
				render.pal.changed = true;
				render.pal.lut.b32[i] = newPal;
			}
		}
		break;
	}
	/* Setup pal index to startup values */
	render.pal.first=256;
	render.pal.last=0;
}

static void RENDER_ResetPal(void) {
	render.pal.first=0;
	render.pal.last=255;
}

void RENDER_SetPal(Bit8u entry,Bit8u red,Bit8u green,Bit8u blue) {
	render.pal.rgb[entry].red=red;
	render.pal.rgb[entry].green=green;
	render.pal.rgb[entry].blue=blue;
	if (render.pal.first>entry) render.pal.first=entry;
	if (render.pal.last<entry) render.pal.last=entry;
}

static void RENDER_EmptyCacheHandler(const void * src) {
}
static void RENDER_EmptyLineHandler(void) {
}


static void RENDER_ClearCacheHandler(const void * src) {
	Bitu x, width;
	Bit32u *srcLine, *cacheLine;
	srcLine = (Bit32u *)src;
	switch (render.scale.inMode) {
	case scalerMode8:
		width = render.src.width / 4;
		cacheLine = (Bit32u*)scalerSourceCache.b8[render.scale.inLine];
		break;
	case scalerMode15:
	case scalerMode16:
		width = render.src.width / 2;
		cacheLine = (Bit32u*)scalerSourceCache.b16[render.scale.inLine];
		break;
	case scalerMode32:
		width = render.src.width;
		cacheLine = (Bit32u*)scalerSourceCache.b32[render.scale.inLine];
		break;
	}
	for (x=0;x<width;x++)
		cacheLine[x] = ~srcLine[x];
	render.scale.clearCacheHandler( src );
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
	if (GCC_UNLIKELY(!GFX_StartUpdate(render.scale.outWrite,render.scale.outPitch)))
		return false;
	render.scale.inLine = 0;
	render.scale.outLine = 0;
	Scaler_ChangedLines[0] = 0;
	Scaler_ChangedLineIndex = 0;

	ScalerCacheBlock_t *cacheBlock;
	switch (render.scale.inMode) {
	case scalerMode8:
		cacheBlock = !render.pal.changed  ? &ScalerCache_8 : &ScalerCache_8Pal;
		break;
	case scalerMode15:
		cacheBlock = &ScalerCache_15;
		break;
	case scalerMode16:
		cacheBlock = &ScalerCache_16;
		break;
	case scalerMode32:
		cacheBlock = &ScalerCache_32;
		break;
	default:
		return false;
	}
	if (render.scale.lineFlags & ScaleFlagSimple) {
		render.scale.cacheHandler = cacheBlock->simple[render.scale.outMode];
	} else {
		render.scale.cacheHandler = cacheBlock->complex[render.scale.outMode];
	}
	/* Clearing the cache will first process the line to make sure it's never the same */
	if (GCC_UNLIKELY( render.scale.clearCache) ) {
//		LOG_MSG("Clearing cache");
		render.scale.clearCache = false;
		render.scale.clearCacheHandler = render.scale.cacheHandler;
		render.scale.cacheHandler = RENDER_ClearCacheHandler;
	}
	render.scale.lineHandler = render.scale.currentHandler;
	render.updating=true;
	return true;
}

void RENDER_EndUpdate( bool fullUpdate ) {
	if (!render.updating)
		return;
	render.scale.cacheHandler = RENDER_EmptyCacheHandler;
	render.scale.lineHandler = RENDER_EmptyLineHandler;
	if (CaptureState & (CAPTURE_IMAGE|CAPTURE_VIDEO)) {
		Bitu pitch, flags;
		switch (render.scale.inMode) {
		case scalerMode8:
			pitch = sizeof(scalerSourceCache.b8[0]);
			break;
		case scalerMode15:
		case scalerMode16:
			pitch = sizeof(scalerSourceCache.b16[0]);
			break;
		case scalerMode32:
			pitch = sizeof(scalerSourceCache.b32[0]);
			break;
		}
		flags = 0;
		if (render.src.dblw != render.src.dblh) {
			if (render.src.dblw) flags|=CAPTURE_FLAG_DBLW;
			if (render.src.dblh) flags|=CAPTURE_FLAG_DBLH;
		}
		CAPTURE_AddImage( render.src.width, render.src.height, render.src.bpp, pitch,
			flags, render.src.fps, scalerSourceCache.b8[0], (Bit8u*)&render.pal.rgb );
	}
	GFX_EndUpdate( fullUpdate ? Scaler_ChangedLines : 0);
	render.updating=false;
}

void RENDER_DrawLine( const void *src ) {
	render.scale.cacheHandler( src );
	render.scale.lineHandler();
}

static Bitu MakeAspectTable(Bitu height,double scaley,Bitu miny) {
	double lines=0;
	Bitu linesadded=0;
	for (Bitu i=0;i<height;i++) {
		lines+=scaley;
		if (lines>=miny) {
			Bitu templines=(Bitu)lines;
			lines-=templines;
			linesadded+=templines;
			Scaler_Aspect[1+i]=templines-miny;
		} else Scaler_Aspect[1+i]=0;
	}
	return linesadded;
}

void RENDER_ReInit( bool stopIt ) {
	if (render.updating) {
		/* Still updating the current screen, shut it down correctly */
		RENDER_EndUpdate( false );
	}

	if (stopIt)
		return;

	Bitu width=render.src.width;
	Bitu height=render.src.height;
	bool dblw=render.src.dblw;
	bool dblh=render.src.dblh;

	double gfx_scalew;
	double gfx_scaleh;
	
	Bitu gfx_flags;
	ScalerLineBlock_t *lineBlock;
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
	lineBlock = &ScaleNormal;
	if (dblh && dblw) {
		/* Initialize always working defaults */
		if (render.scale.size == 2)
			lineBlock = &ScaleNormal2x;
		else if (render.scale.size == 3)
			lineBlock = &ScaleNormal3x;
		else
			lineBlock = &ScaleNormal;
		/* Maybe override them */
		switch (render.scale.op) {
		case scalerOpAdvInterp:
			if (render.scale.size == 2)
				lineBlock = &ScaleAdvInterp2x;
			else if (render.scale.size == 3)
				lineBlock = &ScaleAdvInterp3x;
			break;
		case scalerOpAdvMame:
			if (render.scale.size == 2)
				lineBlock = &ScaleAdvMame2x;
			else if (render.scale.size == 3)
				lineBlock = &ScaleAdvMame3x;
			break;
		case scalerOpTV:
			if (render.scale.size == 2)
				lineBlock = &ScaleTV2x;
			else if (render.scale.size == 3)
				lineBlock = &ScaleTV3x;
			break;
		case scalerOpRGB:
			if (render.scale.size == 2)
				lineBlock = &ScaleRGB2x;
			else if (render.scale.size == 3)
				lineBlock = &ScaleRGB3x;
			break;
		case scalerOpScan:
			if (render.scale.size == 2)
				lineBlock = &ScaleScan2x;
			else if (render.scale.size == 3)
				lineBlock = &ScaleScan3x;
			break;
		}
	} else if (dblw) {
		lineBlock = &ScaleNormalDw;
	} else if (dblh) {
		lineBlock = &ScaleNormalDh;
	} else  {
forcenormal:
		lineBlock = &ScaleNormal;
	}
	gfx_flags = lineBlock->gfxFlags;
	if (render.src.bpp != 8)
        gfx_flags |= GFX_RGBONLY;
	gfx_flags=GFX_GetBestMode(gfx_flags);
	if (!gfx_flags) {
		if (lineBlock == &ScaleNormal) 
			E_Exit("Failed to create a rendering output");
		else 
			goto forcenormal;
	}
	/* Special test for normal2x to switch to normal with hardware scaling */
	width *= lineBlock->xscale;
	if (gfx_flags & GFX_SCALING) {
		height = MakeAspectTable(render.src.height, lineBlock->yscale, lineBlock->yscale );
	} else {
		if ((gfx_flags & GFX_CAN_RANDOM) && gfx_scaleh > 1) {
			gfx_scaleh *= lineBlock->yscale;
			height = MakeAspectTable(render.src.height, gfx_scaleh, lineBlock->yscale );
		} else {
			gfx_flags &= ~GFX_CAN_RANDOM;		//Hardware surface when possible
			height = MakeAspectTable(render.src.height, lineBlock->yscale, lineBlock->yscale);
		}
	}
/* Setup the scaler variables */
	gfx_flags=GFX_SetSize(width,height,gfx_flags,gfx_scalew,gfx_scaleh,&RENDER_ReInit);;
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
	if (gfx_flags & GFX_HARDWARE) {
		render.scale.currentHandler = lineBlock->Linear[ render.scale.outMode ];
	} else {
		render.scale.currentHandler = lineBlock->Random[ render.scale.outMode ];
	}
	switch (render.src.bpp) {
	case 8:
		render.scale.inMode = scalerMode8;
		break;
	case 15:
		render.scale.inMode = scalerMode15;
		break;
	case 16:
		render.scale.inMode = scalerMode16;
		break;
	case 32:
		render.scale.inMode = scalerMode32;
		break;
	default:
		E_Exit("RENDER:Wrong source bpp %d", render.src.bpp );
	}
	render.scale.lineFlags = lineBlock->scaleFlags;
	render.scale.blocks = render.src.width / SCALER_BLOCKSIZE;
	render.scale.lastBlock = render.src.width % SCALER_BLOCKSIZE;
	render.scale.inHeight = render.src.height;
	RENDER_ResetPal();
	/* Signal the next frame to first reinit the cache */
	render.scale.clearCache = true;
	render.active=true;
}

void RENDER_SetSize(Bitu width,Bitu height,Bitu bpp,float fps,double ratio,bool dblw,bool dblh) {
	if (!width || !height || width > SCALER_MAXWIDTH || height > SCALER_MAXHEIGHT) { 
		render.active=false;
		return;	
	}
	RENDER_EndUpdate( false );
	render.src.width=width;
	render.src.height=height;
	render.src.bpp=bpp;
	render.src.dblw=dblw;
	render.src.dblh=dblh;
	render.src.fps=fps;
	render.src.ratio=ratio;
	RENDER_ReInit( false );
}

extern void GFX_SetTitle(Bits cycles, Bits frameskip,bool paused);
static void IncreaseFrameSkip(void) {
	if (render.frameskip.max<10) render.frameskip.max++;
	LOG_MSG("Frame Skip at %d",render.frameskip.max);
	GFX_SetTitle(-1,render.frameskip.max,false);
}

static void DecreaseFrameSkip(void) {
	if (render.frameskip.max>0) render.frameskip.max--;
	LOG_MSG("Frame Skip at %d",render.frameskip.max);
	GFX_SetTitle(-1,render.frameskip.max,false);
}

void RENDER_Init(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);

	//For restarting the renderer.
	static bool running = false;
	bool aspect = render.aspect;
	scalerOperation_t scaleOp = render.scale.op;

	render.pal.first=256;
	render.pal.last=0;
	render.aspect=section->Get_bool("aspect");
	render.frameskip.max=section->Get_int("frameskip");
	render.frameskip.count=0;
	render.active=false;
	const char * scaler;std::string cline;
	if (control->cmdline->FindString("-scaler",cline,false)) {
		scaler=cline.c_str();
	} else {
		scaler=section->Get_string("scaler");
	}
	if (!strcasecmp(scaler,"none")) { render.scale.op = scalerOpNormal;render.scale.size = 1; }
	else if (!strcasecmp(scaler,"normal2x")) { render.scale.op = scalerOpNormal;render.scale.size = 2; }
	else if (!strcasecmp(scaler,"normal3x")) { render.scale.op = scalerOpNormal;render.scale.size = 3; }
	else if (!strcasecmp(scaler,"advmame2x")) { render.scale.op = scalerOpAdvMame;render.scale.size = 2; }
	else if (!strcasecmp(scaler,"advmame3x")) { render.scale.op = scalerOpAdvMame;render.scale.size = 3; }
	else if (!strcasecmp(scaler,"advinterp2x")) { render.scale.op = scalerOpAdvMame;render.scale.size = 2; }
	else if (!strcasecmp(scaler,"advinterp3x")) { render.scale.op = scalerOpAdvMame;render.scale.size = 3; }
	else if (!strcasecmp(scaler,"tv2x")) { render.scale.op = scalerOpTV;render.scale.size = 2; }
	else if (!strcasecmp(scaler,"tv3x")) { render.scale.op = scalerOpTV;render.scale.size = 3; }
	else if (!strcasecmp(scaler,"rgb2x")){ render.scale.op = scalerOpRGB;render.scale.size = 2; }
	else if (!strcasecmp(scaler,"rgb3x")){ render.scale.op = scalerOpRGB;render.scale.size = 3; }
	else if (!strcasecmp(scaler,"scan2x")){ render.scale.op = scalerOpScan;render.scale.size = 2; }
	else if (!strcasecmp(scaler,"scan3x")){ render.scale.op = scalerOpScan;render.scale.size = 3; }
	else {
		render.scale.op = scalerOpNormal;render.scale.size = 1; 
		LOG_MSG("Illegal scaler type %s,falling back to normal.",scaler);
	}

	//If something changed that needs a ReInit
	if(running && (render.aspect != aspect || render.scale.op != scaleOp))
		RENDER_ReInit( false );
	if(!running) render.updating=true;
	running = true;

	MAPPER_AddHandler(DecreaseFrameSkip,MK_f7,MMOD1,"decfskip","Dec Fskip");
	MAPPER_AddHandler(IncreaseFrameSkip,MK_f8,MMOD1,"incfskip","Inc Fskip");
	GFX_SetTitle(-1,render.frameskip.max,false);
}

