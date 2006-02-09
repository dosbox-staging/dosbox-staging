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

/* $Id: sdl_mapper.cpp,v 1.19 2006-02-09 11:47:48 qbix79 Exp $ */

#define OLD_JOYSTICK 1

#include <vector>
#include <list>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>


#include "SDL.h"
#include "SDL_thread.h"

#include "dosbox.h"
#include "video.h"
#include "keyboard.h"
#include "joystick.h"
#include "support.h"
#include "mapper.h"
#include "setup.h"

enum {
	CLR_BLACK=0,
	CLR_WHITE=1,
	CLR_RED=2,
};

enum BB_Types {
	BB_Next,BB_Prev,BB_Add,BB_Del,
	BB_Save,BB_Reset,BB_Exit
};

enum BC_Types {
	BC_Mod1,BC_Mod2,BC_Mod3,
	BC_Hold,
};

#define BMOD_Mod1 0x0001
#define BMOD_Mod2 0x0002
#define BMOD_Mod3 0x0004

#define BFLG_Hold 0x0001
#define BFLG_Repeat 0x0004


#define MAXSTICKS 8
#define MAXACTIVE 16

class CEvent;
class CHandlerEvent;
class CButton;
class CBind;
class CBindGroup;

static void SetActiveEvent(CEvent * event);
static void SetActiveBind(CBind * _bind);
extern Bit8u int10_font_14[256 * 14];

static std::vector<CEvent *> events;
static std::vector<CButton *> buttons;
static std::vector<CBindGroup *> bindgroups;
static std::vector<CHandlerEvent *> handlergroup;
typedef std::list<CBind *> CBindList;
typedef std::list<CEvent *>::iterator CEventList_it;
typedef std::list<CBind *>::iterator CBindList_it;
typedef std::vector<CButton *>::iterator CButton_it;
typedef std::vector<CEvent *>::iterator CEventVector_it;
typedef std::vector<CHandlerEvent *>::iterator CHandlerEventVector_it;
typedef std::vector<CBindGroup *>::iterator CBindGroup_it;

static CBindList holdlist;


class CEvent {
public:
	CEvent(char * _entry) {
		safe_strncpy(entry,_entry,16);
		events.push_back(this);
		bindlist.clear();
		activity=0;
	}
	void AddBind(CBind * bind);
	virtual ~CEvent() {}
	virtual void Active(bool yesno)=0;
	INLINE void Activate(void) {
		if (!activity) Active(true);
		activity++;
	}
	INLINE void DeActivate(void) {
		activity--;
		if (!activity) Active(false);
	}
	void DeActivateAll(void);
	void SetValue(Bits value){
	};
	char * GetName(void) { return entry;}
	CBindList bindlist;
protected:
	Bitu activity;
	char entry[16];
};

class CBind {
public:
	virtual ~CBind () {
		list->remove(this);
//		event->bindlist.remove(this);
	}
	CBind(CBindList * _list) {
		list=_list;
		_list->push_back(this);
		mods=flags=0;
		event=0;
		active=holding=false;
	}
	void AddFlags(char * buf) {
		if (mods & BMOD_Mod1) strcat(buf," mod1");
		if (mods & BMOD_Mod2) strcat(buf," mod2");
		if (mods & BMOD_Mod3) strcat(buf," mod3");
		if (flags & BFLG_Hold) strcat(buf," hold");
	}
	void SetFlags(char * buf) {
		char * word;
		while (*(word=StripWord(buf))) {
			if (!strcasecmp(word,"mod1")) mods|=BMOD_Mod1;
			if (!strcasecmp(word,"mod2")) mods|=BMOD_Mod2;
			if (!strcasecmp(word,"mod3")) mods|=BMOD_Mod3;
			if (!strcasecmp(word,"hold")) flags|=BFLG_Hold;
		}
	}
	void Activate(Bits value) {
		event->SetValue(value);
		if (active) return;
		event->Activate();
		active=true;
	}
	void DeActivate(void) {
		if (!active) return;
		active=false;
		if (flags & BFLG_Hold) {
			if (!holding) {
				holdlist.push_back(this);
				holding=true;
				return;
			} else {
				holdlist.remove(this);
				holding=false;
			}
		}
		event->DeActivate();
	}
	virtual void ConfigName(char * buf)=0;
	virtual void BindName(char * buf)=0;
	Bitu mods,flags;
	Bit16s value;
	CEvent * event;
	CBindList * list;
	bool active,holding;
};


void CEvent::AddBind(CBind * bind) {
	bindlist.push_front(bind);
	bind->event=this;
}
void CEvent::DeActivateAll(void) {
	for (CBindList_it bit=bindlist.begin();bit!=bindlist.end();bit++) {
		(*bit)->DeActivate();
	}
}



class CBindGroup {
public:
	CBindGroup() {
		bindgroups.push_back(this);
	}
	void ActivateBindList(CBindList * list,Bits value);
	void DeactivateBindList(CBindList * list);
	virtual CBind * CreateConfigBind(char *&buf)=0;
	virtual CBind * CreateEventBind(SDL_Event * event)=0;

	virtual bool CheckEvent(SDL_Event * event)=0;
	virtual char * ConfigStart(void)=0;
	virtual char * BindStart(void)=0;
protected:

};


#define MAX_SDLKEYS 323
#define MAX_SCANCODES 212

static bool usescancodes;
Bit8u scancode_map[MAX_SDLKEYS];

#define Z SDLK_UNKNOWN

SDLKey sdlkey_map[MAX_SCANCODES]={SDLK_UNKNOWN,SDLK_ESCAPE,
	SDLK_1,SDLK_2,SDLK_3,SDLK_4,SDLK_5,SDLK_6,SDLK_7,SDLK_8,SDLK_9,SDLK_0,
	/* 0x0c: */
	SDLK_MINUS,SDLK_EQUALS,SDLK_BACKSPACE,SDLK_TAB,
	SDLK_q,SDLK_w,SDLK_e,SDLK_r,SDLK_t,SDLK_y,SDLK_u,SDLK_i,SDLK_o,SDLK_p,
	SDLK_LEFTBRACKET,SDLK_RIGHTBRACKET,SDLK_RETURN,SDLK_LCTRL,
	SDLK_a,SDLK_s,SDLK_d,SDLK_f,SDLK_g,SDLK_h,SDLK_j,SDLK_k,SDLK_l,
	SDLK_SEMICOLON,SDLK_QUOTE,SDLK_BACKQUOTE,SDLK_LSHIFT,SDLK_BACKSLASH,
	SDLK_z,SDLK_x,SDLK_c,SDLK_v,SDLK_b,SDLK_n,SDLK_m,
	/* 0x33: */
	SDLK_COMMA,SDLK_PERIOD,SDLK_SLASH,SDLK_RSHIFT,SDLK_KP_MULTIPLY,
	SDLK_LALT,SDLK_SPACE,SDLK_CAPSLOCK,
	SDLK_F1,SDLK_F2,SDLK_F3,SDLK_F4,SDLK_F5,SDLK_F6,SDLK_F7,SDLK_F8,SDLK_F9,SDLK_F10,
	/* 0x45: */
	SDLK_NUMLOCK,SDLK_SCROLLOCK,
	SDLK_KP7,SDLK_KP8,SDLK_KP9,SDLK_KP_MINUS,SDLK_KP4,SDLK_KP5,SDLK_KP6,SDLK_KP_PLUS,
	SDLK_KP1,SDLK_KP2,SDLK_KP3,SDLK_KP0,SDLK_KP_PERIOD,
	SDLK_UNKNOWN,SDLK_UNKNOWN,
	SDLK_LESS,SDLK_F11,SDLK_F12,
	Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,
	Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,
	Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,
	/* 0xb7: */
	Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z
	/* 0xd4: ... */
};

#undef Z


SDLKey MapSDLCode(Bitu skey) {
	if (usescancodes) {
		if (skey<MAX_SCANCODES) return sdlkey_map[skey];
		else return SDLK_UNKNOWN;
	} else return (SDLKey)skey;
}

