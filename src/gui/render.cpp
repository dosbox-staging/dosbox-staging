/*
 *  Copyright (C) 2002  The DOSBox Team
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

#include <png.h>
#include <dirent.h>

#include "dosbox.h"
#include "video.h"
#include "render.h"
#include "setup.h"
#include "keyboard.h"
#include "cross.h"

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
};


static struct {
	struct {
		Bitu width;
		Bitu height;
		Bitu bpp;
		Bitu pitch;
		float ratio;
	} src;
	Bitu flags;
	RENDER_Handler * handler;	
	Bitu stretch_x[MAX_RES];
	Bitu stretch_y[MAX_RES];
	PalData pal;
	bool remake;
	bool enlarge;
	bool screenshot;
} render;

static const char * snapshots_dir;

/* Take a screenshot of the data that should be rendered */
static void TakeScreenShot(Bit8u * bitmap) {
	Bitu last=0;char file_name[CROSS_LEN];
	DIR * dir;struct dirent * dir_ent;
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep * row_pointers;
	png_color palette[256];
	Bitu i;

/* First try to alloacte the png structures */
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,NULL, NULL);
	if (!png_ptr) return;
	info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
		png_destroy_write_struct(&png_ptr,(png_infopp)NULL);
		return;
    }	
/* Find a filename to open */
	dir=opendir(snapshots_dir);
	if (!dir) {
		LOG_WARN("Can't open snapshot dir %s",snapshots_dir);
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
	sprintf(file_name,"%s%csnap%05d.png",snapshots_dir,CROSS_FILESPLIT,last);
/* Open the actual file */
	FILE * fp=fopen(file_name,"wb");
	if (!fp) {
		LOG_WARN("Can't open snapshot file %s",file_name);
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
		row_pointers[i]=(bitmap+i*render.src.pitch);
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



/* This could go kinda bad with multiple threads */
static void Check_Palette(void) {
	if (render.pal.first>render.pal.last) return;
	
	GFX_SetPalette(render.pal.first,render.pal.last-render.pal.first+1,(GFX_PalEntry *)&render.pal.rgb[render.pal.first]);
	render.pal.first=256;
	render.pal.last=0;
}

static void MakeTables(void) {
	//The stretching tables 
	Bitu i;Bit32u c,a;
	c=0;a=(render.src.width<<16)/gfx_info.width;
	for (i=0;i<gfx_info.width;i++) {
		c=(c&0xffff)+a;
		render.stretch_x[i]=c>> 16;
	}
	c=0;a=(render.src.height<<16)/gfx_info.height;
	for (i=0;i<gfx_info.height;i++) {
		c=(c&0xffff)+a;
		render.stretch_y[i]=(c>>16)*render.src.pitch;
	}
}

static void Draw_8_Normal(Bit8u * src_data,Bit8u * dst_data) {
	for (Bitu y=0;y<gfx_info.height;y++) {
		Bit8u * line_src=src_data;
		Bit8u * line_dest=dst_data;
		for (Bitu x=0;x<gfx_info.width;x++) {
			*line_dest++=*line_src;
			line_src+=render.stretch_x[x];
		}
		src_data+=render.stretch_y[y];
		dst_data+=gfx_info.pitch;
	}
}

void RENDER_Draw(Bit8u * bitdata) {
	Bit8u * src_data;
	Check_Palette();
	if (render.remake) {
		MakeTables();
		render.remake=false;
	}
	render.handler(&src_data);
	if (render.screenshot) {
		TakeScreenShot(src_data);
		render.screenshot=false;
	}
	Draw_8_Normal(src_data,bitdata);
}


void RENDER_SetPal(Bit8u entry,Bit8u red,Bit8u green,Bit8u blue) {
	render.pal.rgb[entry].red=red;
	render.pal.rgb[entry].green=green;
	render.pal.rgb[entry].blue=blue;
	if (render.pal.first>entry) render.pal.first=entry;
	if (render.pal.last<entry) render.pal.last=entry;
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
	render.remake=true;
}

void RENDER_SetSize(Bitu width,Bitu height,Bitu bpp,Bitu pitch,float ratio,Bitu flags, RENDER_Handler * handler) {

	if (!width) return;
	if (!height) return;
	GFX_Stop();
	render.src.width=width;
	render.src.height=height;
	render.src.bpp=bpp;
	render.src.ratio=ratio;
	render.src.pitch=pitch;
	render.handler=handler;

	switch (bpp) {
	case 8:
		GFX_Resize(width,height,bpp,&RENDER_Resize);
		GFX_SetDrawHandler(RENDER_Draw);
		break;	
	default:
		E_Exit("RENDER:Illegal bpp %d",bpp);
	};
	/* Setup the internal render structures to correctly render this screen */
	MakeTables();

	GFX_Start();

}

static void EnableScreenShot(void) {
	render.screenshot=true;
}
void RENDER_Init(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	snapshots_dir=section->Get_string("snapshots");
	render.pal.first=256;
	render.pal.last=0;
	render.enlarge=false;
	KEYBOARD_AddEvent(KBD_f5,CTRL_PRESSED,EnableScreenShot);
}

