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

/* $Id: render.cpp,v 1.24 2004-01-30 13:40:49 qbix79 Exp $ */

#include <sys/types.h>
#include <dirent.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include "video.h"
#include "render.h"
#include "setup.h"
#include "keyboard.h"
#include "cross.h"
#include "support.h"

#define RENDER_MAXWIDTH 1280
#define RENDER_MAXHEIGHT 1024

struct PalData {
	struct { 
		Bit8u red;
		Bit8u green;
		Bit8u blue;
		Bit8u unused;
	} rgb[256];
	volatile Bitu first;
	volatile Bitu last;
	union {
		Bit32u bpp32[256];
		Bit16u bpp16[256];
		Bit32u yuv[256];
	} lookup;
};


static struct {
	struct {
		Bitu width;
		Bitu height;
		Bitu bpp;
		Bitu pitch;
		Bitu scalew;
		Bitu scaleh;
		double ratio;
	} src;
	struct {
		Bitu width;
		Bitu height;
		Bitu pitch;
		Bitu line;
		Bitu bpp;
		RENDER_Operation type;
		RENDER_Operation want_type;
		RENDER_Line_Handler line_handler;
		Bit8u * draw;
		Bit8u * buffer;
		Bit8u * pixels;
	} op;
	struct {
		Bitu count;
		Bitu max;
	} frameskip;
	PalData pal;
	struct {
		Bit8u hlines[RENDER_MAXHEIGHT];
		Bit16u hindex[RENDER_MAXHEIGHT];
	} normal;
	struct {
		Bit16u hindex[RENDER_MAXHEIGHT];
		Bitu line_starts[RENDER_MAXHEIGHT][3];
	} advmame2x;
#if (C_SSHOT)
	struct {
		RENDER_Operation type;
		Bitu bpp,width,height,line;
		const char * dir;
		Bit8u * buffer,* draw;
		bool usesrc;
	} shot;
#endif
	bool active;
	bool aspect;
	bool updating;
} render;

RENDER_Line_Handler RENDER_DrawLine;

/* Forward declerations */
static void RENDER_ResetPal(void);

/* Include the different rendering routines */
#include "render_templates.h"
#include "render_normal.h"
#include "render_scale2x.h"


#if (C_SSHOT)
#include <png.h>

static void RENDER_ShotDraw(Bit8u * src) {
	if (render.shot.usesrc) {
		memcpy(render.shot.draw,src,render.shot.line);
		render.shot.draw+=render.shot.line;
	} else render.op.line_handler(src);
}

/* Take a screenshot of the data that should be rendered */
static void TakeScreenShot(Bit8u * bitmap) {
	Bitu last=0;char file_name[CROSS_LEN];
	DIR * dir;struct dirent * dir_ent;
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep * row_pointers;
	png_color palette[256];
	Bitu i;

/* Find a filename to open */
	dir=opendir(render.shot.dir);
	if (!dir) {
		LOG_MSG("Can't open snapshot directory %s",render.shot.dir);
		return;
	}
	while (dir_ent=readdir(dir)) {
		char tempname[CROSS_LEN];
		strcpy(tempname,dir_ent->d_name);
		char * test=strstr(tempname,".png");
		if (!test) continue;
		*test=0;
		if (strlen(tempname)<5) continue;
		if (strncmp(tempname,"snap",4)!=0) continue;
		Bitu num=atoi(&tempname[4]);
		if (num>=last) last=num+1;
	}
	closedir(dir);
	sprintf(file_name,"%s%csnap%04d.png",render.shot.dir,CROSS_FILESPLIT,last);
	/* Open the actual file */
	FILE * fp=fopen(file_name,"wb");
	if (!fp) {
		LOG_MSG("Can't open file %s for snapshot",file_name);
		return;
	}

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
		row_pointers[i]=(bitmap+i*render.shot.line);
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
	LOG_MSG("Took snapshot in file %s",file_name);
}

static void EnableScreenShot(void) {
	render.shot.type=render.op.type;
	render.op.type=OP_Shot;
}

#endif