Bitu GetKeyCode(SDL_keysym keysym) {
	if (usescancodes) {
		Bitu key=(Bitu)keysym.scancode;
		if (key==0) {
			/* try to retrieve key from symbolic key as scancode is zero */
			if (keysym.sym<MAX_SDLKEYS) key=scancode_map[(Bitu)keysym.sym];
		} 
#if !defined (WIN32)
		/* Linux adds 8 to all scancodes */
		else key-=8;
#endif
		return key;
	} else {
#if defined (WIN32)
		/* special handling of 102-key under windows */
		if ((keysym.sym==SDLK_BACKSLASH) && (keysym.scancode==0x56)) return (Bitu)SDLK_LESS;
#endif
		return (Bitu)keysym.sym;
	}
}


class CKeyBind;
class CKeyBindGroup;

class CKeyBind : public CBind {
public:
	CKeyBind(CBindList * _list,SDLKey _key) : CBind(_list) {
		key = _key;
	}
	void BindName(char * buf) {
		sprintf(buf,"Key %s",SDL_GetKeyName(MapSDLCode((Bitu)key)));
	}
	void ConfigName(char * buf) {
		sprintf(buf,"key %d",MapSDLCode((Bitu)key));
	}
public:
	SDLKey key;
};

class CKeyBindGroup : public  CBindGroup {
public:
	CKeyBindGroup(Bitu _keys) : CBindGroup (){
		lists=new CBindList[_keys];
		for (Bitu i=0;i<_keys;i++) lists[i].clear();
		keys=_keys;
		configname="key";
	}
	~CKeyBindGroup() { delete[] lists; }
	CBind * CreateConfigBind(char *& buf) {
		if (strncasecmp(buf,configname,strlen(configname))) return 0;
		StripWord(buf);char * num=StripWord(buf);
		Bitu code=ConvDecWord(num);
		if (usescancodes) {
			if (code<MAX_SDLKEYS) code=scancode_map[code];
			else code=0;
		}
		CBind * bind=CreateKeyBind((SDLKey)code);
		return bind;
	}
	CBind * CreateEventBind(SDL_Event * event) {
		if (event->type!=SDL_KEYDOWN) return 0;
		return CreateKeyBind((SDLKey)GetKeyCode(event->key.keysym));
	};
	bool CheckEvent(SDL_Event * event) {
		if (event->type!=SDL_KEYDOWN && event->type!=SDL_KEYUP) return false;
		Bitu key=GetKeyCode(event->key.keysym);
//		LOG_MSG("key type %i is %x [%x %x]",event->type,key,event->key.keysym.sym,event->key.keysym.scancode);
		assert(Bitu(event->key.keysym.sym)<keys);
		if (event->type==SDL_KEYDOWN) ActivateBindList(&lists[key],0x7fff);
		else DeactivateBindList(&lists[key]);
		return 0;
	}
	CBind * CreateKeyBind(SDLKey _key) {
		if (!usescancodes) assert((Bitu)_key<keys);
		return new CKeyBind(&lists[(Bitu)_key],_key);
	}
private:
	char * ConfigStart(void) {
		return configname;
	}
	char * BindStart(void) {
		return "Key";
	}
protected:
	char * configname;
	CBindList * lists;
	Bitu keys;
};


class CJAxisBind;
class CJButtonBind;
class CJHatBind;

class CJAxisBind : public CBind {
public:
	CJAxisBind(CBindList * _list,CBindGroup * _group,Bitu _axis,bool _positive) : CBind(_list){
		group = _group;
		axis = _axis;
		positive = _positive;
	}
	void ConfigName(char * buf) {
		sprintf(buf,"%s axis %d %d",group->ConfigStart(),axis,positive ? 1 : 0);
	}
	void BindName(char * buf) {
		sprintf(buf,"%s Axis %d%s",group->BindStart(),axis,positive ? "+" : "-");
	}
protected:
	CBindGroup * group;
	Bitu axis;
	bool positive;
};

class CJButtonBind : public CBind {
public:
	CJButtonBind(CBindList * _list,CBindGroup * _group,Bitu _button) : CBind(_list) {
		group = _group;
		button=_button;
	}
	void ConfigName(char * buf) {
		sprintf(buf,"%s button %d",group->ConfigStart(),button);
	}
	void BindName(char * buf) {
		sprintf(buf,"%s Button %d",group->BindStart(),button);
	}
protected:
	CBindGroup * group;
	Bitu button;
};

class CJHatBind : public CBind {
public:
	CJHatBind(CBindList * _list,CBindGroup * _group,Bitu _hat) : CBind(_list) {
		group = _group;
		hat=_hat;
	}
	void ConfigName(char * buf) {
		sprintf(buf,"%s hat %d",group->ConfigStart(),hat);
	}
	void BindName(char * buf) {
		sprintf(buf,"%s hat %d",group->BindStart(),hat);
	}
protected:
	CBindGroup * group;
	Bitu hat;
	Bitu mask;
};

class CStickBindGroup : public  CBindGroup {
public:
	CStickBindGroup(Bitu _stick) : CBindGroup (){
		stick=_stick;
		sprintf(configname,"stick_%d",stick);
		sdl_joystick=SDL_JoystickOpen(stick);
		assert(sdl_joystick);
		axes=SDL_JoystickNumAxes(sdl_joystick);
		buttons=SDL_JoystickNumButtons(sdl_joystick);
		hats=SDL_JoystickNumHats(sdl_joystick);
		pos_axis_lists=new CBindList[axes];
		neg_axis_lists=new CBindList[axes];
		button_lists=new CBindList[buttons];
		hat_lists=new CBindList[hats];
#if OLD_JOYSTICK
		LOG_MSG("Using joystick %s with %d axes and %d buttons",SDL_JoystickName(stick),axes,buttons);
		//if the first stick is set, we must be the second
		emustick=JOYSTICK_IsEnabled(0);
		JOYSTICK_Enable(emustick,true);
#endif	   
	}
	~CStickBindGroup() {
		SDL_JoystickClose(sdl_joystick);
		delete[] pos_axis_lists;
		delete[] neg_axis_lists;
		delete[] button_lists;
		delete[] hat_lists;
	}
	CBind * CreateConfigBind(char *& buf) {
		if (strncasecmp(configname,buf,strlen(configname))) return 0;
		StripWord(buf);char * type=StripWord(buf);
		CBind * bind=0;
		if (!strcasecmp(type,"axis")) {
			Bitu ax=ConvDecWord(StripWord(buf));
			bool pos=ConvDecWord(StripWord(buf)) > 0;
			bind=CreateAxisBind(ax,pos);
		} else if (!strcasecmp(type,"button")) {
			Bitu but=ConvDecWord(StripWord(buf));			
			bind=CreateButtonBind(but);
		}
		return bind;
	}
	CBind * CreateEventBind(SDL_Event * event) {
		if (event->type==SDL_JOYAXISMOTION) {
			if (event->jaxis .which!=stick) return 0;
			if (abs(event->jaxis.value)<25000) return 0;
			return CreateAxisBind(event->jaxis.axis,event->jaxis.value>0);
		} else if (event->type==SDL_JOYBUTTONDOWN) {
			if (event->jaxis .which!=stick) return 0;
			return CreateButtonBind(event->jbutton.button);
		} else return 0;
	}
	virtual bool CheckEvent(SDL_Event * event) {
#if OLD_JOYSTICK
		SDL_JoyAxisEvent * jaxis = NULL;
		SDL_JoyButtonEvent * jbutton = NULL;

	switch(event->type) {
		case SDL_JOYAXISMOTION:
			jaxis = &event->jaxis;
			if(jaxis->which == stick)
			        if(jaxis->axis == 0)
					JOYSTICK_Move_X(emustick,(float)(jaxis->value/32768.0));
				else if(jaxis->axis == 1)
					JOYSTICK_Move_Y(emustick,(float)(jaxis->value/32768.0));
			break;
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			jbutton = &event->jbutton;
			bool state;
			state=jbutton->type==SDL_JOYBUTTONDOWN;
			if ((jbutton->which == stick) && (jbutton->button<2)) {
				JOYSTICK_Button(emustick,jbutton->button,state);
			}
			break;
	}
#endif
		return false;
	}
private:
	CBind * CreateAxisBind(Bitu axis,bool positive) {
		assert(axis<axes);
		if (positive) return new CJAxisBind(&pos_axis_lists[axis],this,axis,positive);
		else return new CJAxisBind(&neg_axis_lists[axis],this,axis,positive);
	}
	CBind * CreateButtonBind(Bitu button) {
		assert(button<buttons);
		return new CJButtonBind(&button_lists[button],this,button);
	}
	char * ConfigStart(void) {
		return configname;
	}
	char * BindStart(void) {
		return (char *)SDL_JoystickName(stick);
	}
protected:
	CBindList * pos_axis_lists;
	CBindList * neg_axis_lists;
	CBindList * button_lists;
	CBindList * hat_lists;
	Bitu stick,emustick,axes,buttons,hats;
	SDL_Joystick * sdl_joystick;
	char configname[10];
};

