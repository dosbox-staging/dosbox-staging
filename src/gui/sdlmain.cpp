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

/* $Id: sdlmain.cpp,v 1.58 2004-01-29 09:26:45 qbix79 Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#include "SDL.h"

#include "dosbox.h"
#include "video.h"
#include "keyboard.h"
#include "mouse.h"
#include "joystick.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"
#include "support.h"
#include "debug.h"

#if C_OPENGL
#include "SDL_opengl.h"

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#ifdef __WIN32__
#define NVIDIA_PixelDataRange 1
#ifndef WGL_NV_allocate_memory
#define WGL_NV_allocate_memory 1
typedef void * (APIENTRY * PFNWGLALLOCATEMEMORYNVPROC) (int size, float readfreq, float writefreq, float priority);
typedef void (APIENTRY * PFNWGLFREEMEMORYNVPROC) (void *pointer);
PFNWGLALLOCATEMEMORYNVPROC db_glAllocateMemoryNV = NULL;
PFNWGLFREEMEMORYNVPROC db_glFreeMemoryNV = NULL;
#endif
#else 

#endif

#if defined(NVIDIA_PixelDataRange)
#ifndef GL_NV_pixel_data_range
#define GL_NV_pixel_data_range 1
#define GL_WRITE_PIXEL_DATA_RANGE_NV      0x8878
typedef void (APIENTRYP PFNGLPIXELDATARANGENVPROC) (GLenum target, GLsizei length, GLvoid *pointer);
typedef void (APIENTRYP PFNGLFLUSHPIXELDATARANGENVPROC) (GLenum target);
PFNGLPIXELDATARANGENVPROC glPixelDataRangeNV = NULL;
#endif
#endif

#endif //C_OPENGL

//#define DISABLE_JOYSTICK

#if !(ENVIRON_INCLUDED)
extern char** environ;
#endif


#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define STDOUT_FILE	TEXT("stdout.txt")
#define STDERR_FILE	TEXT("stderr.txt")
#define DEFAULT_CONFIG_FILE "/dosbox.conf"
#elif defined(MACOSX)
#define DEFAULT_CONFIG_FILE "/Library/Preferences/DOSBox Preferences"
#else /*linux freebsd*/
#define DEFAULT_CONFIG_FILE "/.dosboxrc"
#endif

enum SCREEN_TYPES	{ 
	SCREEN_SURFACE,
	SCREEN_OVERLAY,
	SCREEN_OPENGL
};


struct SDL_Block {
	bool active;							//If this isn't set don't draw
	bool updating;
	struct {
		Bit32u width;
		Bit32u height;
		Bitu bpp;
		double scalex,scaley;
		GFX_ResetCallBack reset;
	} draw;
	bool wait_on_error;
	struct {
		Bit32u width,height,bpp;
		bool fixed;
		bool fullscreen;
		bool doublebuf;
		SCREEN_TYPES type;
		SCREEN_TYPES want_type;
		double hwscale;
	} desktop;
#if C_OPENGL
	struct {
		Bitu pitch;
		void * framebuf;
		GLuint texture;
		GLuint displaylist;
		GLint max_texsize;
		bool packed_pixel;
		bool paletted_texture;
#if defined(NVIDIA_PixelDataRange)
		bool pixel_data_range;
#endif
	} opengl;
#endif
	SDL_Rect clip;
	SDL_Surface * surface;
	SDL_Overlay * overlay;
	SDL_Joystick * joy;
	SDL_cond *cond;
	struct {
		bool autolock;
		bool autoenable;
		bool requestlock;
		bool locked;
		Bitu sensitivity;
	} mouse;
};

static SDL_Block sdl;
static void CaptureMouse(void);

void GFX_SetTitle(Bits cycles,Bits frameskip){
	char title[200]={0};
	static Bits internal_cycles=0;
	static Bits internal_frameskip=0;
	if(cycles != -1) internal_cycles = cycles;
	if(frameskip != -1) internal_frameskip = frameskip;
	sprintf(title,"DOSBox %s, Cpu Cycles: %8d, Frameskip %2d",VERSION,internal_cycles,internal_frameskip);
	SDL_WM_SetCaption(title,VERSION);
}

/* Reset the screen with current values in the sdl structure */
Bitu GFX_GetBestMode(Bitu bpp,Bitu & gfx_flags) {
	gfx_flags=0;
	switch (sdl.desktop.want_type) {
	case SCREEN_SURFACE:
		if (sdl.desktop.fullscreen) {
			bpp=SDL_VideoModeOK(640,480,bpp,SDL_FULLSCREEN|SDL_HWSURFACE |
				(sdl.desktop.doublebuf ? SDL_DOUBLEBUF  : 0) | ((bpp==8) ? SDL_HWPALETTE : 0)  );
		} else {
			bpp=sdl.desktop.bpp;
		}
		gfx_flags|=GFX_HASCONVERT;
		break;
	case SCREEN_OVERLAY:
		bpp=32;
		gfx_flags|=GFX_HASSCALING;
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
		bpp=32;
		gfx_flags|=GFX_HASSCALING;
		break;
#endif
	}
	return bpp;
}


