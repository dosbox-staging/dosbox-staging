/*
 *  Copyright (C) 2002-2019  The DOSBox Team
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


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#ifdef WIN32
#include <signal.h>
#include <process.h>
#endif

#include "cross.h"
#include "SDL.h"

#include "dosbox.h"
#include "video.h"
#include "mouse.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"
#include "support.h"
#include "debug.h"
#include "mapper.h"
#include "vga.h"
#include "keyboard.h"
#include "cpu.h"
#include "cross.h"
#include "control.h"
#include "glidedef.h"
#include "math.h"
#include "pixelscale.h"

#define MAPPERFILE "mapper-" VERSION ".map"
//#define DISABLE_JOYSTICK

#if C_OPENGL
#include "SDL_opengl.h"

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#ifndef GL_ARB_pixel_buffer_object
#define GL_ARB_pixel_buffer_object 1
#define GL_PIXEL_PACK_BUFFER_ARB           0x88EB
#define GL_PIXEL_UNPACK_BUFFER_ARB         0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING_ARB   0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING_ARB 0x88EF
#endif

#ifndef GL_ARB_vertex_buffer_object
#define GL_ARB_vertex_buffer_object 1
typedef void (APIENTRYP PFNGLGENBUFFERSARBPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLBINDBUFFERARBPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLDELETEBUFFERSARBPROC) (GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP PFNGLBUFFERDATAARBPROC) (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef GLvoid* (APIENTRYP PFNGLMAPBUFFERARBPROC) (GLenum target, GLenum access);
typedef GLboolean (APIENTRYP PFNGLUNMAPBUFFERARBPROC) (GLenum target);
#endif

PFNGLGENBUFFERSARBPROC glGenBuffersARB = NULL;
PFNGLBINDBUFFERARBPROC glBindBufferARB = NULL;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB = NULL;
PFNGLBUFFERDATAARBPROC glBufferDataARB = NULL;
PFNGLMAPBUFFERARBPROC glMapBufferARB = NULL;
PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB = NULL;

#endif //C_OPENGL

#if !(ENVIRON_INCLUDED)
extern char** environ;
#endif

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winuser.h>
#if C_DDRAW
#include <ddraw.h>
struct private_hwdata {
	LPDIRECTDRAWSURFACE3 dd_surface;
	LPDIRECTDRAWSURFACE3 dd_writebuf;
};
#endif

#define STDOUT_FILE	TEXT("stdout.txt")
#define STDERR_FILE	TEXT("stderr.txt")
#define DEFAULT_CONFIG_FILE "/dosbox.conf"
#elif defined(MACOSX)
#define DEFAULT_CONFIG_FILE "/Library/Preferences/DOSBox Preferences"
#else /*linux freebsd*/
#define DEFAULT_CONFIG_FILE "/.dosboxrc"
#endif

#if C_SET_PRIORITY
#include <sys/resource.h>
#define PRIO_TOTAL (PRIO_MAX-PRIO_MIN)
#endif

#ifdef OS2
#define INCL_DOS
#define INCL_WIN
#include <os2.h>
#endif

/* ----------------------------- output-type-specific declarations --------------------- */
/* TODO store them in a separate .h-file or together with output-type specific functions */
enum SCREEN_TYPES
{	SCREEN_SURFACE,
	SCREEN_SURFACE_DDRAW,
	SCREEN_OVERLAY,
	SCREEN_OPENGL
};

typedef Bitu (*FGetBestMode)( Bitu *flags );
typedef char (*FSetSize    )(Bitu width,Bitu height,Bitu flags,double scalex,double scaley, Bitu *retflags);typedef char (*FStartUpdate)( Bit8u **pixels, Bitu *pitch );
typedef void (*FEndUpdate  )( const Bit16u *changedLines );
typedef Bitu (*FGetRgb     )( Bit8u red, Bit8u green, Bit8u blue );
typedef struct
{	FGetBestMode GetBestMode;
	FSetSize     SetSize;
	FStartUpdate StartUpdate;
	FEndUpdate   EndUpdate;
	FGetRgb      GetRgb;} ScreenTypeInfo;

typedef ScreenTypeInfo *PScreenTypeInfo;

PScreenTypeInfo GetScreenTypeInfo( Bitu gotbpp, SCREEN_TYPES type );

enum SURFACE_MODE
{	SM_SIMPLE,
	SM_PERFECT,
	SM_SOFT,
	SM_NEIGHBOR
};

typedef void (*F_SO_Init)(Bit16u w_in, Bit16u h_in, Bit16u *w_out, Bit16u *h_out, int* bpp );
typedef void (*F_SO_EndUpdate)();

typedef struct SurfaceModeInfo
{
	F_SO_Init      Init;
	FStartUpdate   StartUpdate;
	F_SO_EndUpdate EndUpdate;
} SurfaceModeInfo;
/* ------------------ end of output-type-specific declarations ------------------------ */

enum PRIORITY_LEVELS {
	PRIORITY_LEVEL_PAUSE,
	PRIORITY_LEVEL_LOWEST,
	PRIORITY_LEVEL_LOWER,
	PRIORITY_LEVEL_NORMAL,
	PRIORITY_LEVEL_HIGHER,
	PRIORITY_LEVEL_HIGHEST
};

enum GlKind { GlkBilinear, GlkNearest, GlkPerfect };

struct SDL_Block {
	bool inited;
	bool active;							//If this isn't set don't draw
	bool updating;
	struct {
		Bit32u width;
		Bit32u height;
		double aspect;
		Bit32u bpp;
		Bitu flags;
		double scalex,scaley;
		GFX_CallBack_t callback;
	} draw;
	bool wait_on_error;
	struct {
		struct {
			Bit16u width, height;
			bool fixed;
		} full;
		struct {
			Bit16u width, height;
		} window;
		Bit8u bpp;
		bool fullscreen;
		bool fullborderless;
		bool lazy_fullscreen;
		bool lazy_fullscreen_req;
		bool doublebuf;
		SCREEN_TYPES want_type;
		ScreenTypeInfo screen;
	} desktop;
#if C_OPENGL
	struct {
		Bitu pitch;
		void * framebuf;
		GLuint buffer;
		GLuint texture;
		GLuint displaylist;
		GLint max_texsize;
		enum GlKind kind;
		bool packed_pixel;
		bool paletted_texture;
		bool pixel_buffer_object;
		bool vsync;
	} opengl;
#endif
	struct {
		SDL_Surface * surface;
#if C_DDRAW
		RECT rect;
#endif
	} blit;
	struct {
		PRIORITY_LEVELS focus;
		PRIORITY_LEVELS nofocus;
	} priority;
	SDL_Rect clip;
	SDL_Surface * surface;
	SDL_Overlay * overlay;
	SDL_cond *cond;
	struct {
		bool autolock;
		bool autoenable;
		bool requestlock;
		bool locked;
		int xsensitivity;
		int ysensitivity;
	} mouse;
	SURFACE_MODE SurfaceMode;
	SurfaceModeInfo SurfaceInfo;
	double    ps_sharpness;
	ps_pixels ps_buffer;
	ps_info   pixel_scaling;
	char      dbl_h, dbl_w;
	double    ps_scale_x, ps_scale_y;
	SDL_Rect updateRects[1024];
	unsigned rectCount;
	Bitu num_joysticks;
#if defined (WIN32)
	bool using_windib;
	// Time when sdl regains focus (alt-tab) in windowed mode
	Bit32u focus_ticks;
#endif
	// state of alt-keys for certain special handlings
	Bit8u laltstate;
	Bit8u raltstate;
};

static SDL_Block sdl;

static void Cls( SDL_Surface *s )
{	bool opengl = false;
	unsigned char r = 0, g = 0, b = 0; // TODO: make it configurable?
#if C_OPENGL
	if ( ( s->flags & SDL_OPENGL) != 0 )
	{	opengl = true;
		glClearColor ((float)r/255, (float)g/255, (float)b/255, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		SDL_GL_SwapBuffers();
		glClearColor ((float)r/255, (float)g/255, (float)b/255, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
	}
#endif //C_OPENGL
	if( !opengl )
	{	SDL_FillRect(s,NULL,SDL_MapRGB(s->format,r,g,b));
		SDL_Flip(s);
		SDL_FillRect(s,NULL,SDL_MapRGB(s->format,r,g,b));
	}
}

#define SETMODE_SAVES 1  //Don't set Video Mode if nothing changes.
#define SETMODE_SAVES_CLEAR 1 //Clear the screen, when the Video Mode is reused
SDL_Surface* SDL_SetVideoMode_Wrap(int width,int height,int bpp,Bit32u flags){
#if SETMODE_SAVES
	SDL_Surface* s;
	static int i_height = 0;
	static int i_width = 0;
	static int i_bpp = 0;
	static Bit32u i_flags = 0;
	bool opengl = false;
	if (sdl.surface != NULL && height == i_height && width == i_width && bpp == i_bpp && flags == i_flags) {
/*		// I don't see a difference, so disabled for now, as the code isn't finished either
#if SETMODE_SAVES_CLEAR
		Cls( sdl.surface, 0, 0, 0 );
#endif //SETMODE_SAVES_CLEAR
		return sdl.surface;*/
		// Clear an existing surface, because it matter for borderless fullscreen window:
		s = sdl.surface; goto Done;
	}


#ifdef WIN32
	//SDL seems to crash if we are in OpenGL mode currently and change to exactly the same size without OpenGL.
	//This happens when DOSBox is in textmode with aspect=true and output=opengl and the mapper is started.
	//The easiest solution is to change the size. The mapper doesn't care. (PART PXX)

	//Also we have to switch back to windowed mode first, as else it may crash as well.
	//Bug: we end up with a locked mouse cursor, but at least that beats crashing. (output=opengl,aspect=true,fullscreen=true)
	if((i_flags&SDL_OPENGL) && !(flags&SDL_OPENGL) && (i_flags&SDL_FULLSCREEN) && !(flags&SDL_FULLSCREEN)){
		GFX_SwitchFullScreen();
		s = SDL_SetVideoMode_Wrap(width,height,bpp,flags);
		goto Done;
	}

	//PXX
	if ((i_flags&SDL_OPENGL) && !(flags&SDL_OPENGL) && height==i_height && width==i_width && height==480) {
		height++;
	}
#endif //WIN32
#endif //SETMODE_SAVES
	s = SDL_SetVideoMode(width,height,bpp,flags);
#if SETMODE_SAVES
	if (s == NULL) return s; //Only store when successful
	i_height = height;
	i_width = width;
	i_bpp = bpp;
	i_flags = flags;
#endif
	Done: Cls( s );
	return s;
}

extern const char* RunningProgram;
extern bool CPU_CycleAutoAdjust;
//Globals for keyboard initialisation
bool startup_state_numlock=false;
bool startup_state_capslock=false;

void GFX_SetTitle(Bit32s cycles,int frameskip,bool paused){
	char title[200] = { 0 };
	static Bit32s internal_cycles = 0;
	static int internal_frameskip = 0;
	if (cycles != -1) internal_cycles = cycles;
	if (frameskip != -1) internal_frameskip = frameskip;
	if(CPU_CycleAutoAdjust) {
		sprintf(title,"DOSBox %s, CPU speed: max %3d%% cycles, Frameskip %2d, Program: %8s",VERSION,internal_cycles,internal_frameskip,RunningProgram);
	} else {
		sprintf(title,"DOSBox %s, CPU speed: %8d cycles, Frameskip %2d, Program: %8s",VERSION,internal_cycles,internal_frameskip,RunningProgram);
	}

	if (paused) strcat(title," PAUSED");
	SDL_WM_SetCaption(title,VERSION);
}

static unsigned char logo[32*32*4]= {
#include "dosbox_logo.h"
};
static void GFX_SetIcon() {
#if !defined(MACOSX)
	/* Set Icon (must be done before any sdl_setvideomode call) */
	/* But don't set it on OS X, as we use a nicer external icon there. */
	/* Made into a separate call, so it can be called again when we restart the graphics output on win32 */
#ifdef WORDS_BIGENDIAN
	SDL_Surface* logos= SDL_CreateRGBSurfaceFrom((void*)logo,32,32,32,128,0xff000000,0x00ff0000,0x0000ff00,0);
#else
	SDL_Surface* logos= SDL_CreateRGBSurfaceFrom((void*)logo,32,32,32,128,0x000000ff,0x0000ff00,0x00ff0000,0);
#endif
	SDL_WM_SetIcon(logos,NULL);
#endif
}


static void KillSwitch(bool pressed) {
	if (!pressed)
		return;
	throw 1;
}

static void PauseDOSBox(bool pressed) {
	if (!pressed)
		return;
	GFX_SetTitle(-1,-1,true);
	bool paused = true;
	KEYBOARD_ClrBuffer();
	SDL_Delay(500);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// flush event queue.
	}

	while (paused) {
		SDL_WaitEvent(&event);    // since we're not polling, cpu usage drops to 0.
		switch (event.type) {

			case SDL_QUIT: KillSwitch(true); break;
			case SDL_KEYDOWN:   // Must use Pause/Break Key to resume.
			case SDL_KEYUP:
			if(event.key.keysym.sym == SDLK_PAUSE) {

				paused = false;
				GFX_SetTitle(-1,-1,false);
				break;
			}
#if defined (MACOSX)
			if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod == KMOD_RMETA || event.key.keysym.mod == KMOD_LMETA) ) {
				/* On macs, all aps exit when pressing cmd-q */
				KillSwitch(true);
				break;
			}
#endif
		}
	}
}