class C4AxisBindGroup : public  CStickBindGroup {
public:
	C4AxisBindGroup(Bitu _stick) : CStickBindGroup (_stick){
#if OLD_JOYSTICK
		JOYSTICK_Enable(1,true);
#endif	   
	}
        bool CheckEvent(SDL_Event * event) {
#if OLD_JOYSTICK
	        SDL_JoyAxisEvent * jaxis = NULL;
		SDL_JoyButtonEvent * jbutton = NULL;

	switch(event->type) {
		case SDL_JOYAXISMOTION:
			jaxis = &event->jaxis;
			if(jaxis->which == stick && jaxis->axis < 4)
			        if(jaxis->axis & 1)
					JOYSTICK_Move_Y(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
				else
					JOYSTICK_Move_X(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
			break;
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			jbutton = &event->jbutton;
			bool state;
			state=jbutton->type==SDL_JOYBUTTONDOWN;
			if ((jbutton->which == stick) && (jbutton->button<4)) {
			        JOYSTICK_Button((jbutton->button >> 1),
						(jbutton->button & 1),state);
			}
			break;
	}
#endif
		return false;
	}
};

class CFCSBindGroup : public  CStickBindGroup {
public:
	CFCSBindGroup(Bitu _stick) : CStickBindGroup (_stick){
#if OLD_JOYSTICK
		JOYSTICK_Enable(1,true);
		JOYSTICK_Move_Y(1,1.0);
#endif	   
	}
        bool CheckEvent(SDL_Event * event) {
#if OLD_JOYSTICK
	        SDL_JoyAxisEvent * jaxis = NULL;
		SDL_JoyButtonEvent * jbutton = NULL;
		SDL_JoyHatEvent * jhat = NULL;

	switch(event->type) {
		case SDL_JOYAXISMOTION:
			jaxis = &event->jaxis;
			if(jaxis->which == stick)
			        if(jaxis->axis == 0)
					JOYSTICK_Move_X(0,(float)(jaxis->value/32768.0));
				else if(jaxis->axis == 1)
					JOYSTICK_Move_Y(0,(float)(jaxis->value/32768.0));
				else if(jaxis->axis == 2)
					JOYSTICK_Move_X(1,(float)(jaxis->value/32768.0));
			break;
		case SDL_JOYHATMOTION:
			jhat = &event->jhat;
			if(jhat->which == stick) {
				switch(jhat->value) {
					case SDL_HAT_CENTERED:
						JOYSTICK_Move_Y(1,1.0);
						break;
					case SDL_HAT_UP:
						JOYSTICK_Move_Y(1,-1.0);
						break;
					case SDL_HAT_RIGHT:
						JOYSTICK_Move_Y(1,-0.5);
						break;
					case SDL_HAT_DOWN:
						JOYSTICK_Move_Y(1,0.0);
						break;
					case SDL_HAT_LEFT:
						JOYSTICK_Move_Y(1,0.5);
						break;
					case SDL_HAT_LEFTUP:
						if(JOYSTICK_GetMove_Y(1) < 0)
							JOYSTICK_Move_Y(1,0.5);
						else
							JOYSTICK_Move_Y(1,-1.0);
						break;
					case SDL_HAT_RIGHTUP:
						if(JOYSTICK_GetMove_Y(1) < -0.7)
							JOYSTICK_Move_Y(1,-0.5);
						else
							JOYSTICK_Move_Y(1,-1.0);
						break;
					case SDL_HAT_RIGHTDOWN:
						if(JOYSTICK_GetMove_Y(1) < -0.2)
							JOYSTICK_Move_Y(1,0.0);
						else
							JOYSTICK_Move_Y(1,-0.5);
						break;
					case SDL_HAT_LEFTDOWN:
						if(JOYSTICK_GetMove_Y(1) > 0.2)
							JOYSTICK_Move_Y(1,0.0);
						else
							JOYSTICK_Move_Y(1,0.5);
						break;
				}
			}
			
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			jbutton = &event->jbutton;
			bool state;
			state=jbutton->type==SDL_JOYBUTTONDOWN;
			if ((jbutton->which == stick) && (jbutton->button<4)) {
			        JOYSTICK_Button((jbutton->button >> 1),
						(jbutton->button & 1),state);
			}
			break;
	}
#endif
		return false;
	}
};

class CCHBindGroup : public  CStickBindGroup {
public:
	CCHBindGroup(Bitu _stick) : CStickBindGroup (_stick){
#if OLD_JOYSTICK
		JOYSTICK_Enable(1,true);
		button_state=0;
#endif	   
	}
        bool CheckEvent(SDL_Event * event) {
#if OLD_JOYSTICK
	        SDL_JoyAxisEvent * jaxis = NULL;
		SDL_JoyButtonEvent * jbutton = NULL;
		SDL_JoyHatEvent * jhat = NULL;
		static unsigned const button_magic[6]={0x02,0x04,0x10,0x100,0x20,0x200};
		static unsigned const hat_magic[2][5]={{0x8888,0x8000,0x800,0x80,0x08},
						       {0x5440,0x4000,0x400,0x40,0x1000}};
	switch(event->type) {
		case SDL_JOYAXISMOTION:
			jaxis = &event->jaxis;
			if(jaxis->which == stick && jaxis->axis < 4)
			        if(jaxis->axis & 1)
					JOYSTICK_Move_Y(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
				else
					JOYSTICK_Move_X(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
			break;
		case SDL_JOYHATMOTION:
			jhat = &event->jhat;
			if(jhat->which == stick && jhat->hat < 2) {
				if(jhat->value == SDL_HAT_CENTERED)
					button_state&=~hat_magic[jhat->hat][0];
				if(jhat->value & SDL_HAT_UP)
					button_state|=hat_magic[jhat->hat][1];
				if(jhat->value & SDL_HAT_RIGHT)
					button_state|=hat_magic[jhat->hat][2];
				if(jhat->value & SDL_HAT_DOWN)
					button_state|=hat_magic[jhat->hat][3];
				if(jhat->value & SDL_HAT_LEFT)
					button_state|=hat_magic[jhat->hat][4];
			}
			break;
		case SDL_JOYBUTTONDOWN:
			jbutton = &event->jbutton;
			if ((jbutton->which == stick) && (jbutton->button<6))
					button_state|=button_magic[jbutton->button];
			break;
		case SDL_JOYBUTTONUP:
			jbutton = &event->jbutton;
			if ((jbutton->which == stick) && (jbutton->button<6))
				button_state&=~button_magic[jbutton->button];
			break;
	}
		unsigned i;
		Bit16u j;
		j=button_state;
		for(i=0;i<16;i++) if (j & 1) break; else j>>=1;
		JOYSTICK_Button(0,0,i&0x01);
		JOYSTICK_Button(0,1,i>>1&0x01);
		JOYSTICK_Button(1,0,i>>2&0x01);
		JOYSTICK_Button(1,1,i>>3&0x01);
#endif
		
		return false;
	}
protected:
	Bit16u button_state;
};

static struct {
	SDL_Surface * surface;
	SDL_Surface * draw_surface;
	bool exit;
	CEvent * aevent;				//Active Event
	CBind * abind;					//Active Bind
	CBindList_it abindit;			//Location of active bind in list
	bool redraw;
	bool addbind;
	Bitu mods;
	struct {
		CKeyBindGroup * keys;
		CStickBindGroup * stick[MAXSTICKS];
	} grp;
	struct {
		Bitu num;
		SDL_Joystick * opened[MAXSTICKS];
	} sticks;
	const char * filename;
} mapper;

void CBindGroup::ActivateBindList(CBindList * list,Bits value) {
	Bitu validmod=0;
	CBindList_it it;
	for (it=list->begin();it!=list->end();it++) {
		if (((*it)->mods & mapper.mods) == (*it)->mods) {
			if (validmod<(*it)->mods) validmod=(*it)->mods;
		}
	}
	for (it=list->begin();it!=list->end();it++) {
	/*BUG:CRASH if keymapper key is removed*/
		if (validmod==(*it)->mods) (*it)->Activate(value);
	}
}

void CBindGroup::DeactivateBindList(CBindList * list) {
	Bitu validmod=0;
	CBindList_it it;
	for (it=list->begin();it!=list->end();it++) {
		(*it)->DeActivate();
	}
}

static void DrawText(Bitu x,Bitu y,const char * text,Bit8u color) {
	Bit8u * draw=((Bit8u *)mapper.surface->pixels)+(y*mapper.surface->pitch)+x;
	while (*text) {
		Bit8u * font=&int10_font_14[(*text)*14];
		Bitu i,j;Bit8u * draw_line=draw;
		for (i=0;i<14;i++) {
			Bit8u map=*font++;
			for (j=0;j<8;j++) {
				if (map & 0x80) *(draw_line+j)=color;
				else *(draw_line+j)=CLR_BLACK;
				map<<=1;
			}
			draw_line+=mapper.surface->pitch;
		}
		text++;draw+=8;
	}
}

class CButton {
public:
	virtual ~CButton(){};
	CButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy) {
		x=_x;y=_y;dx=_dx;dy=_dy;
		buttons.push_back(this);
		color=CLR_WHITE;
		enabled=true;
	}
	virtual void Draw(void) {
		if (!enabled) return;
		Bit8u * point=((Bit8u *)mapper.surface->pixels)+(y*mapper.surface->pitch)+x;
		for (Bitu lines=0;lines<dy;lines++)  {
			if (lines==0 || lines==(dy-1)) {
				for (Bitu cols=0;cols<dx;cols++) *(point+cols)=color;
			} else {
				*point=color;*(point+dx-1)=color;
			}
			point+=mapper.surface->pitch;
		}
	}
	virtual bool OnTop(Bitu _x,Bitu _y) {
		return ( enabled && (_x>=x) && (_x<x+dx) && (_y>=y) && (_y<y+dy));
	}
	virtual void Click(void) {}
	void Enable(bool yes) { 
		enabled=yes; 
		mapper.redraw=true;
	}
	void SetColor(Bit8u _col) { color=_col; }
protected:
	Bitu x,y,dx,dy;
	Bit8u color;
	bool enabled;
};

class CTextButton : public CButton {
public:
	CTextButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text) : CButton(_x,_y,_dx,_dy) { text=_text;}
	void Draw(void) {
		if (!enabled) return;
		CButton::Draw();
		DrawText(x+2,y+2,text,color);
	}
protected:
	const char * text;
};

class CEventButton : public CTextButton {
public:
	CEventButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text,CEvent * _event) 
	: CTextButton(_x,_y,_dx,_dy,_text) 	{ 
		event=_event;	
	}
	void Click(void) {
		SetActiveEvent(event);
	}
protected:
	CEvent * event;
};

class CCaptionButton : public CButton {
public:
	CCaptionButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy) : CButton(_x,_y,_dx,_dy){
		caption[0]=0;
	}
	void Change(char * format,...) {
		va_list msg;
		va_start(msg,format);
		vsprintf(caption,format,msg);
		va_end(msg);
		mapper.redraw=true;
	}		
	void Draw(void) {
		if (!enabled) return;
		DrawText(x+2,y+2,caption,color);
	}
protected:
	char caption[128];
};