static void ResetScreen(void) {
	GFX_Stop();
	if (sdl.draw.reset) (sdl.draw.reset)();
	GFX_Start();
}

static int int_log2 (int val) {
    int log = 0;
    while ((val >>= 1) != 0)
	log++;
    return log;
}

void GFX_SetSize(Bitu width,Bitu height,Bitu bpp,double scalex,double scaley,GFX_ResetCallBack reset) {
	if (sdl.updating) GFX_EndUpdate();
	sdl.draw.width=width;
	sdl.draw.height=height;
	sdl.draw.bpp=bpp;
	sdl.draw.reset=reset;
	sdl.draw.scalex=scalex;
	sdl.draw.scaley=scaley;

	switch (sdl.desktop.want_type) {
	case SCREEN_SURFACE:
dosurface:
		sdl.desktop.type=SCREEN_SURFACE;
		sdl.clip.w=width;
		sdl.clip.h=height;
		if (sdl.desktop.fullscreen) {
			if (sdl.desktop.fixed) {
				sdl.clip.x=(Sint16)((sdl.desktop.width-width)/2);
				sdl.clip.y=(Sint16)((sdl.desktop.height-height)/2);
				sdl.surface=SDL_SetVideoMode(sdl.desktop.width,sdl.desktop.height,bpp,
					SDL_FULLSCREEN|SDL_HWSURFACE|(sdl.desktop.doublebuf ? SDL_DOUBLEBUF  : 0)|SDL_HWPALETTE);
			} else {
				sdl.clip.x=0;sdl.clip.y=0;
				sdl.surface=SDL_SetVideoMode(width,height,bpp,
					SDL_FULLSCREEN|SDL_HWSURFACE|(sdl.desktop.doublebuf ? SDL_DOUBLEBUF  : 0)|SDL_HWPALETTE);
			}
		} else {
			sdl.clip.x=0;sdl.clip.y=0;
			sdl.surface=SDL_SetVideoMode(width,height,bpp,SDL_HWSURFACE);
		}
		break;
	case SCREEN_OVERLAY:
		if (sdl.overlay) SDL_FreeYUVOverlay(sdl.overlay);
		sdl.overlay=0;
		if (bpp!=32) goto dosurface;
		if (sdl.desktop.fullscreen) {
			if (sdl.desktop.fixed) {
				double ratio_w=(double)sdl.desktop.width/(width*scalex);
				double ratio_h=(double)sdl.desktop.height/(height*scaley);
				if ( ratio_w < ratio_h) {
					sdl.clip.w=(Bit16u)sdl.desktop.width;
					sdl.clip.h=(Bit16u)(height*scaley*ratio_w);
				} else {
					sdl.clip.w=(Bit16u)(width*scalex*ratio_h);
					sdl.clip.h=(Bit16u)sdl.desktop.height;
				}
				sdl.clip.x=(Sint16)((sdl.desktop.width-sdl.clip.w)/2);
				sdl.clip.y=(Sint16)((sdl.desktop.height-sdl.clip.h)/2);
				sdl.surface=SDL_SetVideoMode(sdl.desktop.width,sdl.desktop.height,0,
					SDL_FULLSCREEN|SDL_HWSURFACE);
			} else {
				sdl.clip.x=0;sdl.clip.y=0;
				sdl.clip.w=(Bit16u)(width*scalex);
				sdl.clip.h=(Bit16u)(height*scaley);
				sdl.surface=SDL_SetVideoMode(sdl.clip.w,sdl.clip.h,0,
					SDL_FULLSCREEN|SDL_HWSURFACE);
			}
		} else {
			sdl.clip.x=0;sdl.clip.y=0;
			sdl.clip.w=(Bit16u)(width*scalex*sdl.desktop.hwscale);
			sdl.clip.h=(Bit16u)(height*scaley*sdl.desktop.hwscale);
			sdl.surface=SDL_SetVideoMode(sdl.clip.w,sdl.clip.h,0,SDL_HWSURFACE);
		}
		sdl.overlay=SDL_CreateYUVOverlay(width*2,height,SDL_UYVY_OVERLAY,sdl.surface);
		if (!sdl.overlay) {
			LOG_MSG("SDL:Failed to create overlay, switching back to surface");
			goto dosurface;
		}
		sdl.desktop.type=SCREEN_OVERLAY;
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
	{
		if (sdl.opengl.framebuf) {
#if defined(NVIDIA_PixelDataRange)
			if (sdl.opengl.pixel_data_range) db_glFreeMemoryNV(sdl.opengl.framebuf);
			else
#endif
			free(sdl.opengl.framebuf);
		}
		sdl.opengl.framebuf=0;
		if (bpp!=32) goto dosurface;
		int texsize=2 << int_log2(width > height ? width : height);
		if (texsize>sdl.opengl.max_texsize) {
			LOG_MSG("SDL:OPENGL:No support for texturesize of %d, falling back to surface",texsize);
			goto dosurface;
		}
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
		if (sdl.desktop.fullscreen) {
			if (sdl.desktop.fixed) {
				double ratio_w=(double)sdl.desktop.width/(width*scalex);
				double ratio_h=(double)sdl.desktop.height/(height*scaley);
				if ( ratio_w < ratio_h) {
					sdl.clip.w=(Bit16u)sdl.desktop.width;
					sdl.clip.h=(Bit16u)(height*scaley*ratio_w);
				} else {
					sdl.clip.w=(Bit16u)(width*scalex*ratio_h);
					sdl.clip.h=(Bit16u)sdl.desktop.height;
				}
				sdl.clip.x=(Sint16)((sdl.desktop.width-sdl.clip.w)/2);
				sdl.clip.y=(Sint16)((sdl.desktop.height-sdl.clip.h)/2);
				sdl.surface=SDL_SetVideoMode(sdl.desktop.width,sdl.desktop.height,0,
					SDL_OPENGL|SDL_FULLSCREEN|SDL_HWSURFACE);
			} else {
				sdl.clip.x=0;sdl.clip.y=0;
				sdl.clip.w=(Bit16u)(width*scalex);
				sdl.clip.h=(Bit16u)(height*scaley);
				sdl.surface=SDL_SetVideoMode(sdl.clip.w,sdl.clip.h,0,
					SDL_OPENGL|SDL_FULLSCREEN|SDL_HWSURFACE);
			}
		} else {
			sdl.clip.x=0;sdl.clip.y=0;
			sdl.clip.w=(Bit16u)(width*scalex*sdl.desktop.hwscale);
			sdl.clip.h=(Bit16u)(height*scaley*sdl.desktop.hwscale);
			sdl.surface=SDL_SetVideoMode(sdl.clip.w,sdl.clip.h,0,
				SDL_OPENGL|SDL_HWSURFACE);
		}
		if (!sdl.surface || sdl.surface->format->BitsPerPixel<15) {
			LOG_MSG("SDL:OPENGL:Can't open drawing surface, are you running in 16bpp(or higher) mode?");
			goto dosurface;
		}
		/* Create the texture and display list */
#if defined(NVIDIA_PixelDataRange)
		if (sdl.opengl.pixel_data_range) {
			sdl.opengl.framebuf=db_glAllocateMemoryNV(width*height*4,0.0,1.0,1.0);
			glPixelDataRangeNV(GL_WRITE_PIXEL_DATA_RANGE_NV,width*height*4,sdl.opengl.framebuf);
			glEnableClientState(GL_WRITE_PIXEL_DATA_RANGE_NV);
		} else {
#else 
		{
#endif
			sdl.opengl.framebuf=malloc(width*height*4);		//32 bit color
		}
		sdl.opengl.pitch=width*4;
		glMatrixMode (GL_PROJECTION);
		glDeleteTextures(1,&sdl.opengl.texture);
 		glGenTextures(1,&sdl.opengl.texture);
		glBindTexture(GL_TEXTURE_2D,sdl.opengl.texture);
		// No borders
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		// Bilinear filtering
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texsize, texsize, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 0);

		glClearColor (1.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glShadeModel (GL_FLAT); 
		glDisable (GL_DEPTH_TEST);
		glDisable (GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glEnable(GL_TEXTURE_2D);
		glMatrixMode (GL_MODELVIEW);
		glLoadIdentity ();

		GLfloat tex_width=((GLfloat)(width)/(GLfloat)texsize);
		GLfloat tex_height=((GLfloat)(height)/(GLfloat)texsize);

		if (glIsList(sdl.opengl.displaylist)) glDeleteLists(sdl.opengl.displaylist, 1);
		sdl.opengl.displaylist = glGenLists(1);
		glNewList(sdl.opengl.displaylist, GL_COMPILE);
		glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
		glBegin(GL_QUADS);
		// lower left
		glTexCoord2f(0,tex_height); glVertex2f(-1.0f,-1.0f);
		// lower right
		glTexCoord2f(tex_width,tex_height); glVertex2f(1.0f, -1.0f);
		// upper right
		glTexCoord2f(tex_width,0); glVertex2f(1.0f, 1.0f);
		// upper left
		glTexCoord2f(0,0); glVertex2f(-1.0f, 1.0f);
		glEnd();
		glEndList();
		sdl.desktop.type=SCREEN_OPENGL;
		break;
		}//OPENGL
#endif	//C_OPENGL
	}//CASE
	GFX_Start();
}


static void CaptureMouse(void) {
	sdl.mouse.locked=!sdl.mouse.locked;
	if (sdl.mouse.locked) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		SDL_ShowCursor(SDL_ENABLE);
	}
}

static void SwitchFullScreen(void) {
	sdl.desktop.fullscreen=!sdl.desktop.fullscreen;
	if (sdl.desktop.fullscreen) {
		if (!sdl.mouse.locked) CaptureMouse();
	} else {
		if (sdl.mouse.locked) CaptureMouse();
	}
	ResetScreen();
}

void GFX_SwitchFullScreen(void) {
    SwitchFullScreen();
}

bool GFX_StartUpdate(Bit8u * & pixels,Bitu & pitch) {
	if (!sdl.active || sdl.updating) return false;
	sdl.updating=true;
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		if (SDL_MUSTLOCK(sdl.surface)) {
			if (SDL_LockSurface(sdl.surface)) {
				LOG_MSG("SDL Lock failed");
				sdl.updating=false;
				return false;
			}
		}
		pixels=(Bit8u *)sdl.surface->pixels;
		pixels+=sdl.clip.y*sdl.surface->pitch;
		pixels+=sdl.clip.x*sdl.surface->format->BytesPerPixel;
		pitch=sdl.surface->pitch;
		return true;
	case SCREEN_OVERLAY:
		SDL_LockYUVOverlay(sdl.overlay);
		pixels=(Bit8u *)*(sdl.overlay->pixels);
		pitch=*(sdl.overlay->pitches);
		return true;
#if C_OPENGL
	case SCREEN_OPENGL:
		pixels=(Bit8u *)sdl.opengl.framebuf;
		pitch=sdl.opengl.pitch;
		return true;
#endif
	}
	return false;
}