#if defined (WIN32)
bool GFX_SDLUsingWinDIB(void) {
	return sdl.using_windib;
}
#endif

static int int_log2 (int val) {
    int log = 0;
    while ((val >>= 1) != 0)
	log++;
    return log;
}

static SDL_Surface * GFX_SetupSurfaceScaled(Bitu flags, Bit32u sdl_flags, Bit32u bpp); // Forward decl.

void CheckGotBpp( Bitu *flags, Bitu testbpp )
{	Bitu gotbpp;
	if (sdl.desktop.fullscreen)
	{	gotbpp=SDL_VideoModeOK(640,480,testbpp,SDL_FULLSCREEN|SDL_HWSURFACE|SDL_HWPALETTE); }
	else
	{	gotbpp=sdl.desktop.bpp;  }
	/* If we can't get our favorite mode check for another working one */
	switch (gotbpp)
	{	case 8:
			if (*flags & GFX_CAN_8) *flags&=~(GFX_CAN_15|GFX_CAN_16|GFX_CAN_32);
			break;
		case 15:
			if (*flags & GFX_CAN_15) *flags&=~(GFX_CAN_8|GFX_CAN_16|GFX_CAN_32);
			break;
		case 16:
			if (*flags & GFX_CAN_16) *flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_32);
			break;
		case 24:
		case 32:
			if (*flags & GFX_CAN_32) *flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
			break;
	}
	*flags |= GFX_CAN_RANDOM;
}

Bitu CheckSurface( Bitu *flags )
{	Bitu testbpp;
	*flags &= ~GFX_LOVE_8;		//Disable love for 8bpp modes
	/* Check if we can satisfy the depth it loves */
	if (*flags & GFX_LOVE_8) testbpp=8;
	else if (*flags & GFX_LOVE_15) testbpp=15;
	else if (*flags & GFX_LOVE_16) testbpp=16;
	else if (*flags & GFX_LOVE_32) testbpp=32;
	else testbpp=0;
	return testbpp;
}

void GetChangedLines( const Bit16u *changedLines )
{	sdl.rectCount = 0; // if need be, declare it as a local variable and fill the parameter at the end.
	if ( !changedLines )
	{	return;  }

	Bitu y = 0, index = 0;
	while (y < sdl.draw.height)
	{	if ( index & 1 )
		{	SDL_Rect *rect = &sdl.updateRects[ sdl.rectCount++ ];
			rect->x = 0;
			rect->y = y;
			rect->w = (Bit16u)sdl.draw.width;
			rect->h = changedLines[index];
#if 0
			if (rect->h + rect->y > sdl.surface->h) {
				LOG_MSG("WTF %d +  %d  >%d",rect->h,rect->y,sdl.surface->h);
			}
#endif
		}
		y += changedLines[index];
		index++;
	}
}

static void GetAvailableArea( Bit16u *width, Bit16u *height, bool *fixed )
{	*fixed = false;
	if( sdl.desktop.fullscreen )
	{	if( sdl.desktop.full.fixed )
		{	*width  = sdl.desktop.full.width;
			*height = sdl.desktop.full.height;
			*fixed  = true;
		}
	}
	else
	{	if( sdl.desktop.window.width > 0 )
		{	*width  = sdl.desktop.window.width;
			*height = sdl.desktop.window.height;
			*fixed  = true;
		}
	}
}

// ATT: aspect is the final aspect ratio of the image including its pixel dimensions and PAR
static void GetActualArea( Bit16u av_w, Bit16u av_h, Bit16u *w, Bit16u *h, double aspect )
{	double as_x, as_y;
	if( aspect > 1.0 )
	{	as_y = aspect  ; as_x = 1.0;  }
	else
	{	as_x = 1.0/aspect; as_y = 1.0;  }
	if( av_h / as_y < av_w / as_x )
	{	*h = av_h; *w = round( (double)av_h / aspect );  }
	else
	{	*w = av_w; *h = round( (double)av_w * aspect );  }
}

static Bit32u genflags( Bitu flags )
{	Bit32u res;
	res = 0;
	if( sdl.desktop.fullscreen )
	{	if( sdl.desktop.fullborderless )
		{	res |= SDL_NOFRAME;
			SDL_putenv("SDL_VIDEO_WINDOW_POS=center");
		}
		else
		{	res |= SDL_FULLSCREEN;
			if( sdl.desktop.doublebuf ) res |= SDL_DOUBLEBUF;
		}
	}
	if( flags & GFX_CAN_RANDOM ) res |= SDL_SWSURFACE;
	else                         res |= SDL_HWSURFACE;
	return res;
}

/* ----------------- Operations depending on SCREEN_TYPES ----------------- */
/* ----------------- Possibly move them to a separate unit ---------------- */

ps_format rgb_32_fmt_in    = { 0, 4 };
ps_format rgb_32_fmt_out   = { 0, 4 };

void ssSmInit(Bit16u w_in, Bit16u h_in, Bit16u *w_out, Bit16u *h_out, int* bpp )
{	*w_out = w_in;
	*h_out = h_in;
}

ps_pixels ps_create_buffer( ps_size size )
{	ps_pixels buf;
	buf.pixels = (unsigned char*)calloc( 4, size.w * size.h );
	buf.pitch  = size.w * 4;
	return buf;
}

/* We must know the pixel layout before calling ps_new_*()                  */
/* As a temporary solution, I (Ant_222) am reading it from the current mode */
/* TODO: Rewrite the scaler so as to separate the calculation of the output */
/*       dimensions from the initialization of the scaling structure. then  */
/*       analyse the layout of the actual surface.                          */
static void PsInitFormats( void )
{	unsigned char offs;
	const SDL_VideoInfo * i;
	i = SDL_GetVideoInfo();
	if( i->vfmt->Rshift > 0 && i->vfmt->Bshift > 0 )
	{	offs = 1;  }
	else
	{	offs = 0;  }
	rgb_32_fmt_in .offs = offs;
	rgb_32_fmt_out.offs = offs;
}

/* TODO: these ~Init() functons are too repetitive, therefore generalize them */
/*       via some common interface, e.g.: GetOutputSize, InitScaling:         */
void ssPpInit(Bit16u w_in, Bit16u h_in, Bit16u *w_out, Bit16u *h_out, int* bpp )
{	*bpp = 32;
	ps_size size_in, size_out, size_res;
	PsInitFormats();
	size_in .w =  w_in ; size_in .h =  h_in;
	size_out.w = *w_out; size_out.h = *h_out;
	sdl.pixel_scaling = ps_new_perfect
	(	rgb_32_fmt_in,   size_in,
		rgb_32_fmt_out,  size_out,
		sdl.dbl_w,       sdl.dbl_h, 3,
		sdl.draw.aspect, &size_res
	);
	*h_out = size_res.h;
	*w_out = size_res.w;
	sdl.ps_buffer = ps_create_buffer( size_in );
}

void ssNpInit(Bit16u w_in, Bit16u h_in, Bit16u *w_out, Bit16u *h_out, int* bpp )
{	*bpp = 32;
	ps_size size_in, size_out;
	PsInitFormats();
	GetActualArea( *w_out, *h_out, w_out, h_out, sdl.draw.aspect * sdl.draw.height / sdl.draw.width );
	size_in .w =  w_in ; size_in .h =  h_in;
	size_out.w = *w_out; size_out.h = *h_out;
	sdl.ps_buffer = ps_create_buffer( size_in );
	sdl.pixel_scaling = ps_new_soft
	(	rgb_32_fmt_in,  size_in,
		rgb_32_fmt_out, size_out,
		sdl.dbl_w,      sdl.dbl_h, 3,
		(double)1.0-sdl.ps_sharpness
	);
}

void ssNbInit(Bit16u w_in, Bit16u h_in, Bit16u *w_out, Bit16u *h_out, int* bpp )
{	*bpp = 32;
	ps_size size_in, size_out;
	PsInitFormats();
	GetActualArea( *w_out, *h_out, w_out, h_out, sdl.draw.aspect * sdl.draw.height / sdl.draw.width );
	size_in .w =  w_in ; size_in .h =  h_in;
	size_out.w = *w_out; size_out.h = *h_out;
	sdl.ps_buffer = ps_create_buffer( size_in );
	sdl.pixel_scaling = ps_new_nn
	(	rgb_32_fmt_in,  size_in,
		rgb_32_fmt_out, size_out,
		sdl.dbl_w, sdl.dbl_h, 3
	);
}

char ssGetOutPixels( Bit8u **pixels, Bitu *pitch )
{	if( sdl.blit.surface != NULL )
	{	if (SDL_MUSTLOCK(sdl.blit.surface) && SDL_LockSurface(sdl.blit.surface))
			return 0;
		*pixels=(Bit8u *)sdl.blit.surface->pixels;
		*pitch=sdl.blit.surface->pitch;
	}
	else
	{	if (SDL_MUSTLOCK(sdl.surface) && SDL_LockSurface(sdl.surface))
			return 0;
		*pixels=(Bit8u *)sdl.surface->pixels;
		*pixels+=sdl.clip.y*sdl.surface->pitch;
		*pixels+=sdl.clip.x*sdl.surface->format->BytesPerPixel;

		*pitch=sdl.surface->pitch;
	}
	return 1;
}

char ssSmStartUpdate( Bit8u **pixels, Bitu *pitch )
{	ssGetOutPixels( pixels, pitch );
	return 1;
}

char ssPsStartUpdate( Bit8u **pixels, Bitu *pitch )
{	*pixels = sdl.ps_buffer.pixels;
	*pitch  = sdl.ps_buffer.pitch;
	return 1;
}

void ssImageToScreen()
{	if( sdl.blit.surface )
	{	if( SDL_MUSTLOCK( sdl.blit.surface ) )
			SDL_UnlockSurface(sdl.blit.surface);
		SDL_BlitSurface( sdl.blit.surface, 0, sdl.surface, &sdl.clip );
		SDL_Flip( sdl.surface );
	}
	else
	{	if( SDL_MUSTLOCK( sdl.surface ) ) SDL_UnlockSurface(sdl.surface);
		for( int i = 0; i < sdl.rectCount; i++ )
		{	sdl.updateRects[i].x += sdl.clip.x;
			sdl.updateRects[i].y += sdl.clip.y;
		}
		SDL_UpdateRects( sdl.surface, sdl.rectCount, sdl.updateRects );
	}
}

void ssSmEndUpdate()
{  }

static void rect_sdl_to_ps( SDL_Rect const rect_sdl, ps_rect* rect_ps )
{	rect_ps->x = rect_sdl.x;
	rect_ps->y = rect_sdl.y;
	rect_ps->w = rect_sdl.w;
	rect_ps->h = rect_sdl.h;
}

static void rect_ps_to_sdl( ps_rect const rect_ps, SDL_Rect* rect_sdl )
{	rect_sdl->x = rect_ps.x;
	rect_sdl->y = rect_ps.y;
	rect_sdl->h = rect_ps.h;
	rect_sdl->w = rect_ps.w;
}

void ssPsEndUpdate()
{	int i;
	Bitu pitch_bitu; // for type compatibility
	ps_pixels pix_out;
	ps_rect rect;
	SDL_PixelFormat* format;

	ssGetOutPixels( &pix_out.pixels, &pitch_bitu );
	pix_out.pitch = pitch_bitu;
	for( int i = 0; i < sdl.rectCount; i++ )
	{	rect_sdl_to_ps( sdl.updateRects[i], &rect );
		ps_scale( sdl.pixel_scaling, sdl.ps_buffer, pix_out, &rect );
		rect_ps_to_sdl( rect, &sdl.updateRects[i] );
	}
}

SurfaceModeInfo ssModes[4] =
{	{ ssSmInit, ssSmStartUpdate, ssSmEndUpdate },
	{ ssPpInit, ssPsStartUpdate, ssPsEndUpdate },
	{ ssNpInit, ssPsStartUpdate, ssPsEndUpdate },
	{ ssNbInit, ssPsStartUpdate, ssPsEndUpdate }
};

void ssSetSurfaceMode( SURFACE_MODE mode )
{	sdl.SurfaceInfo = ssModes[ mode ];  }

// TODO: Unity scale should be set later, when we are cetain that the resolution
//       not the windowed-original
Bitu ssBestMode( Bitu *flags )
{	Bitu cs_res = CheckSurface( flags );
	if( sdl.SurfaceMode != SM_SIMPLE )
	{	*flags |= GFX_UNITY_SCALE;  }
	return cs_res;
}