static void MAPPER_SaveBinds(void);
class CBindButton : public CTextButton {
public:	
	CBindButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text,BB_Types _type) 
	: CTextButton(_x,_y,_dx,_dy,_text) 	{ 
		type=_type;
	}
	void Click(void) {
		switch (type) {
		case BB_Add: 
			mapper.addbind=true;
			SetActiveBind(0);
			break;
		case BB_Del:
			if (mapper.abindit!=mapper.aevent->bindlist.end())  {
				delete (*mapper.abindit);
				mapper.abindit=mapper.aevent->bindlist.erase(mapper.abindit);
				if (mapper.abindit==mapper.aevent->bindlist.end()) 
					mapper.abindit=mapper.aevent->bindlist.begin();
			}
			if (mapper.abindit!=mapper.aevent->bindlist.end()) SetActiveBind(*(mapper.abindit));
			else SetActiveBind(0);
			break;
		case BB_Next:
			if (mapper.abindit!=mapper.aevent->bindlist.end()) 
				mapper.abindit++;
			if (mapper.abindit==mapper.aevent->bindlist.end()) 
				mapper.abindit=mapper.aevent->bindlist.begin();
			SetActiveBind(*(mapper.abindit));
			break;
		case BB_Save:
			MAPPER_SaveBinds();
			break;
		case BB_Exit:   
			mapper.exit=true;
			break;
		}
	}
protected:
	BB_Types type;
};

class CCheckButton : public CTextButton {
public:	
	CCheckButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy,const char * _text,BC_Types _type) 
	: CTextButton(_x,_y,_dx,_dy,_text) 	{ 
		type=_type;
	}
	void Draw(void) {
		if (!enabled) return;
		bool checked;
		switch (type) {
		case BC_Mod1:
			checked=(mapper.abind->mods&BMOD_Mod1)>0;
			break;
		case BC_Mod2:
			checked=(mapper.abind->mods&BMOD_Mod2)>0;
			break;
		case BC_Mod3:
			checked=(mapper.abind->mods&BMOD_Mod3)>0;
			break;
		case BC_Hold:
			checked=(mapper.abind->flags&BFLG_Hold)>0;
			break;
		}
		if (checked) {
			Bit8u * point=((Bit8u *)mapper.surface->pixels)+((y+2)*mapper.surface->pitch)+x+dx-dy+2;
			for (Bitu lines=0;lines<(dy-4);lines++)  {
				memset(point,color,dy-4);
				point+=mapper.surface->pitch;
			}
		}
		CTextButton::Draw();
	}
	void Click(void) {
		switch (type) {
		case BC_Mod1:
			mapper.abind->mods^=BMOD_Mod1;
			break;
		case BC_Mod2:
			mapper.abind->mods^=BMOD_Mod2;
			break;
		case BC_Mod3:
			mapper.abind->mods^=BMOD_Mod3;
			break;
		case BC_Hold:
			mapper.abind->flags^=BFLG_Hold;
			break;
		}
		mapper.redraw=true;
	}
protected:
	BC_Types type;
};

class CKeyEvent : public CEvent {
public:
	CKeyEvent(char * _entry,KBD_KEYS _key) : CEvent(_entry) {
		key=_key;
	}
	void Active(bool yesno) {
		KEYBOARD_AddKey(key,yesno);
	};
	KBD_KEYS key;
};

class CJAxisEvent : public CEvent {
public:
	CJAxisEvent(char * _entry,Bitu _stick,bool _yaxis,bool _positive) : CEvent(_entry) {
		stick=_stick;
		yaxis=_yaxis;
		positive=_positive;
	}
	void Active(bool yesno) {
	};
	Bitu stick;
	bool yaxis,positive;
};