void GFX_EndUpdate(void) {
	if (!sdl.updating) return;
	sdl.updating=false;
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		if (SDL_MUSTLOCK(sdl.surface)) {
			SDL_UnlockSurface(sdl.surface);
		}
		SDL_Flip(sdl.surface);
		break;
	case SCREEN_OVERLAY:
		SDL_UnlockYUVOverlay(sdl.overlay);
		SDL_DisplayYUVOverlay(sdl.overlay,&sdl.clip);
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
		glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
				sdl.draw.width, sdl.draw.height, GL_BGRA_EXT,
				GL_UNSIGNED_INT_8_8_8_8_REV, sdl.opengl.framebuf);
		glCallList(sdl.opengl.displaylist);
		SDL_GL_SwapBuffers();
		break;
#endif

	}
}


void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries) {
	/* I should probably not change the GFX_PalEntry :) */
	if (sdl.surface->flags & SDL_HWPALETTE) {
		if (!SDL_SetPalette(sdl.surface,SDL_PHYSPAL,(SDL_Color *)entries,start,count)) {
			E_Exit("SDL:Can't set palette");
		}
	} else {
		if (!SDL_SetPalette(sdl.surface,SDL_LOGPAL,(SDL_Color *)entries,start,count)) {
			E_Exit("SDL:Can't set palette");
		}
	}
}

Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue) {
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		return SDL_MapRGB(sdl.surface->format,red,green,blue);
	case SCREEN_OVERLAY:
		{
			Bit8u y =  ( 9797*(red) + 19237*(green) +  3734*(blue) ) >> 15;
			Bit8u u =  (18492*((blue)-(y)) >> 15) + 128;
			Bit8u v =  (23372*((red)-(y)) >> 15) + 128;
			return (u << 0) | (y << 8) | (v << 16) | (y << 24);
		}
	case SCREEN_OPENGL:
//		return ((red << 0) | (green << 8) | (blue << 16)) | (255 << 24);
		//USE BGRA
		return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
	}
	return 0;
}