char ssSetSize(Bitu width,Bitu height,Bitu flags,double scalex,double scaley, Bitu *retFlags)
{	bool fixed;
	Bit16u av_h, av_w, out_w, out_h;
	int bpp;
	SURFACE_MODE sm;

	if( sdl.pixel_scaling != NULL )
	{	ps_free( sdl.pixel_scaling );
		sdl.pixel_scaling = NULL;
		free( sdl.ps_buffer.pixels );
	}
	av_w = width; av_h = height;
	GetAvailableArea( &av_w, &av_h, &fixed );
	if( !fixed && !sdl.desktop.fullscreen )
	{	if( sdl.draw.aspect > 1.0 )
		{	av_h *= sdl.draw.aspect;  }
		if( sdl.draw.aspect < 1.0 )
		{	av_w /= sdl.draw.aspect;  }
	}
	LOG_MSG("Available area: %ix%i", av_w, av_h);
	ssSetSurfaceMode( sdl.SurfaceMode );

	bpp = 0;
	if (flags & GFX_CAN_8)  bpp= 8;
	if (flags & GFX_CAN_15) bpp=15;
	if (flags & GFX_CAN_16) bpp=16;
	if (flags & GFX_CAN_32) bpp=32;

	out_w = av_w; out_h = av_h;
	sdl.SurfaceInfo.Init( width, height, &out_w, &out_h, &bpp );

	sdl.ps_scale_x = ( double )out_w / width;
	sdl.ps_scale_y = ( double )out_h / height;
	char s[80];
	LOG_MSG
	(	"Scaling: %ix%i (%4.2f) --[%3.1f x %3.1f]--> %4ix%-4i (%4.2f)",
		width, height, sdl.draw.aspect,
		sdl.ps_scale_x, sdl.ps_scale_y,
		out_w, out_h, sdl.ps_scale_y / sdl.ps_scale_x
	);

	Bit32u sfcflags = genflags( flags );
	sdl.clip.w=out_w;
	sdl.clip.h=out_h;
	if (sdl.desktop.fullscreen) {
		sfcflags |= SDL_HWPALETTE;
		if (sdl.desktop.full.fixed) {
			sdl.clip.x=(Sint16)((sdl.desktop.full.width -out_w)/2);
			sdl.clip.y=(Sint16)((sdl.desktop.full.height-out_h)/2);
			sdl.surface=SDL_SetVideoMode_Wrap(sdl.desktop.full.width,
				sdl.desktop.full.height,bpp,sfcflags);

			if (sdl.surface == NULL) E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",sdl.desktop.full.width,sdl.desktop.full.height,bpp,SDL_GetError());
		} else {
			sdl.clip.x=0;sdl.clip.y=0;
			sdl.surface=SDL_SetVideoMode_Wrap(out_w,out_h,bpp,sfcflags);

			if (sdl.surface == NULL)
				E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",(int)width,(int)height,bpp,SDL_GetError());
		}
	} else {
		sdl.clip.x=0;sdl.clip.y=0;
		sdl.surface=SDL_SetVideoMode_Wrap(out_w,out_h,bpp, sfcflags);
#ifdef WIN32
		if (sdl.surface == NULL) {
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
			if (!sdl.using_windib) {
				LOG_MSG("Failed to create hardware surface.\nRestarting video subsystem with windib enabled.");
				putenv("SDL_VIDEODRIVER=windib");
				sdl.using_windib=true;
			} else {
				LOG_MSG("Failed to create hardware surface.\nRestarting video subsystem with directx enabled.");
				putenv("SDL_VIDEODRIVER=directx");
				sdl.using_windib=false;
			}
			SDL_InitSubSystem(SDL_INIT_VIDEO);
			GFX_SetIcon(); //Set Icon again
			sdl.surface = SDL_SetVideoMode_Wrap(out_w,out_h,bpp,SDL_HWSURFACE);
			if(sdl.surface) GFX_SetTitle(-1,-1,false); //refresh title.
		}
#endif
		if (sdl.surface == NULL)
			E_Exit("Could not set windowed video mode %ix%i-%i: %s",(int)width,(int)height,bpp,SDL_GetError());
	}
	if (sdl.surface) {
		switch (sdl.surface->format->BitsPerPixel) {
		case 8:
			*retFlags = GFX_CAN_8;
				 break;
		case 15:
			*retFlags = GFX_CAN_15;
			break;
		case 16:
			*retFlags = GFX_CAN_16;
				 break;
		case 32:
			*retFlags = GFX_CAN_32;
				 break;
		}
		if (*retFlags && (sdl.surface->flags & SDL_HWSURFACE))
			*retFlags |= GFX_HARDWARE;
		if (*retFlags && (sdl.surface->flags & SDL_DOUBLEBUF)) {
		//if( 1==1 ) { //force blit surface for debuggin
			sdl.blit.surface=SDL_CreateRGBSurface(SDL_HWSURFACE,
				sdl.clip.w, sdl.clip.h,
				sdl.surface->format->BitsPerPixel,
				sdl.surface->format->Rmask,
				sdl.surface->format->Gmask,
				sdl.surface->format->Bmask,
			0);
			/* If this one fails be ready for some flickering... */
		}
	}
	SDL_Rect rect;
	rect.x = 0; rect.y = 0; rect.w = sdl.surface->w; rect.h = sdl.surface->h;
	return 1; // Success
}


char ssStartUpdate( Bit8u **pixels, Bitu *pitch )
{	return sdl.SurfaceInfo.StartUpdate( pixels, pitch );  }

void ssEndUpdate( const Bit16u *changedLines )
{	GetChangedLines( changedLines );
	sdl.SurfaceInfo.EndUpdate(); // Output processed image if using buffer and scale the rectangles.
	ssImageToScreen();
}

// Rectangles must already be calculated and if necessary--scaled.
Bitu ssGetRgb( Bit8u red, Bit8u green, Bit8u blue )
{	return SDL_MapRGB(sdl.surface->format,red,green,blue);  }

Bitu soBestMode( Bitu *flags )
{	Bitu testbpp;

	//We only accept 32bit output from the scalers here
	//Can't handle true color inputs
	if( *flags & GFX_RGBONLY || !( *flags & GFX_CAN_32 ) )
	{	testbpp = CheckSurface( flags );  }
	else
	{	*flags|=GFX_SCALING;
		*flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
		testbpp = 0;
	}
	return testbpp;
}

char soSetSize(Bitu width,Bitu height,Bitu flags,double scalex,double scaley, Bitu *retFlags){	if (sdl.overlay) {
		SDL_FreeYUVOverlay(sdl.overlay);
		sdl.overlay=0;
	}

	if (!(flags&GFX_CAN_32) || (flags & GFX_RGBONLY)) return 0;
	if (!GFX_SetupSurfaceScaled(flags, 0,0)) return 0;

	int width_out  = 2 * width;
	int height_out =     height;
	sdl.overlay=SDL_CreateYUVOverlay(width_out,height_out,SDL_UYVY_OVERLAY,sdl.surface);
	if (!sdl.overlay) {
		LOG_MSG("SDL:Failed to create overlay, switching back to surface");
		return 0;
	}

	SDL_LockYUVOverlay( sdl.overlay );
	char* cursor = (char*)sdl.overlay->pixels[0];
	// TODO: Handle margins in sdl.clip and remove this:
	for( long i = 0; i < sdl.overlay->pitches[0]/4*sdl.overlay->h; i++ ) // This is a one time procedure, so it needs no optimization.
	{	*cursor++ = 128;
		*cursor++ = 0;
		*cursor++ = 128;
		*cursor++ = 0;
	}
	SDL_UnlockYUVOverlay( sdl.overlay );
	if( SDL_MUSTLOCK( sdl.surface ) )
	{	SDL_LockSurface( sdl.surface );  }
	int scanLen = sdl.surface->pitch;
	cursor = (char*)sdl.surface->pixels;
	for( int i = 0; i < sdl.surface->h; i++ )
	{	for( int x = 0; x < scanLen; x++ )
		{	*cursor++ = 0;  }
	}
	if( SDL_MUSTLOCK( sdl.surface ) )
	{	SDL_UnlockSurface( sdl.surface );  }
	*retFlags = GFX_CAN_32 | GFX_SCALING | GFX_HARDWARE;
	return 1;
}

char soStartUpdate( Bit8u **pixels, Bitu *pitch )
{	while( 1==1 ) //HACK: No idea why several attempts may be required. People at #SDL couldn't help
	{	if (SDL_LockYUVOverlay(sdl.overlay))
		{	continue;  }
		break;
	}
	if (SDL_MUSTLOCK( sdl.surface ) )
	{	if( SDL_LockSurface( sdl.surface ) )
		{	return 0; }
	}
	*pixels = sdl.overlay->pixels [0];
	*pitch  = sdl.overlay->pitches[0];
	return 1;
}

void soApplyOverlay( SDL_Overlay *overlay )
{	SDL_UnlockYUVOverlay(overlay);
	if( SDL_MUSTLOCK( sdl.surface ) )
	{	SDL_UnlockSurface(sdl.surface);  }
	SDL_DisplayYUVOverlay(overlay,&sdl.clip);
}

void soEndUpdate( const Bit16u *changedLines )
{	soApplyOverlay( sdl.overlay );  }

Bitu soGetRgb( Bit8u red, Bit8u green, Bit8u blue )
{	Bit8u y =  ( 9797*(red) + 19237*(green) +  3734*(blue) ) >> 15;
	Bit8u u =  (18492*((blue)-(y)) >> 15) + 128;
	Bit8u v =  (23372*((red)-(y)) >> 15) + 128;
#ifdef WORDS_BIGENDIAN
	return (y << 0) | (v << 8) | (y << 16) | (u << 24);
#else
	return (u << 0) | (y << 8) | (v << 16) | (y << 24);
#endif
}

Bitu ogBestMode( Bitu *flags )
{	Bitu testbpp;
	//We only accept 32bit output from the scalers here
	if( !( *flags & GFX_CAN_32 ) )
	{	testbpp = CheckSurface( flags );  }
	else
	{	*flags|=GFX_SCALING;
		*flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
		testbpp = 0;
	}
	if( sdl.opengl.kind == GlkPerfect )
	{	*flags |= GFX_UNITY_SCALE;  }
	return testbpp;
}

static void setvsync( bool on )
{	const char * state;
	int res;
	if( on ) state = "on";
	else     state = "off";
#if SDL_VERSION_ATLEAST(1,3,0)
	if( on )
	{						res = SDL_GL_SetSwapInterval(-1);
		if( res != 0 ) res = SDL_GL_SetSwapInterval( 1);
	}
	else res = SDL_GL_SetSwapInterval( 0 );
#else
#if SDL_VERSION_ATLEAST(1,2,11)
	res = SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, on );
#else
	if( on )
		LOG_MSG("...V-Sync not supported in this version of SDL.");
#define SDL_NOVSYNC
#endif
#endif
#ifndef SDL_NOVSYNC
	if( res == 0 ) LOG_MSG("V-Sync turned %s.", state); else
	               LOG_MSG("Failed to turn V-Sync %s: %s", state, SDL_GetError());
#endif
}

