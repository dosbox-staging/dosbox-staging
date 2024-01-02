/*
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#include "mouseif_dos_driver_state.h"

#include <cstddef>

#include "compiler.h"
#include "checks.h"
#include "dos_inc.h"

CHECK_NARROWING();

// ***************************************************************************
// Driver data structure, exactly as visible from the guest side
// ***************************************************************************

#ifdef _MSC_VER
#pragma pack (1)
#endif

struct State { // DOS driver state

	// Structure containing (only!) data which is stored in the guest
	// accessible memory, can be paged by Windows or saved/restored during
	// task switching

	// DANGER, WILL ROBINSON!
	//
	// This whole structure can be read or written from the guest side
	// either by direct memory access, or via the virtual DOS mouse driver,
	// functions 0x15 / 0x16 / 0x17.
	// Do not put here any array indices, pointers, or anything that can
	// crash the emulator if filled-in incorrectly, or that can be used
	// by a malicious code to escape from the emulation environment!

	// XXX finish support, add comments

	// Windows support flags
	struct Win386State {
		uint8_t running        = 0;
		uint8_t drawing_cursor = 0;
	} GCC_ATTRIBUTE(packed) win386_state = {};

	// data to be passed to Windows 3.x
	struct Win386Startup {
		uint8_t  version_minor = 0;
		uint8_t  version_major = 3;
		RealPt   next_info_farptr          = 0;
		RealPt   device_driver_farptr      = 0;
		RealPt   device_driver_data_farptr = 0;
		RealPt   instance_data_farptr      = 0;
	} GCC_ATTRIBUTE(packed) win386_startup = {};

	struct Win386Instance {
		RealPt   data_farptr = 0;
		uint16_t size        = 0;
	} GCC_ATTRIBUTE(packed) win386_instances[2] = {};

	struct InstanceSpecific {
		// precalculated (cached) data
		float mickeys_per_pixel_x = 0.0f;
		float mickeys_per_pixel_y = 0.0f;
		float pixels_per_mickey_x = 0.0f;
		float pixels_per_mickey_y = 0.0f;
		float sensitivity_coeff_x = 0.0f;
		float sensitivity_coeff_y = 0.0f;

		// absolute cursor position
		float absolute_x = 0.0f;
		float absolute_y = 0.0f;

		// counters
		float mickey_counter_x = 0.0f;
		float mickey_counter_y = 0.0f;

		uint16_t times_pressed[num_buttons]   = {0};
		uint16_t times_released[num_buttons]  = {0};
		uint16_t last_pressed_x[num_buttons]  = {0};
		uint16_t last_pressed_y[num_buttons]  = {0};
		uint16_t last_released_x[num_buttons] = {0};
		uint16_t last_released_y[num_buttons] = {0};

		uint16_t last_wheel_moved_x = 0;
		uint16_t last_wheel_moved_y = 0;

		// enabled/disabled state
		bool enabled   = false; // TODO: make use of this
		bool wheel_api = false; // CuteMouse compatible wheel extension

		uint16_t double_speed_threshold = 0; // in mickeys/s
		                                     // TODO: should affect movement

		uint16_t granularity_x = 0; // mask
		uint16_t granularity_y = 0;

		int16_t update_region_x1 = 0;
		int16_t update_region_y1 = 0;
		int16_t update_region_x2 = 0;
		int16_t update_region_y2 = 0;

		// language for driver messages, unused in DOSBox
		uint16_t language = 0;

		uint8_t bios_screen_mode = 0;

		uint8_t sensitivity_x = 0;
		uint8_t sensitivity_y = 0;

		// TODO: find out what it is for (acceleration?), for now
		// just set it to default value on startup
		uint8_t unknown_01 = 50;

		// mouse position allowed range
		int16_t min_pos_x = 0;
		int16_t max_pos_x = 0;
		int16_t min_pos_y = 0;
		int16_t max_pos_y = 0;

		// mouse cursor
		uint8_t page        = 0; // cursor display page number
		bool inhibit_draw   = false;
		uint16_t hidden     = 0;
		uint16_t old_hidden = 0;
		int16_t clip_x      = 0;
		int16_t clip_y      = 0;
		int16_t hot_x       = 0; // cursor hot spot, horizontal
		int16_t hot_y       = 0; // cursor hot spot, vertical

		MouseCursorType cursor_type = MouseCursorType::Software;

		// cursor background storage
		bool background_enabled = false;
		uint16_t background_x   = 0;
		uint16_t background_y   = 0;
		uint8_t background_data[cursor_size * cursor_size] = {0};

		// cursor shape definition
		uint16_t text_mask_and = 0;
		uint16_t text_mask_xor = 0;
		bool user_screen_mask  = false;
		bool user_cursor_mask  = false;
		uint16_t user_screen_mask_data[cursor_size] = {0};
		uint16_t user_cursor_mask_data[cursor_size] = {0};

		// user callback handling
		uint16_t callback_return_segment = 0;
		uint16_t callback_return_offset  = 0;
		uint16_t user_callback_segment   = 0;
		uint16_t user_callback_offset    = 0;
		uint16_t user_callback_mask      = 0;
	} GCC_ATTRIBUTE(packed) instance = {};

} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack ()
#endif

// Code below assumes all the data fits into a single segment;
// this should be more than enough for any sane mouse driver data
static_assert(sizeof(State) <= UINT16_MAX + 1);

static State::InstanceSpecific defaults = {};

// ***************************************************************************
// Private helper routines
// ***************************************************************************

uint16_t MouseDriverState::guest_data_segment  = 0;


uint8_t MouseDriverState::ReadB(const size_t offset)
{
	return real_readb(guest_data_segment, check_cast<uint16_t>(offset));
}

uint16_t MouseDriverState::ReadW(const size_t offset)
{
	return real_readw(guest_data_segment, check_cast<uint16_t>(offset));
}

uint32_t MouseDriverState::ReadD(const size_t offset)
{
	return real_readd(guest_data_segment, check_cast<uint16_t>(offset));
}

void MouseDriverState::WriteB(const size_t offset, const  uint8_t value)
{
	real_writeb(guest_data_segment, check_cast<uint16_t>(offset), value);
}

void MouseDriverState::WriteW(const size_t offset, const  uint16_t value)
{
	real_writew(guest_data_segment, check_cast<uint16_t>(offset), value);
}

void MouseDriverState::WriteD(const size_t offset, const uint32_t value)
{
	real_writed(guest_data_segment, check_cast<uint16_t>(offset), value);	
}

// ***************************************************************************
// Lifecycle
// ***************************************************************************

bool MouseDriverState::Initialize()
{
	if (guest_data_segment != 0) {
		return false; // already initialized
	}

	// XXX deduplicate calculations
	const auto length_bytes = sizeof(State);
	constexpr uint8_t bytes_per_block = 0x10;
	auto length_blocks = static_cast<uint16_t>(length_bytes / bytes_per_block);
	if (length_bytes % bytes_per_block) {
		length_blocks = static_cast<uint16_t>(length_blocks + 1);
	}

	// XXX rather use this
	// DOS_AllocateMemory(&guest_data_segment, &length_blocks);

	guest_data_segment = DOS_GetMemory(length_blocks);
	if (guest_data_segment == 0) {
		return false;
	}

	// Set defaults

	ClearWindowsStruct();

	SetMickeysPerPixelX(defaults.mickeys_per_pixel_x);
	SetMickeysPerPixelY(defaults.mickeys_per_pixel_y);
	SetPixelsPerMickeyX(defaults.pixels_per_mickey_x);
	SetPixelsPerMickeyY(defaults.pixels_per_mickey_y);

	SetSensitivityCoeffX(defaults.sensitivity_coeff_x);
	SetSensitivityCoeffY(defaults.sensitivity_coeff_y);

	SetAbsoluteX(defaults.absolute_x);
	SetAbsoluteY(defaults.absolute_y);

	SetEnabled(defaults.enabled);
	SetWheelApi(defaults.wheel_api);
	
	SetMickeyCounterX(defaults.mickey_counter_x);
	SetMickeyCounterY(defaults.mickey_counter_y);

	for (size_t idx = 0; idx < num_buttons; ++idx) {
		SetTimesPressed(idx, defaults.times_pressed[idx]);
		SetTimesReleased(idx, defaults.times_released[idx]);
		SetLastPressedX(idx, defaults.last_pressed_x[idx]);
		SetLastPressedY(idx, defaults.last_pressed_y[idx]);
		SetLastReleasedX(idx, defaults.last_released_x[idx]);
		SetLastReleasedY(idx, defaults.last_released_y[idx]);
	}
	SetLastWheelMovedX(defaults.last_wheel_moved_x);
	SetLastWheelMovedY(defaults.last_wheel_moved_y);

	SetDoubleSpeedThreshold(defaults.double_speed_threshold);

	SetGranularityX(defaults.granularity_x);
	SetGranularityY(defaults.granularity_y);

	SetUpdateRegionX1(defaults.update_region_x1);
	SetUpdateRegionY1(defaults.update_region_y1);
	SetUpdateRegionX2(defaults.update_region_x2);
	SetUpdateRegionY2(defaults.update_region_y2);

	SetLanguage(defaults.language);

	SetBiosScreenMode(defaults.bios_screen_mode);

	SetSensitivityX(defaults.sensitivity_x);
	SetSensitivityY(defaults.sensitivity_y);
	SetUnknown01(defaults.unknown_01);

	SetMinPosX(defaults.min_pos_x);
	SetMinPosY(defaults.min_pos_y);
	SetMaxPosX(defaults.max_pos_x);
	SetMaxPosY(defaults.max_pos_y);

	SetPage(defaults.page);
	SetInhibitDraw(defaults.inhibit_draw);

	SetHidden(defaults.hidden);
	SetOldHidden(defaults.old_hidden);

	SetClipX(defaults.clip_x);
	SetClipY(defaults.clip_y);
	SetHotX(defaults.hot_x);
	SetHotY(defaults.hot_y);

	SetCursorType(defaults.cursor_type);

	SetBackgroundEnabled(defaults.background_enabled);

	SetBackgroundX(defaults.background_x);
	SetBackgroundY(defaults.background_y);

	for (size_t idx = 0; idx < cursor_size * cursor_size; ++idx) {
		SetBackgroundData(idx, defaults.background_data[idx]);
	}

	SetTextMaskAnd(defaults.text_mask_and);
	SetTextMaskXor(defaults.text_mask_xor);

	SetUserScreenMask(defaults.user_screen_mask);
	SetUserCursorMask(defaults.user_cursor_mask);

	SetUserScreenMaskData({0});
	SetUserCursorMaskData({0});

	SetCallbackReturn(defaults.callback_return_segment,
	                  defaults.callback_return_offset);
	SetUserCallback(defaults.user_callback_segment,
	                defaults.user_callback_offset);
	SetUserCallbackMask(defaults.user_callback_mask);

	return true;
}

void MouseDriverState::ClearWindowsStruct()
{
	// XXX is there some kind of memset available? maybe we should write one?

	size_t offset = offsetof(State, win386_startup);
	for (size_t idx = 0; idx < sizeof(State::win386_startup); ++idx) {
		WriteB(offset + idx, 0);
	}

	offset = offsetof(State, win386_instances);
	for (size_t idx = 0; idx < sizeof(State::win386_instances); ++idx) {
		WriteB(offset + idx, 0);
	}
}

RealPt MouseDriverState::SetupWindowsStruct(const RealPt link_pointer)
{
	// XXX cleanup Win386Startup handling
	State::Win386Startup win386_startup = {};

	// Fill-in Win386Startup
	win386_startup.next_info_farptr = link_pointer;
	size_t offset = offsetof(State, win386_instances);
	win386_startup.device_driver_data_farptr = RealMake(guest_data_segment,
	                                                    check_cast<uint16_t>(offset));

	offset = offsetof(State, win386_startup.version_minor);
	WriteB(offset, win386_startup.version_minor);
	
	offset = offsetof(State, win386_startup.version_major);
	WriteB(offset, win386_startup.version_major);
	
	offset = offsetof(State, win386_startup.next_info_farptr);
	WriteD(offset, win386_startup.next_info_farptr);
	
	offset = offsetof(State, win386_startup.device_driver_farptr);
	WriteD(offset, win386_startup.device_driver_farptr);
	
	offset = offsetof(State, win386_startup.device_driver_data_farptr);
	WriteD(offset, win386_startup.device_driver_data_farptr);
	
	offset = offsetof(State, win386_startup.instance_data_farptr);
	WriteD(offset, win386_startup.instance_data_farptr);

	// Fill-in Win386Instance
	WriteD(offsetof(State, win386_instances[0].data_farptr),
	       RealMake(guest_data_segment, 0));
	WriteW(offsetof(State, win386_instances[0].size), sizeof(State));
	WriteD(offsetof(State, win386_instances[1].data_farptr), 0);
	WriteW(offsetof(State, win386_instances[1].size), 0);

	return RealMake(guest_data_segment, offsetof(State, win386_startup));
}

// ***************************************************************************
// Macro-implemented get/set routines
// ***************************************************************************

// XXX provide SetDefault funtions too
// XXX optimize pointer storage, should be uint32_t

static_assert(sizeof(float) == 4);

#define MAKE_SET_GET_BOOL(parameter, field) \
void MouseDriverState::Set##parameter(const bool value) \
{ \
	WriteB(offsetof(State, field), value ? 1 : 0); \
} \
\
bool MouseDriverState::Is##parameter() \
{ \
	return (ReadB(offsetof(State, field)) != 0); \
}

#define MAKE_SET_GET_UINT8(parameter, field) \
void MouseDriverState::Set##parameter(const uint8_t value) \
{ \
	WriteB(offsetof(State, field), value); \
} \
\
uint8_t MouseDriverState::Get##parameter() \
{ \
	return ReadB(offsetof(State, field)); \
}

#define MAKE_SET_GET_UINT8_ARRAY(parameter, field, size) \
void MouseDriverState::Set##parameter(const size_t idx, const uint8_t value) \
{ \
	assert(idx < size); \
	WriteB(offsetof(State, field) + idx, value); \
} \
\
uint8_t MouseDriverState::Get##parameter(const size_t idx) \
{ \
	assert(idx < size); \
	return ReadB(offsetof(State, field) + idx); \
}

#define MAKE_SET_GET_UINT8_ENUM(parameter, field, enumtype) \
void MouseDriverState::Set##parameter(const enumtype value) \
{ \
	WriteB(offsetof(State, field), enum_val(value)); \
} \
\
enumtype MouseDriverState::Get##parameter() \
{ \
	return static_cast<enumtype>(ReadB(offsetof(State, field))); \
}

#define MAKE_SET_GET_UINT16(parameter, field) \
static_assert(offsetof(State, field) % 2 == 0); \
\
void MouseDriverState::Set##parameter(const uint16_t value) \
{ \
	WriteW(offsetof(State, field), value); \
} \
\
uint16_t MouseDriverState::Get##parameter() \
{ \
	return ReadW(offsetof(State, field)); \
}

#define MAKE_SET_GET_UINT16_ARRAY(parameter, field, size) \
static_assert(offsetof(State, field) % 2 == 0); \
\
void MouseDriverState::Set##parameter(const size_t idx, const uint16_t value) \
{ \
	assert(idx < size); \
	WriteW(offsetof(State, field) + idx * 2, value); \
} \
\
uint16_t MouseDriverState::Get##parameter(const size_t idx) \
{ \
	assert(idx < size); \
	return ReadW(offsetof(State, field) + idx * 2); \
}

#define MAKE_SET_GET_UINT16_BLOCK(parameter, field, size) \
static_assert(offsetof(State, field) % 2 == 0); \
\
void MouseDriverState::Set##parameter(const std::array<uint16_t, size> &value) \
{ \
	for (size_t idx = 0; idx < size; ++idx) { \
		WriteW(offsetof(State, field) + idx * 2, value[idx]); \
	} \
} \
\
std::array<uint16_t, size> MouseDriverState::Get##parameter() \
{ \
	std::array<uint16_t, size> result = {0}; \
	for (size_t idx = 0; idx < size; ++idx) { \
		result[idx] = ReadW(offsetof(State, field) + idx * 2); \
	} \
	return result; \
}

#define MAKE_SET_GET_INT16(parameter, field) \
static_assert(offsetof(State, field) % 2 == 0); \
\
void MouseDriverState::Set##parameter(const int16_t value) \
{ \
	WriteW(offsetof(State, field), static_cast<uint16_t>(value)); \
} \
\
int16_t MouseDriverState::Get##parameter() \
{ \
	return static_cast<int16_t>(ReadW(offsetof(State, field))); \
}

#define MAKE_SET_GET_POINTER(parameter, field_segment, field_offset) \
void MouseDriverState::Set##parameter(const uint16_t segment, const uint16_t offset) \
{ \
	WriteW(offsetof(State, field_segment), segment); \
	WriteW(offsetof(State, field_offset),  offset); \
} \
static_assert(offsetof(State, field_segment) % 2 == 0); \
uint16_t MouseDriverState::Get##parameter##Segment() \
{ \
	return ReadW(offsetof(State, field_segment)); \
} \
static_assert(offsetof(State, field_segment) % 2 == 0); \
uint16_t MouseDriverState::Get##parameter##Offset() \
{ \
	return ReadW(offsetof(State, field_offset)); \
}

#define MAKE_SET_GET_FLOAT(parameter, field) \
static_assert(offsetof(State, field) % 4 == 0); \
\
void MouseDriverState::Set##parameter(const float value) \
{ \
	const auto raw_value_ptr = reinterpret_cast<const void *>(&value); \
	WriteD(offsetof(State, field), *reinterpret_cast<const uint32_t *>(raw_value_ptr)); \
} \
\
float MouseDriverState::Get##parameter() \
{ \
	const auto raw_value = ReadD(offsetof(State, field)); \
	const auto raw_value_ptr = reinterpret_cast<const void *>(&raw_value); \
	return *reinterpret_cast<const float *>(raw_value_ptr); \
}

MAKE_SET_GET_BOOL(Win386Running, win386_state.running)
MAKE_SET_GET_BOOL(Win386DrawingCursor, win386_state.drawing_cursor)

MAKE_SET_GET_FLOAT(MickeysPerPixelX, instance.mickeys_per_pixel_x)
MAKE_SET_GET_FLOAT(MickeysPerPixelY, instance.mickeys_per_pixel_y)
MAKE_SET_GET_FLOAT(PixelsPerMickeyX, instance.pixels_per_mickey_x)
MAKE_SET_GET_FLOAT(PixelsPerMickeyY, instance.pixels_per_mickey_y)

MAKE_SET_GET_FLOAT(SensitivityCoeffX, instance.sensitivity_coeff_x)
MAKE_SET_GET_FLOAT(SensitivityCoeffY, instance.sensitivity_coeff_y)

MAKE_SET_GET_FLOAT(AbsoluteX, instance.absolute_x)
MAKE_SET_GET_FLOAT(AbsoluteY, instance.absolute_y)

MAKE_SET_GET_FLOAT(MickeyCounterX, instance.mickey_counter_x)
MAKE_SET_GET_FLOAT(MickeyCounterY, instance.mickey_counter_y)

MAKE_SET_GET_UINT16_ARRAY(TimesPressed, instance.times_pressed, num_buttons)
MAKE_SET_GET_UINT16_ARRAY(TimesReleased, instance.times_released, num_buttons)
MAKE_SET_GET_UINT16_ARRAY(LastPressedX, instance.last_pressed_x, num_buttons)
MAKE_SET_GET_UINT16_ARRAY(LastPressedY, instance.last_pressed_y, num_buttons)
MAKE_SET_GET_UINT16_ARRAY(LastReleasedX, instance.last_released_x, num_buttons)
MAKE_SET_GET_UINT16_ARRAY(LastReleasedY, instance.last_released_y, num_buttons)
MAKE_SET_GET_UINT16(LastWheelMovedX, instance.last_wheel_moved_x)
MAKE_SET_GET_UINT16(LastWheelMovedY, instance.last_wheel_moved_y)

MAKE_SET_GET_BOOL(Enabled, instance.enabled)

MAKE_SET_GET_BOOL(WheelApi, instance.wheel_api)

MAKE_SET_GET_UINT16(DoubleSpeedThreshold, instance.double_speed_threshold)

MAKE_SET_GET_UINT16(GranularityX, instance.granularity_x)
MAKE_SET_GET_UINT16(GranularityY, instance.granularity_y)

MAKE_SET_GET_INT16(UpdateRegionX1, instance.update_region_x1)
MAKE_SET_GET_INT16(UpdateRegionY1, instance.update_region_y1)
MAKE_SET_GET_INT16(UpdateRegionX2, instance.update_region_x2)
MAKE_SET_GET_INT16(UpdateRegionY2, instance.update_region_y2)

MAKE_SET_GET_UINT16(Language, instance.language)

MAKE_SET_GET_UINT8(BiosScreenMode, instance.bios_screen_mode)

MAKE_SET_GET_UINT8(SensitivityX, instance.sensitivity_x)
MAKE_SET_GET_UINT8(SensitivityY, instance.sensitivity_y)

MAKE_SET_GET_UINT8(Unknown01, instance.unknown_01)

MAKE_SET_GET_INT16(MinPosX, instance.min_pos_x)
MAKE_SET_GET_INT16(MinPosY, instance.min_pos_y)
MAKE_SET_GET_INT16(MaxPosX, instance.max_pos_x)
MAKE_SET_GET_INT16(MaxPosY, instance.max_pos_y)

MAKE_SET_GET_UINT8(Page, instance.page)

MAKE_SET_GET_BOOL(InhibitDraw, instance.inhibit_draw)

MAKE_SET_GET_UINT16(Hidden, instance.hidden)
MAKE_SET_GET_UINT16(OldHidden, instance.old_hidden)

MAKE_SET_GET_INT16(ClipX, instance.clip_x)
MAKE_SET_GET_INT16(ClipY, instance.clip_y)

MAKE_SET_GET_INT16(HotX, instance.hot_x)
MAKE_SET_GET_INT16(HotY, instance.hot_y)

MAKE_SET_GET_UINT8_ENUM(CursorType, instance.cursor_type, MouseCursorType)

MAKE_SET_GET_BOOL(BackgroundEnabled, instance.background_enabled)

MAKE_SET_GET_UINT16(BackgroundX, instance.background_x)
MAKE_SET_GET_UINT16(BackgroundY, instance.background_y)

MAKE_SET_GET_UINT8_ARRAY(BackgroundData, instance.background_data, cursor_size * cursor_size)

MAKE_SET_GET_UINT16(TextMaskAnd, instance.text_mask_and)
MAKE_SET_GET_UINT16(TextMaskXor, instance.text_mask_xor)

MAKE_SET_GET_BOOL(UserScreenMask, instance.user_screen_mask)
MAKE_SET_GET_BOOL(UserCursorMask, instance.user_cursor_mask)

MAKE_SET_GET_UINT16_BLOCK(UserScreenMaskData, instance.user_screen_mask_data, cursor_size)
MAKE_SET_GET_UINT16_BLOCK(UserCursorMaskData, instance.user_cursor_mask_data, cursor_size)

MAKE_SET_GET_POINTER(CallbackReturn, instance.callback_return_segment, instance.callback_return_offset)
MAKE_SET_GET_POINTER(UserCallback, instance.user_callback_segment, instance.user_callback_offset)

MAKE_SET_GET_UINT16(UserCallbackMask, instance.user_callback_mask)
