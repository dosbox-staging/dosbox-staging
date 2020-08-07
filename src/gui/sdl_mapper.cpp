/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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

#include "dosbox.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <thread>
#include <vector>

#include <SDL.h>
#include <SDL_thread.h>

#include "control.h"
#include "joystick.h"
#include "keyboard.h"
#include "mapper.h"
#include "pic.h"
#include "setup.h"
#include "support.h"
#include "video.h"

/* Mouse related */
void GFX_ToggleMouseCapture();
extern SDL_bool mouse_is_captured; //true if mouse is confined to window

enum {
	CLR_BLACK=0,
	CLR_GREY=1,
	CLR_WHITE=2,
	CLR_RED=3,
	CLR_BLUE=4,
	CLR_GREEN=5,
	CLR_LAST=6
};

enum BB_Types {
	BB_Next,BB_Add,BB_Del,
	BB_Save,BB_Exit
};

enum BC_Types {
	BC_Mod1,BC_Mod2,BC_Mod3,
	BC_Hold
};

#define BMOD_Mod1 0x0001
#define BMOD_Mod2 0x0002
#define BMOD_Mod3 0x0004

#define BFLG_Hold 0x0001
#define BFLG_Repeat 0x0004


#define MAXSTICKS 8
#define MAXACTIVE 16
// Use 36 for Android (KEYCODE_BUTTON_1..16 are mapped to SDL buttons 20..35)
#define MAXBUTTON 36
#define MAXBUTTON_CAP 16
#define MAXAXIS       10
#define MAXHAT        2

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
static std::list<CBind *> all_binds;

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
	CEvent(char const * const _entry)
		: bindlist{}
	{
		safe_strncpy(entry, _entry, sizeof(entry));
		events.push_back(this);
	}

	virtual ~CEvent() = default;

	void AddBind(CBind * bind);
	void ClearBinds();
	virtual void Active(bool yesno)=0;
	virtual void ActivateEvent(bool ev_trigger,bool skip_action)=0;
	virtual void DeActivateEvent(bool ev_trigger)=0;
	void DeActivateAll();
	void SetValue(Bits value){
		current_value=value;
	}
	Bits GetValue() {
		return current_value;
	}
	const char * GetName(void) const {
		 return entry;
	}
	virtual bool IsTrigger() = 0;
	CBindList bindlist;
protected:
	Bitu activity = 0;
	char entry[16] = {0};
	Bits current_value = 0;
};

/* class for events which can be ON/OFF only: key presses, joystick buttons, joystick hat */
class CTriggeredEvent : public CEvent {
public:
	CTriggeredEvent(char const * const _entry) : CEvent(_entry) {}
	virtual bool IsTrigger() {
		return true;
	}
	void ActivateEvent(bool ev_trigger,bool skip_action) {
		if (current_value>25000) {
			/* value exceeds boundary, trigger event if not active */
			if (!activity && !skip_action) Active(true);
			if (activity<32767) activity++;
		} else {
			if (activity>0) {
				/* untrigger event if it is fully inactive */
				DeActivateEvent(ev_trigger);
				activity=0;
			}
		}
	}
	void DeActivateEvent(bool /*ev_trigger*/) {
		activity--;
		if (!activity) Active(false);
	}
};

/* class for events which have a non-boolean state: joystick axis movement */
class CContinuousEvent : public CEvent {
public:
	CContinuousEvent(char const * const _entry) : CEvent(_entry) {}
	virtual bool IsTrigger() {
		return false;
	}
	void ActivateEvent(bool ev_trigger,bool skip_action) {
		if (ev_trigger) {
			activity++;
			if (!skip_action) Active(true);
		} else {
			/* test if no trigger-activity is present, this cares especially
			   about activity of the opposite-direction joystick axis for example */
			if (!GetActivityCount()) Active(true);
		}
	}
	void DeActivateEvent(bool ev_trigger) {
		if (ev_trigger) {
			if (activity>0) activity--;
			if (activity==0) {
				/* test if still some trigger-activity is present,
				   adjust the state in this case accordingly */
				if (GetActivityCount()) RepostActivity();
				else Active(false);
			}
		} else {
			if (!GetActivityCount()) Active(false);
		}
	}
	virtual Bitu GetActivityCount() {
		return activity;
	}
	virtual void RepostActivity() {}
};

class CBind {
public:
	CBind(CBindList *binds)
		: list(binds)
	{
		list->push_back(this);
		event=0;
		all_binds.push_back(this);
	}

	virtual ~CBind()
	{
		if (list)
			list->remove(this);
	}

	CBind(const CBind&) = delete; // prevent copy
	CBind& operator=(const CBind&) = delete; // prevent assignment

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
	void ActivateBind(Bits _value,bool ev_trigger,bool skip_action=false) {
		if (event->IsTrigger()) {
			/* use value-boundary for on/off events */
			if (_value>25000) {
				event->SetValue(_value);
				if (active) return;
				event->ActivateEvent(ev_trigger,skip_action);
				active=true;
			} else {
				if (active) {
					event->DeActivateEvent(ev_trigger);
					active=false;
				}
			}
		} else {
			/* store value for possible later use in the activated event */
			event->SetValue(_value);
			event->ActivateEvent(ev_trigger,false);
		}
	}
	void DeActivateBind(bool ev_trigger) {
		if (event->IsTrigger()) {
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
			event->DeActivateEvent(ev_trigger);
		} else {
			/* store value for possible later use in the activated event */
			event->SetValue(0);
			event->DeActivateEvent(ev_trigger);
		}
	}
	virtual void ConfigName(char * buf)=0;
	virtual void BindName(char * buf)=0;

	Bitu mods = 0;
	Bitu flags = 0;
	CEvent *event = nullptr;
	CBindList *list = nullptr;
	bool active = false;
	bool holding = false;
};

void CEvent::AddBind(CBind * bind) {
	bindlist.push_front(bind);
	bind->event=this;
}
void CEvent::ClearBinds() {
	for (CBind *bind : bindlist) {
		all_binds.remove(bind);
		delete bind;
		bind = nullptr;
	}
	bindlist.clear();
}
void CEvent::DeActivateAll() {
	for (CBindList_it bit = bindlist.begin() ; bit != bindlist.end(); ++bit) {
		(*bit)->DeActivateBind(true);
	}
}



class CBindGroup {
public:
	CBindGroup() {
		bindgroups.push_back(this);
	}
	virtual ~CBindGroup() = default;
	void ActivateBindList(CBindList * list,Bits value,bool ev_trigger);
	void DeactivateBindList(CBindList * list,bool ev_trigger);
	virtual CBind * CreateConfigBind(char *&buf)=0;
	virtual CBind * CreateEventBind(SDL_Event * event)=0;

	virtual bool CheckEvent(SDL_Event * event)=0;
	virtual const char * ConfigStart() = 0;
	virtual const char * BindStart() = 0;

protected:

};

class CKeyBind;
class CKeyBindGroup;

class CKeyBind : public CBind {
public:
	CKeyBind(CBindList *_list, SDL_Scancode _key)
		: CBind(_list),
		  key(_key)
	{}

	void BindName(char *buf)
	{
		sprintf(buf, "Key %s", SDL_GetScancodeName(key));
	}

	void ConfigName(char *buf)
	{
		sprintf(buf, "key %d", key);
	}

public:
	SDL_Scancode key;
};

class CKeyBindGroup : public  CBindGroup {
public:
	CKeyBindGroup(Bitu _keys)
		: CBindGroup(),
		  lists(new CBindList[_keys]),
		  keys(_keys)
	{
		for (size_t i = 0; i < keys; i++)
			lists[i].clear();
	}

	~CKeyBindGroup()
	{
		delete[] lists;
		lists = nullptr;
	}

	CKeyBindGroup(const CKeyBindGroup&) = delete; // prevent copy
	CKeyBindGroup& operator=(const CKeyBindGroup&) = delete; // prevent assignment

	CBind * CreateConfigBind(char *& buf)
	{
		if (strncasecmp(buf, configname, strlen(configname)))
			return nullptr;
		StripWord(buf);
		long code = atol(StripWord(buf));
		assert(code > 0);
		return CreateKeyBind((SDL_Scancode)code);
	}

	CBind *CreateEventBind(SDL_Event *event)
	{
		if (event->type != SDL_KEYDOWN)
			return nullptr;
		return CreateKeyBind(event->key.keysym.scancode);
	}

	bool CheckEvent(SDL_Event * event) {
		if (event->type!=SDL_KEYDOWN && event->type!=SDL_KEYUP) return false;
		uintptr_t key = static_cast<uintptr_t>(event->key.keysym.scancode);
		if (event->type==SDL_KEYDOWN) ActivateBindList(&lists[key],0x7fff,true);
		else DeactivateBindList(&lists[key],true);
		return 0;
	}
	CBind * CreateKeyBind(SDL_Scancode _key) {
		return new CKeyBind(&lists[(Bitu)_key],_key);
	}
private:
	const char * ConfigStart() {
		return configname;
	}
	const char * BindStart() {
		return "Key";
	}
protected:
	const char *configname = "key";
	CBindList *lists = nullptr;
	Bitu keys = 0;
};
static std::list<CKeyBindGroup *> keybindgroups;

#define MAX_VJOY_BUTTONS 8
#define MAX_VJOY_HAT 16
#define MAX_VJOY_AXIS 8
static struct {
	int16_t axis_pos[MAX_VJOY_AXIS] = {0};
	bool hat_pressed[MAX_VJOY_HAT] = {false};
	bool button_pressed[MAX_VJOY_BUTTONS] = {false};
} virtual_joysticks[2];


class CJAxisBind;
class CJButtonBind;
class CJHatBind;

class CJAxisBind : public CBind {
public:
	CJAxisBind(CBindList *_list, CBindGroup *_group, int _axis, bool _positive)
		: CBind(_list),
		  group(_group),
		  axis(_axis),
		  positive(_positive)
	{}

	CJAxisBind(const CJAxisBind&) = delete; // prevent copy
	CJAxisBind& operator=(const CJAxisBind&) = delete; // prevent assignment

	void ConfigName(char *buf)
	{
		sprintf(buf, "%s axis %d %d",
		        group->ConfigStart(),
		        axis,
		        positive ? 1 : 0);
	}

	void BindName(char *buf)
	{
		sprintf(buf, "%s Axis %d%s",
		        group->BindStart(),
		        axis,
		        positive ? "+" : "-");
	}

protected:
	CBindGroup *group;
	int axis;
	bool positive;
};

class CJButtonBind : public CBind {
public:
	CJButtonBind(CBindList *_list, CBindGroup *_group, int _button)
		: CBind(_list),
		  group(_group),
		  button(_button)
	{}

	CJButtonBind(const CJButtonBind&) = delete; // prevent copy
	CJButtonBind& operator=(const CJButtonBind&) = delete; // prevent assignment

	void ConfigName(char *buf)
	{
		sprintf(buf, "%s button %d", group->ConfigStart(), button);
	}

	void BindName(char *buf)
	{
		sprintf(buf, "%s Button %d", group->BindStart(), button);
	}

protected:
	CBindGroup *group;
	int button;
};

class CJHatBind : public CBind {
public:
	CJHatBind(CBindList *_list, CBindGroup *_group, uint8_t _hat, uint8_t _dir)
		: CBind(_list),
		  group(_group),
		  hat(_hat),
		  dir(_dir)
	{
		/* allow only one hat position */ // FIXME why?
		if (dir & SDL_HAT_UP)
			dir = SDL_HAT_UP;
		else if (dir & SDL_HAT_RIGHT)
			dir=SDL_HAT_RIGHT;
		else if (dir & SDL_HAT_DOWN)
			dir=SDL_HAT_DOWN;
		else if (dir & SDL_HAT_LEFT)
			dir=SDL_HAT_LEFT;
		else
			E_Exit("MAPPER:JOYSTICK:Invalid hat position");
	}

	CJHatBind(const CJHatBind&) = delete; // prevent copy
	CJHatBind& operator=(const CJHatBind&) = delete; // prevent assignment

	void ConfigName(char *buf)
	{
		sprintf(buf,"%s hat %" PRIu8 " %" PRIu8,
		        group->ConfigStart(), hat, dir);
	}

	void BindName(char *buf)
	{
		sprintf(buf, "%s Hat %" PRIu8 " %s",
		        group->BindStart(),
		        hat,
		        ((dir == SDL_HAT_UP)    ? "up"    :
		         (dir == SDL_HAT_RIGHT) ? "right" :
		         (dir == SDL_HAT_DOWN)  ? "down"  : "left"));
	}

protected:
	CBindGroup *group;
	uint8_t hat;
	uint8_t dir;
};

bool autofire = false;