char ogSetSize(Bitu width,Bitu height,Bitu flags,double scalex,double scaley, Bitu *retFlags) {
	if (sdl.opengl.pixel_buffer_object) {
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
		if (sdl.opengl.buffer) glDeleteBuffersARB(1, &sdl.opengl.buffer);
	} else if (sdl.opengl.framebuf) {
		free(sdl.opengl.framebuf);
	}
	sdl.opengl.framebuf=0;
	if( !( flags & GFX_CAN_32 ) ) return 0;
	int texsize=2 << int_log2(width > height ? width : height);
	if (texsize>sdl.opengl.max_texsize) {
		LOG_MSG("SDL:OPENGL:No support for texturesize of %d, falling back to surface",texsize);
		return 0;
	}
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	setvsync( sdl.opengl.vsync && sdl.desktop.fullscreen && !sdl.desktop.fullborderless );
	if( sdl.opengl.kind == GlkPerfect )
	{	Bit16u avh, avw, w, h;
		bool fixed;
		ps_size szin, szout, szres;
		ps_info ps;
		avw = width; avh = height;
		GetAvailableArea( &avw, &avh, &fixed );
		szin .w = width; szin .h = height;
		szout.w = avw;   szout.h = avh;
		LOG_MSG("Available area: %ix%i", avw, avh);
		ps = ps_new_perfect
		(	rgb_32_fmt_in,   szin,
			rgb_32_fmt_out,  szout,
			sdl.dbl_w,       sdl.dbl_h, 3,
			sdl.draw.aspect, &szres
		);
		ps_free( ps );
		double scalex = (double)szres.w/szin.w;
		double scaley = (double)szres.h/szin.h;
		LOG_MSG
		(	"Scaling: %ix%i (%4.2f) --[%3.1f x %3.1f]--> %4ix%-4i (%4.2f)",
			szin.w, szin.h, sdl.draw.aspect, scalex, scaley, szres.w, szres.h, scaley/scalex
		);
		Bit32u sdl_flags = genflags(flags) | SDL_OPENGL;
		sdl.clip.x = 0; sdl.clip.y = 0;
		if( sdl.desktop.fullscreen )
		{	sdl.clip.w = szres.w; sdl.clip.h = szres.h;
			w = avw; h = avh;
		}
		else
		{	sdl.clip.w = szres.w; sdl.clip.h = szres.h;
			w          = szres.w; h          = szres.h;
		}
		sdl.surface = SDL_SetVideoMode_Wrap( w, h, 0, sdl_flags);
	}
	else GFX_SetupSurfaceScaled(flags, SDL_OPENGL,0);
	if (!sdl.surface || sdl.surface->format->BitsPerPixel<15) {
		LOG_MSG("SDL:OPENGL:Can't open drawing surface, are you running in 16bpp(or higher) mode?");
		return 0;
	}

	/* Create the texture and display list */
	if (sdl.opengl.pixel_buffer_object) {
		glGenBuffersARB(1, &sdl.opengl.buffer);
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl.opengl.buffer);
		glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_EXT, width*height*4, NULL, GL_STREAM_DRAW_ARB);
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
	} else {
		sdl.opengl.framebuf=malloc(width*height*4);		//32 bit color
	}
	sdl.opengl.pitch=width*4;
	if
	(	sdl.clip.x == 0 && sdl.clip.y == 0 &&
		(	sdl.clip.w != sdl.surface->w || sdl.clip.h != sdl.surface->h)
	)
	{	LOG_MSG("attempting to fix the centering to %d %d %d %d",(sdl.surface->w-sdl.clip.w)/2,(sdl.surface->h-sdl.clip.h)/2,sdl.clip.w,sdl.clip.h);
		glViewport((sdl.surface->w-sdl.clip.w)/2,(sdl.surface->h-sdl.clip.h)/2,sdl.clip.w,sdl.clip.h);
	}
	else
	{	glViewport(sdl.clip.x,sdl.clip.y,sdl.clip.w,sdl.clip.h);  }

	glMatrixMode (GL_PROJECTION);
	glDeleteTextures(1,&sdl.opengl.texture);
	glGenTextures(1,&sdl.opengl.texture);
	glBindTexture(GL_TEXTURE_2D,sdl.opengl.texture);
	// No borders
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	if (!sdl.opengl.kind == GlkBilinear || ( (sdl.clip.h % height) == 0 && (sdl.clip.w % width) == 0) ) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	Bit8u* emptytex = new Bit8u[texsize * texsize * 4];
	memset((void*) emptytex, 0, texsize * texsize * 4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texsize, texsize, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (const GLvoid*)emptytex);
	delete [] emptytex;

	//glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
	//glClear(GL_COLOR_BUFFER_BIT);
	//SDL_GL_SwapBuffers();
	//glClear(GL_COLOR_BUFFER_BIT);
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

	glBegin(GL_TRIANGLES);
	// upper left
	glTexCoord2f(0,0); glVertex2f(-1.0f, 1.0f);
	// lower left
	glTexCoord2f(0,tex_height*2); glVertex2f(-1.0f,-3.0f);
	// upper right
	glTexCoord2f(tex_width*2,0); glVertex2f(3.0f, 1.0f);
	glEnd();

	glEndList();
	*retFlags = GFX_CAN_32 | GFX_SCALING;
	if (sdl.opengl.pixel_buffer_object)
		*retFlags |= GFX_HARDWARE;
	return 1;
}

char ogStartUpdate( Bit8u **pixels, Bitu *pitch )
{	if(sdl.opengl.pixel_buffer_object) {
		 glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl.opengl.buffer);
		 *pixels=(Bit8u *)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, GL_WRITE_ONLY);
	} else
		 *pixels=(Bit8u *)sdl.opengl.framebuf;
	*pitch=sdl.opengl.pitch;
	return 1;
}

void ogEndUpdate( const Bit16u *changedLines )
{	// Clear drawing area. Some drivers (on Linux) have more than 2 buffers and the screen might
	// be dirty because of other programs.
	glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	if (sdl.opengl.pixel_buffer_object) {
		glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT);
		glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
				sdl.draw.width, sdl.draw.height, GL_BGRA_EXT,
				GL_UNSIGNED_INT_8_8_8_8_REV, 0);
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
		glCallList(sdl.opengl.displaylist);
		SDL_GL_SwapBuffers();
	} else if (changedLines) {
		Bitu y = 0, index = 0;
		glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
		while (y < sdl.draw.height) {
			if (!(index & 1)) {
				y += changedLines[index];
			} else {
				Bit8u *pixels = (Bit8u *)sdl.opengl.framebuf + y * sdl.opengl.pitch;
				Bitu height = changedLines[index];
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y,
					sdl.draw.width, height, GL_BGRA_EXT,
					GL_UNSIGNED_INT_8_8_8_8_REV, pixels );
				y += height;
			}
			index++;
		}
		glCallList(sdl.opengl.displaylist);
		SDL_GL_SwapBuffers();
	}
}

Bitu ogGetRgb( Bit8u red, Bit8u green, Bit8u blue )
{	return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);  }

Bitu ssdBestMode( Bitu *flags )
{	Bitu testbpp;
	if (!(*flags&(GFX_CAN_15|GFX_CAN_16|GFX_CAN_32)))
	{	testbpp = CheckSurface( &*flags );  }
	else
	{	if (*flags & GFX_LOVE_15)
		{	testbpp=15;  }
		else if (*flags & GFX_LOVE_16) testbpp=16;
		else if (*flags & GFX_LOVE_32) testbpp=32;
		else testbpp=0;
		*flags|=GFX_SCALING;
	}
	return testbpp;
}

#if C_DDRAW
char ssdSetSize(Bitu width,Bitu height,Bitu flags,double scalex,double scaley, Bitu *retFlags){
	int bpp = 0;
	if (flags & GFX_CAN_15) bpp=15;
	if (flags & GFX_CAN_16) bpp=16;
	if (flags & GFX_CAN_32) bpp=32;
	if (!GFX_SetupSurfaceScaled(flags, 0,bpp))
	{	return 0;  }
	sdl.blit.rect.top=sdl.clip.y;
	sdl.blit.rect.left=sdl.clip.x;
	sdl.blit.rect.right=sdl.clip.x+sdl.clip.w;
	sdl.blit.rect.bottom=sdl.clip.y+sdl.clip.h;
	sdl.blit.surface=SDL_CreateRGBSurface(SDL_HWSURFACE,sdl.draw.width,sdl.draw.height,
			sdl.surface->format->BitsPerPixel,
			sdl.surface->format->Rmask,
			sdl.surface->format->Gmask,
			sdl.surface->format->Bmask,
			0);
	if (!sdl.blit.surface || (!sdl.blit.surface->flags&SDL_HWSURFACE)) {
		if (sdl.blit.surface) {
			SDL_FreeSurface(sdl.blit.surface);
			sdl.blit.surface=0;
		}
		LOG_MSG("Failed to create ddraw surface, back to normal surface.");
		return 0; // Error
	}
	switch (sdl.surface->format->BitsPerPixel) {
	case 15:
		*retFlags = GFX_CAN_15 | GFX_SCALING | GFX_HARDWARE;
		break;
	case 16:
		*retFlags = GFX_CAN_16 | GFX_SCALING | GFX_HARDWARE;
				break;
	case 32:
		*retFlags = GFX_CAN_32 | GFX_SCALING | GFX_HARDWARE;
				break;
	}
	return 1; // Success;
}

char ssdStartUpdate( Bit8u **pixels, Bitu *pitch )
{	if (SDL_LockSurface(sdl.blit.surface)) {
//			LOG_MSG("SDL Lock failed");
		return 0;
	}
	*pixels=(Bit8u *)sdl.blit.surface->pixels;
	*pitch=sdl.blit.surface->pitch;
	return 1;
}

void ssdEndUpdate( const Bit16u *changedLines )
{	int ret;
	SDL_UnlockSurface(sdl.blit.surface);
	ret=IDirectDrawSurface3_Blt(
		sdl.surface->hwdata->dd_writebuf,&sdl.blit.rect,
		sdl.blit.surface->hwdata->dd_surface,0,
		DDBLT_WAIT, NULL);
	switch (ret) {
	case DD_OK:
		break;
	case DDERR_SURFACELOST:
		IDirectDrawSurface3_Restore(sdl.blit.surface->hwdata->dd_surface);
		IDirectDrawSurface3_Restore(sdl.surface->hwdata->dd_surface);
		break;
	default:
		LOG_MSG("DDRAW:Failed to blit, error %X",ret);
	}
	SDL_Flip(sdl.surface);
}

Bitu ssdGetRgb( Bit8u red, Bit8u green, Bit8u blue )
{	return SDL_MapRGB(sdl.surface->format,red,green,blue);  }
#endif

ScreenTypeInfo info[10];
void AddScreenType( SCREEN_TYPES type, FGetBestMode bm, FSetSize ss, FStartUpdate su, FEndUpdate eu, FGetRgb gr )
{	PScreenTypeInfo st = &info[type];
	st->GetBestMode = bm;
	st->SetSize     = ss;
	st->StartUpdate = su;
	st->EndUpdate   = eu;
	st->GetRgb      = gr;
}

void InitScreenTypes()
{	AddScreenType( SCREEN_SURFACE,        &ssBestMode,  &ssSetSize,  &ssStartUpdate, &ssEndUpdate, &ssGetRgb );
	AddScreenType( SCREEN_OVERLAY,        &soBestMode,  &soSetSize,  &soStartUpdate, &soEndUpdate, &soGetRgb );
	AddScreenType( SCREEN_OPENGL,         &ogBestMode,  &ogSetSize,  &ogStartUpdate, &ogEndUpdate, &ogGetRgb );
#if C_DDRAW
	AddScreenType( SCREEN_SURFACE_DDRAW,  &ssdBestMode, &ssdSetSize, &ssdStartUpdate, &ssdEndUpdate,&ssdGetRgb );
#endif
}

char stInited = 0;
PScreenTypeInfo GetScreenTypeInfo( SCREEN_TYPES type )
{	if( !stInited )
	{	stInited = 1;
		InitScreenTypes();
	}
	// This precaution is redundant because the availability of DDRAW is tested
	// at compile time, and that too is redundant (repeated use of #if C_DDRAW
	if( info[ type ].GetBestMode == NULL)	{	type = SCREEN_SURFACE;  }
	return &(info[ type ]);
}

/* Reset the screen with current values in the sdl structure */
Bitu GFX_GetBestMode(Bitu flags) {
	int testbpp = GetScreenTypeInfo( sdl.desktop.want_type)->GetBestMode(&flags);
	if( testbpp )
	{	CheckGotBpp( &flags, testbpp );  }
	return flags;
}


void GFX_ResetScreen(void) {
	if(glide.enabled) {
		GLIDE_ResetScreen(true);
		return;
	}
	GFX_Stop();
	if (sdl.draw.callback)
		(sdl.draw.callback)( GFX_CallBackReset );
	GFX_Start();
	CPU_Reset_AutoAdjust();
}

void GFX_ForceFullscreenExit(void) {
	if (sdl.desktop.lazy_fullscreen) {
//		sdl.desktop.lazy_fullscreen_req=true;
		LOG_MSG("GFX LF: invalid screen change");
	} else {
		sdl.desktop.fullscreen=false;
		GFX_ResetScreen();
	}
}


static SDL_Surface * GFX_SetupSurfaceScaled(Bitu flags, Bit32u sdl_flags, Bit32u bpp) {
	Bit16u fixedWidth;
	Bit16u fixedHeight;

	sdl_flags |= genflags(flags);
	if (sdl.desktop.fullscreen) {
		fixedWidth = sdl.desktop.full.fixed ? sdl.desktop.full.width : 0;
		fixedHeight = sdl.desktop.full.fixed ? sdl.desktop.full.height : 0;
	} else {
		fixedWidth = sdl.desktop.window.width;
		fixedHeight = sdl.desktop.window.height;
	}

	if (fixedWidth && fixedHeight) {
		double ratio_w=(double)fixedWidth/(sdl.draw.width*sdl.draw.scalex);
		double ratio_h=(double)fixedHeight/(sdl.draw.height*sdl.draw.scaley);
		if ( ratio_w < ratio_h) {
			sdl.clip.w=fixedWidth;
			sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley*ratio_w + 0.1); //possible rounding issues
		} else {
			/*
			 * The 0.4 is there to correct for rounding issues.
			 * (partly caused by the rounding issues fix in RENDER_SetSize)
			 */
			sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex*ratio_h + 0.4);
			sdl.clip.h=(Bit16u)fixedHeight;
		}
		if (sdl.desktop.fullscreen)
			sdl.surface = SDL_SetVideoMode_Wrap(fixedWidth,fixedHeight,bpp,sdl_flags);
		else
			sdl.surface = SDL_SetVideoMode_Wrap(sdl.clip.w,sdl.clip.h,bpp,sdl_flags);
		if (sdl.surface && sdl.surface->flags & SDL_FULLSCREEN) {
			sdl.clip.x=(Sint16)((sdl.surface->w-sdl.clip.w)/2);
			sdl.clip.y=(Sint16)((sdl.surface->h-sdl.clip.h)/2);
		} else {
			sdl.clip.x = 0;
			sdl.clip.y = 0;
		}
	} else {
		sdl.clip.x=0;sdl.clip.y=0;
		sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex);
		sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley);
		sdl.surface=SDL_SetVideoMode_Wrap(sdl.clip.w,sdl.clip.h,bpp,sdl_flags);
	}
	return sdl.surface;
}

void GFX_TearDown(void) {
	if (sdl.updating)
		GFX_EndUpdate( 0 );

	if (sdl.blit.surface) {
		SDL_FreeSurface(sdl.blit.surface);
		sdl.blit.surface=0;
	}
}