class CJButtonEvent : public CEvent {
public:
	CJButtonEvent(char * _entry,Bitu _stick,Bitu _button) : CEvent(_entry) {
		stick=_stick;
		button=_button;
	}
	void Active(bool yesno) {
	};
	Bitu stick,button;
};


class CModEvent : public CEvent {
public:
	CModEvent(char * _entry,Bitu _wmod) : CEvent(_entry) {
		wmod=_wmod;
	}
	void Active(bool yesno) {
		if (yesno) mapper.mods|=(1 << (wmod-1));
		else mapper.mods&=~(1 << (wmod-1));
	};
protected:
	Bitu wmod;
};

class CHandlerEvent : public CEvent {
public:
	CHandlerEvent(char * _entry,MAPPER_Handler * _handler,MapKeys _key,Bitu _mod,char * _buttonname) : CEvent(_entry) {
		handler=_handler;
		defmod=_mod;
		defkey=_key;
		buttonname=_buttonname;
		handlergroup.push_back(this);
	}
	void Active(bool yesno) {
		if (yesno) (*handler)();
	};
	char * ButtonName(void) {
		return buttonname;
	}
	void MakeDefaultBind(char * buf) {
		Bitu key=0;
		switch (defkey) {
		case MK_f1:case MK_f2:case MK_f3:case MK_f4:
		case MK_f5:case MK_f6:case MK_f7:case MK_f8:
		case MK_f9:case MK_f10:case MK_f11:case MK_f12:	
			key=SDLK_F1+(defkey-MK_f1);
			break;
		case MK_return:
			key=SDLK_RETURN;
			break;
		case MK_kpminus:
			key=SDLK_KP_MINUS;
			break;
		case MK_scrolllock:
			key=SDLK_SCROLLOCK;
			break;
		case MK_pause:
			key=SDLK_PAUSE;
			break;
		case MK_printscreen:
			key=SDLK_PRINT;
			break;
		}
		sprintf(buf,"%s \"key %d%s%s%s\"",
			entry,
			key,
			defmod & 1 ? " mod1" : "",
			defmod & 2 ? " mod2" : "",
			defmod & 4 ? " mod3" : ""
		);
	}
protected:
	MapKeys defkey;
	Bitu defmod;
	MAPPER_Handler * handler;
public:
	char * buttonname;
};


static struct {
	CCaptionButton *  event_title;
	CCaptionButton *  bind_title;
	CCaptionButton *  selected;
	CCaptionButton *  action;
	CBindButton * save;
	CBindButton * exit;   
	CBindButton * add;
	CBindButton * del;
	CBindButton * next;
	CBindButton * prev;
	CCheckButton * mod1,* mod2,* mod3,* hold;
} bind_but;

static void SetActiveBind(CBind * _bind) {
	mapper.abind=_bind;
	if (_bind) {
		bind_but.bind_title->Enable(true);
		char buf[256];_bind->BindName(buf);
		bind_but.bind_title->Change("BIND:%s",buf);
		bind_but.del->Enable(true);
		bind_but.next->Enable(true);
		bind_but.mod1->Enable(true);
		bind_but.mod2->Enable(true);
		bind_but.mod3->Enable(true);
		bind_but.hold->Enable(true);
	} else {
		bind_but.bind_title->Enable(false);
		bind_but.del->Enable(false);
		bind_but.prev->Enable(false);
		bind_but.next->Enable(false);
		bind_but.mod1->Enable(false);
		bind_but.mod2->Enable(false);
		bind_but.mod3->Enable(false);
		bind_but.hold->Enable(false);
	}
}

static void SetActiveEvent(CEvent * event) {
	mapper.aevent=event;
	mapper.redraw=true;
	mapper.addbind=false;
	bind_but.event_title->Change("EVENT:%s",event ? event->GetName(): "none");
	if (!event) {
		bind_but.action->Change("Select an event to change");
		bind_but.add->Enable(false);
		SetActiveBind(0);
	} else {
		mapper.abindit=event->bindlist.begin();
		if (mapper.abindit!=event->bindlist.end()) {
			SetActiveBind(*(mapper.abindit));
		} else SetActiveBind(0);
		bind_but.add->Enable(true);
	}
}

static void DrawButtons(void) {
	SDL_FillRect(mapper.surface,0,0);
	SDL_LockSurface(mapper.surface);
	for (CButton_it but_it = buttons.begin();but_it!=buttons.end();but_it++) {
		(*but_it)->Draw();
	}
	SDL_UnlockSurface(mapper.surface);
	SDL_Flip(mapper.surface);
}

static void AddKeyButtonEvent(Bitu x,Bitu y,Bitu dx,Bitu dy,const char * title,const char * entry,KBD_KEYS key) {
	char buf[64];
	strcpy(buf,"key_");
	strcat(buf,entry);
	CKeyEvent * event=new CKeyEvent(buf,key);
	CButton * button=new CEventButton(x,y,dx,dy,title,event);
}

static void AddJAxisButton(Bitu x,Bitu y,Bitu dx,Bitu dy,const char * title,Bitu stick,bool yaxis,bool positive) {
	char buf[64];
	sprintf(buf,"jaxis_%d%s%s",stick,yaxis ? "Y":"X",positive ? "+" : "-");
	CJAxisEvent	* event=new CJAxisEvent(buf,stick,yaxis,positive);
	CButton * button=new CEventButton(x,y,dx,dy,title,event);
}

static void AddJButtonButton(Bitu x,Bitu y,Bitu dx,Bitu dy,const char * title,Bitu _stick,Bitu _button) {
	char buf[64];
	sprintf(buf,"jbutton_%d_%d",_stick,_button);
	CJButtonEvent * event=new CJButtonEvent(buf,_stick,_button);
	CButton * button=new CEventButton(x,y,dx,dy,title,event);
}


static void AddModButton(Bitu x,Bitu y,Bitu dx,Bitu dy,const char * title,Bitu _mod) {
	char buf[64];
	sprintf(buf,"mod_%d",_mod);
	CModEvent * event=new CModEvent(buf,_mod);
	CButton * button=new CEventButton(x,y,dx,dy,title,event);
}

struct KeyBlock {
	const char * title;
	const char * entry;
	KBD_KEYS key;
};
static KeyBlock combo_f[12]={
	{"F1","f1",KBD_f1},		{"F2","f2",KBD_f2},		{"F3","f3",KBD_f3},		
	{"F4","f4",KBD_f4},		{"F5","f5",KBD_f5},		{"F6","f6",KBD_f6},
	{"F7","f7",KBD_f7},		{"F8","f8",KBD_f8},		{"F9","f9",KBD_f9},
	{"F10","f10",KBD_f10},	{"F11","f11",KBD_f11},	{"F12","f12",KBD_f12},
};

static KeyBlock combo_1[14]={
	{"`~","grave",KBD_grave},	{"1!","1",KBD_1},	{"2@","2",KBD_2},
	{"3#","3",KBD_3},			{"4$","4",KBD_4},	{"5%","5",KBD_5},
	{"6^","6",KBD_6},			{"7&","7",KBD_7},	{"8*","8",KBD_8},
	{"9(","9",KBD_9},			{"0)","0",KBD_0},	{"-_","minus",KBD_minus},	
	{"=+","equals",KBD_equals},	{"\x1B","bspace",KBD_backspace},
};

static KeyBlock combo_2[12]={
	{"q","q",KBD_q},			{"w","w",KBD_w},	{"e","e",KBD_e},
	{"r","r",KBD_r},			{"t","t",KBD_t},	{"y","y",KBD_y},
	{"u","u",KBD_u},			{"i","i",KBD_i},	{"o","o",KBD_o},	
	{"p","p",KBD_p},			{"[","lbracket",KBD_leftbracket},	
	{"]","rbracket",KBD_rightbracket},	
};

static KeyBlock combo_3[12]={
	{"a","a",KBD_a},			{"s","s",KBD_s},	{"d","d",KBD_d},
	{"f","f",KBD_f},			{"g","g",KBD_g},	{"h","h",KBD_h},
	{"j","j",KBD_j},			{"k","k",KBD_k},	{"l","l",KBD_l},
	{";","semicolon",KBD_semicolon},				{"'","quote",KBD_quote},
	{"\\","backslash",KBD_backslash},	
};

