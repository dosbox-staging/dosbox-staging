/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2025-2025  The DOSBox Staging Team
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

#ifndef DOSBOX_MOUSEIF_DOS_DRIVER_STATE_H
#define DOSBOX_MOUSEIF_DOS_DRIVER_STATE_H

#include "dos_inc.h"


static constexpr auto MaxMouseButtons  = 3;
static constexpr auto MaxUpdateRegions = 2;

static constexpr uint8_t CursorSize = 16;

enum class MouseCursor : uint8_t { Software = 0, Hardware = 1, Text = 2 };


class MouseDriverState final : public MemStruct {
public:
	MouseDriverState(const uint16_t segment) : segment(segment) {
		SetPt(segment);
	}

	static uint16_t GetStateSize() {
		static_assert(sizeof(State) <= UINT16_MAX);
		return static_cast<uint16_t>(sizeof(State));
	}

	uint16_t GetWin386StateOffset() const;
	uint16_t GetWin386StartupOffset() const;
	uint16_t GetWin386InstancesOffset() const;

	uint8_t Win386State_GetRunning() const;
	void Win386State_SetRunning(const uint8_t value);
	
	uint8_t Win386State_GetDrawingCursor() const;
	void Win386State_SetDrawingCursor(const uint8_t value);

	uint8_t Win386Startup_GetVersionMinor() const;
	void Win386Startup_SetVersionMinor(const uint8_t value);

	uint8_t Win386Startup_GetVersionMajor() const;
	void Win386Startup_SetVersionMajor(const uint8_t value);

	RealPt Win386Startup_GetNextInfoFarPtr() const;
	void Win386Startup_SetNextInfoFarPtr(const RealPt value);

	RealPt Win386Startup_GetDeviceDriverFarPtr() const;
	void Win386Startup_SetDeviceDriverFarPtr(const RealPt value);

	RealPt Win386Startup_GetDeviceDriverDataFarPtr() const;
	void Win386Startup_SetDeviceDriverDataFarPtr(const RealPt value);

	RealPt Win386Startup_GetInstanceDataFarPtr() const;
	void Win386Startup_SetInstanceDataFarPtr(const RealPt value);

	RealPt Win386Instance_GetDataFarPtr(const size_t index) const;
	void Win386Instance_SetDataFarPtr(const size_t index, const RealPt value);

	uint16_t Win386Instance_GetSize(const size_t index) const;
	void Win386Instance_SetSize(const size_t index, const uint16_t value);

	bool IsWin386Running() const;
	bool IsWin386Cursor() const;
	void SetWin386Running(const bool value);
	void SetWin386Cursor(const bool value);

	bool IsEnabled() const;
	void SetEnabled(const bool value);

	uint8_t GetWheelApi() const;
	void SetWheelApi(const uint8_t value);

	uint16_t GetTimesPressed(const size_t index) const;
	uint16_t GetTimesReleased(const size_t index) const;
	void SetTimesPressed(const size_t index, const uint16_t value);
	void SetTimesReleased(const size_t index, const uint16_t value);

	uint16_t GetLastReleasedX(const size_t index) const;
	uint16_t GetLastReleasedY(const size_t index) const;
	void SetLastReleasedX(const size_t index, const uint16_t value);
	void SetLastReleasedY(const size_t index, const uint16_t value);

	uint16_t GetLastPressedX(const size_t index) const;
	uint16_t GetLastPressedY(const size_t index) const;
	void SetLastPressedX(const size_t index, const uint16_t value);
	void SetLastPressedY(const size_t index, const uint16_t value);

	uint16_t GetLastWheelMovedX() const;
	uint16_t GetLastWheelMovedY() const;
	void SetLastWheelMovedX(const uint16_t value);
	void SetLastWheelMovedY(const uint16_t value);

	float GetMickeyCounterX() const;
	float GetMickeyCounterY() const;
	void SetMickeyCounterX(const float value);
	void SetMickeyCounterY(const float value);

	float GetMickeysPerPixelX() const;
	float GetMickeysPerPixelY() const;
	float GetPixelsPerMickeyX() const;
	float GetPixelsPerMickeyY() const;
	void SetMickeysPerPixelX(const float value);
	void SetMickeysPerPixelY(const float value);
	void SetPixelsPerMickeyX(const float value);
	void SetPixelsPerMickeyY(const float value);

	uint16_t GetDoubleSpeedThreshold() const;
	void SetDoubleSpeedThreshold(const uint16_t value);