Bitu GFX_SetSize(Bitu width,Bitu height,Bitu flags,double scalex,double scaley,GFX_CallBack_t callback, double aspect ) {
	if (sdl.updating)
		GFX_EndUpdate( 0 );

	sdl.draw.width=width;
	sdl.draw.height=height;
	sdl.draw.aspect=aspect;
	sdl.draw.callback=callback;
	sdl.draw.scalex=scalex;
	sdl.draw.scaley=scaley;

	sdl.dbl_h = ( flags & GFX_DBL_H ) > 0;
	sdl.dbl_w = ( flags & GFX_DBL_W ) > 0;

	int bpp=0;
	Bitu retFlags = 0;

	if (sdl.blit.surface) {
		SDL_FreeSurface(sdl.blit.surface);
		sdl.blit.surface=0;
	}

	ScreenTypeInfo* screen;
	screen = GetScreenTypeInfo( sdl.desktop.want_type );
	if( screen->SetSize(width, height, flags, scalex, scaley, &retFlags) )
	{	goto success;  }
	screen  = GetScreenTypeInfo( SCREEN_SURFACE ); // Backup
	screen->SetSize(width, height, flags, scalex, scaley, &retFlags);
success:
	sdl.desktop.screen = *screen;	if (retFlags)
		GFX_Start();
	if (!sdl.mouse.autoenable) SDL_ShowCursor(sdl.mouse.autolock?SDL_DISABLE:SDL_ENABLE);
	return retFlags;
}

void GFX_CaptureMouse(void) {
	sdl.mouse.locked=!sdl.mouse.locked;
	if (sdl.mouse.locked) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
        mouselocked=sdl.mouse.locked;
}

void GFX_UpdateSDLCaptureState(void) {
	if (sdl.mouse.locked) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
	CPU_Reset_AutoAdjust();
	GFX_SetTitle(-1,-1,false);
}

bool mouselocked; //Global variable for mapper
static void CaptureMouse(bool pressed) {
	if (!pressed)
		return;
	GFX_CaptureMouse();
}

#if defined (WIN32)
STICKYKEYS stick_keys = {sizeof(STICKYKEYS), 0};
void sticky_keys(bool restore){
	static bool inited = false;
	if (!inited){
		inited = true;
		SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
	}
	if (restore) {
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
		return;
	}
	//Get current sticky keys layout:
	STICKYKEYS s = {sizeof(STICKYKEYS), 0};
	SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	if ( !(s.dwFlags & SKF_STICKYKEYSON)) { //Not on already
		s.dwFlags &= ~SKF_HOTKEYACTIVE;
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	}
}
#endif

void GFX_SwitchFullScreen(void) {
	sdl.desktop.fullscreen=!sdl.desktop.fullscreen;
	if (sdl.desktop.fullscreen) {
		if (!sdl.mouse.locked) GFX_CaptureMouse();
#if defined (WIN32)
		sticky_keys(false); //disable sticky keys in fullscreen mode
#endif
	} else {
		if (sdl.mouse.locked) GFX_CaptureMouse();
#if defined (WIN32)
		sticky_keys(true); //restore sticky keys to default state in windowed mode.
#endif
	}
	if (glide.enabled)
		GLIDE_ResetScreen();
	else
	GFX_ResetScreen();
}

static void SwitchFullScreen(bool pressed) {
	if (!pressed)
		return;

	if (sdl.desktop.lazy_fullscreen) {
//		sdl.desktop.lazy_fullscreen_req=true;
		LOG_MSG("GFX LF: fullscreen switching not supported");
	} else {
		GFX_SwitchFullScreen();
	}
}

void GFX_SwitchLazyFullscreen(bool lazy) {
	sdl.desktop.lazy_fullscreen=lazy;
	sdl.desktop.lazy_fullscreen_req=false;
}

void GFX_SwitchFullscreenNoReset(void) {
	sdl.desktop.fullscreen=!sdl.desktop.fullscreen;
}

bool GFX_LazyFullscreenRequested(void) {
	if (sdl.desktop.lazy_fullscreen) return sdl.desktop.lazy_fullscreen_req;
	return false;
}

void GFX_RestoreMode(void) {
	GFX_SetSize(sdl.draw.width,sdl.draw.height,sdl.draw.flags,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback, sdl.draw.aspect); //TODO: Is passing sdl.aspect correct here?
	GFX_UpdateSDLCaptureState();
}


bool GFX_StartUpdate(Bit8u * & pixels,Bitu & pitch) {
	if (!sdl.active || sdl.updating)
	{	return false;  }
	if( !sdl.desktop.screen.StartUpdate( &pixels, &pitch ) )
	{	return false;  }
	sdl.updating = true;
	return true;
}


void GFX_EndUpdate( const Bit16u *changedLines ) {
	if (!sdl.updating)
		return;
	sdl.updating=false;
	sdl.desktop.screen.EndUpdate( changedLines );
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

Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue)
{	return sdl.desktop.screen.GetRgb( red, green, blue );	}

void GFX_Stop() {
	if (sdl.updating)
		GFX_EndUpdate( 0 );
	sdl.active=false;
}

void GFX_Start() {
	sdl.active=true;
}

static void GUI_ShutDown(Section * /*sec*/) {
	GFX_Stop();
	if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackStop );
	if (sdl.mouse.locked) GFX_CaptureMouse();
	if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
}


static void SetPriority(PRIORITY_LEVELS level) {

#if C_SET_PRIORITY
// Do nothing if priorties are not the same and not root, else the highest
// priority can not be set as users can only lower priority (not restore it)

	if((sdl.priority.focus != sdl.priority.nofocus ) &&
		(getuid()!=0) ) return;

#endif
	switch (level) {
#ifdef WIN32
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_LOWER:
		SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_NORMAL:
		SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHER:
		SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHEST:
		SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
		break;
#elif C_SET_PRIORITY
/* Linux use group as dosbox has mulitple threads under linux */
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		setpriority (PRIO_PGRP, 0,PRIO_MAX);
		break;
	case PRIORITY_LEVEL_LOWER:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/3));
		break;
	case PRIORITY_LEVEL_NORMAL:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/2));
		break;
	case PRIORITY_LEVEL_HIGHER:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/5) );
		break;
	case PRIORITY_LEVEL_HIGHEST:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/4) );
		break;
#endif
	default:
		break;
	}
}

extern Bit8u int10_font_14[256 * 14];
static void OutputString(Bitu x,Bitu y,const char * text,Bit32u color,Bit32u color2,SDL_Surface * output_surface) {
	Bit32u * draw=(Bit32u*)(((Bit8u *)output_surface->pixels)+((y)*output_surface->pitch))+x;
	while (*text) {
		Bit8u * font=&int10_font_14[(*text)*14];
		Bitu i,j;
		Bit32u * draw_line=draw;
		for (i=0;i<14;i++) {
			Bit8u map=*font++;
			for (j=0;j<8;j++) {
				if (map & 0x80) *((Bit32u*)(draw_line+j))=color; else *((Bit32u*)(draw_line+j))=color2;
				map<<=1;
			}
			draw_line+=output_surface->pitch/4;
		}
		text++;
		draw+=8;
	}
}

#include "dosbox_splash.h"

//extern void UI_Run(bool);
void Restart(bool pressed);

enum ResKind { RES_ORIGINAL, RES_DESKTOP, RES_FIXED };

ResKind ParseRes( char* res, Bit16u* w, Bit16u* h )
{	char* height = NULL;
	*w = 0; *h = 0;
	ResKind kind = RES_ORIGINAL;
	if( res == NULL || *res == '\0' )
	{	goto end;  }
	char res_buf[100];
	safe_strncpy( res_buf, res, sizeof( res_buf ) );
	res = lowcase ( res_buf );//so x and X are allowed
	if( strcmp( res,"original" ) == 0 )
	{	goto end;  }
	if( strcmp( res,"desktop" ) == 0 )
	{	kind = RES_DESKTOP;
		goto end;
	}
	height = const_cast<char*>(strchr(res,'x'));
	if( height == NULL || *height == '\0' )
	{	goto end;  }
	kind    = RES_FIXED;
	*height = 0;
	*h   = ( Bit16u )atoi( height + 1 );
	*w   = ( Bit16u )atoi( res );
end:
	return kind;
}

static void GUI_StartUp(Section * sec) {
	sec->AddDestroyFunction(&GUI_ShutDown);
	Section_prop * section=static_cast<Section_prop *>(sec);
	sdl.active=false;
	sdl.updating=false;

	GFX_SetIcon();

	sdl.desktop.lazy_fullscreen=false;
	sdl.desktop.lazy_fullscreen_req=false;

	sdl.desktop.fullscreen=section->Get_bool("fullscreen");
	sdl.desktop.fullborderless=section->Get_bool("fullborderless");
	sdl.wait_on_error=section->Get_bool("waitonerror");

	Prop_multival* p=section->Get_multival("priority");
	std::string focus = p->GetSection()->Get_string("active");
	std::string notfocus = p->GetSection()->Get_string("inactive");

	if      (focus == "lowest")  { sdl.priority.focus = PRIORITY_LEVEL_LOWEST;  }
	else if (focus == "lower")   { sdl.priority.focus = PRIORITY_LEVEL_LOWER;   }
	else if (focus == "normal")  { sdl.priority.focus = PRIORITY_LEVEL_NORMAL;  }
	else if (focus == "higher")  { sdl.priority.focus = PRIORITY_LEVEL_HIGHER;  }
	else if (focus == "highest") { sdl.priority.focus = PRIORITY_LEVEL_HIGHEST; }

	if      (notfocus == "lowest")  { sdl.priority.nofocus=PRIORITY_LEVEL_LOWEST;  }
	else if (notfocus == "lower")   { sdl.priority.nofocus=PRIORITY_LEVEL_LOWER;   }
	else if (notfocus == "normal")  { sdl.priority.nofocus=PRIORITY_LEVEL_NORMAL;  }
	else if (notfocus == "higher")  { sdl.priority.nofocus=PRIORITY_LEVEL_HIGHER;  }
	else if (notfocus == "highest") { sdl.priority.nofocus=PRIORITY_LEVEL_HIGHEST; }
	else if (notfocus == "pause")   {
		/* we only check for pause here, because it makes no sense
		 * for DOSBox to be paused while it has focus
		 */
		sdl.priority.nofocus=PRIORITY_LEVEL_PAUSE;
	}

	SetPriority(sdl.priority.focus); //Assume focus on startup
	sdl.mouse.locked=false;
	mouselocked=false; //Global for mapper
	sdl.mouse.requestlock=false;

	ResKind deskRes = ParseRes
	(	(char*)section->Get_string("fullresolution"),
		&sdl.desktop.full.width, &sdl.desktop.full.height
	);
	// fix the fullscreen resolution for a borderelss window:
	sdl.desktop.full.fixed = sdl.desktop.fullborderless || deskRes != RES_ORIGINAL;
	sdl.desktop.doublebuf=section->Get_bool("fulldouble");
#if SDL_VERSION_ATLEAST(1, 2, 10)
#ifdef WIN32
	const SDL_VideoInfo* vidinfo = SDL_GetVideoInfo();
	if (vidinfo) {
		int sdl_w = vidinfo->current_w;
		int sdl_h = vidinfo->current_h;
		int win_w = GetSystemMetrics(SM_CXSCREEN);
		int win_h = GetSystemMetrics(SM_CYSCREEN);
		if (sdl_w != win_w && sdl_h != win_h) 
			LOG_MSG("Windows dpi/blurry apps scaling detected! The screen might be too large or not\n"
			        "show properly, please see the DOSBox options file (fullresolution) for details.\n");
		}
#else
	if (!sdl.desktop.full.width || !sdl.desktop.full.height){
		//Can only be done on the very first call! Not restartable.
		//On windows don't use it as SDL returns the values without taking in account the dpi scaling
		const SDL_VideoInfo* vidinfo = SDL_GetVideoInfo();
		if (vidinfo) {
			sdl.desktop.full.width = vidinfo->current_w;
			sdl.desktop.full.height = vidinfo->current_h;
		}
	}
#endif
#endif

	if (!sdl.desktop.full.width) {
#ifdef WIN32
		sdl.desktop.full.width=(Bit16u)GetSystemMetrics(SM_CXSCREEN);
#else
		LOG_MSG("Your fullscreen resolution can NOT be determined, it's assumed to be 1024x768.\nPlease edit the configuration file if this value is wrong.");
		sdl.desktop.full.width=1024;
#endif
	}
	if (!sdl.desktop.full.height) {
#ifdef WIN32
		sdl.desktop.full.height=(Bit16u)GetSystemMetrics(SM_CYSCREEN);
#else
		sdl.desktop.full.height=768;
#endif
	}
	ResKind windowRes = ParseRes
	(	(char*)section->Get_string("windowresolution"),
		&sdl.desktop.window.width, &sdl.desktop.window.height
	);
	if( windowRes == RES_DESKTOP )
	{	// Ant_222: I don't know how else to correct for the window borders:
		sdl.desktop.window.height = sdl.desktop.full.height - 42;
		sdl.desktop.window.width  = sdl.desktop.full.width  - 16;
	}

	sdl.mouse.autoenable=section->Get_bool("autolock");
	if (!sdl.mouse.autoenable) SDL_ShowCursor(SDL_DISABLE);
	sdl.mouse.autolock=false;

	Prop_multival* p3 = section->Get_multival("sensitivity");
	sdl.mouse.xsensitivity = p3->GetSection()->Get_int("xsens");
	sdl.mouse.ysensitivity = p3->GetSection()->Get_int("ysens");
	std::string output=section->Get_string("output");

	/* Setup Mouse correctly if fullscreen */
	if(sdl.desktop.fullscreen) GFX_CaptureMouse();

	if (output == "surface") {
		sdl.desktop.want_type=SCREEN_SURFACE;
	} else if (output == "surfacepp") {
		sdl.desktop.want_type=SCREEN_SURFACE;
		sdl.SurfaceMode = SM_PERFECT;
	} else if (output == "surfacenp") {
		sdl.ps_sharpness=(double)section->Get_int("surfacenp-sharpness") / 100.0;
		sdl.desktop.want_type=SCREEN_SURFACE;
		// fallback to nearest-neighbor for better performace:
		if( sdl.ps_sharpness == 1.0 )
		{	sdl.SurfaceMode = SM_NEIGHBOR;  }
		else
		{	sdl.SurfaceMode = SM_SOFT;  }
	} else if (output == "surfacenb") {
		sdl.desktop.want_type=SCREEN_SURFACE;
		sdl.SurfaceMode = SM_NEIGHBOR;
#if C_DDRAW
	} else if (output == "ddraw") {
		sdl.desktop.want_type=SCREEN_SURFACE_DDRAW;
#endif
	} else if (output == "overlay") {
		sdl.desktop.want_type=SCREEN_OVERLAY;
#if C_OPENGL
	} else if (output == "opengl") {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.opengl.kind = GlkBilinear;
	} else if (output == "openglnb") {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.opengl.kind = GlkNearest;
	} else if (output == "openglpp") {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.opengl.kind = GlkPerfect;
#endif
	} else {
		LOG_MSG("SDL: Unsupported output device %s, switching back to surface",output.c_str());
		sdl.desktop.want_type=SCREEN_SURFACE;//SHOULDN'T BE POSSIBLE anymore
	}

	sdl.overlay=0;
#if C_OPENGL
	if (sdl.desktop.want_type == SCREEN_OPENGL) { /* OPENGL is requested */
		sdl.surface = SDL_SetVideoMode_Wrap(640,400,0,SDL_OPENGL);
		if (sdl.surface == NULL) {
			LOG_MSG("Could not initialize OpenGL, switching back to surface");
			sdl.desktop.want_type = SCREEN_SURFACE;
		} else {
			sdl.opengl.vsync = section->Get_bool("glfullvsync");
			sdl.opengl.buffer=0;
			sdl.opengl.framebuf=0;
			sdl.opengl.texture=0;
			sdl.opengl.displaylist=0;
			glGetIntegerv (GL_MAX_TEXTURE_SIZE, &sdl.opengl.max_texsize);
			glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)SDL_GL_GetProcAddress("glGenBuffersARB");
			glBindBufferARB = (PFNGLBINDBUFFERARBPROC)SDL_GL_GetProcAddress("glBindBufferARB");
			glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)SDL_GL_GetProcAddress("glDeleteBuffersARB");
			glBufferDataARB = (PFNGLBUFFERDATAARBPROC)SDL_GL_GetProcAddress("glBufferDataARB");
			glMapBufferARB = (PFNGLMAPBUFFERARBPROC)SDL_GL_GetProcAddress("glMapBufferARB");
			glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)SDL_GL_GetProcAddress("glUnmapBufferARB");
			const char * gl_ext = (const char *)glGetString (GL_EXTENSIONS);
			if(gl_ext && *gl_ext){
				sdl.opengl.packed_pixel=(strstr(gl_ext,"EXT_packed_pixels") != NULL);
				sdl.opengl.paletted_texture=(strstr(gl_ext,"EXT_paletted_texture") != NULL);
				sdl.opengl.pixel_buffer_object=(strstr(gl_ext,"GL_ARB_pixel_buffer_object") != NULL ) &&
				    glGenBuffersARB && glBindBufferARB && glDeleteBuffersARB && glBufferDataARB &&
				    glMapBufferARB && glUnmapBufferARB;
    			} else {
				sdl.opengl.packed_pixel = false;
				sdl.opengl.paletted_texture = false;
				sdl.opengl.pixel_buffer_object = false;
			}
			LOG_MSG("OpenGL extensions: packed pixel %d, paletted_texture %d, pixel_bufer_object %d",sdl.opengl.packed_pixel,sdl.opengl.paletted_texture,sdl.opengl.pixel_buffer_object);
		}
	} /* OPENGL is requested end */