void GFX_Stop() {
	sdl.active=false;
}

void GFX_Start() {
	sdl.active=true;
}

static void GUI_ShutDown(Section * sec) {
	GFX_Stop();
	if (sdl.mouse.locked) CaptureMouse();
	if (sdl.desktop.fullscreen) SwitchFullScreen();
}

static void KillSwitch(void){
	throw 1;
}

static void GUI_StartUp(Section * sec) {
	sec->AddDestroyFunction(&GUI_ShutDown);
	Section_prop * section=static_cast<Section_prop *>(sec);
	sdl.active=false;
	sdl.updating=false;
	sdl.desktop.fullscreen=section->Get_bool("fullscreen");
	sdl.wait_on_error=section->Get_bool("waitonerror");
	sdl.mouse.locked=false;
	sdl.mouse.requestlock=false;
	sdl.desktop.fixed=section->Get_bool("fullfixed");
	sdl.desktop.width=section->Get_int("fullwidth");
	sdl.desktop.height=section->Get_int("fullheight");
	sdl.desktop.doublebuf=section->Get_bool("fulldouble");
	sdl.desktop.hwscale=section->Get_float("hwscale");
	if (!sdl.desktop.width) {
#ifdef WIN32
		sdl.desktop.width=GetSystemMetrics(SM_CXSCREEN);
#else	
		sdl.desktop.width=1024;
#endif
	}
	if (!sdl.desktop.height) {
#ifdef WIN32
		sdl.desktop.height=GetSystemMetrics(SM_CYSCREEN);
#else	
		sdl.desktop.height=768;
#endif
	}
    sdl.mouse.autoenable=section->Get_bool("autolock");
	sdl.mouse.autolock=false;
	sdl.mouse.sensitivity=section->Get_int("sensitivity");
	const char * output=section->Get_string("output");
	if (!strcasecmp(output,"surface")) {
		sdl.desktop.want_type=SCREEN_SURFACE;
	} else if (!strcasecmp(output,"overlay")) {
		sdl.desktop.want_type=SCREEN_OVERLAY;
#if C_OPENGL
	} else if (!strcasecmp(output,"opengl")) {
		sdl.desktop.want_type=SCREEN_OPENGL;
#endif
	} else {
		LOG_MSG("SDL:Unsupported output device %s, switching back to surface",output);
		sdl.desktop.want_type=SCREEN_SURFACE;
	}

	sdl.overlay=0;
#if C_OPENGL
	sdl.surface=SDL_SetVideoMode(640,400,0,SDL_OPENGL);
	sdl.opengl.framebuf=0;
	sdl.opengl.texture=0;
	sdl.opengl.displaylist=0;
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &sdl.opengl.max_texsize);
#if defined(__WIN32__) && defined(NVIDIA_PixelDataRange)
	glPixelDataRangeNV = (PFNGLPIXELDATARANGENVPROC) wglGetProcAddress("glPixelDataRangeNV");
	db_glAllocateMemoryNV = (PFNWGLALLOCATEMEMORYNVPROC) wglGetProcAddress("wglAllocateMemoryNV");
	db_glFreeMemoryNV = (PFNWGLFREEMEMORYNVPROC) wglGetProcAddress("wglFreeMemoryNV");
