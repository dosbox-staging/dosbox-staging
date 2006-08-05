/*
 *  Copyright (C) 2002-2006  The DOSBox Team
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

/* $Id: render.cpp,v 1.47 2006-08-05 09:06:44 qbix79 Exp $ */

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
ScalerLineHandler_t RENDER_DrawLine;

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
				render.pal.modified[i] = 1;
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

void RENDER_SetPal(Bit8u entry,Bit8u red,Bit8u green,Bit8u blue) {
	render.pal.rgb[entry].red=red;
	render.pal.rgb[entry].green=green;
	render.pal.rgb[entry].blue=blue;
	if (render.pal.first>entry) render.pal.first=entry;
	if (render.pal.last<entry) render.pal.last=entry;
}

static void RENDER_EmptyLineHandler(const void * src) {
}

static void RENDER_ClearCacheHandler(const void * src) {
	Bitu x, width;
	Bit32u *srcLine, *cacheLine;
	srcLine = (Bit32u *)src;
	cacheLine = (Bit32u *)render.scale.cacheRead;
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
	if (GCC_UNLIKELY(!GFX_StartUpdate(render.scale.outWrite,render.scale.outPitch)))
		return false;
	render.scale.inLine = 0;
	render.scale.outLine = 0;
	render.scale.cacheRead = (Bit8u*)&scalerSourceCache;
	Scaler_ChangedLines[0] = 0;
	Scaler_ChangedLineIndex = 0;
	/* Clearing the cache will first process the line to make sure it's never the same */
	if (GCC_UNLIKELY( render.scale.clearCache) ) {
//		LOG_MSG("Clearing cache");
		render.scale.clearCache = false;
		RENDER_DrawLine = RENDER_ClearCacheHandler;
	} else {
		if (render.pal.changed)
			RENDER_DrawLine = render.scale.linePalHandler;
		else 
			RENDER_DrawLine = render.scale.lineHandler;
	}
	render.updating=true;
	return true;
}