static KeyBlock combo_4[11]={
	{"<","lessthan",KBD_extra_lt_gt},
	{"z","z",KBD_z},			{"x","x",KBD_x},	{"c","c",KBD_c},
	{"v","v",KBD_v},			{"b","b",KBD_b},	{"n","n",KBD_n},
	{"m","m",KBD_m},			{",","comma",KBD_comma},
	{".","period",KBD_period},						{"/","slash",KBD_slash},		
};


static void CreateLayout(void) {
	Bitu i;
	/* Create the buttons for the Keyboard */
#define BW 28
#define BH 20
#define DX 5
#define PX(_X_) ((_X_)*BW + DX)
#define PY(_Y_) (30+(_Y_)*BH)
	AddKeyButtonEvent(PX(0),PY(0),BW,BH,"ESC","esc",KBD_esc);
	for (i=0;i<12;i++) AddKeyButtonEvent(PX(2+i),PY(0),BW,BH,combo_f[i].title,combo_f[i].entry,combo_f[i].key);
	for (i=0;i<14;i++) AddKeyButtonEvent(PX(  i),PY(1),BW,BH,combo_1[i].title,combo_1[i].entry,combo_1[i].key);

	AddKeyButtonEvent(PX(0),PY(2),BW*2,BH,"TAB","tab",KBD_tab);
	for (i=0;i<12;i++) AddKeyButtonEvent(PX(2+i),PY(2),BW,BH,combo_2[i].title,combo_2[i].entry,combo_2[i].key);

	AddKeyButtonEvent(PX(14),PY(2),BW*2,BH*2,"ENTER","enter",KBD_enter);
	
	AddKeyButtonEvent(PX(0),PY(3),BW*2,BH,"CLCK","capslock",KBD_capslock);
	for (i=0;i<12;i++) AddKeyButtonEvent(PX(2+i),PY(3),BW,BH,combo_3[i].title,combo_3[i].entry,combo_3[i].key);

	AddKeyButtonEvent(PX(0),PY(4),BW*2,BH,"SHIFT","lshift",KBD_leftshift);
	for (i=0;i<11;i++) AddKeyButtonEvent(PX(2+i),PY(4),BW,BH,combo_4[i].title,combo_4[i].entry,combo_4[i].key);
	AddKeyButtonEvent(PX(13),PY(4),BW*3,BH,"SHIFT","rshift",KBD_rightshift);

	/* Last Row */
	AddKeyButtonEvent(PX(0) ,PY(5),BW*2,BH,"CTRL","lctrl",KBD_leftctrl);
	AddKeyButtonEvent(PX(3) ,PY(5),BW*2,BH,"ALT","lalt",KBD_leftalt);
	AddKeyButtonEvent(PX(5) ,PY(5),BW*6,BH,"SPACE","space",KBD_space);
	AddKeyButtonEvent(PX(11),PY(5),BW*2,BH,"ALT","ralt",KBD_rightalt);
	AddKeyButtonEvent(PX(14),PY(5),BW*2,BH,"CTRL","rctrl",KBD_rightctrl);

	/* Arrow Keys */
   
	AddKeyButtonEvent(PX(0),PY(7),BW,BH,"PRT","printscreen",KBD_printscreen);
	AddKeyButtonEvent(PX(1),PY(7),BW,BH,"SCL","scrolllock",KBD_scrolllock);
	AddKeyButtonEvent(PX(2),PY(7),BW,BH,"PAU","pause",KBD_pause);
	AddKeyButtonEvent(PX(0),PY(8),BW,BH,"INS","insert",KBD_insert);
	AddKeyButtonEvent(PX(1),PY(8),BW,BH,"HOM","home",KBD_home);
	AddKeyButtonEvent(PX(2),PY(8),BW,BH,"PUP","pageup",KBD_pageup);
	AddKeyButtonEvent(PX(0),PY(9),BW,BH,"DEL","delete",KBD_delete);
	AddKeyButtonEvent(PX(1),PY(9),BW,BH,"END","end",KBD_end);
	AddKeyButtonEvent(PX(2),PY(9),BW,BH,"PDN","pagedown",KBD_pagedown);
	AddKeyButtonEvent(PX(1),PY(10),BW,BH,"\x18","up",KBD_up);
	AddKeyButtonEvent(PX(0),PY(11),BW,BH,"\x1B","left",KBD_left);
	AddKeyButtonEvent(PX(1),PY(11),BW,BH,"\x19","down",KBD_down);
	AddKeyButtonEvent(PX(2),PY(11),BW,BH,"\x1A","right",KBD_right);
	/* Numeric KeyPad */
	AddKeyButtonEvent(PX(4),PY(7),BW,BH,"NUM","numlock",KBD_numlock);
	AddKeyButtonEvent(PX(5),PY(7),BW,BH,"/","kp_divide",KBD_kpdivide);
	AddKeyButtonEvent(PX(6),PY(7),BW,BH,"*","kp_multiply",KBD_kpmultiply);
	AddKeyButtonEvent(PX(7),PY(7),BW,BH,"-","kp_minus",KBD_kpminus);
	AddKeyButtonEvent(PX(4),PY(8),BW,BH,"7","kp_7",KBD_kp7);
	AddKeyButtonEvent(PX(5),PY(8),BW,BH,"8","kp_8",KBD_kp8);
	AddKeyButtonEvent(PX(6),PY(8),BW,BH,"9","kp_9",KBD_kp9);
	AddKeyButtonEvent(PX(7),PY(8),BW,BH*2,"+","kp_plus",KBD_kpplus);
	AddKeyButtonEvent(PX(4),PY(9),BW,BH,"4","kp_4",KBD_kp4);
	AddKeyButtonEvent(PX(5),PY(9),BW,BH,"5","kp_5",KBD_kp5);
	AddKeyButtonEvent(PX(6),PY(9),BW,BH,"6","kp_6",KBD_kp6);
	AddKeyButtonEvent(PX(4),PY(10),BW,BH,"1","kp_1",KBD_kp1);
	AddKeyButtonEvent(PX(5),PY(10),BW,BH,"2","kp_2",KBD_kp2);
	AddKeyButtonEvent(PX(6),PY(10),BW,BH,"3","kp_3",KBD_kp3);
	AddKeyButtonEvent(PX(7),PY(10),BW,BH*2,"ENT","kp_enter",KBD_kpenter);
	AddKeyButtonEvent(PX(4),PY(11),BW*2,BH,"0","kp_0",KBD_kp0);
	AddKeyButtonEvent(PX(6),PY(11),BW,BH,".","kp_period",KBD_kpperiod);

#if (!OLD_JOYSTICK)
	/* Joystick Buttons/Texts */
	AddJButtonButton(PX(17),PY(0),BW,BH,"1" ,0,0);
	AddJAxisButton  (PX(18),PY(0),BW,BH,"Y-",0,true,false);
	AddJButtonButton(PX(19),PY(0),BW,BH,"2" ,0,1);
	AddJAxisButton  (PX(17),PY(1),BW,BH,"X-",0,false,false);
	AddJAxisButton  (PX(18),PY(1),BW,BH,"Y+",0,true,true);
	AddJAxisButton  (PX(19),PY(1),BW,BH,"X+",0,false,true);

	AddJButtonButton(PX(17),PY(3),BW,BH,"1" ,1,0);
	AddJAxisButton  (PX(18),PY(3),BW,BH,"Y-",1,true,false);
	AddJButtonButton(PX(19),PY(3),BW,BH,"2" ,1,1);
	AddJAxisButton  (PX(17),PY(4),BW,BH,"X-",1,false,false);
	AddJAxisButton  (PX(18),PY(4),BW,BH,"Y+",1,true,true);
	AddJAxisButton  (PX(19),PY(4),BW,BH,"X+",1,false,true);	
#endif   
	/* The modifier buttons */
	AddModButton(PX(0),PY(13),50,20,"Mod1",1);
	AddModButton(PX(2),PY(13),50,20,"Mod2",2);
	AddModButton(PX(4),PY(13),50,20,"Mod3",3);
	/* Create Handler buttons */
	Bitu xpos=3;Bitu ypos=7;
	for (CHandlerEventVector_it hit=handlergroup.begin();hit!=handlergroup.end();hit++) {
		new CEventButton(PX(xpos*3),PY(ypos),BW*3,BH,(*hit)->ButtonName(),(*hit));
		xpos++;
		if (xpos>6) {
			xpos=3;ypos++;
		}
	}
	/* Create some text buttons */
	new CTextButton(PX(6),00,124,20,"Keyboard Layout");
#if (!OLD_JOYSTICK)
	new CTextButton(PX(16),00,124,20,"Joystick Layout");
#endif
	bind_but.action=new CCaptionButton(200,330,0,0);

	bind_but.event_title=new CCaptionButton(0,350,0,0);
	bind_but.bind_title=new CCaptionButton(00,365,0,0);

	/* Create binding support buttons */
	
	bind_but.mod1=new CCheckButton(20,410,60,20, "mod1",BC_Mod1);
	bind_but.mod2=new CCheckButton(20,432,60,20, "mod2",BC_Mod2);
	bind_but.mod3=new CCheckButton(20,454,60,20, "mod3",BC_Mod3);
	bind_but.hold=new CCheckButton(100,410,60,20,"hold",BC_Hold);

	bind_but.prev=new CBindButton(200,400,50,20,"Prev",BB_Prev);
	bind_but.next=new CBindButton(250,400,50,20,"Next",BB_Next);

	bind_but.add=new CBindButton(250,380,50,20,"Add",BB_Add);
	bind_but.del=new CBindButton(300,380,50,20,"Del",BB_Del);

	bind_but.save=new CBindButton(400,450,50,20,"Save",BB_Save);
	bind_but.exit=new CBindButton(450,450,50,20,"Exit",BB_Exit);

	bind_but.bind_title->Change("Bind Title");
}