#endif
	const char * gl_ext = (const char *)glGetString (GL_EXTENSIONS);
	sdl.opengl.packed_pixel=strstr(gl_ext,"EXT_packed_pixels") > 0;
	sdl.opengl.paletted_texture=strstr(gl_ext,"EXT_paletted_texture") > 0;
#if defined(NVIDIA_PixelDataRange)
	sdl.opengl.pixel_data_range=strstr(gl_ext,"GL_NV_pixel_data_range") >0 &&
		glPixelDataRangeNV && db_glAllocateMemoryNV && db_glFreeMemoryNV;
#endif
#endif	//OPENGL
	/* Initialize screen for first time */
	sdl.surface=SDL_SetVideoMode(640,400,0,0);
	sdl.desktop.bpp=sdl.surface->format->BitsPerPixel;
	if (sdl.desktop.bpp==24) {
		LOG_MSG("SDL:You are running in 24 bpp mode, this will slow down things!");
	}
	GFX_SetSize(640,400,8,1.0,1.0,0);
//	SDL_EnableKeyRepeat(250,30);
	SDL_EnableUNICODE(1);
/* Get some Keybinds */
	KEYBOARD_AddEvent(KBD_f9,KBD_MOD_CTRL,KillSwitch);
	KEYBOARD_AddEvent(KBD_f10,KBD_MOD_CTRL,CaptureMouse);
	KEYBOARD_AddEvent(KBD_enter,KBD_MOD_ALT,SwitchFullScreen);

}

void Mouse_AutoLock(bool enable) {
	sdl.mouse.autolock=enable;
	if (enable && sdl.mouse.autoenable) sdl.mouse.requestlock=true;
	else sdl.mouse.requestlock=false;
}

