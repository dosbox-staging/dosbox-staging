/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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

#include <sys/types.h>
#include <dirent.h>

#include "dosbox.h"
#include "video.h"
#include "render.h"
#include "setup.h"
#include "keyboard.h"
#include "cross.h"
#include "support.h"

#define MAX_RES 2048

struct PalData {
	struct { 
		Bit8u red;
		Bit8u green;
		Bit8u blue;
		Bit8u unused;
	} rgb[256];
	Bitu first;
	Bitu last;
	union {
		Bit32u bpp32[256];
		Bit16u bpp16[256];
	} lookup;
};


static struct {
	struct {
		Bitu width;
		Bitu height;
		Bitu bpp;
		Bitu pitch;
		Bitu flags;
		float ratio;
		RENDER_Draw_Handler draw_handler;
	} src;
	struct {
		Bitu width;
		Bitu height;
		Bitu pitch;
		Bitu bpp;		/* The type of BPP the operation requires for input */
		RENDER_Operation type;
		RENDER_Operation want_type;
		RENDER_Part_Handler part_handler;
		void * dest;
		void * buffer;
		void * pixels;
	} op;
	struct {
		Bitu count;
		Bitu max;
	} frameskip;
	Bitu flags;
	PalData pal;
#if (C_SSHOT)
	struct {
		RENDER_Operation type;
		Bitu pitch;
		const char * dir;
	} shot;
#endif
	bool keep_small;
	bool screenshot;
	bool active;
} render;

/* Forward declerations */
static void RENDER_ResetPal(void);

/* Include the different rendering routines */
#include "render_normal.h"
#include "render_scale2x.h"


