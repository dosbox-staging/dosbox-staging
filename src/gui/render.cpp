/*
 *  Copyright (C) 2002-2004  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: render.cpp,v 1.29 2004-07-12 12:42:20 qbix79 Exp $ */

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

struct PalData {
	struct { 
		Bit8u red;
		Bit8u green;
		Bit8u blue;
		Bit8u unused;
	} rgb[256];
	volatile Bitu first;
	volatile Bitu last;
};

static struct {
	struct {
		Bitu width;
		Bitu height;
		Bitu bpp;
		bool dblw,dblh;
		double ratio;
	} src;
	struct {
		Bitu width;
		Bitu height;
		Bitu pitch;
		GFX_Modes mode;
		RENDER_Operation type;
		RENDER_Operation want_type;
		RENDER_Line_Handler line_handler;
	} op;
	struct {
		Bitu count;
		Bitu max;
	} frameskip;
	PalData pal;
#if (C_SSHOT)
	struct {
		Bitu bpp,width,height,rowlen;
		Bit8u * buffer,* draw;
		bool take,taking;
	} shot;
#endif
	bool active;
	bool aspect;
	bool updating;
} render;

RENDER_Line_Handler RENDER_DrawLine;

#if (C_SSHOT)
#include <png.h>

static void RENDER_ShotDraw(const Bit8u * src) {
	memcpy(render.shot.draw,src,render.shot.rowlen);
	render.shot.draw+=render.shot.rowlen;
	render.op.line_handler(src);
}