class CStickBindGroup : public CBindGroup {
public:
	CStickBindGroup(int _stick, Bitu _emustick, bool _dummy=false)
		: CBindGroup(),
		  stick(_stick), // the number of the physical device (SDL numbering)
		  emustick(_emustick), // the number of the emulated device
		  is_dummy(_dummy)
	{
		sprintf(configname, "stick_%u", static_cast<unsigned>(emustick));
		if (is_dummy)
			return;

		// initialize binding lists and position data
		pos_axis_lists = new CBindList[MAXAXIS];
		neg_axis_lists = new CBindList[MAXAXIS];
		button_lists = new CBindList[MAXBUTTON];
		hat_lists = new CBindList[4];

		// initialize emulated joystick state
		emulated_axes=2;
		emulated_buttons=2;
		emulated_hats=0;
		JOYSTICK_Enable(emustick,true);

		sdl_joystick=SDL_JoystickOpen(_stick);
		if (sdl_joystick==NULL) {
			button_wrap=emulated_buttons;
			return;
		}

		const int sdl_axes = SDL_JoystickNumAxes(sdl_joystick);
		if (sdl_axes < 0)
			LOG_MSG("SDL: Can't detect axes; %s", SDL_GetError());
		axes = clamp(sdl_axes, 0, MAXAXIS);

		const int sdl_hats = SDL_JoystickNumHats(sdl_joystick);
		if (sdl_hats < 0)
			LOG_MSG("SDL: Can't detect hats; %s", SDL_GetError());
		hats = clamp(sdl_hats, 0, MAXHAT);

		buttons = SDL_JoystickNumButtons(sdl_joystick); // TODO returns -1 on error
		button_wrap = buttons;
		button_cap = buttons;
		if (button_wrapping_enabled) {
			button_wrap=emulated_buttons;
			if (buttons>MAXBUTTON_CAP) button_cap = MAXBUTTON_CAP;
		}
		if (button_wrap > MAXBUTTON)
			button_wrap = MAXBUTTON;

		LOG_MSG("MAPPER: Initialized %s with %d axes, %d buttons, and %d hat(s)",
		        SDL_JoystickNameForIndex(stick), axes, buttons, hats);
	}

	~CStickBindGroup()
	{
		SDL_JoystickClose(sdl_joystick);
		sdl_joystick = nullptr;

		delete[] pos_axis_lists;
		pos_axis_lists = nullptr;

		delete[] neg_axis_lists;
		neg_axis_lists = nullptr;

		delete[] button_lists;
		button_lists = nullptr;

		delete[] hat_lists;
		hat_lists = nullptr;
	}

	CStickBindGroup(const CStickBindGroup&) = delete; // prevent copy
	CStickBindGroup& operator=(const CStickBindGroup&) = delete; // prevent assignment

	CBind * CreateConfigBind(char *& buf)
	{
		if (strncasecmp(configname,buf,strlen(configname))) return 0;
		StripWord(buf);
		char *type = StripWord(buf);
		CBind *bind = nullptr;
		if (!strcasecmp(type,"axis")) {
			int ax = atoi(StripWord(buf));
			int pos = atoi(StripWord(buf));
			bind = CreateAxisBind(ax, pos > 0); // TODO double check, previously it was != 0
		} else if (!strcasecmp(type, "button")) {
			int but = atoi(StripWord(buf));
			bind = CreateButtonBind(but);
		} else if (!strcasecmp(type, "hat")) {
			uint8_t hat = static_cast<uint8_t>(atoi(StripWord(buf)));
			uint8_t dir = static_cast<uint8_t>(atoi(StripWord(buf)));
			bind = CreateHatBind(hat, dir);
		}
		return bind;
	}

	CBind * CreateEventBind(SDL_Event * event) {
		if (event->type==SDL_JOYAXISMOTION) {
			if (event->jaxis.which!=stick) return 0;
#if defined (REDUCE_JOYSTICK_POLLING)
			if (event->jaxis.axis >= axes)
				return nullptr;
#endif
			if (abs(event->jaxis.value) < 25000)
				return 0;
			return CreateAxisBind(event->jaxis.axis,
			                      event->jaxis.value > 0);
		} else if (event->type==SDL_JOYBUTTONDOWN) {
			if (event->jbutton.which!=stick) return 0;
#if defined (REDUCE_JOYSTICK_POLLING)
			return CreateButtonBind(event->jbutton.button%button_wrap);
#else
			return CreateButtonBind(event->jbutton.button);
#endif
		} else if (event->type==SDL_JOYHATMOTION) {
			if (event->jhat.which!=stick) return 0;
			if (event->jhat.value==0) return 0;
			if (event->jhat.value>(SDL_HAT_UP|SDL_HAT_RIGHT|SDL_HAT_DOWN|SDL_HAT_LEFT)) return 0;
			return CreateHatBind(event->jhat.hat, event->jhat.value);
		} else return 0;
	}

	virtual bool CheckEvent(SDL_Event * event) {
		SDL_JoyAxisEvent * jaxis = NULL;
		SDL_JoyButtonEvent * jbutton = NULL;
		Bitu but = 0;

		switch(event->type) {
			case SDL_JOYAXISMOTION:
				jaxis = &event->jaxis;
				if(jaxis->which == stick) {
					if(jaxis->axis == 0)
						JOYSTICK_Move_X(emustick,(float)(jaxis->value/32768.0));
					else if(jaxis->axis == 1)
						JOYSTICK_Move_Y(emustick,(float)(jaxis->value/32768.0));
				}
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				jbutton = &event->jbutton;
				bool state;
				state=jbutton->type==SDL_JOYBUTTONDOWN;
				but = jbutton->button % emulated_buttons;
				if (jbutton->which == stick) {
					JOYSTICK_Button(emustick,but,state);
				}
				break;
		}
		return false;
	}

	virtual void UpdateJoystick() {
		if (is_dummy) return;
		/* query SDL joystick and activate bindings */
		ActivateJoystickBoundEvents();

		bool button_pressed[MAXBUTTON];
		std::fill_n(button_pressed, MAXBUTTON, false);
		for (int i = 0; i < MAX_VJOY_BUTTONS; i++) {
			if (virtual_joysticks[emustick].button_pressed[i])
				button_pressed[i % button_wrap]=true;
		}
		for (int i = 0; i < emulated_buttons; i++) {
			if (autofire && (button_pressed[i]))
				JOYSTICK_Button(emustick,i,(++button_autofire[i])&1);
			else
				JOYSTICK_Button(emustick,i,button_pressed[i]);
		}

		JOYSTICK_Move_X(emustick,((float)virtual_joysticks[emustick].axis_pos[0])/32768.0f);
		JOYSTICK_Move_Y(emustick,((float)virtual_joysticks[emustick].axis_pos[1])/32768.0f);
	}

	void ActivateJoystickBoundEvents() {
		if (GCC_UNLIKELY(sdl_joystick==NULL)) return;

		bool button_pressed[MAXBUTTON];
		std::fill_n(button_pressed, MAXBUTTON, false);
		/* read button states */
		for (int i = 0; i < button_cap; i++) {
			if (SDL_JoystickGetButton(sdl_joystick, i))
				button_pressed[i % button_wrap]=true;
		}
		for (int i = 0; i < button_wrap; i++) {
			/* activate binding if button state has changed */
			if (button_pressed[i]!=old_button_state[i]) {
				if (button_pressed[i]) ActivateBindList(&button_lists[i],32767,true);
				else DeactivateBindList(&button_lists[i],true);
				old_button_state[i]=button_pressed[i];
			}
		}
		for (int i = 0; i < axes; i++) {
			Sint16 caxis_pos = SDL_JoystickGetAxis(sdl_joystick, i);
			/* activate bindings for joystick position */
			if (caxis_pos>1) {
				if (old_neg_axis_state[i]) {
					DeactivateBindList(&neg_axis_lists[i],false);
					old_neg_axis_state[i] = false;
				}
				ActivateBindList(&pos_axis_lists[i],caxis_pos,false);
				old_pos_axis_state[i] = true;
			} else if (caxis_pos<-1) {
				if (old_pos_axis_state[i]) {
					DeactivateBindList(&pos_axis_lists[i],false);
					old_pos_axis_state[i] = false;
				}
				if (caxis_pos!=-32768) caxis_pos=(Sint16)abs(caxis_pos);
				else caxis_pos=32767;
				ActivateBindList(&neg_axis_lists[i],caxis_pos,false);
				old_neg_axis_state[i] = true;
			} else {
				/* center */
				if (old_pos_axis_state[i]) {
					DeactivateBindList(&pos_axis_lists[i],false);
					old_pos_axis_state[i] = false;
				}
				if (old_neg_axis_state[i]) {
					DeactivateBindList(&neg_axis_lists[i],false);
					old_neg_axis_state[i] = false;
				}
			}
		}
		for (int i = 0; i < hats; i++) {
			assert(i < MAXHAT);
			const uint8_t chat_state = SDL_JoystickGetHat(sdl_joystick, i);

			/* activate binding if hat state has changed */
			if ((chat_state & SDL_HAT_UP) != (old_hat_state[i] & SDL_HAT_UP)) {
				if (chat_state & SDL_HAT_UP) ActivateBindList(&hat_lists[(i<<2)+0],32767,true);
				else DeactivateBindList(&hat_lists[(i<<2)+0],true);
			}
			if ((chat_state & SDL_HAT_RIGHT) != (old_hat_state[i] & SDL_HAT_RIGHT)) {
				if (chat_state & SDL_HAT_RIGHT) ActivateBindList(&hat_lists[(i<<2)+1],32767,true);
				else DeactivateBindList(&hat_lists[(i<<2)+1],true);
			}
			if ((chat_state & SDL_HAT_DOWN) != (old_hat_state[i] & SDL_HAT_DOWN)) {
				if (chat_state & SDL_HAT_DOWN) ActivateBindList(&hat_lists[(i<<2)+2],32767,true);
				else DeactivateBindList(&hat_lists[(i<<2)+2],true);
			}
			if ((chat_state & SDL_HAT_LEFT) != (old_hat_state[i] & SDL_HAT_LEFT)) {
				if (chat_state & SDL_HAT_LEFT) ActivateBindList(&hat_lists[(i<<2)+3],32767,true);
				else DeactivateBindList(&hat_lists[(i<<2)+3],true);
			}
			old_hat_state[i] = chat_state;
		}
	}

private:
	CBind * CreateAxisBind(int axis, bool positive)
	{
		if (axis < 0 || axis >= axes)
			return nullptr;
		if (positive)
			return new CJAxisBind(&pos_axis_lists[axis],
			                      this, axis, positive);
		else
			return new CJAxisBind(&neg_axis_lists[axis],
			                      this, axis, positive);
	}

	CBind * CreateButtonBind(int button)
	{
		if (button < 0 || button >= button_wrap) 
			return nullptr;
		return new CJButtonBind(&button_lists[button],
		                        this,
		                        button);
	}

	CBind *CreateHatBind(uint8_t hat, uint8_t value)
	{
		if (is_dummy)
			return nullptr;
		assert(hat_lists);

		Bitu hat_dir;
		if (value & SDL_HAT_UP)
			hat_dir = 0;
		else if (value & SDL_HAT_RIGHT)
			hat_dir = 1;
		else if (value & SDL_HAT_DOWN)
			hat_dir = 2;
		else if (value & SDL_HAT_LEFT)
			hat_dir = 3;
		else
			return nullptr;
		return new CJHatBind(&hat_lists[(hat << 2) + hat_dir],
		                     this, hat, value);
	}

	const char * ConfigStart()
	{
		return configname;
	}

	const char * BindStart()
	{
		if (sdl_joystick)
			return SDL_JoystickNameForIndex(stick);
		else
			return "[missing joystick]";
	}

protected:
	CBindList *pos_axis_lists = nullptr;
	CBindList *neg_axis_lists = nullptr;
	CBindList *button_lists = nullptr;
	CBindList *hat_lists = nullptr;
	int axes = 0;
	int emulated_axes = 0;
	int buttons = 0;
	int button_cap = 0;
	int button_wrap = 0;
	int emulated_buttons = 0;
	int hats = 0;
	int emulated_hats = 0;
	int stick;
	Bitu emustick;
	SDL_Joystick *sdl_joystick = nullptr;
	char configname[10];
	unsigned button_autofire[MAXBUTTON] = {};
	bool old_button_state[MAXBUTTON] = {};
	bool old_pos_axis_state[MAXAXIS] = {};
	bool old_neg_axis_state[MAXAXIS] = {};
	uint8_t old_hat_state[MAXHAT] = {};
	bool is_dummy;
};

std::list<CStickBindGroup *> stickbindgroups;