	uint16_t GetGranularityX() const;
	uint16_t GetGranularityY() const;
	void SetGranularityX(const uint16_t value);
	void SetGranularityY(const uint16_t value);

	int16_t GetUpdateRegionX(const size_t index) const;
	int16_t GetUpdateRegionY(const size_t index) const;
	void SetUpdateRegionX(const size_t index, const int16_t value);
	void SetUpdateRegionY(const size_t index, const int16_t value);

	uint16_t GetLanguage() const;
	void SetLanguage(const uint16_t value);

	uint8_t GetBiosScreenMode() const;
	void SetBiosScreenMode(const uint8_t value);

	uint8_t GetSensitivityX() const;
	uint8_t GetSensitivityY() const;
	void SetSensitivityX(const uint8_t value);
	void SetSensitivityY(const uint8_t value);

	uint8_t GetUnknownValue01() const;
	void SetUnknownValue01(const uint8_t value);

	float GetSensitivityCoeffX() const;
	float GetSensitivityCoeffY() const;
	void SetSensitivityCoeffX(const float value);
	void SetSensitivityCoeffY(const float value);

	int16_t GetMinPosX() const;
	int16_t GetMinPosY() const;
	int16_t GetMaxPosX() const;
	int16_t GetMaxPosY() const;
	void SetMinPosX(const int16_t value);
	void SetMinPosY(const int16_t value);
	void SetMaxPosX(const int16_t value);
	void SetMaxPosY(const int16_t value);

	uint8_t GetPage() const;
	void SetPage(const uint8_t value);

	bool IsInhibitDraw() const;
	void SetInhibitDraw(const bool value);

	uint16_t GetHidden() const;
	uint16_t GetOldHidden() const;
	void SetHidden(const uint16_t value);
	void SetOldHidden(const uint16_t value);

	int16_t GetClipX() const;
	int16_t GetClipY() const;
	void SetClipX(const int16_t value);
	void SetClipY(const int16_t value);

	int16_t GetHotX() const;
	int16_t GetHotY() const;
	void SetHotX(const int16_t value);
	void SetHotY(const int16_t value);

	bool IsBackgroundEnabled() const;
	void SetBackgroundEnabled(const bool value);

	uint16_t GetBackgroundPosX() const;
	uint16_t GetBackgroundPosY() const;
	void SetBackgroundPosX(const uint16_t value);
	void SetBackgroundPosY(const uint16_t value);

	uint8_t GetBackgroundData(const size_t index) const;
	void SetBackgroundData(const size_t index, const uint8_t value);

	MouseCursor GetCursorType() const;
	void SetCursorType(const MouseCursor value);

	uint16_t GetTextAndMask() const;
	uint16_t GetTextXorMask() const;
	void SetTextAndMask(const uint16_t value);
	void SetTextXorMask(const uint16_t value);

	bool IsUserScreenMask() const;
	bool IsUserCursorMask() const;
	void SetUserScreenMask(const bool value);
	void SetUserCursorMask(const bool value);

	uint16_t GetUserDefScreenMask(const size_t index) const;
	uint16_t GetUserDefCursorMask(const size_t index) const;
	void SetUserDefScreenMask(const size_t index, const uint16_t value);
	void SetUserDefCursorMask(const size_t index, const uint16_t value);

	uint16_t GetUserCallbackMask() const;
	uint16_t GetUserCallbackSegment() const;
	uint16_t GetUserCallbackOffset() const;	
	void SetUserCallbackMask(const uint16_t value);
	void SetUserCallbackSegment(const uint16_t value);
	void SetUserCallbackOffset(const uint16_t value);

private:
	uint16_t segment = 0;

	#ifdef _MSC_VER
	#pragma pack(1)
	#endif

	struct State { // DOS driver state

		// Structure containing (only!) data which should be
		// saved/restored during task switching

		// DANGER, WILL ROBINSON!
		//
		// This whole structure can be read or written from the guest
		// side (using driver functions or direct memory access) or
		// virtualized by software like Microsoft Windows.
		// Do not put here any host side pointers, anything that can
		// crash the emulator if filled-in incorrectly, or that can be
		// used by malicious code to break out of emulation!

		// XXX support structs below
		// XXX check https://git.javispedro.com/cgit/vbados.git/tree/mousetsr.c?id=4091173cceea8f2c3b0cdbc19a1adb6bd87c0a94#n1413