void RENDER_EndUpdate( bool fullUpdate ) {
	if (!render.updating)
		return;
	RENDER_DrawLine = RENDER_EmptyLineHandler;
	if (CaptureState & (CAPTURE_IMAGE|CAPTURE_VIDEO)) {
		Bitu pitch, flags;
		flags = 0;
		if (render.src.dblw != render.src.dblh) {
			if (render.src.dblw) flags|=CAPTURE_FLAG_DBLW;
			if (render.src.dblh) flags|=CAPTURE_FLAG_DBLH;
		}
		float fps = render.src.fps;
		pitch = render.scale.cachePitch;
		if (render.frameskip.max)
			fps /= 1+render.frameskip.max;
		CAPTURE_AddImage( render.src.width, render.src.height, render.src.bpp, pitch,
			flags, fps, (Bit8u *)&scalerSourceCache, (Bit8u*)&render.pal.rgb );
	}
	GFX_EndUpdate( fullUpdate ? Scaler_ChangedLines : 0);
	render.updating=false;
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

void RENDER_CallBack( GFX_CallBackFunctions_t function ) {
	if (render.updating) {
		/* Still updating the current screen, shut it down correctly */
		RENDER_EndUpdate( false );
	}

	if (function == GFX_CallBackStop)
		return;
	
	if (function == GFX_CallBackRedraw) {
		//LOG_MSG("redraw");
		render.scale.clearCache = true;
		return;
	}

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
	if (dblh && dblw) {
		/* Initialize always working defaults */
		if (render.scale.size == 2)
			simpleBlock = &ScaleNormal2x;
		else if (render.scale.size == 3)
			simpleBlock = &ScaleNormal3x;
		else
			simpleBlock = &ScaleNormal1x;
		/* Maybe override them */
		switch (render.scale.op) {
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
		}
	} else if (dblw) {
		simpleBlock = &ScaleNormalDw;
	} else if (dblh) {
		simpleBlock = &ScaleNormalDh;
	} else  {
forcenormal:
		complexBlock = 0;
		simpleBlock = &ScaleNormal1x;
	}
	if (complexBlock) {
		if ((width >= SCALER_COMPLEXWIDTH - 16) || height >= SCALER_COMPLEXHEIGHT - 16) {
			LOG_MSG("Scaler can't handle this resolution, going back to normal");
			goto forcenormal;
		}
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
		if (gfx_flags & GFX_CAN_8)
			gfx_flags |= GFX_LOVE_8;
		else
			gfx_flags |= GFX_LOVE_32;
			break;
	case 15:
			gfx_flags |= GFX_LOVE_15;
			gfx_flags = (gfx_flags & ~GFX_CAN_8) | GFX_RGBONLY;
			break;
	case 16:
			gfx_flags |= GFX_LOVE_16;
			gfx_flags = (gfx_flags & ~GFX_CAN_8) | GFX_RGBONLY;
			break;
	case 32:
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
	if (gfx_flags & GFX_SCALING) {
		height = MakeAspectTable(render.src.height, yscale, yscale );
	} else {
		if ((gfx_flags & GFX_CAN_RANDOM) && gfx_scaleh > 1) {
			gfx_scaleh *= yscale;
			height = MakeAspectTable(render.src.height, gfx_scaleh, yscale );
		} else {
			gfx_flags &= ~GFX_CAN_RANDOM;		//Hardware surface when possible
			height = MakeAspectTable(render.src.height, yscale, yscale);
		}
	}
/* Setup the scaler variables */
	gfx_flags=GFX_SetSize(width,height,gfx_flags,gfx_scalew,gfx_scaleh,&RENDER_CallBack);
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
		if (complexBlock) {
			lineBlock = &ScalerCache;
			render.scale.complexHandler = complexBlock->Linear[ render.scale.outMode ];
		} else {
			render.scale.complexHandler = 0;
			lineBlock = &simpleBlock->Linear;
		}
	} else {
		if (complexBlock) {
			lineBlock = &ScalerCache;
			render.scale.complexHandler = complexBlock->Random[ render.scale.outMode ];
		} else {
			render.scale.complexHandler = 0;
			lineBlock = &simpleBlock->Random;
		}
	}
	switch (render.src.bpp) {
	case 8:
		render.scale.lineHandler = (*lineBlock)[0][render.scale.outMode];
		render.scale.linePalHandler = (*lineBlock)[4][render.scale.outMode];
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
	case 32:
		render.scale.lineHandler = (*lineBlock)[3][render.scale.outMode];
		render.scale.linePalHandler = 0;
		render.scale.inMode = scalerMode32;
		render.scale.cachePitch = render.src.width * 4;
		break;
	default:
		E_Exit("RENDER:Wrong source bpp %d", render.src.bpp );
	}
	render.scale.blocks = render.src.width / SCALER_BLOCKSIZE;
	render.scale.lastBlock = render.src.width % SCALER_BLOCKSIZE;
	render.scale.inHeight = render.src.height;
	/* Reset the palette change detection to it's initial value */
	render.pal.first= 0;
	render.pal.last = 255;
	render.pal.changed = false;
	memset(render.pal.modified, 0, sizeof(render.pal.modified));
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
	RENDER_CallBack( GFX_CallBackReset );
}

extern void GFX_SetTitle(Bits cycles, Bits frameskip,bool paused);
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
	else if (!strcasecmp(scaler,"advinterp2x")) { render.scale.op = scalerOpAdvInterp;render.scale.size = 2; }
	else if (!strcasecmp(scaler,"advinterp3x")) { render.scale.op = scalerOpAdvInterp;render.scale.size = 3; }
	else if (!strcasecmp(scaler,"hq2x")) { render.scale.op = scalerOpHQ;render.scale.size = 2; }
	else if (!strcasecmp(scaler,"hq3x")) { render.scale.op = scalerOpHQ;render.scale.size = 3; }
	else if (!strcasecmp(scaler,"2xsai")) { render.scale.op = scalerOpSaI;render.scale.size = 2; }
	else if (!strcasecmp(scaler,"super2xsai")) { render.scale.op = scalerOpSuperSaI;render.scale.size = 2; }
	else if (!strcasecmp(scaler,"supereagle")) { render.scale.op = scalerOpSuperEagle;render.scale.size = 2; }
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
		RENDER_CallBack( GFX_CallBackReset );
	if(!running) render.updating=true;
	running = true;

	MAPPER_AddHandler(DecreaseFrameSkip,MK_f7,MMOD1,"decfskip","Dec Fskip");
	MAPPER_AddHandler(IncreaseFrameSkip,MK_f8,MMOD1,"incfskip","Inc Fskip");
	GFX_SetTitle(-1,render.frameskip.max,false);
}