class C4AxisBindGroup : public  CStickBindGroup {
public:
	C4AxisBindGroup(Bitu _stick,Bitu _emustick) : CStickBindGroup (_stick,_emustick){
		emulated_axes=4;
		emulated_buttons=4;
		if (button_wrapping_enabled) button_wrap=emulated_buttons;
		JOYSTICK_Enable(1,true);
	}

	bool CheckEvent(SDL_Event * event) {
		SDL_JoyAxisEvent * jaxis = NULL;
		SDL_JoyButtonEvent * jbutton = NULL;
		Bitu but = 0;

		switch(event->type) {
			case SDL_JOYAXISMOTION:
				jaxis = &event->jaxis;
				if(jaxis->which == stick && jaxis->axis < 4) {
					if(jaxis->axis & 1)
						JOYSTICK_Move_Y(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
					else
						JOYSTICK_Move_X(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
				}
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				jbutton = &event->jbutton;
				bool state;
				state=jbutton->type==SDL_JOYBUTTONDOWN;
				but = jbutton->button % emulated_buttons;
				if (jbutton->which == stick) {
					JOYSTICK_Button((but >> 1),(but & 1),state);
				}
				break;
		}
		return false;
	}

	virtual void UpdateJoystick() {
		/* query SDL joystick and activate bindings */
		ActivateJoystickBoundEvents();

		bool button_pressed[MAXBUTTON];
		std::fill_n(button_pressed, MAXBUTTON, false);
		for (int i = 0; i < MAX_VJOY_BUTTONS; i++) {
			if (virtual_joysticks[0].button_pressed[i])
				button_pressed[i % button_wrap]=true;
		}
		for (int i = 0; i < emulated_buttons; i++) {
			if (autofire && (button_pressed[i]))
				JOYSTICK_Button(i>>1,i&1,(++button_autofire[i])&1);
			else
				JOYSTICK_Button(i>>1,i&1,button_pressed[i]);
		}

		JOYSTICK_Move_X(0,((float)virtual_joysticks[0].axis_pos[0])/32768.0f);
		JOYSTICK_Move_Y(0,((float)virtual_joysticks[0].axis_pos[1])/32768.0f);
		JOYSTICK_Move_X(1,((float)virtual_joysticks[0].axis_pos[2])/32768.0f);
		JOYSTICK_Move_Y(1,((float)virtual_joysticks[0].axis_pos[3])/32768.0f);
	}
};

class CFCSBindGroup : public  CStickBindGroup {
public:
	CFCSBindGroup(Bitu _stick, Bitu _emustick)
		: CStickBindGroup(_stick, _emustick)
	{
		emulated_axes=4;
		emulated_buttons=4;
		emulated_hats=1;
		if (button_wrapping_enabled) button_wrap=emulated_buttons;
		JOYSTICK_Enable(1,true);
		JOYSTICK_Move_Y(1,1.0);
	}

	bool CheckEvent(SDL_Event * event) {
		SDL_JoyAxisEvent * jaxis = NULL;
		SDL_JoyButtonEvent * jbutton = NULL;
		SDL_JoyHatEvent * jhat = NULL;
		Bitu but = 0;

		switch(event->type) {
			case SDL_JOYAXISMOTION:
				jaxis = &event->jaxis;
				if(jaxis->which == stick) {
					if(jaxis->axis == 0)
						JOYSTICK_Move_X(0,(float)(jaxis->value/32768.0));
					else if(jaxis->axis == 1)
						JOYSTICK_Move_Y(0,(float)(jaxis->value/32768.0));
					else if(jaxis->axis == 2)
						JOYSTICK_Move_X(1,(float)(jaxis->value/32768.0));
				}
				break;
			case SDL_JOYHATMOTION:
				jhat = &event->jhat;
				if(jhat->which == stick) DecodeHatPosition(jhat->value);
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				jbutton = &event->jbutton;
				bool state;
				state=jbutton->type==SDL_JOYBUTTONDOWN;
				but = jbutton->button % emulated_buttons;
				if (jbutton->which == stick) {
						JOYSTICK_Button((but >> 1),(but & 1),state);
				}
				break;
		}
		return false;
	}

	virtual void UpdateJoystick() {
		/* query SDL joystick and activate bindings */
		ActivateJoystickBoundEvents();

		bool button_pressed[MAXBUTTON];
		for (int i = 0; i < MAXBUTTON; i++)
			button_pressed[i] = false;
		for (int i = 0; i < MAX_VJOY_BUTTONS; i++) {
			if (virtual_joysticks[0].button_pressed[i])
				button_pressed[i % button_wrap]=true;
		}
		for (int i = 0; i < emulated_buttons; i++) {
			if (autofire && (button_pressed[i]))
				JOYSTICK_Button(i>>1,i&1,(++button_autofire[i])&1);
			else
				JOYSTICK_Button(i>>1,i&1,button_pressed[i]);
		}

		JOYSTICK_Move_X(0,((float)virtual_joysticks[0].axis_pos[0])/32768.0f);
		JOYSTICK_Move_Y(0,((float)virtual_joysticks[0].axis_pos[1])/32768.0f);
		JOYSTICK_Move_X(1,((float)virtual_joysticks[0].axis_pos[2])/32768.0f);

		Uint8 hat_pos=0;
		if (virtual_joysticks[0].hat_pressed[0]) hat_pos|=SDL_HAT_UP;
		else if (virtual_joysticks[0].hat_pressed[2]) hat_pos|=SDL_HAT_DOWN;
		if (virtual_joysticks[0].hat_pressed[3]) hat_pos|=SDL_HAT_LEFT;
		else if (virtual_joysticks[0].hat_pressed[1]) hat_pos|=SDL_HAT_RIGHT;

		if (hat_pos!=old_hat_position) {
			DecodeHatPosition(hat_pos);
			old_hat_position=hat_pos;
		}
	}

private:
	uint8_t old_hat_position = 0;

	void DecodeHatPosition(Uint8 hat_pos) {
		switch(hat_pos) {
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
};

class CCHBindGroup : public CStickBindGroup {
public:
	CCHBindGroup(Bitu _stick, Bitu _emustick)
		: CStickBindGroup(_stick, _emustick)
	{
		emulated_axes=4;
		emulated_buttons=6;
		emulated_hats=1;
		if (button_wrapping_enabled) button_wrap=emulated_buttons;
		JOYSTICK_Enable(1,true);
	}

	bool CheckEvent(SDL_Event * event) {
		SDL_JoyAxisEvent * jaxis = NULL;
		SDL_JoyButtonEvent * jbutton = NULL;
		SDL_JoyHatEvent * jhat = NULL;
		Bitu but = 0;
		static unsigned const button_magic[6]={0x02,0x04,0x10,0x100,0x20,0x200};
		static unsigned const hat_magic[2][5]={{0x8888,0x8000,0x800,0x80,0x08},
							   {0x5440,0x4000,0x400,0x40,0x1000}};
		switch(event->type) {
			case SDL_JOYAXISMOTION:
				jaxis = &event->jaxis;
				if(jaxis->which == stick && jaxis->axis < 4) {
					if(jaxis->axis & 1)
						JOYSTICK_Move_Y(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
					else
						JOYSTICK_Move_X(jaxis->axis>>1 & 1,(float)(jaxis->value/32768.0));
				}
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
				but = jbutton->button % emulated_buttons;
				if (jbutton->which == stick)
					button_state|=button_magic[but];
				break;
			case SDL_JOYBUTTONUP:
				jbutton = &event->jbutton;
				but = jbutton->button % emulated_buttons;
				if (jbutton->which == stick)
					button_state&=~button_magic[but];
				break;
		}

		unsigned i;
		Bit16u j;
		j=button_state;
		for(i=0;i<16;i++) if (j & 1) break; else j>>=1;
		JOYSTICK_Button(0,0,i&1);
		JOYSTICK_Button(0,1,(i>>1)&1);
		JOYSTICK_Button(1,0,(i>>2)&1);
		JOYSTICK_Button(1,1,(i>>3)&1);
		return false;
	}

	void UpdateJoystick() {
		static unsigned const button_priority[6]={7,11,13,14,5,6};
		static unsigned const hat_priority[2][4]={{0,1,2,3},{8,9,10,12}};

		/* query SDL joystick and activate bindings */
		ActivateJoystickBoundEvents();

		JOYSTICK_Move_X(0,((float)virtual_joysticks[0].axis_pos[0])/32768.0f);
		JOYSTICK_Move_Y(0,((float)virtual_joysticks[0].axis_pos[1])/32768.0f);
		JOYSTICK_Move_X(1,((float)virtual_joysticks[0].axis_pos[2])/32768.0f);
		JOYSTICK_Move_Y(1,((float)virtual_joysticks[0].axis_pos[3])/32768.0f);

		Bitu bt_state=15;

		for (int i = 0; i < (hats < 2 ? hats : 2); i++) {
			Uint8 hat_pos=0;
			if (virtual_joysticks[0].hat_pressed[(i<<2)+0]) hat_pos|=SDL_HAT_UP;
			else if (virtual_joysticks[0].hat_pressed[(i<<2)+2]) hat_pos|=SDL_HAT_DOWN;
			if (virtual_joysticks[0].hat_pressed[(i<<2)+3]) hat_pos|=SDL_HAT_LEFT;
			else if (virtual_joysticks[0].hat_pressed[(i<<2)+1]) hat_pos|=SDL_HAT_RIGHT;

			if (hat_pos & SDL_HAT_UP)
				if (bt_state>hat_priority[i][0]) bt_state=hat_priority[i][0];
			if (hat_pos & SDL_HAT_DOWN)
				if (bt_state>hat_priority[i][1]) bt_state=hat_priority[i][1];
			if (hat_pos & SDL_HAT_RIGHT)
				if (bt_state>hat_priority[i][2]) bt_state=hat_priority[i][2];
			if (hat_pos & SDL_HAT_LEFT)
				if (bt_state>hat_priority[i][3]) bt_state=hat_priority[i][3];
		}

		bool button_pressed[MAXBUTTON];
		std::fill_n(button_pressed, MAXBUTTON, false);
		for (int i = 0; i < MAX_VJOY_BUTTONS; i++) {
			if (virtual_joysticks[0].button_pressed[i])
				button_pressed[i % button_wrap] = true;
		}
		for (int i = 0; i < 6; i++) {
			if ((button_pressed[i]) && (bt_state>button_priority[i]))
				bt_state=button_priority[i];
		}

		if (bt_state>15) bt_state=15;
		JOYSTICK_Button(0,0,(bt_state&8)==0);
		JOYSTICK_Button(0,1,(bt_state&4)==0);
		JOYSTICK_Button(1,0,(bt_state&2)==0);
		JOYSTICK_Button(1,1,(bt_state&1)==0);
	}

protected:
	uint16_t button_state = 0;
};


void MAPPER_TriggerEvent(const CEvent *event, const bool deactivation_state) {
	assert(event);
	for (auto &bind : event->bindlist) {
		bind->ActivateBind(32767, true, false);
		bind->DeActivateBind(deactivation_state);
	}
}

class Typer {
	public:
		Typer() = default;
		Typer(const Typer&) = delete; // prevent copy
		Typer& operator=(const Typer&) = delete; // prevent assignment
		~Typer() {
			Stop();
		}
		void Start(std::vector<CEvent*>     *ext_events,
		           std::vector<std::string> &ext_sequence,
                   const uint32_t           wait_ms,
                   const uint32_t           pace_ms) {
			// Guard against empty inputs
			if (!ext_events || ext_sequence.empty())
				return;
			Wait();
			m_events = ext_events;
			m_sequence = std::move(ext_sequence);
			m_wait_ms = wait_ms;
			m_pace_ms = pace_ms;
			m_stop_requested = false;
			m_instance = std::thread(&Typer::Callback, this);
		}
		void Wait() {
			if (m_instance.joinable())
				m_instance.join();
		}
		void Stop() {
			m_stop_requested = true;
			Wait();
		}
	private:
		void Callback() {
 			// quit before our initial wait time
 			if (m_stop_requested)
				return;
			std::this_thread::sleep_for(std::chrono::milliseconds(m_wait_ms));
			for (const auto &button : m_sequence) {
				bool found = false;
				// comma adds an extra pause, similar to the pause used in a phone number
				if (button == ",") {
					found = true;
					 // quit before the pause
					if (m_stop_requested)
						return;
					std::this_thread::sleep_for(std::chrono::milliseconds(m_pace_ms));
				// Otherwise trigger the matching button if we have one
				} else {
					const std::string bind_name = "key_" + button;
					for (auto &event : *m_events) {
						if (bind_name == event->GetName()) {
							found = true;
							MAPPER_TriggerEvent(event, true);
							break;
						}
					}
				}
				/*
				*  Terminate the sequence for safety reasons if we can't find a button.
				*  For example, we don't wan't DEAL becoming DEL, or 'rem' becoming 'rm'
				*/
				if (!found) {
					LOG_MSG("MAPPER: Couldn't find a button named '%s', stopping.",
							button.c_str());
					return;
				}
				if (m_stop_requested) // quit before the pacing delay
					return;
				std::this_thread::sleep_for(std::chrono::milliseconds(m_pace_ms));
			}
		}
		std::thread              m_instance;
		std::vector<std::string> m_sequence;
		std::vector<CEvent*>     *m_events = nullptr;
		uint32_t                 m_wait_ms = 0;
		uint32_t                 m_pace_ms = 0;
		bool                     m_stop_requested = false;
};

static struct CMapper {
	SDL_Window *window = nullptr;
	SDL_Rect draw_rect = {0, 0, 0, 0};
	SDL_Surface *draw_surface_nonpaletted = nullptr; // Needed for SDL_BlitScaled
	SDL_Surface *surface = nullptr;
	SDL_Surface *draw_surface = nullptr;
	bool exit = false;
	CEvent *aevent = nullptr; // Active Event
	CBind *abind = nullptr; // Active Bind
	CBindList_it abindit; //Location of active bind in list
	bool redraw = false;
	bool addbind = false;
	Bitu mods = 0;
	struct {
		CStickBindGroup * stick[MAXSTICKS] = {nullptr};
		unsigned int num = 0;
		unsigned int num_groups = 0;
	} sticks;
	Typer typist;
	std::string filename = "";
} mapper;

void CBindGroup::ActivateBindList(CBindList * list,Bits value,bool ev_trigger) {
	assert(list);
	Bitu validmod=0;
	CBindList_it it;
	for (it = list->begin(); it != list->end(); ++it) {
		if (((*it)->mods & mapper.mods) == (*it)->mods) {
			if (validmod<(*it)->mods) validmod=(*it)->mods;
		}
	}
	for (it = list->begin(); it != list->end(); ++it) {
		if (validmod==(*it)->mods) (*it)->ActivateBind(value,ev_trigger);
	}
}

void CBindGroup::DeactivateBindList(CBindList * list,bool ev_trigger) {
	assert(list);
	CBindList_it it;
	for (it = list->begin(); it != list->end(); ++it) {
		(*it)->DeActivateBind(ev_trigger);
	}
}

static void DrawText(Bitu x,Bitu y,const char * text,Bit8u color) {
	Bit8u * draw = ((Bit8u *)mapper.draw_surface->pixels) + (y * mapper.draw_surface->w) + x;
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
			draw_line += mapper.draw_surface->w;
		}
		text++;draw+=8;
	}
}

class CButton {
public:
	CButton(Bitu p_x, Bitu p_y, Bitu p_dx, Bitu p_dy)
		: x(p_x),
		  y(p_y),
		  dx(p_dx),
		  dy(p_dy),
		  color(CLR_WHITE),
		  enabled(true)
	{
		buttons.push_back(this);
	}

	virtual ~CButton() = default;

	virtual void Draw() {
		if (!enabled)
			return;
		Bit8u * point = ((Bit8u *)mapper.draw_surface->pixels) + (y * mapper.draw_surface->w) + x;
		for (Bitu lines=0;lines<dy;lines++)  {
			if (lines==0 || lines==(dy-1)) {
				for (Bitu cols=0;cols<dx;cols++) *(point+cols)=color;
			} else {
				*point=color;*(point+dx-1)=color;
			}
			point += mapper.draw_surface->w;
		}
	}
	virtual bool OnTop(Bitu _x,Bitu _y) {
		return ( enabled && (_x>=x) && (_x<x+dx) && (_y>=y) && (_y<y+dy));
	}
	virtual void BindColor() {}
	virtual void Click() {}
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

// Inform PVS-Studio that new CTextButtons won't be leaked when they go out of scope
//+V773:SUPPRESS, class:CTextButton
class CTextButton : public CButton {
public:
	CTextButton(Bitu  x, Bitu y, Bitu dx, Bitu dy, const char *txt)
		: CButton(x, y, dx, dy),
		  text(txt)
	{}

	CTextButton(const CTextButton&) = delete; // prevent copy
	CTextButton& operator=(const CTextButton&) = delete; // prevent assignment

	void Draw()
	{
		if (!enabled)
			return;
		CButton::Draw();
		DrawText(x + 2, y + 2, text, color);
	}

protected:
	const char *text = nullptr;
};

class CClickableTextButton : public CTextButton {
public:
	CClickableTextButton(Bitu _x, Bitu _y, Bitu _dx, Bitu _dy, const char * _text)
		: CTextButton(_x, _y, _dx, _dy, _text)
	{}

	void BindColor()
	{
		this->SetColor(CLR_WHITE);
	}
};

class CEventButton;
static CEventButton * last_clicked = NULL;

class CEventButton : public CClickableTextButton {
public:
	CEventButton(Bitu x, Bitu y, Bitu dx, Bitu dy, const char *text, CEvent *ev)
		: CClickableTextButton(x, y, dx, dy, text),
		  event(ev)
	{}

	CEventButton(const CEventButton&) = delete; // prevent copy
	CEventButton& operator=(const CEventButton&) = delete; // prevent assignment

	void BindColor() {
		this->SetColor(event->bindlist.begin()==event->bindlist.end() ? CLR_GREY : CLR_WHITE);
	}
	void Click() {
		if (last_clicked) last_clicked->BindColor();
		this->SetColor(CLR_GREEN);
		SetActiveEvent(event);
		last_clicked=this;
	}
protected:
	CEvent * event = nullptr;
};

class CCaptionButton : public CButton {
public:
	CCaptionButton(Bitu _x,Bitu _y,Bitu _dx,Bitu _dy) : CButton(_x,_y,_dx,_dy){
		caption[0]=0;
	}
	void Change(const char * format,...) GCC_ATTRIBUTE(__format__(__printf__,2,3));

	void Draw() {
		if (!enabled) return;
		DrawText(x+2,y+2,caption,color);
	}
protected:
	char caption[128] = {};
};

void CCaptionButton::Change(const char * format,...) {
	va_list msg;
	va_start(msg,format);
	vsprintf(caption,format,msg);
	va_end(msg);
	mapper.redraw=true;
}		

static void change_action_text(const char* text,Bit8u col);

static void MAPPER_SaveBinds();

class CBindButton : public CClickableTextButton {
public:	
	CBindButton(Bitu _x, Bitu _y, Bitu _dx, Bitu _dy, const char * _text, BB_Types _type)
		: CClickableTextButton(_x, _y, _dx, _dy, _text),
		  type(_type)
	{}

	void Click() {
		switch (type) {
		case BB_Add: 
			mapper.addbind=true;
			SetActiveBind(0);
			change_action_text("Press a key/joystick button or move the joystick.",CLR_RED);
			break;
		case BB_Del:
			if (mapper.abindit != mapper.aevent->bindlist.end()) {
				auto *active_bind = *mapper.abindit;
				all_binds.remove(active_bind);
				delete active_bind;
				mapper.abindit=mapper.aevent->bindlist.erase(mapper.abindit);
				if (mapper.abindit==mapper.aevent->bindlist.end()) 
					mapper.abindit=mapper.aevent->bindlist.begin();
			}
			if (mapper.abindit!=mapper.aevent->bindlist.end()) SetActiveBind(*(mapper.abindit));
			else SetActiveBind(0);
			break;
		case BB_Next:
			if (mapper.abindit!=mapper.aevent->bindlist.end()) 
				++mapper.abindit;
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

class CCheckButton : public CClickableTextButton {
public:
	CCheckButton(Bitu x, Bitu y, Bitu dx, Bitu dy, const char *text, BC_Types t)
		: CClickableTextButton(x, y, dx, dy, text),
		  type(t)
	{}

	void Draw() {
		if (!enabled) return;
		bool checked=false;
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
			Bit8u * point=((Bit8u *)mapper.draw_surface->pixels)+((y+2)*mapper.draw_surface->w)+x+dx-dy+2;
			for (Bitu lines=0;lines<(dy-4);lines++)  {
				memset(point,color,dy-4);
				point+=mapper.draw_surface->w;
			}
		}
		CClickableTextButton::Draw();
	}
	void Click() {
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

class CKeyEvent : public CTriggeredEvent {
public:
	CKeyEvent(char const * const entry, KBD_KEYS k)
		: CTriggeredEvent(entry),
		  key(k)
	{}

	void Active(bool yesno) {
		KEYBOARD_AddKey(key,yesno);
	}

	KBD_KEYS key;
};

class CJAxisEvent : public CContinuousEvent {
public:
	CJAxisEvent(char const * const entry, Bitu s, Bitu a, bool p, CJAxisEvent *op_axis)
		: CContinuousEvent(entry),
		  stick(s),
		  axis(a),
		  positive(p),
		  opposite_axis(op_axis)
	{
		if (opposite_axis)
			opposite_axis->SetOppositeAxis(this);
	}

	CJAxisEvent(const CJAxisEvent&) = delete; // prevent copy
	CJAxisEvent& operator=(const CJAxisEvent&) = delete; // prevent assignment

	void Active(bool /*moved*/) {
		virtual_joysticks[stick].axis_pos[axis]=(Bit16s)(GetValue()*(positive?1:-1));
	}
	virtual Bitu GetActivityCount() {
		return activity|opposite_axis->activity;
	}
	virtual void RepostActivity() {
		/* caring for joystick movement into the opposite direction */
		opposite_axis->Active(true);
	}
protected:
	void SetOppositeAxis(CJAxisEvent * _opposite_axis) {
		opposite_axis=_opposite_axis;
	}
	Bitu stick,axis;
	bool positive;
	CJAxisEvent * opposite_axis;
};

class CJButtonEvent : public CTriggeredEvent {
public:
	CJButtonEvent(char const * const entry, Bitu s, Bitu btn)
		: CTriggeredEvent(entry),
		  stick(s),
		  button(btn)
	{}

	void Active(bool pressed)
	{
		virtual_joysticks[stick].button_pressed[button]=pressed;
	}

protected:
	Bitu stick,button;
};

class CJHatEvent : public CTriggeredEvent {
public:
	CJHatEvent(char const * const entry, Bitu s, Bitu h, Bitu d)
		: CTriggeredEvent(entry),
		  stick(s),
		  hat(h),
		  dir(d)
	{}

	void Active(bool pressed)
	{
		virtual_joysticks[stick].hat_pressed[(hat<<2)+dir]=pressed;
	}

protected:
	Bitu stick,hat,dir;
};

class CModEvent : public CTriggeredEvent {
public:
	CModEvent(char const * const _entry, int _wmod)
		: CTriggeredEvent(_entry),
		  wmod(_wmod)
	{}

	void Active(bool yesno)
	{
		if (yesno)
			mapper.mods |= (static_cast<Bitu>(1) << (wmod-1));
		else
			mapper.mods &= ~(static_cast<Bitu>(1) << (wmod-1));
	}

protected:
	int wmod;
};

class CHandlerEvent : public CTriggeredEvent {
public:
	CHandlerEvent(char const * const entry, MAPPER_Handler *handle, MapKeys k, Bitu mod, char const * const bname)
		: CTriggeredEvent(entry),
		  defkey(k),
		  defmod(mod),
		  handler(handle),
		  buttonname(bname)
	{
		handlergroup.push_back(this);
	}
	~CHandlerEvent() = default;
	CHandlerEvent(const CHandlerEvent&) = delete; // prevent copy
	CHandlerEvent& operator=(const CHandlerEvent&) = delete; // prevent assignment

	void Active(bool yesno) { (*handler)(yesno); }

	const char * ButtonName()
	{
		return buttonname;
	}

	void MakeDefaultBind(char *buf)
	{
		SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
		switch (defkey) {
		case MK_f1:          key = SDL_SCANCODE_F1; break;
		case MK_f2:          key = SDL_SCANCODE_F2; break;
		case MK_f3:          key = SDL_SCANCODE_F3; break;
		case MK_f4:          key = SDL_SCANCODE_F4; break;
		case MK_f5:          key = SDL_SCANCODE_F5; break;
		case MK_f6:          key = SDL_SCANCODE_F6; break;
		case MK_f7:          key = SDL_SCANCODE_F7; break;
		case MK_f8:          key = SDL_SCANCODE_F8; break;
		case MK_f9:          key = SDL_SCANCODE_F9; break;
		case MK_f10:         key = SDL_SCANCODE_F10; break;
		case MK_f11:         key = SDL_SCANCODE_F11; break;
		case MK_f12:         key = SDL_SCANCODE_F12; break;
		case MK_return:      key = SDL_SCANCODE_RETURN; break;
		case MK_kpminus:     key = SDL_SCANCODE_KP_MINUS; break;
		case MK_scrolllock:  key = SDL_SCANCODE_SCROLLLOCK; break;
		case MK_pause:       key = SDL_SCANCODE_PAUSE; break;
		case MK_printscreen: key = SDL_SCANCODE_PRINTSCREEN; break;
		case MK_home:        key = SDL_SCANCODE_HOME; break;
		}
		sprintf(buf, "%s \"key %d%s%s%s\"",
		        entry,
		        static_cast<int>(key),
		        defmod & 1 ? " mod1" : "",
		        defmod & 2 ? " mod2" : "",
		        defmod & 4 ? " mod3" : "");
	}

protected:
	MapKeys defkey;
	Bitu defmod;
	MAPPER_Handler * handler;
public:
	const char * buttonname;
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
	CCheckButton * mod1,* mod2,* mod3,* hold;
} bind_but;


static void change_action_text(const char* text,Bit8u col) {
	bind_but.action->Change(text,"");
	bind_but.action->SetColor(col);
}


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
		change_action_text("Select an event to change.",CLR_WHITE);
		bind_but.add->Enable(false);
		SetActiveBind(0);
	} else {
		change_action_text("Select a different event or hit the Add/Del/Next buttons.",CLR_WHITE);
		mapper.abindit=event->bindlist.begin();
		if (mapper.abindit!=event->bindlist.end()) {
			SetActiveBind(*(mapper.abindit));
		} else SetActiveBind(0);
		bind_but.add->Enable(true);
	}
}

extern SDL_Window * GFX_SetSDLSurfaceWindow(Bit16u width, Bit16u height);
extern SDL_Rect GFX_GetSDLSurfaceSubwindowDims(Bit16u width, Bit16u height);
extern void GFX_UpdateDisplayDimensions(int width, int height);

static void DrawButtons() {
	SDL_FillRect(mapper.draw_surface,0,CLR_BLACK);
	for (CButton_it but_it = buttons.begin(); but_it != buttons.end(); ++but_it) {
		(*but_it)->Draw();
	}
	// We can't just use SDL_BlitScaled (say for Android) in one step
	SDL_BlitSurface(mapper.draw_surface, NULL, mapper.draw_surface_nonpaletted, NULL);
	SDL_BlitScaled(mapper.draw_surface_nonpaletted, NULL, mapper.surface, &mapper.draw_rect);
	//SDL_BlitSurface(mapper.draw_surface, NULL, mapper.surface, NULL);
	SDL_UpdateWindowSurface(mapper.window);
}

static CKeyEvent * AddKeyButtonEvent(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,char const * const entry,KBD_KEYS key) {
	char buf[64];
	safe_strcpy(buf, "key_");
	safe_strcat(buf, entry);
	CKeyEvent * event=new CKeyEvent(buf,key);
	new CEventButton(x,y,dx,dy,title,event);
	return event;
}

static CJAxisEvent * AddJAxisButton(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,Bitu stick,Bitu axis,bool positive,CJAxisEvent * opposite_axis) {
	char buf[64];
	sprintf(buf, "jaxis_%d_%d%s",
	        static_cast<int>(stick),
	        static_cast<int>(axis),
	        positive ? "+" : "-");
	CJAxisEvent	* event=new CJAxisEvent(buf,stick,axis,positive,opposite_axis);
	new CEventButton(x,y,dx,dy,title,event);
	return event;
}
static CJAxisEvent * AddJAxisButton_hidden(Bitu stick,Bitu axis,bool positive,CJAxisEvent * opposite_axis) {
	char buf[64];
	sprintf(buf, "jaxis_%d_%d%s",
	        static_cast<int>(stick),
	        static_cast<int>(axis),
	        positive ? "+" : "-");
	return new CJAxisEvent(buf,stick,axis,positive,opposite_axis);
}

static void AddJButtonButton(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,Bitu stick,Bitu button) {
	char buf[64];
	sprintf(buf, "jbutton_%d_%d",
	        static_cast<int>(stick),
	        static_cast<int>(button));
	CJButtonEvent * event=new CJButtonEvent(buf,stick,button);
	new CEventButton(x,y,dx,dy,title,event);
}
static void AddJButtonButton_hidden(Bitu stick,Bitu button) {
	char buf[64];
	sprintf(buf, "jbutton_%d_%d",
	        static_cast<int>(stick),
	        static_cast<int>(button));
	new CJButtonEvent(buf,stick,button);
}

static void AddJHatButton(Bitu x,Bitu y,Bitu dx,Bitu dy,char const * const title,Bitu _stick,Bitu _hat,Bitu _dir) {
	char buf[64];
	sprintf(buf, "jhat_%d_%d_%d",
	        static_cast<int>(_stick),
	        static_cast<int>(_hat),
	        static_cast<int>(_dir));
	CJHatEvent * event=new CJHatEvent(buf,_stick,_hat,_dir);
	new CEventButton(x,y,dx,dy,title,event);
}

static void AddModButton(Bitu x, Bitu y, Bitu dx, Bitu dy, char const * const title, int mod)
{
	char buf[64];
	sprintf(buf, "mod_%d", mod);
	CModEvent *event = new CModEvent(buf, mod);
	new CEventButton(x, y, dx, dy, title, event);
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
	{"Q","q",KBD_q},			{"W","w",KBD_w},	{"E","e",KBD_e},
	{"R","r",KBD_r},			{"T","t",KBD_t},	{"Y","y",KBD_y},
	{"U","u",KBD_u},			{"I","i",KBD_i},	{"O","o",KBD_o},
	{"P","p",KBD_p},			{"[{","lbracket",KBD_leftbracket},
	{"]}","rbracket",KBD_rightbracket},	
};

static KeyBlock combo_3[12]={
	{"A","a",KBD_a},			{"S","s",KBD_s},	{"D","d",KBD_d},
	{"F","f",KBD_f},			{"G","g",KBD_g},	{"H","h",KBD_h},
	{"J","j",KBD_j},			{"K","k",KBD_k},	{"L","l",KBD_l},
	{";:","semicolon",KBD_semicolon},				{"'\"","quote",KBD_quote},
	{"\\|","backslash",KBD_backslash},
};

static KeyBlock combo_4[11]={
	{"<>","lessthan",KBD_extra_lt_gt},
	{"Z","z",KBD_z},			{"X","x",KBD_x},	{"C","c",KBD_c},
	{"V","v",KBD_v},			{"B","b",KBD_b},	{"N","n",KBD_n},
	{"M","m",KBD_m},			{",<","comma",KBD_comma},
	{".>","period",KBD_period},						{"/?","slash",KBD_slash},
};

static CKeyEvent * caps_lock_event=NULL;
static CKeyEvent * num_lock_event=NULL;

static void CreateLayout() {
	Bitu i;
	/* Create the buttons for the Keyboard */
#define BW 28
#define BH 20
#define DX 5
#define PX(_X_) ((_X_)*BW + DX)
#define PY(_Y_) (10+(_Y_)*BH)
	AddKeyButtonEvent(PX(0),PY(0),BW,BH,"ESC","esc",KBD_esc);
	for (i=0;i<12;i++) AddKeyButtonEvent(PX(2+i),PY(0),BW,BH,combo_f[i].title,combo_f[i].entry,combo_f[i].key);
	for (i=0;i<14;i++) AddKeyButtonEvent(PX(  i),PY(1),BW,BH,combo_1[i].title,combo_1[i].entry,combo_1[i].key);

	AddKeyButtonEvent(PX(0),PY(2),BW*2,BH,"TAB","tab",KBD_tab);
	for (i=0;i<12;i++) AddKeyButtonEvent(PX(2+i),PY(2),BW,BH,combo_2[i].title,combo_2[i].entry,combo_2[i].key);

	AddKeyButtonEvent(PX(14),PY(2),BW*2,BH*2,"ENTER","enter",KBD_enter);
	
	caps_lock_event=AddKeyButtonEvent(PX(0),PY(3),BW*2,BH,"CLCK","capslock",KBD_capslock);
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
#define XO 17
#define YO 0

	AddKeyButtonEvent(PX(XO+0),PY(YO),BW,BH,"PRT","printscreen",KBD_printscreen);
	AddKeyButtonEvent(PX(XO+1),PY(YO),BW,BH,"SCL","scrolllock",KBD_scrolllock);
	AddKeyButtonEvent(PX(XO+2),PY(YO),BW,BH,"PAU","pause",KBD_pause);
	AddKeyButtonEvent(PX(XO+0),PY(YO+1),BW,BH,"INS","insert",KBD_insert);
	AddKeyButtonEvent(PX(XO+1),PY(YO+1),BW,BH,"HOM","home",KBD_home);
	AddKeyButtonEvent(PX(XO+2),PY(YO+1),BW,BH,"PUP","pageup",KBD_pageup);
	AddKeyButtonEvent(PX(XO+0),PY(YO+2),BW,BH,"DEL","delete",KBD_delete);
	AddKeyButtonEvent(PX(XO+1),PY(YO+2),BW,BH,"END","end",KBD_end);
	AddKeyButtonEvent(PX(XO+2),PY(YO+2),BW,BH,"PDN","pagedown",KBD_pagedown);
	AddKeyButtonEvent(PX(XO+1),PY(YO+4),BW,BH,"\x18","up",KBD_up);
	AddKeyButtonEvent(PX(XO+0),PY(YO+5),BW,BH,"\x1B","left",KBD_left);
	AddKeyButtonEvent(PX(XO+1),PY(YO+5),BW,BH,"\x19","down",KBD_down);
	AddKeyButtonEvent(PX(XO+2),PY(YO+5),BW,BH,"\x1A","right",KBD_right);
#undef XO
#undef YO
#define XO 0
#define YO 7
	/* Numeric KeyPad */
	num_lock_event=AddKeyButtonEvent(PX(XO),PY(YO),BW,BH,"NUM","numlock",KBD_numlock);
	AddKeyButtonEvent(PX(XO+1),PY(YO),BW,BH,"/","kp_divide",KBD_kpdivide);
	AddKeyButtonEvent(PX(XO+2),PY(YO),BW,BH,"*","kp_multiply",KBD_kpmultiply);
	AddKeyButtonEvent(PX(XO+3),PY(YO),BW,BH,"-","kp_minus",KBD_kpminus);
	AddKeyButtonEvent(PX(XO+0),PY(YO+1),BW,BH,"7","kp_7",KBD_kp7);
	AddKeyButtonEvent(PX(XO+1),PY(YO+1),BW,BH,"8","kp_8",KBD_kp8);
	AddKeyButtonEvent(PX(XO+2),PY(YO+1),BW,BH,"9","kp_9",KBD_kp9);
	AddKeyButtonEvent(PX(XO+3),PY(YO+1),BW,BH*2,"+","kp_plus",KBD_kpplus);
	AddKeyButtonEvent(PX(XO),PY(YO+2),BW,BH,"4","kp_4",KBD_kp4);
	AddKeyButtonEvent(PX(XO+1),PY(YO+2),BW,BH,"5","kp_5",KBD_kp5);
	AddKeyButtonEvent(PX(XO+2),PY(YO+2),BW,BH,"6","kp_6",KBD_kp6);
	AddKeyButtonEvent(PX(XO+0),PY(YO+3),BW,BH,"1","kp_1",KBD_kp1);
	AddKeyButtonEvent(PX(XO+1),PY(YO+3),BW,BH,"2","kp_2",KBD_kp2);
	AddKeyButtonEvent(PX(XO+2),PY(YO+3),BW,BH,"3","kp_3",KBD_kp3);
	AddKeyButtonEvent(PX(XO+3),PY(YO+3),BW,BH*2,"ENT","kp_enter",KBD_kpenter);
	AddKeyButtonEvent(PX(XO),PY(YO+4),BW*2,BH,"0","kp_0",KBD_kp0);
	AddKeyButtonEvent(PX(XO+2),PY(YO+4),BW,BH,".","kp_period",KBD_kpperiod);
#undef XO
#undef YO
#define XO 10
#define YO 8
	/* Joystick Buttons/Texts */
	/* Buttons 1+2 of 1st Joystick */
	AddJButtonButton(PX(XO),PY(YO),BW,BH,"1" ,0,0);
	AddJButtonButton(PX(XO+2),PY(YO),BW,BH,"2" ,0,1);
	/* Axes 1+2 (X+Y) of 1st Joystick */
	CJAxisEvent * cjaxis=AddJAxisButton(PX(XO+1),PY(YO),BW,BH,"Y-",0,1,false,NULL);
	AddJAxisButton  (PX(XO+1),PY(YO+1),BW,BH,"Y+",0,1,true,cjaxis);
	cjaxis=AddJAxisButton  (PX(XO),PY(YO+1),BW,BH,"X-",0,0,false,NULL);
	AddJAxisButton  (PX(XO+2),PY(YO+1),BW,BH,"X+",0,0,true,cjaxis);

	CJAxisEvent * tmp_ptr;
	if (joytype==JOY_2AXIS) {
		/* Buttons 1+2 of 2nd Joystick */
		AddJButtonButton(PX(XO+4),PY(YO),BW,BH,"1" ,1,0);
		AddJButtonButton(PX(XO+4+2),PY(YO),BW,BH,"2" ,1,1);
		/* Buttons 3+4 of 1st Joystick, not accessible */
		AddJButtonButton_hidden(0,2);
		AddJButtonButton_hidden(0,3);

		/* Axes 1+2 (X+Y) of 2nd Joystick */
		cjaxis  = AddJAxisButton(PX(XO+4),PY(YO+1),BW,BH,"X-",1,0,false,NULL);
		tmp_ptr = AddJAxisButton(PX(XO+4+2),PY(YO+1),BW,BH,"X+",1,0,true,cjaxis);
		(void)tmp_ptr;
		cjaxis  = AddJAxisButton(PX(XO+4+1),PY(YO+0),BW,BH,"Y-",1,1,false,NULL);
		tmp_ptr = AddJAxisButton(PX(XO+4+1),PY(YO+1),BW,BH,"Y+",1,1,true,cjaxis);
		(void)tmp_ptr;
		/* Axes 3+4 (X+Y) of 1st Joystick, not accessible */
		cjaxis  = AddJAxisButton_hidden(0,2,false,NULL);
		tmp_ptr = AddJAxisButton_hidden(0,2,true,cjaxis);
		(void)tmp_ptr;
		cjaxis  = AddJAxisButton_hidden(0,3,false,NULL);
		tmp_ptr = AddJAxisButton_hidden(0,3,true,cjaxis);
		(void)tmp_ptr;
	} else {
		/* Buttons 3+4 of 1st Joystick */
		AddJButtonButton(PX(XO+4),PY(YO),BW,BH,"3" ,0,2);
		AddJButtonButton(PX(XO+4+2),PY(YO),BW,BH,"4" ,0,3);
		/* Buttons 1+2 of 2nd Joystick, not accessible */
		AddJButtonButton_hidden(1,0);
		AddJButtonButton_hidden(1,1);

		/* Axes 3+4 (X+Y) of 1st Joystick */
		cjaxis  = AddJAxisButton(PX(XO+4),PY(YO+1),BW,BH,"X-",0,2,false,NULL);
		tmp_ptr = AddJAxisButton(PX(XO+4+2),PY(YO+1),BW,BH,"X+",0,2,true,cjaxis);
		(void)tmp_ptr;
		cjaxis  = AddJAxisButton(PX(XO+4+1),PY(YO+0),BW,BH,"Y-",0,3,false,NULL);
		tmp_ptr = AddJAxisButton(PX(XO+4+1),PY(YO+1),BW,BH,"Y+",0,3,true,cjaxis);
		(void)tmp_ptr;
		/* Axes 1+2 (X+Y) of 2nd Joystick , not accessible*/
		cjaxis  = AddJAxisButton_hidden(1,0,false,NULL);
		tmp_ptr = AddJAxisButton_hidden(1,0,true,cjaxis);
		(void)tmp_ptr;
		cjaxis  = AddJAxisButton_hidden(1,1,false,NULL);
		tmp_ptr = AddJAxisButton_hidden(1,1,true,cjaxis);
		(void)tmp_ptr;
	}

	if (joytype==JOY_CH) {
		/* Buttons 5+6 of 1st Joystick */
		AddJButtonButton(PX(XO+8),PY(YO),BW,BH,"5" ,0,4);
		AddJButtonButton(PX(XO+8+2),PY(YO),BW,BH,"6" ,0,5);
	} else {
		/* Buttons 5+6 of 1st Joystick, not accessible */
		AddJButtonButton_hidden(0,4);
		AddJButtonButton_hidden(0,5);
	}

	/* Hat directions up, left, down, right */
	AddJHatButton(PX(XO+8+1),PY(YO),BW,BH,"UP",0,0,0);
	AddJHatButton(PX(XO+8+0),PY(YO+1),BW,BH,"LFT",0,0,3);
	AddJHatButton(PX(XO+8+1),PY(YO+1),BW,BH,"DWN",0,0,2);
	AddJHatButton(PX(XO+8+2),PY(YO+1),BW,BH,"RGT",0,0,1);

	/* Labels for the joystick */
	CTextButton * btn;
	if (joytype ==JOY_2AXIS) {
		new CTextButton(PX(XO+0),PY(YO-1),3*BW,20,"Joystick 1");
		new CTextButton(PX(XO+4),PY(YO-1),3*BW,20,"Joystick 2");
		btn=new CTextButton(PX(XO+8),PY(YO-1),3*BW,20,"Disabled");
		btn->SetColor(CLR_GREY);
	} else if(joytype ==JOY_4AXIS || joytype == JOY_4AXIS_2) {
		new CTextButton(PX(XO+0),PY(YO-1),3*BW,20,"Axis 1/2");
		new CTextButton(PX(XO+4),PY(YO-1),3*BW,20,"Axis 3/4");
		btn=new CTextButton(PX(XO+8),PY(YO-1),3*BW,20,"Disabled");
		btn->SetColor(CLR_GREY);
	} else if(joytype == JOY_CH) {
		new CTextButton(PX(XO+0),PY(YO-1),3*BW,20,"Axis 1/2");
		new CTextButton(PX(XO+4),PY(YO-1),3*BW,20,"Axis 3/4");
		new CTextButton(PX(XO+8),PY(YO-1),3*BW,20,"Hat/D-pad");
	} else if ( joytype==JOY_FCS) {
		new CTextButton(PX(XO+0),PY(YO-1),3*BW,20,"Axis 1/2");
		new CTextButton(PX(XO+4),PY(YO-1),3*BW,20,"Axis 3");
		new CTextButton(PX(XO+8),PY(YO-1),3*BW,20,"Hat/D-pad");
	} else if(joytype == JOY_NONE) {
		btn=new CTextButton(PX(XO+0),PY(YO-1),3*BW,20,"Disabled");
		btn->SetColor(CLR_GREY);
		btn=new CTextButton(PX(XO+4),PY(YO-1),3*BW,20,"Disabled");
		btn->SetColor(CLR_GREY);
		btn=new CTextButton(PX(XO+8),PY(YO-1),3*BW,20,"Disabled");
		btn->SetColor(CLR_GREY);
	}
   
   
   
	/* The modifier buttons */
	AddModButton(PX(0),PY(14),50,20,"Mod1",1);
	AddModButton(PX(2),PY(14),50,20,"Mod2",2);
	AddModButton(PX(4),PY(14),50,20,"Mod3",3);
	/* Create Handler buttons */
	Bitu xpos=3;Bitu ypos=11;
	for (CHandlerEventVector_it hit = handlergroup.begin(); hit != handlergroup.end(); ++hit) {
		new CEventButton(PX(xpos*3),PY(ypos),BW*3,BH,(*hit)->ButtonName(),(*hit));
		xpos++;
		if (xpos>6) {
			xpos=3;ypos++;
		}
	}
	/* Create some text buttons */
//	new CTextButton(PX(6),0,124,20,"Keyboard Layout");
//	new CTextButton(PX(17),0,124,20,"Joystick Layout");

	bind_but.action=new CCaptionButton(180,350,0,0);

	bind_but.event_title=new CCaptionButton(0,350,0,0);
	bind_but.bind_title=new CCaptionButton(0,365,0,0);

	/* Create binding support buttons */
	
	bind_but.mod1=new CCheckButton(20,410,60,20, "mod1",BC_Mod1);
	bind_but.mod2=new CCheckButton(20,432,60,20, "mod2",BC_Mod2);
	bind_but.mod3=new CCheckButton(20,454,60,20, "mod3",BC_Mod3);
	bind_but.hold=new CCheckButton(100,410,60,20,"hold",BC_Hold);

	bind_but.next=new CBindButton(250,400,50,20,"Next",BB_Next);

	bind_but.add=new CBindButton(250,380,50,20,"Add",BB_Add);
	bind_but.del=new CBindButton(300,380,50,20,"Del",BB_Del);

	bind_but.save=new CBindButton(400,450,50,20,"Save",BB_Save);
	bind_but.exit=new CBindButton(450,450,50,20,"Exit",BB_Exit);

	bind_but.bind_title->Change("Bind Title");
}

static SDL_Color map_pal[CLR_LAST]={
	{0x00,0x00,0x00,0x00},			//0=black
	{0x7f,0x7f,0x7f,0x00},			//1=grey
	{0xff,0xff,0xff,0x00},			//2=white
	{0xff,0x00,0x00,0x00},			//3=red
	{0x10,0x30,0xff,0x00},			//4=blue
	{0x00,0xff,0x20,0x00}			//5=green
};

static void CreateStringBind(char * line) {
	line=trim(line);
	char * eventname=StripWord(line);
	CEvent * event = nullptr;
	for (CEventVector_it ev_it = events.begin(); ev_it != events.end(); ++ev_it) {
		if (!strcasecmp((*ev_it)->GetName(),eventname)) {
			event=*ev_it;
			goto foundevent;
		}
	}
	LOG_MSG("MAPPER: Can't find key binding for %s event", eventname);
	return ;
foundevent:
	CBind * bind = nullptr;
	for (char * bindline=StripWord(line);*bindline;bindline=StripWord(line)) {
		for (CBindGroup_it it = bindgroups.begin(); it != bindgroups.end(); ++it) {
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
	const char *eventend;
	SDL_Scancode key;
} DefaultKeys[] = {
	{"f1",  SDL_SCANCODE_F1},  {"f2", SDL_SCANCODE_F2},  {"f3",  SDL_SCANCODE_F3},
	{"f4",  SDL_SCANCODE_F4},  {"f5", SDL_SCANCODE_F5},  {"f6",  SDL_SCANCODE_F6},
	{"f7",  SDL_SCANCODE_F7},  {"f8", SDL_SCANCODE_F8},  {"f9",  SDL_SCANCODE_F9},
	{"f10", SDL_SCANCODE_F10}, {"f11",SDL_SCANCODE_F11}, {"f12", SDL_SCANCODE_F12},

	{"1", SDL_SCANCODE_1}, {"2", SDL_SCANCODE_2}, {"3", SDL_SCANCODE_3},
	{"4", SDL_SCANCODE_4}, {"5", SDL_SCANCODE_5}, {"6", SDL_SCANCODE_6},
	{"7", SDL_SCANCODE_7}, {"8", SDL_SCANCODE_8}, {"9", SDL_SCANCODE_9},
	{"0", SDL_SCANCODE_0},

	{"a", SDL_SCANCODE_A}, {"b", SDL_SCANCODE_B}, {"c", SDL_SCANCODE_C},
	{"d", SDL_SCANCODE_D}, {"e", SDL_SCANCODE_E}, {"f", SDL_SCANCODE_F},
	{"g", SDL_SCANCODE_G}, {"h", SDL_SCANCODE_H}, {"i", SDL_SCANCODE_I},
	{"j", SDL_SCANCODE_J}, {"k", SDL_SCANCODE_K}, {"l", SDL_SCANCODE_L},
	{"m", SDL_SCANCODE_M}, {"n", SDL_SCANCODE_N}, {"o", SDL_SCANCODE_O},
	{"p", SDL_SCANCODE_P}, {"q", SDL_SCANCODE_Q}, {"r", SDL_SCANCODE_R},
	{"s", SDL_SCANCODE_S}, {"t", SDL_SCANCODE_T}, {"u", SDL_SCANCODE_U},
	{"v", SDL_SCANCODE_V}, {"w", SDL_SCANCODE_W}, {"x", SDL_SCANCODE_X},
	{"y", SDL_SCANCODE_Y}, {"z", SDL_SCANCODE_Z},

	{"space",       SDL_SCANCODE_SPACE},
	{"esc",         SDL_SCANCODE_ESCAPE},
	{"equals",      SDL_SCANCODE_EQUALS},
	{"grave",       SDL_SCANCODE_GRAVE},
	{"tab",         SDL_SCANCODE_TAB},
	{"enter",       SDL_SCANCODE_RETURN},
	{"bspace",      SDL_SCANCODE_BACKSPACE},
	{"lbracket",    SDL_SCANCODE_LEFTBRACKET},
	{"rbracket",    SDL_SCANCODE_RIGHTBRACKET},
	{"minus",       SDL_SCANCODE_MINUS},
	{"capslock",    SDL_SCANCODE_CAPSLOCK},
	{"semicolon",   SDL_SCANCODE_SEMICOLON},
	{"quote",       SDL_SCANCODE_APOSTROPHE},
	{"backslash",   SDL_SCANCODE_BACKSLASH},
	{"lshift",      SDL_SCANCODE_LSHIFT},
	{"rshift",      SDL_SCANCODE_RSHIFT},
	{"lalt",        SDL_SCANCODE_LALT},
	{"ralt",        SDL_SCANCODE_RALT},
	{"lctrl",       SDL_SCANCODE_LCTRL},
	{"rctrl",       SDL_SCANCODE_RCTRL},
	{"comma",       SDL_SCANCODE_COMMA},
	{"period",      SDL_SCANCODE_PERIOD},
	{"slash",       SDL_SCANCODE_SLASH},
	{"printscreen", SDL_SCANCODE_PRINTSCREEN},
	{"scrolllock",  SDL_SCANCODE_SCROLLLOCK},
	{"pause",       SDL_SCANCODE_PAUSE},
	{"pagedown",    SDL_SCANCODE_PAGEDOWN},
	{"pageup",      SDL_SCANCODE_PAGEUP},
	{"insert",      SDL_SCANCODE_INSERT},
	{"home",        SDL_SCANCODE_HOME},
	{"delete",      SDL_SCANCODE_DELETE},
	{"end",         SDL_SCANCODE_END},
	{"up",          SDL_SCANCODE_UP},
	{"left",        SDL_SCANCODE_LEFT},
	{"down",        SDL_SCANCODE_DOWN},
	{"right",       SDL_SCANCODE_RIGHT},

	{"kp_1", SDL_SCANCODE_KP_1}, {"kp_2", SDL_SCANCODE_KP_2}, {"kp_3", SDL_SCANCODE_KP_3},
	{"kp_4", SDL_SCANCODE_KP_4}, {"kp_5", SDL_SCANCODE_KP_5}, {"kp_6", SDL_SCANCODE_KP_6},
	{"kp_7", SDL_SCANCODE_KP_7}, {"kp_8", SDL_SCANCODE_KP_8}, {"kp_9", SDL_SCANCODE_KP_9},
	{"kp_0", SDL_SCANCODE_KP_0},

	{"numlock",     SDL_SCANCODE_NUMLOCKCLEAR},
	{"kp_divide",   SDL_SCANCODE_KP_DIVIDE},
	{"kp_multiply", SDL_SCANCODE_KP_MULTIPLY},
	{"kp_minus",    SDL_SCANCODE_KP_MINUS},
	{"kp_plus",     SDL_SCANCODE_KP_PLUS},
	{"kp_period",   SDL_SCANCODE_KP_PERIOD},
	{"kp_enter",    SDL_SCANCODE_KP_ENTER},

	/* Is that the extra backslash key ("less than" key) */
	/* on some keyboards with the 102-keys layout??      */
	{"lessthan",SDL_SCANCODE_NONUSBACKSLASH},

	{0, SDL_SCANCODE_UNKNOWN}
};

static void ClearAllBinds() {
	// wait for the auto-typer to complete because it might be accessing events
	mapper.typist.Wait();

	for (CEvent *event : events) {
		event->ClearBinds();
	}
}

static void CreateDefaultBinds() {
	ClearAllBinds();
	char buffer[512];
	Bitu i=0;
	while (DefaultKeys[i].eventend) {
		sprintf(buffer, "key_%s \"key %d\"",
		        DefaultKeys[i].eventend,
		        static_cast<int>(DefaultKeys[i].key));
		CreateStringBind(buffer);
		i++;
	}
	sprintf(buffer, "mod_1 \"key %d\"", SDL_SCANCODE_RCTRL);
	CreateStringBind(buffer);
	sprintf(buffer, "mod_1 \"key %d\"", SDL_SCANCODE_LCTRL);
	CreateStringBind(buffer);
	sprintf(buffer, "mod_2 \"key %d\"", SDL_SCANCODE_RALT);
	CreateStringBind(buffer);
	sprintf(buffer, "mod_2 \"key %d\"", SDL_SCANCODE_LALT);
	CreateStringBind(buffer);
	for (CHandlerEventVector_it hit = handlergroup.begin(); hit != handlergroup.end(); ++hit) {
		(*hit)->MakeDefaultBind(buffer);
		CreateStringBind(buffer);
	}

	/* joystick1, buttons 1-6 */
	sprintf(buffer,"jbutton_0_0 \"stick_0 button 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jbutton_0_1 \"stick_0 button 1\" ");CreateStringBind(buffer);
	sprintf(buffer,"jbutton_0_2 \"stick_0 button 2\" ");CreateStringBind(buffer);
	sprintf(buffer,"jbutton_0_3 \"stick_0 button 3\" ");CreateStringBind(buffer);
	sprintf(buffer,"jbutton_0_4 \"stick_0 button 4\" ");CreateStringBind(buffer);
	sprintf(buffer,"jbutton_0_5 \"stick_0 button 5\" ");CreateStringBind(buffer);
	/* joystick2, buttons 1-2 */
	sprintf(buffer,"jbutton_1_0 \"stick_1 button 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jbutton_1_1 \"stick_1 button 1\" ");CreateStringBind(buffer);

	/* joystick1, axes 1-4 */
	sprintf(buffer,"jaxis_0_0- \"stick_0 axis 0 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_0+ \"stick_0 axis 0 1\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_1- \"stick_0 axis 1 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_1+ \"stick_0 axis 1 1\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_2- \"stick_0 axis 2 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_2+ \"stick_0 axis 2 1\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_3- \"stick_0 axis 3 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_0_3+ \"stick_0 axis 3 1\" ");CreateStringBind(buffer);
	/* joystick2, axes 1-2 */
	sprintf(buffer,"jaxis_1_0- \"stick_1 axis 0 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_1_0+ \"stick_1 axis 0 1\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_1_1- \"stick_1 axis 1 0\" ");CreateStringBind(buffer);
	sprintf(buffer,"jaxis_1_1+ \"stick_1 axis 1 1\" ");CreateStringBind(buffer);

	/* joystick1, hat */
	sprintf(buffer,"jhat_0_0_0 \"stick_0 hat 0 1\" ");CreateStringBind(buffer);
	sprintf(buffer,"jhat_0_0_1 \"stick_0 hat 0 2\" ");CreateStringBind(buffer);
	sprintf(buffer,"jhat_0_0_2 \"stick_0 hat 0 4\" ");CreateStringBind(buffer);
	sprintf(buffer,"jhat_0_0_3 \"stick_0 hat 0 8\" ");CreateStringBind(buffer);
	LOG_MSG("MAPPER: Loaded default key bindings");
}

void MAPPER_AddHandler(MAPPER_Handler * handler,MapKeys key, Bitu mods,char const * const eventname,char const * const buttonname) {
	//Check if it already exists=> if so return.
	for(CHandlerEventVector_it it=handlergroup.begin(); it != handlergroup.end(); ++it)
		if(strcmp((*it)->buttonname,buttonname) == 0) return;

	char tempname[17];
	safe_strcpy(tempname, "hand_");
	safe_strcat(tempname, eventname);
	new CHandlerEvent(tempname,handler,key,mods,buttonname);
	return ;
}

static void MAPPER_SaveBinds() {
	const char *filename = mapper.filename.c_str();
	FILE * savefile=fopen(filename,"wt+");
	if (!savefile) {
		LOG_MSG("MAPPER: Can't open %s for saving the key bindings", filename);
		return;
	}
	char buf[128];
	for (CEventVector_it event_it = events.begin(); event_it != events.end(); ++event_it) {
		CEvent * event=*(event_it);
		fprintf(savefile,"%s ",event->GetName());
		for (CBindList_it bind_it = event->bindlist.begin(); bind_it != event->bindlist.end(); ++bind_it) {
			CBind * bind=*(bind_it);
			bind->ConfigName(buf);
			bind->AddFlags(buf);
			fprintf(savefile,"\"%s\" ",buf);
		}
		fprintf(savefile,"\n");
	}
	fclose(savefile);
	change_action_text("Mapper file saved.",CLR_WHITE);
	LOG_MSG("MAPPER: Wrote key bindings to %s", filename);
}

static bool MAPPER_CreateBindsFromFile() {
	const char* filename = mapper.filename.c_str();
	FILE * loadfile = fopen(filename,"rt");
	if (!loadfile)
		return false;
	ClearAllBinds();
	uint32_t bind_tally = 0;
	char linein[512];
	while (fgets(linein,512,loadfile)) {
		CreateStringBind(linein);
		++bind_tally;
	}
	fclose(loadfile);
	LOG_MSG("MAPPER: Loaded %d key bindings from %s", bind_tally, filename);
	return true;
}

void MAPPER_CheckEvent(SDL_Event * event) {
	for (CBindGroup_it it = bindgroups.begin(); it != bindgroups.end(); ++it) {
		if ((*it)->CheckEvent(event)) return;
	}
}

void BIND_MappingEvents() {
	SDL_Event event;
	static bool isButtonPressed = false;
	static CButton *lastHoveredButton = NULL;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_MOUSEBUTTONDOWN:
			isButtonPressed = true;
			/* Further check where are we pointing at right now */
		case SDL_MOUSEMOTION:
			if (!isButtonPressed)
				break;
			/* Normalize position in case a scaled sub-window is used (say on Android) */
			event.button.x = (event.button.x - mapper.draw_rect.x) * mapper.draw_surface->w / mapper.draw_rect.w;
			if ((event.button.x < 0) || (event.button.x >= mapper.draw_surface->w))
				break;
			event.button.y = (event.button.y - mapper.draw_rect.y) * mapper.draw_surface->h / mapper.draw_rect.h;
			if ((event.button.y < 0) || (event.button.y >= mapper.draw_surface->h))
				break;
			/* Maybe we have been pointing at a specific button for a little while  */
			if (lastHoveredButton) {
				/* Check if there's any change */
				if (lastHoveredButton->OnTop(event.button.x,event.button.y))
					break;
				/* Not pointing at given button anymore */
				if (lastHoveredButton == last_clicked)
					lastHoveredButton->Click();
				else
					lastHoveredButton->BindColor();
				mapper.redraw = true;
				lastHoveredButton = nullptr;
			}
			/* Check which button are we currently pointing at */
			for (CButton_it but_it = buttons.begin(); but_it != buttons.end(); ++but_it) {
				if (dynamic_cast<CClickableTextButton *>(*but_it) && (*but_it)->OnTop(event.button.x,event.button.y)) {
					(*but_it)->SetColor(CLR_RED);
					mapper.redraw = true;
					lastHoveredButton = *but_it;
					break;
				}
			}
			break;
		case SDL_MOUSEBUTTONUP:
			isButtonPressed = false;
			if (lastHoveredButton) {
				/* For most buttons the actual new color is going to be green; But not for a few others. */
				lastHoveredButton->BindColor();
				mapper.redraw = true;
				lastHoveredButton = NULL;
			}
			/* Normalize position in case a scaled sub-window is used (say on Android) */
			event.button.x = (event.button.x - mapper.draw_rect.x) * mapper.draw_surface->w / mapper.draw_rect.w;
			if ((event.button.x < 0) || (event.button.x>=mapper.draw_surface->w))
				break;
			event.button.y = (event.button.y - mapper.draw_rect.y) * mapper.draw_surface->h / mapper.draw_rect.h;
			if ((event.button.y < 0) || (event.button.y>=mapper.draw_surface->h))
				break;
			/* Check the press */
			for (CButton_it but_it = buttons.begin(); but_it != buttons.end(); ++but_it) {
				if (dynamic_cast<CClickableTextButton *>(*but_it) && (*but_it)->OnTop(event.button.x,event.button.y)) {
					(*but_it)->Click();
					break;
				}
			}
			break;
		case SDL_WINDOWEVENT:
			/* The resize event MAY arrive e.g. when the mapper is
			 * toggled, at least on X11. Furthermore, the restore
			 * event should be handled on Android.
			 */
			if ((event.window.event == SDL_WINDOWEVENT_RESIZED)
			    || (event.window.event == SDL_WINDOWEVENT_RESTORED)) {
				mapper.surface = SDL_GetWindowSurface(mapper.window);
				if (mapper.surface == nullptr)
					E_Exit("Couldn't refresh mapper window surface after resize or restoration: %s", SDL_GetError());
				GFX_UpdateDisplayDimensions(event.window.data1, event.window.data2);
				mapper.draw_rect = GFX_GetSDLSurfaceSubwindowDims(640, 480);
				DrawButtons();
			}
			break;
		case SDL_QUIT:
			isButtonPressed = false;
			lastHoveredButton = NULL;
			mapper.exit=true;
			break;
		default:
			if (mapper.addbind) for (CBindGroup_it it = bindgroups.begin(); it != bindgroups.end(); ++it) {
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

/**
 *  Queries SDL's joysticks and sets joytype accordingly.
 *  If no joysticks are valid then joytype is left at JOY_NONE.
 *  Also resets mapper.sticks.num_groups to 0, mapper.sticks.num
 *  to the number of found SDL joysticks, and enables the boolean
 *  joysticks_active if joystick support is enabled and are present.
 */ 
static void QueryJoysticks() {
	// Initialize SDL's Joystick and Event subsystems, if needed
	if (SDL_WasInit(SDL_INIT_JOYSTICK) != SDL_INIT_JOYSTICK)
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);

	// Record how many joysticks are present and set our desired minimum axis
	const int num_joysticks = SDL_NumJoysticks();
	const int req_min_axis = num_joysticks > 1 ? 2 : 1;

	// Check which, if any, of the first two joysticks are useable
	bool useable[2] = {false};
	for (int i = 0; i < std::min(num_joysticks, 2); ++i) {
		SDL_Joystick *stick = SDL_JoystickOpen(i);
		useable[i] = (SDL_JoystickNumAxes(stick) >= req_min_axis) ||
		             (SDL_JoystickNumButtons(stick) > 0) ? true : false;
		SDL_JoystickClose(stick);
	}

	// Set the type of joystick based which are useable
	const bool first_usable = useable[0];
	const bool second_usable = useable[1];	
	if (first_usable && second_usable) {
		joytype = JOY_2AXIS;
		LOG_MSG("MAPPER: Found two or more joysticks");
	} else if (first_usable) {
		joytype = JOY_4AXIS;
		LOG_MSG("MAPPER: Found one joystick");
	} else if (second_usable) {
		joytype = JOY_4AXIS_2;
		LOG_MSG("MAPPER: Found second joystick is usable");
	} else {
		joytype = JOY_NONE;
		LOG_MSG("MAPPER: Found no joysticks");
	}

	// If we made it here, then update the other two external variables
	mapper.sticks.num_groups = 0;
	mapper.sticks.num = num_joysticks;
}

static void CreateBindGroups() {
	bindgroups.clear();
	CKeyBindGroup* key_bind_group = new CKeyBindGroup(SDL_NUM_SCANCODES);
	keybindgroups.push_back(key_bind_group);

	if (joytype != JOY_NONE) {
#if defined (REDUCE_JOYSTICK_POLLING)
		// direct access to the SDL joystick, thus removed from the event handling
		if (mapper.sticks.num) SDL_JoystickEventState(SDL_DISABLE);
#else
		// enable joystick event handling
		if (mapper.sticks.num) SDL_JoystickEventState(SDL_ENABLE);
		else return;
#endif
		// Free up our previously assigned joystick slot before assinging below
		if (mapper.sticks.stick[mapper.sticks.num_groups]) {
			delete mapper.sticks.stick[mapper.sticks.num_groups];
			mapper.sticks.stick[mapper.sticks.num_groups] = nullptr;
		}

		Bit8u joyno=0;
		switch (joytype) {
		case JOY_NONE:
			break;
		case JOY_4AXIS:
			mapper.sticks.stick[mapper.sticks.num_groups++]=new C4AxisBindGroup(joyno,joyno);
			stickbindgroups.push_back(new CStickBindGroup(joyno+1U,joyno+1U,true));
			break;
		case JOY_4AXIS_2:
			mapper.sticks.stick[mapper.sticks.num_groups++]=new C4AxisBindGroup(joyno+1U,joyno);
			stickbindgroups.push_back(new CStickBindGroup(joyno,joyno+1U,true));
			break;
		case JOY_FCS:
			mapper.sticks.stick[mapper.sticks.num_groups++]=new CFCSBindGroup(joyno,joyno);
			stickbindgroups.push_back(new CStickBindGroup(joyno+1U,joyno+1U,true));
			break;
		case JOY_CH:
			mapper.sticks.stick[mapper.sticks.num_groups++]=new CCHBindGroup(joyno,joyno);
			stickbindgroups.push_back(new CStickBindGroup(joyno+1U,joyno+1U,true));
			break;
		case JOY_2AXIS:
		default:
			mapper.sticks.stick[mapper.sticks.num_groups++]=new CStickBindGroup(joyno,joyno);
			if((joyno+1U) < mapper.sticks.num) {
				delete mapper.sticks.stick[mapper.sticks.num_groups];
				mapper.sticks.stick[mapper.sticks.num_groups++]=new CStickBindGroup(joyno+1U,joyno+1U);
			} else {
				stickbindgroups.push_back(new CStickBindGroup(joyno+1U,joyno+1U,true));
			}
			break;
		}
	}
}

bool MAPPER_IsUsingJoysticks() {
	return (mapper.sticks.num > 0);
}

#if defined (REDUCE_JOYSTICK_POLLING)
void MAPPER_UpdateJoysticks() {
	for (Bitu i=0; i<mapper.sticks.num_groups; i++) {
		assert(mapper.sticks.stick[i]);
		mapper.sticks.stick[i]->UpdateJoystick();
	}
}
#endif

void MAPPER_LosingFocus() {
	for (CEventVector_it evit = events.begin(); evit != events.end(); ++evit) {
		if(*evit != caps_lock_event && *evit != num_lock_event)
			(*evit)->DeActivateAll();
	}
}

void MAPPER_RunEvent(Bitu /*val*/) {
	KEYBOARD_ClrBuffer();	//Clear buffer
	GFX_LosingFocus();		//Release any keys pressed (buffer gets filled again).
	MAPPER_DisplayUI();
}

void MAPPER_Run(bool pressed) {
	if (pressed)
		return;
	PIC_AddEvent(MAPPER_RunEvent,0);	//In case mapper deletes the key object that ran it
}

SDL_Surface* SDL_SetVideoMode_Wrap(int width,int height,int bpp,Bit32u flags);

void MAPPER_DisplayUI() {
	int cursor = SDL_ShowCursor(SDL_QUERY);
	SDL_ShowCursor(SDL_ENABLE);
	bool mousetoggle = false;
	if (mouse_is_captured) {
		mousetoggle = true;
		GFX_ToggleMouseCapture();
	}

	/* Be sure that there is no update in progress */
	GFX_EndUpdate( 0 );
	mapper.window = GFX_SetSDLSurfaceWindow(640, 480);
	if (mapper.window == nullptr)
		E_Exit("Could not initialize video mode for mapper: %s", SDL_GetError());
	mapper.surface = SDL_GetWindowSurface(mapper.window);
	if (mapper.surface == nullptr)
		E_Exit("Could not retrieve window surface for mapper: %s", SDL_GetError());

	/* Set some palette entries */
	mapper.draw_surface=SDL_CreateRGBSurface(0,640,480,8,0,0,0,0);
	// Needed for SDL_BlitScaled
	mapper.draw_surface_nonpaletted=SDL_CreateRGBSurface(0,640,480,32,0x0000ff00,0x00ff0000,0xff000000,0);
	mapper.draw_rect=GFX_GetSDLSurfaceSubwindowDims(640,480);
	// Sorry, but SDL_SetSurfacePalette requires a full palette.
	SDL_Palette *sdl2_map_pal_ptr = SDL_AllocPalette(256);
	SDL_SetPaletteColors(sdl2_map_pal_ptr, map_pal, 0, CLR_LAST);
	SDL_SetSurfacePalette(mapper.draw_surface, sdl2_map_pal_ptr);
	if (last_clicked) {
		last_clicked->BindColor();
		last_clicked=NULL;
	}
	/* Go in the event loop */
	mapper.exit=false;	
	mapper.redraw=true;
	SetActiveEvent(0);
#if defined (REDUCE_JOYSTICK_POLLING)
	SDL_JoystickEventState(SDL_ENABLE);
#endif
	while (!mapper.exit) {
		if (mapper.redraw) {
			mapper.redraw=false;		
			DrawButtons();
		} else {
			SDL_UpdateWindowSurface(mapper.window);
		}
		BIND_MappingEvents();
		SDL_Delay(1);
	}
	/* ONE SHOULD NOT FORGET TO DO THIS!
	Unless a memory leak is desired... */
	SDL_FreeSurface(mapper.draw_surface);
	SDL_FreeSurface(mapper.draw_surface_nonpaletted);
	SDL_FreePalette(sdl2_map_pal_ptr);
#if defined (REDUCE_JOYSTICK_POLLING)
	SDL_JoystickEventState(SDL_DISABLE);
#endif
	if (mousetoggle)
		GFX_ToggleMouseCapture();
	SDL_ShowCursor(cursor);
	GFX_ResetScreen();
}

static void MAPPER_Destroy(Section *sec) {
	(void) sec; // unused but present for API compliance

	// Stop any ongoing typing as soon as possible (because it access events)
	mapper.typist.Stop();

	// Release all the accumulated allocations by the mapper 
 	for (auto & ptr : events)
		delete ptr;
	events.clear();

	for (auto & ptr : all_binds)
		delete ptr;
	all_binds.clear();

	for (auto & ptr : buttons)
		delete ptr;
	buttons.clear();

	for (auto & ptr : keybindgroups)
		delete ptr;
	keybindgroups.clear();

	for (auto & ptr : stickbindgroups)
		delete ptr;
	stickbindgroups.clear();

	// Free any allocated sticks
	for (int i = 0; i < MAXSTICKS; ++i) {
		delete mapper.sticks.stick[i];
		mapper.sticks.stick[i] = nullptr;
	}

	// Empty the remaining lists now that their pointers are defunct
	bindgroups.clear();
	handlergroup.clear();
	holdlist.clear();

	// Decrement our reference pointer to the Joystick subsystem
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

void MAPPER_BindKeys() {
	//Release any keys pressed, or else they'll get stuck
	GFX_LosingFocus(); 

	const Section *section = control->GetSection("joystick");
	assert(section);
	const std::string joystick_type = section->GetPropValue("joysticktype");
	if (!joystick_type.empty() && joystick_type != "none")
		QueryJoysticks();

	// Create the graphical layout for all registered key-binds
	if (buttons.empty())
		CreateLayout();

	if (bindgroups.empty())
		CreateBindGroups();

	// Create binds from file or fallback to internals
	if (!MAPPER_CreateBindsFromFile())
		CreateDefaultBinds();

	for (CButton_it but_it = buttons.begin(); but_it != buttons.end(); ++but_it)
		(*but_it)->BindColor();

	if (SDL_GetModState()&KMOD_CAPS)
		MAPPER_TriggerEvent(caps_lock_event, false);

	if (SDL_GetModState()&KMOD_NUM)
		MAPPER_TriggerEvent(num_lock_event, false);
}

std::vector<std::string> MAPPER_GetEventNames(const std::string &prefix) {
	std::vector<std::string> key_names;
	key_names.reserve(events.size());
	for (auto & e : events) {
		const std::string name = e->GetName();
		const std::size_t found = name.find(prefix);
		if (found != std::string::npos) {
			const std::string key_name = name.substr(found + prefix.length());
			key_names.push_back(key_name);
		}
	}
	return key_names;
}

void MAPPER_AutoType(std::vector<std::string> &sequence,
                     const uint32_t wait_ms,
                     const uint32_t pace_ms) {
	mapper.typist.Start(&events, sequence, wait_ms, pace_ms);
}

// Activate user-specified or default binds
static void MAPPER_ConfigureBindings(Section *sec) {
	(void) sec; // unused but present for API compliance
	Section_prop const *const section=static_cast<Section_prop *>(sec);
	Prop_path const *const pp = section->Get_path("mapperfile");
	mapper.filename = pp->realpath;

	/*  Because the mapper is initialized before several other of DOSBox's
	 *  submodules have a chance to register their key bindings, we defer
	 *  the mapper's setup and instead manully BindKeys() in SDL main only
	 *  after -all- subsystems have been initialized, which ensures that all
	 *  binding a present, and thus are also layed out in the mapper's GUI.
	 */
	static bool init_phase = true;
	if (init_phase) {
		init_phase = false;
		return;
	}
	MAPPER_BindKeys();
}

void MAPPER_StartUp(Section * sec) {
	Section_prop * section = static_cast<Section_prop *>(sec);

	 //runs after this function ends and for subsequent config -set "sdl mapperfile=file.map" commands
	section->AddInitFunction(&MAPPER_ConfigureBindings, true);

	// runs one-time on shutdown
	section->AddDestroyFunction(&MAPPER_Destroy, false);
	MAPPER_AddHandler(&MAPPER_Run,MK_f1,MMOD1,"mapper","Mapper");
}