static void HandleKey(SDL_KeyboardEvent * key) {
	KBD_KEYS code;
	switch (key->keysym.sym) {
	case SDLK_1:code=KBD_1;break;
	case SDLK_2:code=KBD_2;break;
	case SDLK_3:code=KBD_3;break;
	case SDLK_4:code=KBD_4;break;
	case SDLK_5:code=KBD_5;break;
	case SDLK_6:code=KBD_6;break;
	case SDLK_7:code=KBD_7;break;
	case SDLK_8:code=KBD_8;break;
	case SDLK_9:code=KBD_9;break;
	case SDLK_0:code=KBD_0;break;

	case SDLK_q:code=KBD_q;break;
	case SDLK_w:code=KBD_w;break;
	case SDLK_e:code=KBD_e;break;
	case SDLK_r:code=KBD_r;break;
	case SDLK_t:code=KBD_t;break;
	case SDLK_y:code=KBD_y;break;
	case SDLK_u:code=KBD_u;break;
	case SDLK_i:code=KBD_i;break;
	case SDLK_o:code=KBD_o;break;
	case SDLK_p:code=KBD_p;break;

	case SDLK_a:code=KBD_a;break;
	case SDLK_s:code=KBD_s;break;
	case SDLK_d:code=KBD_d;break;
	case SDLK_f:code=KBD_f;break;
	case SDLK_g:code=KBD_g;break;
	case SDLK_h:code=KBD_h;break;
	case SDLK_j:code=KBD_j;break;
	case SDLK_k:code=KBD_k;break;
	case SDLK_l:code=KBD_l;break;

	case SDLK_z:code=KBD_z;break;
	case SDLK_x:code=KBD_x;break;
	case SDLK_c:code=KBD_c;break;
	case SDLK_v:code=KBD_v;break;
	case SDLK_b:code=KBD_b;break;
	case SDLK_n:code=KBD_n;break;
	case SDLK_m:code=KBD_m;break;


	case SDLK_F1:code=KBD_f1;break;
	case SDLK_F2:code=KBD_f2;break;
	case SDLK_F3:code=KBD_f3;break;
	case SDLK_F4:code=KBD_f4;break;
	case SDLK_F5:code=KBD_f5;break;
	case SDLK_F6:code=KBD_f6;break;
	case SDLK_F7:code=KBD_f7;break;
	case SDLK_F8:code=KBD_f8;break;
	case SDLK_F9:code=KBD_f9;break;
	case SDLK_F10:code=KBD_f10;break;
	case SDLK_F11:code=KBD_f11;break;
	case SDLK_F12:code=KBD_f12;break;

	case SDLK_ESCAPE:code=KBD_esc;break;
	case SDLK_TAB:code=KBD_tab;break;
	case SDLK_BACKSPACE:code=KBD_backspace;break;
	case SDLK_RETURN:code=KBD_enter;break;
	case SDLK_SPACE:code=KBD_space;break;

	case SDLK_LALT:code=KBD_leftalt;break;
	case SDLK_RALT:code=KBD_rightalt;break;
	case SDLK_LCTRL:code=KBD_leftctrl;break;
	case SDLK_RCTRL:code=KBD_rightctrl;break;
	case SDLK_LSHIFT:code=KBD_leftshift;break;
	case SDLK_RSHIFT:code=KBD_rightshift;break;

	case SDLK_CAPSLOCK:code=KBD_capslock;break;
	case SDLK_SCROLLOCK:code=KBD_scrolllock;break;
	case SDLK_NUMLOCK:code=KBD_numlock;break;
	
	case SDLK_BACKQUOTE:code=KBD_grave;break;
	case SDLK_MINUS:code=KBD_minus;break;
	case SDLK_EQUALS:code=KBD_equals;break;
	case SDLK_BACKSLASH:code=KBD_backslash;break;
	case SDLK_LEFTBRACKET:code=KBD_leftbracket;break;
	case SDLK_RIGHTBRACKET:code=KBD_rightbracket;break;

	case SDLK_SEMICOLON:code=KBD_semicolon;break;
	case SDLK_QUOTE:code=KBD_quote;break;
	case SDLK_PERIOD:code=KBD_period;break;
	case SDLK_COMMA:code=KBD_comma;break;
	case SDLK_SLASH:code=KBD_slash;break;

	case SDLK_INSERT:code=KBD_insert;break;
	case SDLK_HOME:code=KBD_home;break;
	case SDLK_PAGEUP:code=KBD_pageup;break;
	case SDLK_DELETE:code=KBD_delete;break;
	case SDLK_END:code=KBD_end;break;
	case SDLK_PAGEDOWN:code=KBD_pagedown;break;
	case SDLK_LEFT:code=KBD_left;break;
	case SDLK_UP:code=KBD_up;break;
	case SDLK_DOWN:code=KBD_down;break;
	case SDLK_RIGHT:code=KBD_right;break;

	case SDLK_KP1:code=KBD_kp1;break;
	case SDLK_KP2:code=KBD_kp2;break;
	case SDLK_KP3:code=KBD_kp3;break;
	case SDLK_KP4:code=KBD_kp4;break;
	case SDLK_KP5:code=KBD_kp5;break;
	case SDLK_KP6:code=KBD_kp6;break;
	case SDLK_KP7:code=KBD_kp7;break;
	case SDLK_KP8:code=KBD_kp8;break;
	case SDLK_KP9:code=KBD_kp9;break;
	case SDLK_KP0:code=KBD_kp0;break;

	case SDLK_KP_DIVIDE:code=KBD_kpslash;break;
	case SDLK_KP_MULTIPLY:code=KBD_kpmultiply;break;
	case SDLK_KP_MINUS:code=KBD_kpminus;break;
	case SDLK_KP_PLUS:code=KBD_kpplus;break;
	case SDLK_KP_ENTER:code=KBD_kpenter;break;
	case SDLK_KP_PERIOD:code=KBD_kpperiod;break;

	/* Special Keys */
	default:
		code=KBD_1;
		LOG(LOG_KEYBOARD,LOG_ERROR)("Unhandled SDL keysym %d",key->keysym.sym);
		break;
	}
	/* Check the modifiers */
	Bitu mod=
		((key->keysym.mod & KMOD_CTRL) ? KBD_MOD_CTRL : 0) |
		((key->keysym.mod & KMOD_ALT) ? KBD_MOD_ALT : 0) |
		((key->keysym.mod & KMOD_SHIFT) ? KBD_MOD_SHIFT : 0);
	Bitu ascii=key->keysym.unicode<128 ? key->keysym.unicode : 0;
#ifdef MACOSX
	// HACK: Fix backspace on Mac OS X 
	// REMOVE ME oneday
	if (code==KBD_backspace)
		ascii=8;
#endif
	KEYBOARD_AddKey(code,ascii,mod,(key->state==SDL_PRESSED));
}

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) {
	if (sdl.mouse.locked) 
		Mouse_CursorMoved((float)motion->xrel*sdl.mouse.sensitivity/100,(float)motion->yrel*sdl.mouse.sensitivity/100);
}

