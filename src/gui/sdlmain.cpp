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

#include <string.h>
#include <stdio.h>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include "dosbox.h"
#include "video.h"
#include "keyboard.h"
#include "mouse.h"
#include "joystick.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"


//#define DISABLE_JOYSTICK


SDL_Block sdl;

GFX_Info gfx_info;

static void RestorePalette(void) {
	if (sdl.bpp!=8) return;
/* Use some other way of doing this */
	if (sdl.full_screen) {
		if (!SDL_SetPalette(sdl.surface,SDL_PHYSPAL,sdl.pal,0,256)) {
			E_Exit("SDL:Can't set palette");
		}
	} else {
		if (!SDL_SetPalette(sdl.surface,SDL_LOGPAL,sdl.pal,0,256)) {
			E_Exit("SDL:Can't set palette");
		}
	}
	return;
}
/* Reset the screen with current values in the sdl structure */
static void ResetScreen(void) {
	if (sdl.full_screen) { 
	/* First get the original resolution */
		sdl.surface=SDL_SetVideoMode(sdl.width,sdl.height,sdl.bpp,SDL_HWSURFACE|SDL_HWPALETTE|SDL_FULLSCREEN|SDL_DOUBLEBUF);
	} else {
		sdl.surface=SDL_SetVideoMode(sdl.width,sdl.height,sdl.bpp,SDL_SWSURFACE|SDL_RESIZABLE);
	}
	if (sdl.surface==0) {
		E_Exit("SDL:Would be nice if I could get a surface.");
	}
	SDL_WM_SetCaption(VERSION,VERSION);
/* also fill up gfx_info structure */
	gfx_info.width=sdl.width;
	gfx_info.height=sdl.height;
	gfx_info.bpp=sdl.bpp;
	gfx_info.pitch=sdl.surface->pitch;
	RestorePalette();
}


void GFX_Resize(Bitu width,Bitu height,Bitu bpp,GFX_ResizeHandler * resize) {
	GFX_Stop();
	sdl.width=width;
	sdl.height=height;
	sdl.bpp=bpp;
	sdl.resize=resize;
	ResetScreen();
	GFX_Start();
}

static void CaptureMouse() {
	sdl.mouse_grabbed=!sdl.mouse_grabbed;
	if (sdl.mouse_grabbed) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		SDL_ShowCursor(SDL_ENABLE);
	}
}

static void SwitchFullScreen(void) {
	GFX_Stop();
	sdl.full_screen=!sdl.full_screen;
	if (sdl.full_screen) {
		if (sdl.resize) {
			sdl.width=0;sdl.height=0;
			(*sdl.resize)(&sdl.width,&sdl.height);
		}
	}
	ResetScreen();
	GFX_Start();
}
//only prototype existed
void GFX_SwitchFullScreen(void) {
    SwitchFullScreen();
}

static void GFX_Redraw() {
#if C_THREADED
	if (SDL_mutexP(sdl.mutex)) {
		E_Exit("Can't Lock Mutex");
	};
#endif	
	if (sdl.active) {
		SDL_LockSurface(sdl.surface );
		if (sdl.surface->pixels && sdl.draw) (*sdl.draw)((Bit8u *)sdl.surface->pixels);
		SDL_UnlockSurface(sdl.surface );
		 if (sdl.full_screen) SDL_Flip(sdl.surface);
		 else SDL_UpdateRect(sdl.surface,0,0,0,0);
	};
#if C_THREADED	
	if (SDL_mutexV(sdl.mutex)) {
		E_Exit("Can't Release Mutex");
	}
#endif
}

static int SDLGFX_Thread(void * data) {
	do {
		GFX_Redraw();
		SDL_Delay(1000/70);
	} while (true); 
	return 1;
}

void GFX_SetPalette(Bitu start,Bitu count,GFX_PalEntry * entries) {
/* I should probably not change the GFX_PalEntry :) */
#if C_THREADED
	if (SDL_mutexP(sdl.mutex)) {
		E_Exit("SDL:Can't Lock Mutex");
	};
#endif
	if (sdl.full_screen) {
		if (!SDL_SetPalette(sdl.surface,SDL_PHYSPAL,(SDL_Color *)entries,start,count)) {
			E_Exit("SDL:Can't set palette");
		}
	} else {
		if (!SDL_SetPalette(sdl.surface,SDL_LOGPAL,(SDL_Color *)entries,start,count)) {
			E_Exit("SDL:Can't set palette");
		}
	}
	/* Copy palette entries into some internal back up table */
#if C_THREADED
	if (SDL_mutexV(sdl.mutex)) {
		E_Exit("SDL:Can't Release Mutex");
	}
#endif
	memcpy(&sdl.pal[start],entries,count*sizeof(SDL_Color));
}