#endif	//OPENGL
	/* Initialize screen for first time */
	sdl.surface=SDL_SetVideoMode_Wrap(640,400,0,0);
	if (sdl.surface == NULL) E_Exit("Could not initialize video: %s",SDL_GetError());
	sdl.desktop.bpp=sdl.surface->format->BitsPerPixel;
	if (sdl.desktop.bpp==24) {
		LOG_MSG("SDL: You are running in 24 bpp mode, this will slow down things!");
	}
	GFX_Stop();
	SDL_WM_SetCaption("DOSBox",VERSION);

/* The endian part is intentionally disabled as somehow it produces correct results without according to rhoenie*/
//#if SDL_BYTEORDER == SDL_BIG_ENDIAN
//    Bit32u rmask = 0xff000000;
//    Bit32u gmask = 0x00ff0000;
//    Bit32u bmask = 0x0000ff00;
//#else
    Bit32u rmask = 0x000000ff;
    Bit32u gmask = 0x0000ff00;
    Bit32u bmask = 0x00ff0000;
//#endif

/* Please leave the Splash screen stuff in working order in DOSBox. We spend a lot of time making DOSBox. */
	SDL_Surface* splash_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 400, 32, rmask, gmask, bmask, 0);
	if (splash_surf) {
		SDL_FillRect(splash_surf, NULL, SDL_MapRGB(splash_surf->format, 0, 0, 0));

		Bit8u* tmpbufp = new Bit8u[640*400*3];
		DOSBOX_SPLASH_RUN_LENGTH_DECODE(tmpbufp,dosbox_splash.rle_pixel_data,640*400,3);
		for (Bitu y=0; y<400; y++) {

			Bit8u* tmpbuf = tmpbufp + y*640*3;
			Bit32u * draw=(Bit32u*)(((Bit8u *)splash_surf->pixels)+((y)*splash_surf->pitch));
			for (Bitu x=0; x<640; x++) {
//#if SDL_BYTEORDER == SDL_BIG_ENDIAN
//				*draw++ = tmpbuf[x*3+2]+tmpbuf[x*3+1]*0x100+tmpbuf[x*3+0]*0x10000+0x00000000;
//#else
				*draw++ = tmpbuf[x*3+0]+tmpbuf[x*3+1]*0x100+tmpbuf[x*3+2]*0x10000+0x00000000;
//#endif
			}
		}

		bool exit_splash = false;

		static Bitu max_splash_loop = 600;
		static Bitu splash_fade = 100;
		static bool use_fadeout = true;

		for (Bit32u ct = 0,startticks = GetTicks();ct < max_splash_loop;ct = GetTicks()-startticks) {
			SDL_Event evt;
			while (SDL_PollEvent(&evt)) {
				if (evt.type == SDL_QUIT) {
					exit_splash = true;
					break;
				}
			}
			if (exit_splash) break;

			if (ct<1) {
				SDL_FillRect(sdl.surface, NULL, SDL_MapRGB(sdl.surface->format, 0, 0, 0));
				SDL_SetAlpha(splash_surf, SDL_SRCALPHA,255);
				SDL_BlitSurface(splash_surf, NULL, sdl.surface, NULL);
				SDL_Flip(sdl.surface);
			} else if (ct>=max_splash_loop-splash_fade) {
				if (use_fadeout) {
					SDL_FillRect(sdl.surface, NULL, SDL_MapRGB(sdl.surface->format, 0, 0, 0));
					SDL_SetAlpha(splash_surf, SDL_SRCALPHA, (Bit8u)((max_splash_loop-1-ct)*255/(splash_fade-1)));
					SDL_BlitSurface(splash_surf, NULL, sdl.surface, NULL);
					SDL_Flip(sdl.surface);
				}
			}

		}

		if (use_fadeout) {
			SDL_FillRect(sdl.surface, NULL, SDL_MapRGB(sdl.surface->format, 0, 0, 0));
			SDL_Flip(sdl.surface);
		}
		SDL_FreeSurface(splash_surf);
		delete [] tmpbufp;

	}

	/* Get some Event handlers */
	MAPPER_AddHandler(KillSwitch,MK_f9,MMOD1,"shutdown","ShutDown");
	MAPPER_AddHandler(CaptureMouse,MK_f10,MMOD1,"capmouse","Cap Mouse");
	MAPPER_AddHandler(SwitchFullScreen,MK_return,MMOD2,"fullscr","Fullscreen");
	MAPPER_AddHandler(Restart,MK_home,MMOD1|MMOD2,"restart","Restart");
#if C_DEBUG
	/* Pause binds with activate-debugger */
#else
	MAPPER_AddHandler(&PauseDOSBox, MK_pause, MMOD2, "pause", "Pause DBox");
#endif
	/* Get Keyboard state of numlock and capslock */
	SDLMod keystate = SDL_GetModState();
	if(keystate&KMOD_NUM) startup_state_numlock = true;
	if(keystate&KMOD_CAPS) startup_state_capslock = true;
}

void Mouse_AutoLock(bool enable) {
	sdl.mouse.autolock=enable;
	if (sdl.mouse.autoenable) sdl.mouse.requestlock=enable;
	else {
		SDL_ShowCursor(enable?SDL_DISABLE:SDL_ENABLE);
		sdl.mouse.requestlock=false;
	}
}

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) {
	if (sdl.mouse.locked || !sdl.mouse.autoenable)
		Mouse_CursorMoved((float)motion->xrel*sdl.mouse.xsensitivity/100.0f,
						  (float)motion->yrel*sdl.mouse.ysensitivity/100.0f,
						  (float)(motion->x-sdl.clip.x)/(sdl.clip.w-1)*sdl.mouse.xsensitivity/100.0f,
						  (float)(motion->y-sdl.clip.y)/(sdl.clip.h-1)*sdl.mouse.ysensitivity/100.0f,
						  sdl.mouse.locked);
}