#if (C_SSHOT)
#include <png.h>

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

	if (render.src.bpp==8) {
		png_set_IHDR(png_ptr, info_ptr, render.src.width, render.src.height,
			8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		for (i=0;i<256;i++) {
			palette[i].red=render.pal.rgb[i].red;
			palette[i].green=render.pal.rgb[i].green;
			palette[i].blue=render.pal.rgb[i].blue;
		}
		png_set_PLTE(png_ptr, info_ptr, palette,256);
	} else {
		png_set_IHDR(png_ptr, info_ptr, render.src.width, render.src.height,
			8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	}
	/*Allocate an array of scanline pointers*/
	row_pointers=(png_bytep*)malloc(render.src.height*sizeof(png_bytep));
	for (i=0;i<render.src.height;i++) {
		row_pointers[i]=(bitmap+i*render.src.width);
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


/* This could go kinda bad with multiple threads */
static void Check_Palette(void) {
	if (render.pal.first>render.pal.last) return;
	switch (render.op.bpp) {
	case 8:
		GFX_SetPalette(render.pal.first,render.pal.last-render.pal.first+1,(GFX_PalEntry *)&render.pal.rgb[render.pal.first]);
		break;
	case 16:
		for (;render.pal.first<=render.pal.last;render.pal.first++) {
			render.pal.lookup.bpp16[render.pal.first]=GFX_GetRGB(
				render.pal.rgb[render.pal.first].red,
				render.pal.rgb[render.pal.first].green,
				render.pal.rgb[render.pal.first].blue);
		}
		break;
	case 32:
		for (;render.pal.first<=render.pal.last;render.pal.first++) {
			render.pal.lookup.bpp32[render.pal.first]=
			GFX_GetRGB(
				render.pal.rgb[render.pal.first].red,
				render.pal.rgb[render.pal.first].green,
				render.pal.rgb[render.pal.first].blue);
		}
		break;
	};
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

void RENDER_DoUpdate(void) {
	if (render.frameskip.count<render.frameskip.max) {
		render.frameskip.count++;
		return;
	}
	render.frameskip.count=0;
	if (render.src.bpp==8) Check_Palette();
	GFX_DoUpdate();
}

static void RENDER_DrawScreen(void * data) {
	switch (render.op.type) {
doagain:
	case OP_None:
		render.op.dest=render.op.pixels=data;
		render.src.draw_handler(render.op.part_handler);
		break;
	case OP_Scale2x:
		render.op.dest=render.op.pixels=data;
		render.src.draw_handler(render.op.part_handler);
		break;
#if (C_SSHOT)
	case OP_Shot:
		render.shot.pitch=render.op.pitch;
		render.op.pitch=render.src.width;
		render.op.pixels=malloc(render.src.width*render.src.height);
		render.src.draw_handler(Normal_DN_8);
		TakeScreenShot((Bit8u *)render.op.pixels);
		free(render.op.pixels);
		render.op.pitch=render.shot.pitch;
		render.op.type=render.shot.type;
		goto doagain;
#endif
	}
}

static void RENDER_Resize(Bitu * width,Bitu * height) {
	/* Calculate the new size the window should be */
	if (!*width && !*height) {
		/* Special command to reset any resizing for fullscreen */
		*width=render.src.width;
		*height=render.src.height;
	} else {
		if ((*width/render.src.ratio)<*height) *height=(Bitu)(*width/render.src.ratio);
		else *width=(Bitu)(*height*render.src.ratio);
	}
}

void RENDER_SetSize(Bitu width,Bitu height,Bitu bpp,Bitu pitch,float ratio,Bitu flags,RENDER_Draw_Handler draw_handler) {
	if ((!width) || (!height) || (!pitch)) { 
		render.active=false;return;	
	}
	GFX_Stop();
	render.src.width=width;
	render.src.height=height;
	render.src.bpp=bpp;
	render.src.pitch=pitch;
	render.src.ratio=ratio;
	render.src.flags=flags;
	render.src.draw_handler=draw_handler;

	GFX_ModeCallBack mode_callback=0;
	switch (render.op.want_type) {

	case OP_None:
normalop:
		switch (render.src.flags) {
		case DoubleNone:
			flags=0;			
			break;
		case DoubleWidth:
			width*=2;
			flags=GFX_SHADOW;
			break;
		case DoubleHeight:
			height*=2;
			flags=0;
			break;
		case DoubleBoth:
			if (render.keep_small) {
				render.src.flags=0;
				flags=0;
			} else {
				width*=2;height*=2;
				flags=GFX_SHADOW;
			}
			break;
		}
		mode_callback=Render_Normal_CallBack;
		break;
	case OP_Scale2x:
		switch (render.src.flags) {
		case DoubleBoth:
			if (render.keep_small) goto normalop;
			mode_callback=Render_Scale2x_CallBack;
			width*=2;height*=2;
#if defined (SCALE2X_NORMAL)
			flags=GFX_SHADOW;
#elif defined (SCALE2X_MMX)
			flags=GFX_FIXED_BPP;
#endif
			break;
		default:
			goto normalop;
		}		

		break;
	default:
		goto normalop;
	
	}
	GFX_SetSize(width,height,bpp,flags,mode_callback,RENDER_DrawScreen);
	GFX_Start();
}

static void IncreaseFrameSkip(void) {
	if (render.frameskip.max<10) render.frameskip.max++;
	LOG_MSG("Frame Skip at %d",render.frameskip.max);
}

static void DecreaseFrameSkip(void) {
	if (render.frameskip.max>0) render.frameskip.max--;
	LOG_MSG("Frame Skip at %d",render.frameskip.max);
}

void RENDER_Init(Section * sec) {
	MSG_Add("RENDER_CONFIGFILE_HELP","Available renderers:scale2x, none\n");
	Section_prop * section=static_cast<Section_prop *>(sec);

	render.pal.first=256;
	render.pal.last=0;
	render.keep_small=section->Get_bool("keepsmall");
	render.frameskip.max=section->Get_int("frameskip");
	render.frameskip.count=0;
#if (C_SSHOT)
	render.shot.dir=section->Get_string("snapshots");
	KEYBOARD_AddEvent(KBD_f5,KBD_MOD_CTRL,EnableScreenShot);
#endif
	const char *  scaler=section->Get_string("scaler");
	if (!strcasecmp(scaler,"none")) render.op.want_type=OP_None;
	else if (!strcasecmp(scaler,"scale2x")) render.op.want_type=OP_Scale2x;
	else {
		render.op.want_type=OP_None;
		LOG_MSG("Illegal scaler type %s,falling back to none.",scaler);
	}
	KEYBOARD_AddEvent(KBD_f7,KBD_MOD_CTRL,DecreaseFrameSkip);
	KEYBOARD_AddEvent(KBD_f8,KBD_MOD_CTRL,IncreaseFrameSkip);
}