void GFX_SetDrawHandler(GFX_DrawHandler * handler) {
	sdl.draw=handler;
}

void GFX_Stop() {
#if C_THREADED
	SDL_mutexP(sdl.mutex);
#endif
	sdl.active=false;
#if C_THREADED
	SDL_mutexV(sdl.mutex);
#endif
}


void GFX_Start() {
	sdl.active=true;
}


void GUI_StartUp(Section * sec) {
 	Section_prop * section=static_cast<Section_prop *>(sec);
	sdl.active=false;
	sdl.full_screen=false;
	sdl.draw=0;
	GFX_Resize(640,400,8,0);
#if C_THREADED
	sdl.mutex=SDL_CreateMutex();
	sdl.thread = SDL_CreateThread(&SDLGFX_Thread,0);
#else 
	TIMER_RegisterMicroHandler(GFX_Redraw,1000000/70);
#endif
	SDL_EnableKeyRepeat(250,30);
	
/* Get some Keybinds */
	KEYBOARD_AddEvent(KBD_f9,CTRL_PRESSED,SwitchFullScreen);
	KEYBOARD_AddEvent(KBD_f10,CTRL_PRESSED,CaptureMouse);
	KEYBOARD_AddEvent(KBD_enter,ALT_PRESSED,SwitchFullScreen);
}

void GFX_ShutDown() {
	if (sdl.full_screen) SwitchFullScreen();
	if (sdl.mouse_grabbed) CaptureMouse();
	GFX_Stop();
}


static void HandleKey(SDL_KeyboardEvent * key) {
	Bit32u code;
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

//	KBD_esc,KBD_tab,KBD_backspace,KBD_enter,KBD_space,

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

//	case SDLK_:code=key_;break;
	/* Special Keys */
	default:
//TODO maybe give warning for keypress unknown		
		return;
	}
	KEYBOARD_AddKey(code,(key->state==SDL_PRESSED));
}

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) {
	if (!sdl.mouse_grabbed) {
		Mouse_CursorSet((float)motion->x/(float)sdl.width,(float)motion->y/(float)sdl.height);
	} else {
		Mouse_CursorMoved((float)motion->xrel/(float)sdl.width,(float)motion->yrel/(float)sdl.height);
	}
}

static void HandleMouseButton(SDL_MouseButtonEvent * button) {
	switch (button->state) {
	case SDL_PRESSED:
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


static void HandleVideoResize(SDL_ResizeEvent * resize) {
	Bitu width,height;
	width=resize->w;
	height=resize->h;
	if (sdl.resize) {
		GFX_Stop();
		(*sdl.resize)(&width,&height);
		sdl.width=width;
		sdl.height=height;
		ResetScreen();
		GFX_Start();
	}
}

void GFX_Events() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
	    switch (event.type) {
		case SDL_KEYDOWN:
		case SDL_KEYUP:
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
			HandleVideoResize(&event.resize);
			break;
		case SDL_QUIT:
			E_Exit("Closed the SDL Window");
			break;
		}
    }
}

void GFX_ShowMsg(char * msg) {
	char buf[1024];
	strcpy(buf,msg);
	strcat(buf,"\n");
	printf(buf);
};

int main(int argc, char* argv[]) {
	try {
		CommandLine com_line(argc,argv);
		Config myconf(&com_line);
		control=&myconf;

		if ( SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER
#ifndef DISABLE_JOYSTICK
		|SDL_INIT_JOYSTICK
	
#endif
		) < 0 ) E_Exit("Can't init SDL %s",SDL_GetError());
		Section_prop * sdl_sec=control->AddSection_prop("SDL",&GUI_StartUp);
		sdl_sec->Add_bool("FULLSCREEN",false);
		/* Init all the dosbox subsystems */
		DOSBOX_Init();
		std::string config_file;
		if (control->cmdline->FindString("-conf",config_file,true)) {
			
		} else {
			config_file="dosbox.conf";
		}
		/* Parse the config file */
		control->ParseConfigFile(config_file.c_str());
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
		if (control->cmdline->FindExist("-fullscreen")) {
			sdl.full_screen=true;
		} else {
			sdl.full_screen=sdl_sec->Get_bool("FULLSCREEN");
		}
		if (sdl.full_screen) {
			sdl.full_screen=false;
			SwitchFullScreen();
		}


		/* Start up main machine */
		control->StartUp();
		/* Shutdown everything */
	} catch (char * error) {
		LOG_ERROR("Exit to error: %s",error);
	}
	GFX_Stop();
	return 0;
};