static void Check_Palette(void) {
	if (render.pal.first>render.pal.last) return;
	Bitu i;
	switch (render.op.bpp) {
	case 8:
		GFX_SetPalette(render.pal.first,render.pal.last-render.pal.first+1,(GFX_PalEntry *)&render.pal.rgb[render.pal.first]);
		break;
	case 16:
		for (i=render.pal.first;i<=render.pal.last;i++) {
			Bit8u r=render.pal.rgb[i].red;
			Bit8u g=render.pal.rgb[i].green;
			Bit8u b=render.pal.rgb[i].blue;
			render.pal.lookup.bpp16[i]=GFX_GetRGB(r,g,b);
		}
		break;
	case 24:
	case 32:
		for (i=render.pal.first;i<=render.pal.last;i++) {
			Bit8u r=render.pal.rgb[i].red;
			Bit8u g=render.pal.rgb[i].green;
			Bit8u b=render.pal.rgb[i].blue;
			render.pal.lookup.bpp32[i]=GFX_GetRGB(r,g,b);
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

static void RENDER_EmptyLineHandler(Bit8u * src) {

}

bool RENDER_StartUpdate(void) {
	if (render.updating) return false;
	if (render.frameskip.count<render.frameskip.max) {
		render.frameskip.count++;
		return false;
	}
	render.frameskip.count=0;
	if (render.src.bpp==8) Check_Palette();
	render.op.line=0;
	switch (render.op.type) {
	case OP_None:
	case OP_Normal2x:
	case OP_AdvMame2x:
		if (!GFX_StartUpdate(render.op.pixels,render.op.pitch)) return false;
		RENDER_DrawLine=render.op.line_handler;;
		break;
#if (C_SSHOT)
	case OP_Shot:
		if (render.shot.buffer) free(render.shot.buffer);
		if (render.shot.usesrc) {
			render.shot.width=render.src.width;
			render.shot.height=render.src.height;
			render.shot.bpp=render.src.bpp;
		} else {
			render.shot.width=render.op.width;
			render.shot.height=render.op.height;
			render.shot.bpp=render.op.bpp;
		}
		switch (render.shot.bpp) {
		case 8:render.shot.line=render.shot.width;break;
		case 15:
		case 16:render.shot.line=render.shot.width*2;break;
		case 24:render.shot.line=render.shot.width*3;break;
		case 32:render.shot.line=render.shot.width*4;break;
		}
		render.shot.buffer=(Bit8u*)malloc(render.shot.line*render.shot.height);
		render.op.pixels=render.shot.buffer;
		render.op.pitch=render.shot.line;
		render.shot.draw=render.shot.buffer;
		RENDER_DrawLine=RENDER_ShotDraw;
		break;
#endif
	}
	render.updating=true;
	return true;
}

void RENDER_EndUpdate(void) {
	if (!render.updating) return;
#if (C_SSHOT)
	switch (render.op.type) {
	case OP_None:
	case OP_Normal2x:
	case OP_AdvMame2x:
		break;
	case OP_Shot:
		TakeScreenShot(render.shot.buffer);
		free(render.shot.buffer);
		render.shot.buffer=0;
		render.op.type=render.shot.type;
		break;
	}
#endif	/* If Things are added to please check the define */   
	GFX_EndUpdate();
	RENDER_DrawLine=RENDER_EmptyLineHandler;
	render.updating=false;
}

static void SetAdvMameTable(Bitu index,Bits src0,Bits src1,Bits src2) {
	if (src0<0) src0=0;
	if ((Bitu)src0>=render.src.height) src0=render.src.height-1;
	if ((Bitu)src1>=render.src.height) src1=render.src.height-1;
	if ((Bitu)src2>=render.src.height) src2=render.src.height-1;
	render.advmame2x.line_starts[index][0]=src0*render.src.pitch;
	render.advmame2x.line_starts[index][1]=src1*render.src.pitch;
	render.advmame2x.line_starts[index][2]=src2*render.src.pitch;
}

void RENDER_ReInit(void) {
	if (render.updating) RENDER_EndUpdate();
	Bitu width=render.src.width;
	Bitu height=render.src.height;

	Bitu scalew=render.src.scalew;
	Bitu scaleh=render.src.scaleh;

	double gfx_scalew=1.0;
	double gfx_scaleh=1.0;
	
	if (render.src.ratio>1.0) gfx_scaleh*=render.src.ratio;
	else gfx_scalew*=(1/render.src.ratio);

	Bitu gfx_flags;
	Bitu bpp=GFX_GetBestMode(render.src.bpp,gfx_flags);
	Bitu index;
	switch (bpp) {
	case 8:	index=0;break;
	case 16:index=1;break;
	case 24:index=2;break;
	case 32:index=3;break;
	}
	/* Initial scaler testing */
	switch (render.op.want_type) {
	case OP_Normal2x:
	case OP_None:
		render.op.type=render.op.want_type;
normalop:
		if (gfx_flags & GFX_HASSCALING) {
			gfx_scalew*=scalew;
			gfx_scaleh*=scaleh;
			render.op.line_handler=Normal_8[index];
			for (Bitu i=0;i<render.src.height;i++) {
				render.normal.hindex[i]=i;
				render.normal.hlines[i]=0;
			}
		} else {
			gfx_scaleh*=scaleh;
			if (scalew==2) {
				if (scaleh>1 && (render.op.type==OP_None)) {
					render.op.line_handler=Normal_8[index];
					scalew>>=1;gfx_scaleh/=2;
				} else {
                    render.op.line_handler=Normal_2x_8[index];
				}
			} else render.op.line_handler=Normal_8[index];
			width*=scalew;
			double lines=0.0;
			gfx_scaleh=(gfx_scaleh*render.src.height-(double)render.src.height)/(double)render.src.height;
			height=0;
			for (Bitu i=0;i<render.src.height;i++) {
				render.normal.hindex[i]=height++;
				Bitu temp_lines=(Bitu)(lines);
				lines=lines+gfx_scaleh-temp_lines;
				render.normal.hlines[i]=temp_lines;
				height+=temp_lines;
			}
		}
		break;
	case OP_AdvMame2x:
		if (scalew!=2){
			render.op.type=OP_Normal2x;
			goto normalop;
		}
		if (gfx_flags & GFX_HASSCALING) {
			height=scaleh*height;
		} else {
			height=(Bitu)(gfx_scaleh*scaleh*height);
		}
		width<<=1;
		{
			Bits i;
			double src_add=(double)height/(double)render.src.height;		
			double src_index=0;
			double src_lines=0;
			Bitu height=0;
			for (i=0;i<=(Bits)render.src.height;i++) {
				render.advmame2x.hindex[i]=(Bitu)src_index;
				Bitu lines=(Bitu)src_lines;
				src_lines-=lines;
				src_index+=src_add;
				src_lines+=src_add;
				switch (lines) {
				case 0:
					break;
				case 1:
					SetAdvMameTable(height++,i,i,i);
					break;
				case 2:
					SetAdvMameTable(height++,i-1,i,i+1);
					SetAdvMameTable(height++,i+1,i,i-1);
					break;
				default:
					SetAdvMameTable(height++,i-1,i,i+1);
					for (lines-=2;lines>0;lines--) SetAdvMameTable(height++,i,i,i);
					SetAdvMameTable(height++,i+1,i,i-1);
					break;
				}
			}
			render.op.line_handler=AdvMame2x_8_Table[index];
		}
		break;
	}
	render.op.bpp=bpp;
	render.op.width=width;
	render.op.height=height;
	GFX_SetSize(width,height,bpp,gfx_scalew,gfx_scaleh,&RENDER_ReInit);
	RENDER_ResetPal();
	render.active=true;
}


void RENDER_SetSize(Bitu width,Bitu height,Bitu bpp,Bitu pitch,double ratio,Bitu scalew,Bitu scaleh) {
	if ((!width) || (!height) || (!pitch)) { 
		render.active=false;
		return;	
	}
	render.src.width=width;
	render.src.height=height;
	render.src.bpp=bpp;
	render.src.pitch=pitch;
	render.src.ratio=render.aspect ? ratio : 1.0;
	render.src.scalew=scalew;
	render.src.scaleh=scaleh;
	RENDER_ReInit();
}

extern void GFX_SetTitle(Bits cycles, Bits frameskip);
static void IncreaseFrameSkip(void) {
	if (render.frameskip.max<10) render.frameskip.max++;
	LOG_MSG("Frame Skip at %d",render.frameskip.max);
	GFX_SetTitle(-1,render.frameskip.max);
}

static void DecreaseFrameSkip(void) {
	if (render.frameskip.max>0) render.frameskip.max--;
	LOG_MSG("Frame Skip at %d",render.frameskip.max);
	GFX_SetTitle(-1,render.frameskip.max);
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
	render.shot.dir=section->Get_string("snapdir");
	render.shot.usesrc=true;
	KEYBOARD_AddEvent(KBD_f5,KBD_MOD_CTRL,EnableScreenShot);
#endif
	const char * scaler;std::string cline;
	if (control->cmdline->FindString("-scaler",cline,false)) {
		scaler=cline.c_str();
	} else {
		scaler=section->Get_string("scaler");
	}
	if (!strcasecmp(scaler,"none")) render.op.want_type=OP_None;
	else if (!strcasecmp(scaler,"normal2x")) render.op.want_type=OP_Normal2x;
	else if (!strcasecmp(scaler,"advmame2x")) render.op.want_type=OP_AdvMame2x;
	else {
		render.op.want_type=OP_None;
		LOG_MSG("Illegal scaler type %s,falling back to none.",scaler);
	}
	KEYBOARD_AddEvent(KBD_f7,KBD_MOD_CTRL,DecreaseFrameSkip);
	KEYBOARD_AddEvent(KBD_f8,KBD_MOD_CTRL,IncreaseFrameSkip);
	GFX_SetTitle(-1,render.frameskip.max);
}