static SDL_Color map_pal[4]={
	{0x00,0x00,0x00,0x00},			//0=black
	{0xff,0xff,0xff,0x00},			//1=white
	{0xff,0x00,0x00,0x00},			//2=red
};

static void CreateStringBind(char * line) {
	line=trim(line);
	char * eventname=StripWord(line);
	CEvent * event;
	for (CEventVector_it ev_it=events.begin();ev_it!=events.end();ev_it++) {
		if (!strcasecmp((*ev_it)->GetName(),eventname)) {
			event=*ev_it;
			goto foundevent;
		}
	}
	LOG_MSG("Can't find matching event for %s",eventname);
	return ;
foundevent:
	CBind * bind;
	for (char * bindline=StripWord(line);*bindline;bindline=StripWord(line)) {
		for (CBindGroup_it it=bindgroups.begin();it!=bindgroups.end();it++) {
			bind=(*it)->CreateConfigBind(bindline);
			if (bind) {
				event->AddBind(bind);
				bind->SetFlags(bindline);
				break;
			}
		}
	}
}

static struct {
	char * eventend;
	Bitu key;
} DefaultKeys[]={
	{"f1",SDLK_F1},		{"f2",SDLK_F2},		{"f3",SDLK_F3},		{"f4",SDLK_F4},
	{"f5",SDLK_F5},		{"f6",SDLK_F6},		{"f7",SDLK_F7},		{"f8",SDLK_F8},
	{"f9",SDLK_F9},		{"f10",SDLK_F10},	{"f11",SDLK_F11},	{"f12",SDLK_F12},

	{"1",SDLK_1},		{"2",SDLK_2},		{"3",SDLK_3},		{"4",SDLK_4},
	{"5",SDLK_5},		{"6",SDLK_6},		{"7",SDLK_7},		{"8",SDLK_8},
	{"9",SDLK_9},		{"0",SDLK_0},

	{"a",SDLK_a},		{"b",SDLK_b},		{"c",SDLK_c},		{"d",SDLK_d},
	{"e",SDLK_e},		{"f",SDLK_f},		{"g",SDLK_g},		{"h",SDLK_h},
	{"i",SDLK_i},		{"j",SDLK_j},		{"k",SDLK_k},		{"l",SDLK_l},
	{"m",SDLK_m},		{"n",SDLK_n},		{"o",SDLK_o},		{"p",SDLK_p},
	{"q",SDLK_q},		{"r",SDLK_r},		{"s",SDLK_s},		{"t",SDLK_t},
	{"u",SDLK_u},		{"v",SDLK_v},		{"w",SDLK_w},		{"x",SDLK_x},
	{"y",SDLK_y},		{"z",SDLK_z},		{"space",SDLK_SPACE},
	{"esc",SDLK_ESCAPE},	{"equals",SDLK_EQUALS},		{"grave",SDLK_BACKQUOTE},
	{"tab",SDLK_TAB},		{"enter",SDLK_RETURN},		{"bspace",SDLK_BACKSPACE},
	{"lbracket",SDLK_LEFTBRACKET},						{"rbracket",SDLK_RIGHTBRACKET},
	{"minus",SDLK_MINUS},	{"capslock",SDLK_CAPSLOCK},	{"semicolon",SDLK_SEMICOLON},
	{"quote", SDLK_QUOTE},	{"backslash",SDLK_BACKSLASH},	{"lshift",SDLK_LSHIFT},
	{"rshift",SDLK_RSHIFT},	{"lalt",SDLK_LALT},			{"ralt",SDLK_RALT},
	{"lctrl",SDLK_LCTRL},	{"rctrl",SDLK_RCTRL},		{"comma",SDLK_COMMA},
	{"period",SDLK_PERIOD},	{"slash",SDLK_SLASH},		{"printscreen",SDLK_PRINT},
	{"scrolllock",SDLK_SCROLLOCK},	{"pause",SDLK_PAUSE},		{"pagedown",SDLK_PAGEDOWN},
	{"pageup",SDLK_PAGEUP},	{"insert",SDLK_INSERT},		{"home",SDLK_HOME},
	{"delete",SDLK_DELETE},	{"end",SDLK_END},			{"up",SDLK_UP},
	{"left",SDLK_LEFT},		{"down",SDLK_DOWN},			{"right",SDLK_RIGHT},
	{"kp_0",SDLK_KP0},	{"kp_1",SDLK_KP1},	{"kp_2",SDLK_KP2},	{"kp_3",SDLK_KP3},
	{"kp_4",SDLK_KP4},	{"kp_5",SDLK_KP5},	{"kp_6",SDLK_KP6},	{"kp_7",SDLK_KP7},
	{"kp_8",SDLK_KP8},	{"kp_9",SDLK_KP9},	{"numlock",SDLK_NUMLOCK},
	{"kp_divide",SDLK_KP_DIVIDE},	{"kp_multiply",SDLK_KP_MULTIPLY},
	{"kp_minus",SDLK_KP_MINUS},		{"kp_plus",SDLK_KP_PLUS},
	{"kp_period",SDLK_KP_PERIOD},	{"kp_enter",SDLK_KP_ENTER},		{"lessthan",SDLK_LESS},
	{0,0}
};

static void CreateDefaultBinds(void) {
	char buffer[512];
	Bitu i=0;
	while (DefaultKeys[i].eventend) {
		sprintf(buffer,"key_%s \"key %d\"",DefaultKeys[i].eventend,DefaultKeys[i].key);
		CreateStringBind(buffer);
		i++;
	}
	sprintf(buffer,"mod_1 \"key %d\"",SDLK_RCTRL);CreateStringBind(buffer);
	sprintf(buffer,"mod_1 \"key %d\"",SDLK_LCTRL);CreateStringBind(buffer);
	sprintf(buffer,"mod_2 \"key %d\"",SDLK_RALT);CreateStringBind(buffer);
	sprintf(buffer,"mod_2 \"key %d\"",SDLK_LALT);CreateStringBind(buffer);
	for (CHandlerEventVector_it hit=handlergroup.begin();hit!=handlergroup.end();hit++) {
		(*hit)->MakeDefaultBind(buffer);
		CreateStringBind(buffer);
	}
	/* JOYSTICK */
	if (SDL_NumJoysticks()>0) {
//	default mapping
	}
   
}