static void HandleMouseButton(SDL_MouseButtonEvent * button) {
	switch (button->state) {
	case SDL_PRESSED:
		if (sdl.mouse.requestlock && !sdl.mouse.locked) {
			CaptureMouse();
			// Dont pass klick to mouse handler
			break;
		}
		switch (button->button) {
		case SDL_BUTTON_LEFT:
			Mouse_ButtonPressed(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonPressed(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonPressed(2);
			break;
		}
		break;
	case SDL_RELEASED:
		switch (button->button) {
		case SDL_BUTTON_LEFT:
			Mouse_ButtonReleased(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonReleased(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonReleased(2);
			break;
		}
		break;
	}
}

static void HandleJoystickAxis(SDL_JoyAxisEvent * jaxis) {
	switch (jaxis->axis) {
	case 0:
		JOYSTICK_Move_X(0,(float)(jaxis->value/32768.0));
		break;
	case 1:
		JOYSTICK_Move_Y(0,(float)(jaxis->value/32768.0));
		break;
	}
}

static void HandleJoystickButton(SDL_JoyButtonEvent * jbutton) {
	bool state;
	state=jbutton->type==SDL_JOYBUTTONDOWN;
	if (jbutton->button<2) {
		JOYSTICK_Button(0,jbutton->button,state);
	}
}


static Bit8u laltstate = SDL_KEYUP;

void GFX_Events() {
	
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
	    switch (event.type) {
		case SDL_ACTIVEEVENT:
			if (event.active.state & SDL_APPINPUTFOCUS) {
				if (!event.active.gain && sdl.mouse.locked) {
					CaptureMouse();	
				}
			}
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			// ignore event lalt+tab
			if (event.key.keysym.sym==SDLK_LALT) laltstate = event.key.type;
			if ((event.key.keysym.sym==SDLK_TAB) && (laltstate==SDL_KEYDOWN)) break;
			HandleKey(&event.key);
			break;
		case SDL_MOUSEMOTION:
			HandleMouseMotion(&event.motion);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			HandleMouseButton(&event.button);
			break;
		case SDL_JOYAXISMOTION:
			HandleJoystickAxis(&event.jaxis);
			break;
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			HandleJoystickButton(&event.jbutton);
			break;
		case SDL_VIDEORESIZE:
//			HandleVideoResize(&event.resize);
			break;
		case SDL_QUIT:
			throw(0);
			break;
		}
    }
}

void GFX_ShowMsg(char * format,...) {
	char buf[512];
	va_list msg;
	va_start(msg,format);
	vsprintf(buf,format,msg);
        strcat(buf,"\n");
	va_end(msg);
	printf(buf);       
};

int main(int argc, char* argv[]) {
	try {
		CommandLine com_line(argc,argv);
		Config myconf(&com_line);
		control=&myconf;

		/* Can't disable the console with debugger enabled */
#if defined(WIN32) && !(C_DEBUG)
		if (control->cmdline->FindExist("-noconsole")) {
			FreeConsole();
			/* Redirect standard input and standard output */
			freopen(STDOUT_FILE, "w", stdout);
			freopen(STDERR_FILE, "w", stderr);
			setvbuf(stdout, NULL, _IOLBF, BUFSIZ);	/* Line buffered */
			setbuf(stderr, NULL);					/* No buffering */
		} else {
			if (AllocConsole()) {
				fclose(stdin);
				fclose(stdout);
				fclose(stderr);
				freopen("CONIN$","w",stdin);
				freopen("CONOUT$","w",stdout);
				freopen("CONOUT$","w",stderr);
			}
		}
#endif  //defined(WIN32) && !(C_DEBUG)

#if C_DEBUG
		DEBUG_SetupConsole();
#endif

		if ( SDL_Init( SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_CDROM
#ifndef DISABLE_JOYSTICK
		|SDL_INIT_JOYSTICK
	
#endif
		) < 0 ) E_Exit("Can't init SDL %s",SDL_GetError());
		Section_prop * sdl_sec=control->AddSection_prop("sdl",&GUI_StartUp);
		sdl_sec->Add_bool("fullscreen",false);
		sdl_sec->Add_bool("fulldouble",false);
		sdl_sec->Add_bool("fullfixed",false);
		sdl_sec->Add_int("fullwidth",0);
		sdl_sec->Add_int("fullheight",0);
		sdl_sec->Add_string("output","surface");
		sdl_sec->Add_float("hwscale",1.0);
		sdl_sec->Add_bool("autolock",true);
		sdl_sec->Add_int("sensitivity",100);
		sdl_sec->Add_bool("waitonerror",true);

		MSG_Add("SDL_CONFIGFILE_HELP",
			"fullscreen -- Start dosbox directly in fullscreen.\n"
			"fulldouble -- Use double buffering in fullscreen.\n"
			"fullfixed -- Don't resize the screen when in fullscreen.\n"
			"fullwidth/height -- What resolution to use for fullscreen, use together with fullfixed.\n"
			"output -- What to use for output: surface,overlay"
#if C_OPENGL
			",opengl"
#endif
			".\n"
			"hwscale -- Extra scaling of window if the output devive supports hardware scaling.\n"
			"autolock -- Mouse will automatically lock, if you click on the screen.\n"
			"sensitiviy -- Mouse sensitivity.\n"
			"waitonerror -- Wait before closing the console if dosbox has an error.\n"
		);
		/* Init all the dosbox subsystems */
		DOSBOX_Init();
		std::string config_file;
		if (control->cmdline->FindString("-conf",config_file,true)) {
			
		} else {
			config_file="dosbox.conf";
		}
		/* Parse the config file
		 * try open config file in $HOME if can't open dosbox.conf or specified file
		 */
		if (control->ParseConfigFile(config_file.c_str()) == false)  {
			if ((getenv("HOME") != NULL)) {
				config_file = (std::string)getenv("HOME") + 
					      (std::string)DEFAULT_CONFIG_FILE;
				if (control->ParseConfigFile(config_file.c_str()) == false) {
					LOG_MSG("CONFIG: Using default settings. Create a configfile to change them");
				}
			   
			}
		}
#if (ENVIRON_LINKED)
		control->ParseEnv(environ);
#endif
		/* Init all the sections */
		control->Init();
		/* Some extra SDL Functions */
#ifndef DISABLE_JOYSTICK
		if (SDL_NumJoysticks()>0) {
			SDL_JoystickEventState(SDL_ENABLE);
			sdl.joy=SDL_JoystickOpen(0);
			LOG_MSG("Using joystick %s with %d axes and %d buttons",SDL_JoystickName(0),SDL_JoystickNumAxes(sdl.joy),SDL_JoystickNumButtons(sdl.joy));
			JOYSTICK_Enable(0,true);
		}
#endif	
		if (control->cmdline->FindExist("-fullscreen") || sdl_sec->Get_bool("fullscreen")) {
			if(!sdl.desktop.fullscreen) { //only switch if not allready in fullscreen
				SwitchFullScreen();
			}
		}
		/* Start up main machine */
		control->StartUp();
		/* Shutdown everything */
	} catch (char * error) {
        if (sdl.desktop.fullscreen) SwitchFullScreen();
	    if (sdl.mouse.locked) CaptureMouse();
		LOG_MSG("Exit to error: %s",error);
		if(sdl.wait_on_error) {
			//TODO Maybe look for some way to show message in linux?
#if (C_DEBUG)
			LOG_MSG("Press enter to continue",error);
			fgetc(stdin);
#elif defined(WIN32)
			Sleep(5000);
#endif 
		}

	}
	catch (int){ 
		if (sdl.desktop.fullscreen) SwitchFullScreen();
	    if (sdl.mouse.locked) CaptureMouse();
	}
	return 0;
};