static void HandleMouseButton(SDL_MouseButtonEvent * button) {
	switch (button->state) {
	case SDL_PRESSED:
		if (sdl.mouse.requestlock && !sdl.mouse.locked) {
			GFX_CaptureMouse();
			// Don't pass click to mouse handler
			break;
		}
		if (!sdl.mouse.autoenable && sdl.mouse.autolock && button->button == SDL_BUTTON_MIDDLE) {
			GFX_CaptureMouse();
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

void GFX_LosingFocus(void) {
	sdl.laltstate=SDL_KEYUP;
	sdl.raltstate=SDL_KEYUP;
	MAPPER_LosingFocus();
}

bool GFX_IsFullscreen(void) {
	return sdl.desktop.fullscreen;
}

#if defined(MACOSX)
#define DB_POLLSKIP 3
#else
//Not used yet, see comment below
#define DB_POLLSKIP 1
#endif

#if defined(LINUX)
#define SDL_XORG_FIX 1
#else
#define SDL_XORG_FIX 0
#endif

void GFX_Events() {
	//Don't poll too often. This can be heavy on the OS, especially Macs.
	//In idle mode 3000-4000 polls are done per second without this check.
	//Macs, with this code,  max 250 polls per second. (non-macs unused default max 500)
	//Currently not implemented for all platforms, given the ALT-TAB stuff for WIN32.
#if defined (MACOSX)
	static int last_check = 0;
	int current_check = GetTicks();
	if (current_check - last_check <=  DB_POLLSKIP) return;
	last_check = current_check;
#endif

	SDL_Event event;
#if defined (REDUCE_JOYSTICK_POLLING)
	static int poll_delay = 0;
	int time = GetTicks();
	if (time - poll_delay > 20) {
		poll_delay = time;
		if (sdl.num_joysticks > 0) SDL_JoystickUpdate();
		MAPPER_UpdateJoysticks();
	}
#endif
	while (SDL_PollEvent(&event)) {
#if SDL_XORG_FIX
		// Special code for broken SDL with Xorg 1.20.1, where pairs of inputfocus gain and loss events are generated
		// when locking the mouse in windowed mode.
		if (event.type == SDL_ACTIVEEVENT && event.active.state == SDL_APPINPUTFOCUS && event.active.gain == 0) {
			SDL_Event test; //Check if the next event would undo this one.
			if (SDL_PeepEvents(&test,1,SDL_PEEKEVENT,SDL_ACTIVEEVENTMASK) == 1 && test.active.state == SDL_APPINPUTFOCUS && test.active.gain == 1) {
				// Skip both events.
				SDL_PeepEvents(&test,1,SDL_GETEVENT,SDL_ACTIVEEVENTMASK);
				continue;
			}
		}
#endif

		switch (event.type) {
		case SDL_ACTIVEEVENT:
			if (event.active.state & SDL_APPINPUTFOCUS) {
				if (event.active.gain) {
#ifdef WIN32
					if (!sdl.desktop.fullscreen) sdl.focus_ticks = GetTicks();
#endif
					if (sdl.desktop.fullscreen && !sdl.mouse.locked)
						GFX_CaptureMouse();
					SetPriority(sdl.priority.focus);
					CPU_Disable_SkipAutoAdjust();
				} else {
					if (sdl.mouse.locked) {
#ifdef WIN32
						if (sdl.desktop.fullscreen) {
							VGA_KillDrawing();
							GFX_ForceFullscreenExit();
						}
#endif
						GFX_CaptureMouse();
					}
					SetPriority(sdl.priority.nofocus);
					GFX_LosingFocus();
					CPU_Enable_SkipAutoAdjust();
				}
			}

			/* Non-focus priority is set to pause; check to see if we've lost window or input focus
			 * i.e. has the window been minimised or made inactive?
			 */
			if (sdl.priority.nofocus == PRIORITY_LEVEL_PAUSE) {
				if ((event.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) && (!event.active.gain)) {
					/* Window has lost focus, pause the emulator.
					 * This is similar to what PauseDOSBox() does, but the exit criteria is different.
					 * Instead of waiting for the user to hit Alt-Break, we wait for the window to
					 * regain window or input focus.
					 */
					bool paused = true;
					SDL_Event ev;

					GFX_SetTitle(-1,-1,true);
					KEYBOARD_ClrBuffer();
//					SDL_Delay(500);
//					while (SDL_PollEvent(&ev)) {
						// flush event queue.
//					}

					while (paused) {
						// WaitEvent waits for an event rather than polling, so CPU usage drops to zero
						SDL_WaitEvent(&ev);

						switch (ev.type) {
						case SDL_QUIT: throw(0); break; // a bit redundant at linux at least as the active events gets before the quit event.
						case SDL_ACTIVEEVENT:     // wait until we get window focus back
							if (ev.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) {
								// We've got focus back, so unpause and break out of the loop
								if (ev.active.gain) {
									paused = false;
									GFX_SetTitle(-1,-1,false);
								}

								/* Now poke a "release ALT" command into the keyboard buffer
								 * we have to do this, otherwise ALT will 'stick' and cause
								 * problems with the app running in the DOSBox.
								 */
								KEYBOARD_AddKey(KBD_leftalt, false);
								KEYBOARD_AddKey(KBD_rightalt, false);
							}
							break;
						}
					}
				}
			}
			break;
		case SDL_MOUSEMOTION:
			HandleMouseMotion(&event.motion);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			HandleMouseButton(&event.button);
			break;
		case SDL_VIDEORESIZE:
//			HandleVideoResize(&event.resize);
			break;
		case SDL_QUIT:
			throw(0);
			break;
		case SDL_VIDEOEXPOSE:
			if ((sdl.draw.callback) && (!glide.enabled)) sdl.draw.callback( GFX_CallBackRedraw );
			break;
#ifdef WIN32
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			// ignore event alt+tab
			if (event.key.keysym.sym==SDLK_LALT) sdl.laltstate = event.key.type;
			if (event.key.keysym.sym==SDLK_RALT) sdl.raltstate = event.key.type;
			if (((event.key.keysym.sym==SDLK_TAB)) &&
				((sdl.laltstate==SDL_KEYDOWN) || (sdl.raltstate==SDL_KEYDOWN))) break;
			// This can happen as well.
			if (((event.key.keysym.sym == SDLK_TAB )) && (event.key.keysym.mod & KMOD_ALT)) break;
			// ignore tab events that arrive just after regaining focus. (likely the result of alt-tab)
			if ((event.key.keysym.sym == SDLK_TAB) && (GetTicks() - sdl.focus_ticks < 2)) break;
#endif
#if defined (MACOSX)
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			/* On macs CMD-Q is the default key to close an application */
			if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod == KMOD_RMETA || event.key.keysym.mod == KMOD_LMETA) ) {
				KillSwitch(true);
				break;
			}
#endif
		default:
			void MAPPER_CheckEvent(SDL_Event * event);
			MAPPER_CheckEvent(&event);
		}
	}
}

#if defined (WIN32)
static BOOL WINAPI ConsoleEventHandler(DWORD event) {
	switch (event) {
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
		raise(SIGTERM);
		return TRUE;
	case CTRL_C_EVENT:
	default: //pass to the next handler
		return FALSE;
	}
}
#endif


/* static variable to show wether there is not a valid stdout.
 * Fixes some bugs when -noconsole is used in a read only directory */
static bool no_stdout = false;
void GFX_ShowMsg(char const* format,...) {
	char buf[512];

	va_list msg;
	va_start(msg,format);
	vsnprintf(buf,sizeof(buf),format,msg);
	va_end(msg);

	buf[sizeof(buf) - 1] = '\0';
	if (!no_stdout) puts(buf); //Else buf is parsed again. (puts adds end of line)
}


void Config_Add_SDL() {
	Section_prop * sdl_sec=control->AddSection_prop("sdl",&GUI_StartUp);
	sdl_sec->AddInitFunction(&MAPPER_StartUp);
	Prop_bool* Pbool;
	Prop_string* Pstring;
	Prop_int* Pint;
	Prop_multival* Pmulti;

	Pbool = sdl_sec->Add_bool("fullscreen",Property::Changeable::Always,false);
	Pbool->Set_help("Start dosbox directly in fullscreen. (Press ALT-Enter to go back)");

	Pbool = sdl_sec->Add_bool("fullborderless",Property::Changeable::Always,false);
	Pbool->Set_help("Emulate fullscreen as a borderless window");

	Pbool = sdl_sec->Add_bool("fulldouble",Property::Changeable::Always,false);
	Pbool->Set_help("Use double buffering in fullscreen. It can reduce screen flickering, but it can also result in a slow DOSBox.");

	Pstring = sdl_sec->Add_string("fullresolution",Property::Changeable::Always,"desktop");
	Pstring->Set_help("What resolution to use for fullscreen: original, desktop or a fixed size (e.g. 1024x768).\n"
	                  "Using your monitor's native resolution with aspect=true might give the best results.\n"
			  "If you end up with small window on a large screen, try an output different from surface."
	                  "On Windows 10 with display scaling (Scale and layout) set to a value above 100%, it is recommended\n"
	                  "to use a lower full/windowresolution, in order to avoid window size problems.");

	Pstring = sdl_sec->Add_string("windowresolution",Property::Changeable::Always,"desktop");
	Pstring->Set_help("Scale the window to this size IF the output device supports hardware scaling.\n"
	                  "(output=surface does not!)");

	const char* outputs[] = {
		"surface", "surfacepp", "surfacenp", "surfacenb", "overlay",
#if C_OPENGL
		"opengl", "openglnb", "openglpp",
#endif
#if C_DDRAW
		"ddraw",
#endif
		0
	};
	Pstring = sdl_sec->Add_string("output",Property::Changeable::Always,
#if C_OPENGL
	"openglpp"
#else
	"surfacepp"
#endif
	);
	Pstring->Set_help
	(	"What video system to use for output.\n"
		"Some values are aliases for output-scaler combinations:\n"
		"  surfacepp and openglpp -- pixel-perfect scaling;\n"
		"  surfacenp -- near-perfect scaling via bilinear interpolation;\n"
		"  surfacenb and openglnb -- nearest-neighbor scaling."
	);
	Pint = sdl_sec->Add_int("surfacenp-sharpness",Property::Changeable::Always,50);
	Pint->SetMinMax(0, 100);
	Pint->Set_help
	(	"Sharpness for the 'surfacenp' output type,\n"
		"Measured in percent."
	);

	Pstring->Set_values(outputs);

#if C_OPENGL
	Pbool = sdl_sec->Add_bool("glfullvsync",Property::Changeable::Always,false);
	Pbool->Set_help("Activate V-Sync for OpenGL in fullscreen.");
#endif

	Pbool = sdl_sec->Add_bool("autolock",Property::Changeable::Always,true);
	Pbool->Set_help("Mouse will automatically lock, if you click on the screen. (Press CTRL-F10 to unlock)");

	Pmulti = sdl_sec->Add_multi("sensitivity",Property::Changeable::Always, ",");
	Pmulti->Set_help("Mouse sensitivity. The optional second parameter specifies vertical sensitivity (e.g. 100,-50).");
	Pmulti->SetValue("100");
	Pint = Pmulti->GetSection()->Add_int("xsens",Property::Changeable::Always,100);
	Pint->SetMinMax(-1000,1000);
	Pint = Pmulti->GetSection()->Add_int("ysens",Property::Changeable::Always,100);
	Pint->SetMinMax(-1000,1000);

	Pbool = sdl_sec->Add_bool("waitonerror",Property::Changeable::Always, true);
	Pbool->Set_help("Wait before closing the console if dosbox has an error.");

	Pmulti = sdl_sec->Add_multi("priority", Property::Changeable::Always, ",");
	Pmulti->SetValue("higher,normal");
	Pmulti->Set_help("Priority levels for dosbox. Second entry behind the comma is for when dosbox is not focused/minimized.\n"
	                 "pause is only valid for the second entry.");

	const char* actt[] = { "lowest", "lower", "normal", "higher", "highest", "pause", 0};
	Pstring = Pmulti->GetSection()->Add_string("active",Property::Changeable::Always,"higher");
	Pstring->Set_values(actt);

	const char* inactt[] = { "lowest", "lower", "normal", "higher", "highest", "pause", 0};
	Pstring = Pmulti->GetSection()->Add_string("inactive",Property::Changeable::Always,"normal");
	Pstring->Set_values(inactt);

	Pstring = sdl_sec->Add_path("mapperfile",Property::Changeable::Always,MAPPERFILE);
	Pstring->Set_help("File used to load/save the key/event mappings from. Resetmapper only works with the default value.");

	Pbool = sdl_sec->Add_bool("usescancodes",Property::Changeable::Always,true);
	Pbool->Set_help("Avoid usage of symkeys, might not work on all operating systems.");
}

static void show_warning(char const * const message) {
	bool textonly = true;
#ifdef WIN32
	textonly = false;
	if ( !sdl.inited && SDL_Init(SDL_INIT_VIDEO|SDL_INIT_NOPARACHUTE) < 0 ) textonly = true;
	sdl.inited = true;
#endif
	printf("%s",message);
	if(textonly) return;
	if(!sdl.surface) sdl.surface = SDL_SetVideoMode_Wrap(640,400,0,0);
	if(!sdl.surface) return;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	Bit32u rmask = 0xff000000;
	Bit32u gmask = 0x00ff0000;
	Bit32u bmask = 0x0000ff00;
#else
	Bit32u rmask = 0x000000ff;
	Bit32u gmask = 0x0000ff00;
	Bit32u bmask = 0x00ff0000;
#endif
	SDL_Surface* splash_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 400, 32, rmask, gmask, bmask, 0);
	if (!splash_surf) return;

	int x = 120,y = 20;
	std::string m(message),m2;
	std::string::size_type a,b,c,d;

	while(m.size()) { //Max 50 characters. break on space before or on a newline
		c = m.find('\n');
		d = m.rfind(' ',50);
		if(c>d) a=b=d; else a=b=c;
		if( a != std::string::npos) b++;
		m2 = m.substr(0,a); m.erase(0,b);
		OutputString(x,y,m2.c_str(),0xffffffff,0,splash_surf);
		y += 20;
	}

	SDL_BlitSurface(splash_surf, NULL, sdl.surface, NULL);
	SDL_Flip(sdl.surface);
	SDL_Delay(12000);
}

static void launcheditor() {
	std::string path,file;
	Cross::CreatePlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;
	FILE* f = fopen(path.c_str(),"r");
	if(!f && !control->PrintConfig(path.c_str())) {
		printf("tried creating %s. but failed.\n",path.c_str());
		exit(1);
	}
	if(f) fclose(f);
/*	if(edit.empty()) {
		printf("no editor specified.\n");
		exit(1);
	}*/
	std::string edit;
	while(control->cmdline->FindString("-editconf",edit,true)) //Loop until one succeeds
		execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
	//if you get here the launching failed!
	printf("can't find editor(s) specified at the command line.\n");
	exit(1);
}
#if C_DEBUG
extern void DEBUG_ShutDown(Section * /*sec*/);
#endif

void restart_program(std::vector<std::string> & parameters) {
	char** newargs = new char* [parameters.size() + 1];
	// parameter 0 is the executable path
	// contents of the vector follow
	// last one is NULL
	for(Bitu i = 0; i < parameters.size(); i++) newargs[i] = (char*)parameters[i].c_str();
	newargs[parameters.size()] = NULL;
	SDL_CloseAudio();
	SDL_Delay(50);
	SDL_Quit();
#if C_DEBUG
	// shutdown curses
	DEBUG_ShutDown(NULL);
#endif

	if(execvp(newargs[0], newargs) == -1) {
#ifdef WIN32
		if(newargs[0][0] == '\"') {
			//everything specifies quotes around it if it contains a space, however my system disagrees
			std::string edit = parameters[0];
			edit.erase(0,1);edit.erase(edit.length() - 1,1);
			//However keep the first argument of the passed argv (newargs) with quotes, as else repeated restarts go wrong.
			if(execvp(edit.c_str(), newargs) == -1) E_Exit("Restarting failed");
		}
#endif
		E_Exit("Restarting failed");
	}
	delete [] newargs;
}
void Restart(bool pressed) { // mapper handler
	restart_program(control->startup_params);
}