void MAPPER_AddHandler(MAPPER_Handler * handler,MapKeys key,Bitu mods,char * eventname,char * buttonname) {
	//Check if it allready exists=> if so return.
	for(CHandlerEventVector_it it=handlergroup.begin();it!=handlergroup.end();it++)
		if(strcmp((*it)->buttonname,buttonname) == 0) return;

	char tempname[17];
	strcpy(tempname,"hand_");
	strcat(tempname,eventname);
	new CHandlerEvent(tempname,handler,key,mods,buttonname);
	return ;
}

static void MAPPER_SaveBinds(void) {
	FILE * savefile=fopen(mapper.filename,"wb+");
	if (!savefile) {
		LOG_MSG("Can't open %s for saving the mappings",mapper.filename);
		return;
	}
	char buf[128];
	for (CEventVector_it event_it=events.begin();event_it!=events.end();event_it++) {
		CEvent * event=*(event_it);
		fprintf(savefile,"%s ",event->GetName());
		for (CBindList_it bind_it=event->bindlist.begin();bind_it!=event->bindlist.end();bind_it++) {
			CBind * bind=*(bind_it);
			bind->ConfigName(buf);
			bind->AddFlags(buf);
			fprintf(savefile,"\"%s\" ",buf);
		}
		fprintf(savefile,"\n");
	}
	fclose(savefile);
}

static bool MAPPER_LoadBinds(void) {
	FILE * loadfile=fopen(mapper.filename,"rb+");
	if (!loadfile) return false;
	char linein[512];
	while (fgets(linein,512,loadfile)) {
		CreateStringBind(linein);
	}
	fclose(loadfile);
	return true;
}

void MAPPER_CheckEvent(SDL_Event * event) {
	for (CBindGroup_it it=bindgroups.begin();it!=bindgroups.end();it++) {
		if ((*it)->CheckEvent(event)) return;
	}
}

void BIND_MappingEvents(void) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_MOUSEBUTTONDOWN:
			/* Check the press */
			for (CButton_it but_it = buttons.begin();but_it!=buttons.end();but_it++) {
				if ((*but_it)->OnTop(event.button.x,event.button.y)) {
					(*but_it)->Click();
				}
			}	
			break;
		case SDL_QUIT:
			mapper.exit=true;
			break;
		default:
			if (mapper.addbind) for (CBindGroup_it it=bindgroups.begin();it!=bindgroups.end();it++) {
				CBind * newbind=(*it)->CreateEventBind(&event);
				if (!newbind) continue;
				mapper.aevent->AddBind(newbind);
				SetActiveEvent(mapper.aevent);
				mapper.addbind=false;
				break;
			}
		}
	}
}

static void CreateBindGroups(void) {
	bindgroups.clear();
	new CKeyBindGroup(SDLK_LAST);
	if (joytype != JOY_NONE) {
		Bitu numsticks=SDL_NumJoysticks();
		if (numsticks) SDL_JoystickEventState(SDL_ENABLE);
#if OLD_JOYSTICK
		else return;
#endif
		Bit8u joyno=0;
		switch (joytype) {
		case JOY_4AXIS:
			new C4AxisBindGroup(joyno);
			break;
		case JOY_FCS:
			new CFCSBindGroup(joyno);
			break;
		case JOY_CH:
			new CCHBindGroup(joyno);
			break;
		case JOY_2AXIS:
		default:
			new CStickBindGroup(joyno);
			if((joyno+1) < numsticks)
				new CStickBindGroup(joyno+1);
			break;
		}
	}
}

void MAPPER_LosingFocus(void) {
	for (CEventVector_it evit=events.begin();evit!=events.end();evit++) {
		(*evit)->DeActivateAll();
	}
}

void MAPPER_Run(void) {
	/* Deactive all running binds */
	for (CEventVector_it evit=events.begin();evit!=events.end();evit++) {
		(*evit)->DeActivateAll();
	}
		
	bool mousetoggle=false;
	if(mouselocked) {
		mousetoggle=true;
		GFX_CaptureMouse();
	}

	/* Be sure that there is no update in progress */
	GFX_EndUpdate( 0 );
	mapper.surface=SDL_SetVideoMode(640,480,8,0);
	if (mapper.surface == NULL) E_Exit("Could not initialize video mode for mapper: %s",SDL_GetError());

	/* Set some palette entries */
	SDL_SetPalette(mapper.surface, SDL_LOGPAL|SDL_PHYSPAL, map_pal, 0, 4);
	/* Go in the event loop */
	mapper.exit=false;	
	mapper.redraw=true;
	SetActiveEvent(0);
	while (!mapper.exit) {
		if (mapper.redraw) {
			mapper.redraw=false;		
			DrawButtons();
		}
		BIND_MappingEvents();
		SDL_Delay(1);
	}
	if(mousetoggle) GFX_CaptureMouse();
	GFX_ResetScreen();
}

void MAPPER_Init(void) {
	CreateLayout();
	CreateBindGroups();
	if (!MAPPER_LoadBinds()) CreateDefaultBinds();
}

void MAPPER_StartUp(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	usescancodes=false;

	if (section->Get_bool("usescancodes")) {
		usescancodes=true;

		/* Note: table has to be tested/updated for various OSs */
#if !defined (WIN32)
		sdlkey_map[0x5a]=SDLK_UP;
		sdlkey_map[0x60]=SDLK_DOWN;
		sdlkey_map[0x5c]=SDLK_LEFT;
		sdlkey_map[0x5e]=SDLK_RIGHT;
		sdlkey_map[0x59]=SDLK_HOME;
		sdlkey_map[0x5f]=SDLK_END;
		sdlkey_map[0x5b]=SDLK_PAGEUP;
		sdlkey_map[0x61]=SDLK_PAGEDOWN;
		sdlkey_map[0x62]=SDLK_INSERT;
		sdlkey_map[0x63]=SDLK_DELETE;
		sdlkey_map[0x68]=SDLK_KP_DIVIDE;
		sdlkey_map[0x64]=SDLK_KP_ENTER;
		sdlkey_map[0x65]=SDLK_RCTRL;
		sdlkey_map[0x66]=SDLK_PAUSE;
		sdlkey_map[0x67]=SDLK_PRINT;
		sdlkey_map[0x69]=SDLK_RALT;
#else
		sdlkey_map[0xc8]=SDLK_UP;
		sdlkey_map[0xd0]=SDLK_DOWN;
		sdlkey_map[0xcb]=SDLK_LEFT;
		sdlkey_map[0xcd]=SDLK_RIGHT;
		sdlkey_map[0xc7]=SDLK_HOME;
		sdlkey_map[0xcf]=SDLK_END;
		sdlkey_map[0xc9]=SDLK_PAGEUP;
		sdlkey_map[0xd1]=SDLK_PAGEDOWN;
		sdlkey_map[0xd2]=SDLK_INSERT;
		sdlkey_map[0xd3]=SDLK_DELETE;
		sdlkey_map[0xb5]=SDLK_KP_DIVIDE;
		sdlkey_map[0x9c]=SDLK_KP_ENTER;
		sdlkey_map[0x9d]=SDLK_RCTRL;
		sdlkey_map[0xc5]=SDLK_PAUSE;
		sdlkey_map[0xb7]=SDLK_PRINT;
		sdlkey_map[0xb8]=SDLK_RALT;
#endif

		Bitu i;
		for (i=0; i<MAX_SDLKEYS; i++) scancode_map[i]=0;
		for (i=0; i<MAX_SCANCODES; i++) {
			SDLKey key=sdlkey_map[i];
			if (key<MAX_SDLKEYS) scancode_map[key]=i;
		}
	}

	mapper.filename=section->Get_string("mapperfile");
	MAPPER_AddHandler(&MAPPER_Run,MK_f1,MMOD1,"mapper","Mapper");
}