/* Take a screenshot of the data that should be rendered */
static void TakeScreenShot(Bit8u * bitmap) {
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep * row_pointers;
	png_color palette[256];
	Bitu i;

/* Find a filename to open */
	/* Open the actual file */
	FILE * fp=OpenCaptureFile("Screenshot",".png");
	if (!fp) return;
	/* First try to alloacte the png structures */
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,NULL, NULL);
	if (!png_ptr) return;
	info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
		png_destroy_write_struct(&png_ptr,(png_infopp)NULL);
		return;
    }

	/* Finalize the initing of png library */
	png_init_io(png_ptr, fp);
	png_set_compression_level(png_ptr,Z_BEST_COMPRESSION);
	
	/* set other zlib parameters */
	png_set_compression_mem_level(png_ptr, 8);
	png_set_compression_strategy(png_ptr,Z_DEFAULT_STRATEGY);
	png_set_compression_window_bits(png_ptr, 15);
	png_set_compression_method(png_ptr, 8);
	png_set_compression_buffer_size(png_ptr, 8192);

	if (render.shot.bpp==8) {
		png_set_IHDR(png_ptr, info_ptr, render.shot.width, render.shot.height,
			8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		for (i=0;i<256;i++) {
			palette[i].red=render.pal.rgb[i].red;
			palette[i].green=render.pal.rgb[i].green;
			palette[i].blue=render.pal.rgb[i].blue;
		}
		png_set_PLTE(png_ptr, info_ptr, palette,256);
	} else {
		png_set_IHDR(png_ptr, info_ptr, render.shot.width, render.shot.height,
			render.shot.bpp, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	}
	/*Allocate an array of scanline pointers*/
	row_pointers=(png_bytep*)malloc(render.shot.height*sizeof(png_bytep));
	for (i=0;i<render.shot.height;i++) {
		row_pointers[i]=(bitmap+i*render.shot.rowlen);
	}
	/*tell the png library what to encode.*/
	png_set_rows(png_ptr, info_ptr, row_pointers);
	
	/*Write image to file*/
	png_write_png(png_ptr, info_ptr, 0, NULL);

	/*close file*/
	fclose(fp);
	
	/*Destroy PNG structs*/
	png_destroy_write_struct(&png_ptr, &info_ptr);
	
	/*clean up dynamically allocated RAM.*/
	free(row_pointers);
}

static void EnableScreenShot(void) {
	render.shot.take=true;
}

#endif


static void Check_Palette(void) {
	if (render.pal.first>render.pal.last) return;
	Bitu i;
	switch (render.op.mode) {
	case GFX_8:
		GFX_SetPalette(render.pal.first,render.pal.last-render.pal.first+1,(GFX_PalEntry *)&render.pal.rgb[render.pal.first]);
		break;
	case GFX_15:
	case GFX_16:
		for (i=render.pal.first;i<=render.pal.last;i++) {
			Bit8u r=render.pal.rgb[i].red;
			Bit8u g=render.pal.rgb[i].green;
			Bit8u b=render.pal.rgb[i].blue;
			Scaler_PaletteLut.b16[i]=GFX_GetRGB(r,g,b);
		}
		break;
	case GFX_32:
		for (i=render.pal.first;i<=render.pal.last;i++) {
			Bit8u r=render.pal.rgb[i].red;
			Bit8u g=render.pal.rgb[i].green;
			Bit8u b=render.pal.rgb[i].blue;
			Scaler_PaletteLut.b32[i]=GFX_GetRGB(r,g,b);
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

static void RENDER_EmptyLineHandler(const Bit8u * src) {

}

bool RENDER_StartUpdate(void) {
	if (render.updating) return false;
	if (render.frameskip.count<render.frameskip.max) {
		render.frameskip.count++;
		return false;
	}
	render.frameskip.count=0;
	if (render.src.bpp==8) Check_Palette();
	Scaler_Line=0;
	Scaler_Index=Scaler_Data;
	if (!GFX_StartUpdate(Scaler_DstWrite,Scaler_DstPitch)) return false;
	RENDER_DrawLine=render.op.line_handler;
#if (C_SSHOT)
	if (render.shot.take) {
		render.shot.take=false;
		if (render.shot.buffer) {
			free(render.shot.buffer);
		}
		render.shot.width=render.src.width;
		render.shot.height=render.src.height;
		render.shot.bpp=render.src.bpp;
		switch (render.shot.bpp) {
		case 8:render.shot.rowlen=render.shot.width;break;
		case 15:
		case 16:render.shot.rowlen=render.shot.width*2;break;
		case 32:render.shot.rowlen=render.shot.width*4;break;
		}
		render.shot.buffer=(Bit8u*)malloc(render.shot.rowlen*render.shot.height);
		render.shot.draw=render.shot.buffer;
		RENDER_DrawLine=RENDER_ShotDraw;
		render.shot.taking=true;
	}
#endif
	render.updating=true;
	return true;
}

void RENDER_EndUpdate(void) {
	if (!render.updating) return;
#if (C_SSHOT)
	if (render.shot.taking) {
		render.shot.taking=false;
		TakeScreenShot(render.shot.buffer);
		free(render.shot.buffer);
		render.shot.buffer=0;
	}
#endif	/* If Things are added to please check the define */   
	GFX_EndUpdate();
	RENDER_DrawLine=RENDER_EmptyLineHandler;
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
			Scaler_Data[i]=templines-miny;
		} else Scaler_Data[i]=0;
	}
	return linesadded;
}

void RENDER_ReInit(void) {
	if (render.updating) RENDER_EndUpdate();
	Bitu width=render.src.width;
	Bitu height=render.src.height;
	bool dblw=render.src.dblw;
	bool dblh=render.src.dblh;

	double gfx_scalew=1.0;
	double gfx_scaleh=1.0;
	
	if (render.src.ratio>1.0) gfx_scaleh*=render.src.ratio;
	else gfx_scalew*=(1/render.src.ratio);

	Bitu gfx_flags;
	ScalerBlock * block;
	if (dblh && dblw) {
		render.op.type=render.op.want_type;
	} else if (dblw) {
		render.op.type=OP_Normal2x;
	} else if (dblh) {
		render.op.type=OP_Normal;
		gfx_scaleh*=2;
	} else  {
forcenormal:
		render.op.type=OP_Normal;
	}
	switch (render.op.type) {
	case OP_Normal:block=&Normal_8;break;
	case OP_Normal2x:block=(dblh) ? &Normal2x_8 : &NormalDbl_8;break;
	case OP_AdvMame2x:block=&AdvMame2x_8;break;
	case OP_AdvMame3x:block=&AdvMame3x_8;break;
	case OP_Interp2x:block=&Interp2x_8;break;
	case OP_AdvInterp2x:block=&AdvInterp2x_8;break;
	case OP_TV2x:block=&TV2x_8;break;
	}
	gfx_flags=GFX_GetBestMode(block->flags);
	if (!gfx_flags) {
		if (render.op.type==OP_Normal) E_Exit("Failed to create a rendering output");
		else goto forcenormal;
	}
	/* Special test for normal2x to switch to normal with hardware scaling */
	if (gfx_flags & HAVE_SCALING && render.op.type==OP_Normal2x) {
		if (dblw) gfx_scalew*=2;
		if (dblh) gfx_scaleh*=2;
		block=&Normal_8;
		render.op.type=OP_Normal;
	}
	width*=block->xscale;
	if (gfx_flags & HAVE_SCALING) {
		height=MakeAspectTable(render.src.height,block->yscale,block->miny);
	} else {
		gfx_scaleh*=block->yscale;
		height=MakeAspectTable(render.src.height,gfx_scaleh,block->miny);
	}
/* Setup the scaler variables */
	render.op.mode=GFX_SetSize(width,height,gfx_flags,gfx_scalew,gfx_scaleh,&RENDER_ReInit);;
	if (render.op.mode==GFX_NONE) E_Exit("Failed to create a rendering output");
	render.op.line_handler=block->handlers[render.op.mode];
	render.op.width=width;
	render.op.height=height;
	Scaler_SrcWidth=render.src.width;
	Scaler_SrcHeight=render.src.height;
	RENDER_ResetPal();
	render.active=true;
}

void RENDER_SetSize(Bitu width,Bitu height,Bitu bpp,double ratio,bool dblw,bool dblh) {
	if (!width || !height) { 
		render.active=false;
		return;	
	}
	render.src.width=width;
	render.src.height=height;
	render.src.bpp=bpp;
	render.src.dblw=dblw;
	render.src.dblh=dblh;
	render.src.ratio=render.aspect ? ratio : 1.0;
	RENDER_ReInit();
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

	render.pal.first=256;
	render.pal.last=0;
	render.aspect=section->Get_bool("aspect");
	render.frameskip.max=section->Get_int("frameskip");
	render.frameskip.count=0;
	render.updating=true;
#if (C_SSHOT)
	MAPPER_AddHandler(EnableScreenShot,MK_f5,MMOD1,"scrshot","Screenshot");
#endif
	const char * scaler;std::string cline;
	if (control->cmdline->FindString("-scaler",cline,false)) {
		scaler=cline.c_str();
	} else {
		scaler=section->Get_string("scaler");
	}
	if (!strcasecmp(scaler,"none")) render.op.want_type=OP_Normal;
	else if (!strcasecmp(scaler,"normal2x")) render.op.want_type=OP_Normal2x;
	else if (!strcasecmp(scaler,"advmame2x")) render.op.want_type=OP_AdvMame2x;
	else if (!strcasecmp(scaler,"advmame3x")) render.op.want_type=OP_AdvMame3x;
	else if (!strcasecmp(scaler,"advinterp2x")) render.op.want_type=OP_AdvInterp2x;
	else if (!strcasecmp(scaler,"interp2x")) render.op.want_type=OP_Interp2x;
	else if (!strcasecmp(scaler,"tv2x")) render.op.want_type=OP_TV2x;
	else {
		render.op.want_type=OP_Normal;
		LOG_MSG("Illegal scaler type %s,falling back to normal.",scaler);
	}
	MAPPER_AddHandler(DecreaseFrameSkip,MK_f7,MMOD1,"decfskip","Dec Fskip");
	MAPPER_AddHandler(IncreaseFrameSkip,MK_f8,MMOD1,"incfskip","Inc Fskip");
	GFX_SetTitle(-1,render.frameskip.max,false);
}

