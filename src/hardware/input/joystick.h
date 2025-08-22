// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_JOYSTICK_H
#define DOSBOX_JOYSTICK_H

#include "dosbox.h"

#include <string>

#include "config/config.h"
#include "config/setup.h"

void JOYSTICK_AddConfigSection(const ConfigPtr& conf);

void JOYSTICK_Enable(uint8_t which, bool enabled);

void JOYSTICK_Button(uint8_t which, int num, bool pressed);

// takes in the joystick axis absolute value from -32768 to 32767
void JOYSTICK_Move_X(uint8_t which, int16_t x_val);
void JOYSTICK_Move_Y(uint8_t which, int16_t y_val);

bool JOYSTICK_IsAccessible(uint8_t which);

bool JOYSTICK_GetButton(uint8_t which, int num);

// returns a percentage from -1.0 to +1.0 along the axis
double JOYSTICK_GetMove_X(uint8_t which);
double JOYSTICK_GetMove_Y(uint8_t which);

void JOYSTICK_ParseConfiguredType();

enum JoystickType {
	JOY_UNSET = 1 << 0,
	JOY_NONE_FOUND = 1 << 1,       // Not a conf option; only set during auto-setup
	JOY_DISABLED = 1 << 2,         // SDL's joystick subsystem left uninitialized
	JOY_ONLY_FOR_MAPPING = 1 << 3, // Hidden from DOS, but still mappable
	JOY_AUTO = 1 << 4,             // Specific type is determined during auto-setup
	JOY_2AXIS = 1 << 5,
	JOY_4AXIS = 1 << 6,
	JOY_4AXIS_2 = 1 << 7,
	JOY_FCS = 1 << 8,
	JOY_CH = 1 << 9,
};

extern JoystickType joytype;
extern bool button_wrapping_enabled;

#endif