static void launchcaptures(std::string const& edit) {
	std::string path,file;
	Section* t = control->GetSection("dosbox");
	if(t) file = t->GetPropValue("captures");
	if(!t || file == NO_SUCH_PROPERTY) {
		printf("Config system messed up.\n");
		exit(1);
	}
	Cross::CreatePlatformConfigDir(path);
	path += file;
	Cross::CreateDir(path);
	struct stat cstat;
	if(stat(path.c_str(),&cstat) || (cstat.st_mode & S_IFDIR) == 0) {
		printf("%s doesn't exists or isn't a directory.\n",path.c_str());
		exit(1);
	}
/*	if(edit.empty()) {
		printf("no editor specified.\n");
		exit(1);
	}*/

	execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
	//if you get here the launching failed!
	printf("can't find filemanager %s\n",edit.c_str());
	exit(1);
}

static void printconfiglocation() {
	std::string path,file;
	Cross::CreatePlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;

	FILE* f = fopen(path.c_str(),"r");
	if(!f && !control->PrintConfig(path.c_str())) {
		printf("tried creating %s. but failed",path.c_str());
		exit(1);
	}
	if(f) fclose(f);
	printf("%s\n",path.c_str());
	exit(0);
}

static void eraseconfigfile() {
	FILE* f = fopen("dosbox.conf","r");
	if(f) {
		fclose(f);
		show_warning("Warning: dosbox.conf exists in current working directory.\nThis will override the configuration file at runtime.\n");
	}
	std::string path,file;
	Cross::GetPlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;
	f = fopen(path.c_str(),"r");
	if(!f) exit(0);
	fclose(f);
	unlink(path.c_str());
	exit(0);
}

static void erasemapperfile() {
	FILE* g = fopen("dosbox.conf","r");
	if(g) {
		fclose(g);
		show_warning("Warning: dosbox.conf exists in current working directory.\nKeymapping might not be properly reset.\n"
		             "Please reset configuration as well and delete the dosbox.conf.\n");
	}

	std::string path,file=MAPPERFILE;
	Cross::GetPlatformConfigDir(path);
	path += file;
	FILE* f = fopen(path.c_str(),"r");
	if(!f) exit(0);
	fclose(f);
	unlink(path.c_str());
	exit(0);
}

void Disable_OS_Scaling() {
#if defined (WIN32)
	typedef BOOL (*function_set_dpi_pointer)();
	function_set_dpi_pointer function_set_dpi;
	function_set_dpi = (function_set_dpi_pointer) GetProcAddress(LoadLibrary("user32.dll"), "SetProcessDPIAware");
	if (function_set_dpi) {
		function_set_dpi();
	}
#endif
}

//extern void UI_Init(void);
int main(int argc, char* argv[]) {
	try {
		Disable_OS_Scaling(); //Do this early on, maybe override it through some parameter.

		CommandLine com_line(argc,argv);
		Config myconf(&com_line);
		control=&myconf;
		/* Init the configuration system and add default values */
		Config_Add_SDL();
		DOSBOX_Init();

		std::string editor;
		if(control->cmdline->FindString("-editconf",editor,false)) launcheditor();
		if(control->cmdline->FindString("-opencaptures",editor,true)) launchcaptures(editor);
		if(control->cmdline->FindExist("-eraseconf")) eraseconfigfile();
		if(control->cmdline->FindExist("-resetconf")) eraseconfigfile();
		if(control->cmdline->FindExist("-erasemapper")) erasemapperfile();
		if(control->cmdline->FindExist("-resetmapper")) erasemapperfile();

		/* Can't disable the console with debugger enabled */
#if defined(WIN32) && !(C_DEBUG)
		if (control->cmdline->FindExist("-noconsole")) {
			FreeConsole();
			/* Redirect standard input and standard output */
			if(freopen(STDOUT_FILE, "w", stdout) == NULL)
				no_stdout = true; // No stdout so don't write messages
			freopen(STDERR_FILE, "w", stderr);
			setvbuf(stdout, NULL, _IOLBF, BUFSIZ);	/* Line buffered */
			setbuf(stderr, NULL);					/* No buffering */
		} else {
			if (AllocConsole()) {
				fclose(stdin);
				fclose(stdout);
				fclose(stderr);
				freopen("CONIN$","r",stdin);
				freopen("CONOUT$","w",stdout);
				freopen("CONOUT$","w",stderr);
			}
			SetConsoleTitle("DOSBox Status Window");
		}
#endif  //defined(WIN32) && !(C_DEBUG)
		if (control->cmdline->FindExist("-version") ||
		    control->cmdline->FindExist("--version") ) {
			printf("\nDOSBox version %s, copyright 2002-2019 DOSBox Team.\n\n",VERSION);
			printf("DOSBox is written by the DOSBox Team (See AUTHORS file))\n");
			printf("DOSBox comes with ABSOLUTELY NO WARRANTY.  This is free software,\n");
			printf("and you are welcome to redistribute it under certain conditions;\n");
			printf("please read the COPYING file thoroughly before doing so.\n\n");
			return 0;
		}
		if(control->cmdline->FindExist("-printconf")) printconfiglocation();

#if C_DEBUG
		DEBUG_SetupConsole();
#endif

#if defined(WIN32)
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) ConsoleEventHandler,TRUE);
#endif

#ifdef OS2
        PPIB pib;
        PTIB tib;
        DosGetInfoBlocks(&tib, &pib);
        if (pib->pib_ultype == 2) pib->pib_ultype = 3;
        setbuf(stdout, NULL);
        setbuf(stderr, NULL);
#endif

	/* Display Welcometext in the console */
	LOG_MSG("DOSBox version %s",VERSION);
	LOG_MSG("Copyright 2002-2019 DOSBox Team, published under GNU GPL.");
	LOG_MSG("---");

	/* Init SDL */
#if SDL_VERSION_ATLEAST(1, 2, 14)
	/* Or debian/ubuntu with older libsdl version as they have done this themselves, but then differently.
	 * with this variable they will work correctly. I've only tested the 1.2.14 behaviour against the windows version
	 * of libsdl
	 */
	putenv(const_cast<char*>("SDL_DISABLE_LOCK_KEYS=1"));
#endif
	// Don't init timers, GetTicks seems to work fine and they can use a fair amount of power (Macs again) 
	// Please report problems with audio and other things.
	if ( SDL_Init( SDL_INIT_AUDIO|SDL_INIT_VIDEO | /*SDL_INIT_TIMER |*/ SDL_INIT_CDROM
		|SDL_INIT_NOPARACHUTE
		) < 0 ) E_Exit("Can't init SDL %s",SDL_GetError());
	sdl.inited = true;

#ifndef DISABLE_JOYSTICK
	//Initialise Joystick separately. This way we can warn when it fails instead
	//of exiting the application
	if( SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0 ) LOG_MSG("Failed to init joystick support");
#endif

	sdl.laltstate = SDL_KEYUP;
	sdl.raltstate = SDL_KEYUP;

#if defined (WIN32)
#if SDL_VERSION_ATLEAST(1, 2, 10)
		sdl.using_windib=true;
#else
		sdl.using_windib=false;
#endif
		char sdl_drv_name[128];
		if (getenv("SDL_VIDEODRIVER")==NULL) {
			if (SDL_VideoDriverName(sdl_drv_name,128)!=NULL) {
				sdl.using_windib=false;
				if (strcmp(sdl_drv_name,"directx")!=0) {
					SDL_QuitSubSystem(SDL_INIT_VIDEO);
					putenv("SDL_VIDEODRIVER=directx");
					if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) {
						putenv("SDL_VIDEODRIVER=windib");
						if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) E_Exit("Can't init SDL Video %s",SDL_GetError());
						sdl.using_windib=true;
					}
				}
			}
		} else {
			char* sdl_videodrv = getenv("SDL_VIDEODRIVER");
			if (strcmp(sdl_videodrv,"directx")==0) sdl.using_windib = false;
			else if (strcmp(sdl_videodrv,"windib")==0) sdl.using_windib = true;
		}
		if (SDL_VideoDriverName(sdl_drv_name,128)!=NULL) {
			if (strcmp(sdl_drv_name,"windib")==0) LOG_MSG("SDL_Init: Starting up with SDL windib video driver.\n          Try to update your video card and directx drivers!");
		}
#endif
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	glide.fullscreen = &sdl.desktop.fullscreen;
	sdl.num_joysticks=SDL_NumJoysticks();

	/* Parse configuration files */
	std::string config_file, config_path, config_combined;
	Cross::GetPlatformConfigDir(config_path);

	//First parse -userconf
	if(control->cmdline->FindExist("-userconf",true)){
		config_file.clear();
		Cross::GetPlatformConfigDir(config_path);
		Cross::GetPlatformConfigName(config_file);
		config_combined = config_path + config_file;
		control->ParseConfigFile(config_combined.c_str());
		if(!control->configfiles.size()) {
			//Try to create the userlevel configfile.
			config_file.clear();
			Cross::CreatePlatformConfigDir(config_path);
			Cross::GetPlatformConfigName(config_file);
			config_combined = config_path + config_file;
			if(control->PrintConfig(config_combined.c_str())) {
				LOG_MSG("CONFIG: Generating default configuration.\nWriting it to %s",config_combined.c_str());
				//Load them as well. Makes relative paths much easier
				control->ParseConfigFile(config_combined.c_str());
			}
		}
	}

	//Second parse -conf switches
	while(control->cmdline->FindString("-conf",config_file,true)) {
		if (!control->ParseConfigFile(config_file.c_str())) {
			// try to load it from the user directory
			if (!control->ParseConfigFile((config_path + config_file).c_str())) {
				LOG_MSG("CONFIG: Can't open specified config file: %s",config_file.c_str());
			}
		}
	}
	// if none found => parse localdir conf
	if(!control->configfiles.size()) control->ParseConfigFile("dosbox.conf");

	// if none found => parse userlevel conf
	if(!control->configfiles.size()) {
		config_file.clear();
		Cross::GetPlatformConfigName(config_file);
		control->ParseConfigFile((config_path + config_file).c_str());
	}

	if(!control->configfiles.size()) {
		//Try to create the userlevel configfile.
		config_file.clear();
		Cross::CreatePlatformConfigDir(config_path);
		Cross::GetPlatformConfigName(config_file);
		config_combined = config_path + config_file;
		if(control->PrintConfig(config_combined.c_str())) {
			LOG_MSG("CONFIG: Generating default configuration.\nWriting it to %s",config_combined.c_str());
			//Load them as well. Makes relative paths much easier
			control->ParseConfigFile(config_combined.c_str());
		} else {
			LOG_MSG("CONFIG: Using default settings. Create a configfile to change them");
		}
	}


#if (ENVIRON_LINKED)
		control->ParseEnv(environ);
#endif
//		UI_Init();
//		if (control->cmdline->FindExist("-startui")) UI_Run(false);
		/* Init all the sections */
		control->Init();
		/* Some extra SDL Functions */
		Section_prop * sdl_sec=static_cast<Section_prop *>(control->GetSection("sdl"));

		if (control->cmdline->FindExist("-fullscreen") || sdl_sec->Get_bool("fullscreen")) {
			if(!sdl.desktop.fullscreen) { //only switch if not already in fullscreen
				GFX_SwitchFullScreen();
			}
		}

		/* Init the keyMapper */
		MAPPER_Init();
		if (control->cmdline->FindExist("-startmapper")) MAPPER_RunInternal();
		/* Start up main machine */
		control->StartUp();
		/* Shutdown everything */
	} catch (char * error) {
#if defined (WIN32)
		sticky_keys(true);
#endif
		GFX_ShowMsg("Exit to error: %s",error);
		fflush(NULL);
		if(sdl.wait_on_error) {
			//TODO Maybe look for some way to show message in linux?
#if (C_DEBUG)
			GFX_ShowMsg("Press enter to continue");
			fflush(NULL);
			fgetc(stdin);
#elif defined(WIN32)
			Sleep(5000);
#endif
		}

	}
	catch (int){
		; //nothing, pressed killswitch
	}
	catch(...){
		; // Unknown error, let's just exit.
	}
#if defined (WIN32)
	sticky_keys(true); //Might not be needed if the shutdown function switches to windowed mode, but it doesn't hurt
#endif
	//Force visible mouse to end user. Somehow this sometimes doesn't happen
	SDL_WM_GrabInput(SDL_GRAB_OFF);
	SDL_ShowCursor(SDL_ENABLE);

	SDL_Quit();//Let's hope sdl will quit as well when it catches an exception
	return 0;
}

void GFX_GetSize(int &width, int &height, bool &fullscreen) {
	width = sdl.draw.width;
	height = sdl.draw.height;
	fullscreen = sdl.desktop.fullscreen;
}

bool OpenGL_using(void) {
	return (sdl.desktop.want_type==SCREEN_OPENGL?true:false);
}