		// Windows support structures - do not alter the content of the
		// structures!

		struct Win386State {
			uint8_t running        = 0;
			uint8_t drawing_cursor = 0;
		} GCC_ATTRIBUTE(packed) win386_state = {};

		struct Win386Startup {
			uint8_t  version_minor = 0;
			uint8_t  version_major = 0;
			RealPt   next_info_farptr          = 0;
			RealPt   device_driver_farptr      = 0;
			RealPt   device_driver_data_farptr = 0;
			RealPt   instance_data_farptr      = 0;
		} GCC_ATTRIBUTE(packed) win386_startup = {};

		struct Win386Instances {
			RealPt   instance0_data_farptr = 0;
			uint16_t instance0_size        = 0;
			RealPt   instance1_data_farptr = 0;
			uint16_t instance1_size        = 0;
		} GCC_ATTRIBUTE(packed) win386_instances = {};

		// Simulated driver data

		uint8_t is_win386_running = 0;
		uint8_t is_win386_cursor  = 0;

		// TODO: make use of this
		uint8_t is_enabled = 0;

		// CuteMouse compatible WheelAPI extension level, 0 = disabled
		uint8_t wheel_api = 0;

		uint16_t times_pressed[MaxMouseButtons]   = {0};
		uint16_t times_released[MaxMouseButtons]  = {0};
		uint16_t last_released_x[MaxMouseButtons] = {0};
		uint16_t last_released_y[MaxMouseButtons] = {0};
		uint16_t last_pressed_x[MaxMouseButtons]  = {0};
		uint16_t last_pressed_y[MaxMouseButtons]  = {0};

		uint16_t last_wheel_moved_x = 0;
		uint16_t last_wheel_moved_y = 0;

		float mickey_counter_x = 0.0f;
		float mickey_counter_y = 0.0f;

		float mickeys_per_pixel_x = 0.0f;
		float mickeys_per_pixel_y = 0.0f;
		float pixels_per_mickey_x = 0.0f;
		float pixels_per_mickey_y = 0.0f;

		// in mickeys/s, TODO: should affect movement
		uint16_t double_speed_threshold = 0;

		// Granularity bitmasks
		uint16_t granularity_x = 0;
		uint16_t granularity_y = 0;

		int16_t update_region_x[MaxUpdateRegions] = {0};
		int16_t update_region_y[MaxUpdateRegions] = {0};

		// Language of driver messages, not used
		uint16_t language = 0;

		uint8_t bios_screen_mode = 0;

		uint8_t sensitivity_x = 0;
		uint8_t sensitivity_y = 0;

		// TODO: find out what this should be used for (acceleration?)
		uint8_t unknown_01 = 0;

		float sensitivity_coeff_x = 0.0f;
		float sensitivity_coeff_y = 0.0f;

		// Mouse position allowed range
		int16_t minpos_x = 0;
		int16_t maxpos_x = 0;
		int16_t minpos_y = 0;
		int16_t maxpos_y = 0;

		// Mouse cursor
		uint8_t page         = 0; // cursor display page number
		uint8_t inhibit_draw = false;
		uint16_t hidden      = 0;
		uint16_t old_hidden  = 0;
		int16_t clip_x       = 0;
		int16_t clip_y       = 0;
		int16_t hot_x        = 0; // cursor hot spot, horizontal
		int16_t hot_y        = 0; // cursor hot spot, vertical

		uint8_t  background_enabled = false;
		uint16_t background_pos_x   = 0;
		uint16_t background_pos_y   = 0;
		uint8_t  background_data[CursorSize * CursorSize] = {0};

		uint8_t cursor_type = 0;

		// Cursor shape definition
		uint16_t text_and_mask     = 0;
		uint16_t text_xor_mask     = 0;
		uint8_t  user_screen_mask  = false;
		uint8_t  user_cursor_mask  = false;
		uint16_t user_def_screen_mask[CursorSize] = {0};
		uint16_t user_def_cursor_mask[CursorSize] = {0};

		// User callback
		uint16_t user_callback_mask    = 0;
		uint16_t user_callback_segment = 0;
		uint16_t user_callback_offset  = 0;

	} GCC_ATTRIBUTE(packed);

	static State virtual_driver_state;

	#ifdef _MSC_VER
	#pragma pack()
	#endif
};

#endif // DOSBOX_MOUSEIF_DOS_DRIVER_STATE_H
