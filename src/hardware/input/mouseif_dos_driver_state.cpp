// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/mouseif_dos_driver_state.h"

#include <bit>
#include <cstring>

#include "utils/checks.h"
#include "misc/support.h"

CHECK_NARROWING();

// State of virtual (non-TSR) mouse driver
MouseDriverState::State MouseDriverState::virtual_driver_state = {};

// A trick to require a semicolon after each SET_* macro call
#define REQUIRE_SEMICOLON static_assert(true, "semicolon required")

// Boolean
#define GET_BOOL(f) \
	(pt ? static_cast<bool>(SGET_BYTE(State, f)) \
	    : static_cast<bool>(virtual_driver_state.f))
#define SET_BOOL(f, v) \
	if (pt) { \
		const auto tmp = static_cast<uint8_t>(v ? 1 : 0); \
		SSET_BYTE(State, f, tmp); \
	} else { \
		virtual_driver_state.f = v; \
	} \
	REQUIRE_SEMICOLON

// Unsigned integer, 8-bit
#define GET_UINT8(f) \
	(pt ? SGET_BYTE(State, f) : static_cast<uint8_t>(virtual_driver_state.f))
#define SET_UINT8(f, v) \
	if (pt) { \
		SSET_BYTE(State, f, v); \
	} else { \
		virtual_driver_state.f = v; \
	} \
	REQUIRE_SEMICOLON

// Unsigned integer, 16-bit
#define GET_UINT16(f) \
	(pt ? SGET_WORD(State, f) : static_cast<uint16_t>(virtual_driver_state.f))
#define SET_UINT16(f, v) \
	if (pt) { \
		SSET_WORD(State, f, v); \
	} else { \
		virtual_driver_state.f = v; \
	} \
	REQUIRE_SEMICOLON

// Unsigned integer, 32-bit
#define GET_UINT32(f) \
	(pt ? SGET_DWORD(State, f) : static_cast<uint32_t>(virtual_driver_state.f))
#define SET_UINT32(f, v) \
	if (pt) { \
		SSET_DWORD(State, f, v); \
	} else { \
		virtual_driver_state.f = v; \
	} \
	REQUIRE_SEMICOLON

// Signed integer, 8-bit
#define GET_INT8(f) \
	(pt ? std::bit_cast<int8_t>(SGET_BYTE(State, f)) \
	    : std::bit_cast<int8_t>(virtual_driver_state.f))
#define SET_INT8(f, v) \
	if (pt) { \
		const auto tmp = std::bit_cast<uint8_t>(v); \
		SSET_BYTE(State, f, tmp); \
	} else { \
		virtual_driver_state.f = v; \
	} \
	REQUIRE_SEMICOLON

// Signed integer, 16-bit
#define GET_INT16(f) \
	(pt ? std::bit_cast<int16_t>(SGET_WORD(State, f)) \
	    : std::bit_cast<int16_t>(virtual_driver_state.f))
#define SET_INT16(f, v) \
	if (pt) { \
		const auto tmp = std::bit_cast<uint16_t>(v); \
		SSET_WORD(State, f, tmp); \
	} else { \
		virtual_driver_state.f = v; \
	} \
	REQUIRE_SEMICOLON

// Signed integer, 32-bit
#define GET_INT32(f) \
	(pt ? std::bit_cast<int32_t>(SGET_DWORD(State, f)) \
	    : std::bit_cast<int32_t>(virtual_driver_state.f))
#define SET_INT32(f, v) \
	if (pt) { \
		const auto tmp = std::bit_cast<uint32_t>(v); \
		SSET_DWORD(State, f, tmp); \
	} else { \
		virtual_driver_state.f = v; \
	} \
	REQUIRE_SEMICOLON

// Float, 32-bit
static_assert(sizeof(float) == sizeof(uint32_t));
#define GET_FLOAT(f) \
	(pt ? std::bit_cast<float>(SGET_DWORD(State, f)) : virtual_driver_state.f)
#define SET_FLOAT(f, v) \
	if (pt) { \
		const auto tmp = std::bit_cast<uint32_t>(v); \
		SSET_DWORD(State, f, tmp); \
	} else { \
		virtual_driver_state.f = v; \
	} \
	REQUIRE_SEMICOLON

// Array of unsigned integer, 8-bit
#define GET_UINT8_ARRAY(f, i) \
	(pt ? SGET_BYTE_ARRAY(State, f, i) : virtual_driver_state.f[i])
#define SET_UINT8_ARRAY(f, i, v) \
	if (pt) { \
		SSET_BYTE_ARRAY(State, f, i, v); \
	} else { \
		virtual_driver_state.f[i] = v; \
	} \
	REQUIRE_SEMICOLON

// Array of unsigned integer, 16-bit
#define GET_UINT16_ARRAY(f, i) \
	(pt ? SGET_WORD_ARRAY(State, f, i) : virtual_driver_state.f[i])
#define SET_UINT16_ARRAY(f, i, v) \
	if (pt) { \
		SSET_WORD_ARRAY(State, f, i, v); \
	} else { \
		virtual_driver_state.f[i] = v; \
	} \
	REQUIRE_SEMICOLON

// Array of signed integer, 16-bit
#define GET_INT16_ARRAY(f, i) \
	(pt ? std::bit_cast<int16_t>(SGET_WORD_ARRAY(State, f, i)) \
	    : std::bit_cast<int16_t>(virtual_driver_state.f[i]))
#define SET_INT16_ARRAY(f, i, v) \
	if (pt) { \
		const auto tmp = std::bit_cast<uint16_t>(v); \
		SSET_WORD_ARRAY(State, f, i, tmp); \
	} else { \
		virtual_driver_state.f[i] = v; \
	} \
	REQUIRE_SEMICOLON


std::vector<uint8_t> MouseDriverState::ReadBinaryData() const
{
	std::vector<uint8_t> result = {};
	result.resize(GetSize());

	if (pt) {
		MEM_BlockRead(pt, result.data(), result.size());
	} else {
		std::memcpy(result.data(), &virtual_driver_state, GetSize());
	}

	return result;
}

void MouseDriverState::WriteBinaryData(const std::vector<uint8_t>& source)
{
	if (source.size() != GetSize()) {
		assert(false);
		return;
	}

	if (pt) {
		MEM_BlockWrite(pt, source.data(), source.size());
	} else {
		std::memcpy(&virtual_driver_state, source.data(), GetSize());
	}
}

uint16_t MouseDriverState::GetWin386StateOffset() const
{
	static_assert(sizeof(State) <= UINT16_MAX);
	return static_cast<uint16_t>(offsetof(State, win386_state));
}

uint16_t MouseDriverState::GetWin386StartupOffset() const
{
	static_assert(sizeof(State) <= UINT16_MAX);
	return static_cast<uint16_t>(offsetof(State, win386_startup));
}

uint16_t MouseDriverState::GetWin386InstancesOffset() const
{
	static_assert(sizeof(State) <= UINT16_MAX);
	return static_cast<uint16_t>(offsetof(State, win386_instances));
}

uint8_t MouseDriverState::Win386State_GetRunning() const
{
	return GET_UINT8(win386_state.running);
}

void MouseDriverState::Win386State_SetRunning(const uint8_t value)
{
	SET_UINT8(win386_state.running, value);
}

uint8_t MouseDriverState::Win386State_GetDrawingCursor() const
{
	return GET_UINT8(win386_state.drawing_cursor);
}

void MouseDriverState::Win386State_SetDrawingCursor(const uint8_t value)
{
	SET_UINT8(win386_state.drawing_cursor, value);
}

uint8_t MouseDriverState::Win386Startup_GetVersionMinor() const
{
	return GET_UINT8(win386_startup.version_minor);
}

void MouseDriverState::Win386Startup_SetVersionMinor(const uint8_t value)
{
	SET_UINT8(win386_startup.version_minor, value);
}

uint8_t MouseDriverState::Win386Startup_GetVersionMajor() const
{
	return GET_UINT8(win386_startup.version_major);
}

void MouseDriverState::Win386Startup_SetVersionMajor(const uint8_t value)
{
	SET_UINT8(win386_startup.version_major, value);
}

RealPt MouseDriverState::Win386Startup_GetNextInfoPtr() const
{
	return GET_UINT32(win386_startup.next_info_ptr);
}

void MouseDriverState::Win386Startup_SetNextInfoPtr(const RealPt value)
{
	SET_UINT32(win386_startup.next_info_ptr, value);
}

RealPt MouseDriverState::Win386Startup_GetDeviceDriverPtr() const
{
	return GET_UINT32(win386_startup.device_driver_ptr);
}

void MouseDriverState::Win386Startup_SetDeviceDriverPtr(const RealPt value)
{
	SET_UINT32(win386_startup.device_driver_ptr, value);
}

RealPt MouseDriverState::Win386Startup_GetDeviceDriverDataPtr() const
{
	return GET_UINT32(win386_startup.device_driver_data_ptr);
}

void MouseDriverState::Win386Startup_SetDeviceDriverDataPtr(const RealPt value)
{
	SET_UINT32(win386_startup.device_driver_data_ptr, value);
}

RealPt MouseDriverState::Win386Startup_GetInstanceDataPtr() const
{
	return GET_UINT32(win386_startup.instance_data_ptr);
}

void MouseDriverState::Win386Startup_SetInstanceDataPtr(const RealPt value)
{
	SET_UINT32(win386_startup.instance_data_ptr, value);
}

RealPt MouseDriverState::Win386Instance_GetDataPtr(const size_t index) const
{
	switch (index) {
	case 0: return GET_UINT32(win386_instances.instance0_data_ptr);
	case 1: return GET_UINT32(win386_instances.instance1_data_ptr);
	default: assert(false); return 0;
	}
}

void MouseDriverState::Win386Instance_SetDataPtr(const size_t index, const RealPt value)
{
	switch (index) {
	case 0: SET_UINT32(win386_instances.instance0_data_ptr, value); return;
	case 1: SET_UINT32(win386_instances.instance1_data_ptr, value); return;
	default: assert(false); return;
	}
}

uint16_t MouseDriverState::Win386Instance_GetSize(const size_t index) const
{
	switch (index) {
	case 0: return GET_UINT16(win386_instances.instance0_size);
	case 1: return GET_UINT16(win386_instances.instance1_size);
	default: assert(false); return 0;
	}
}

void MouseDriverState::Win386Instance_SetSize(const size_t index, const uint16_t value)
{
	switch (index) {
	case 0: SET_UINT16(win386_instances.instance0_size, value); return;
	case 1: SET_UINT16(win386_instances.instance1_size, value); return;
	default: assert(false); return;
	}
}

bool MouseDriverState::Win386Pending_IsCursorMoved() const
{
	return GET_BOOL(win386_pending.is_cursor_moved);
}

bool MouseDriverState::Win386Pending_IsButtonChanged() const
{
	return GET_BOOL(win386_pending.is_button_changed);
}

void MouseDriverState::Win386Pending_SetCursorMoved(const bool value)
{
	SET_BOOL(win386_pending.is_cursor_moved, value);
}

void MouseDriverState::Win386Pending_SetButtonChanged(const bool value)
{
	SET_BOOL(win386_pending.is_button_changed, value);
}

uint16_t MouseDriverState::Win386Pending_GetXAbs() const
{
	return GET_UINT16(win386_pending.x_abs);
}

uint16_t MouseDriverState::Win386Pending_GetYAbs() const
{
	return GET_UINT16(win386_pending.y_abs);
}

void MouseDriverState::Win386Pending_SetXAbs(const uint16_t value)
{
	SET_UINT16(win386_pending.x_abs, value);
}

void MouseDriverState::Win386Pending_SetYAbs(const uint16_t value)
{
	SET_UINT16(win386_pending.y_abs, value);
}

MouseButtons12S MouseDriverState::Win386Pending_GetButtons() const
{
	return GET_UINT8(win386_pending.buttons);
}

void MouseDriverState::Win386Pending_SetButtons(const MouseButtons12S value)
{
	SET_UINT8(win386_pending.buttons, value._data);
}

bool MouseDriverState::IsWin386Cursor() const
{
	return GET_BOOL(is_win386_cursor);
}

void MouseDriverState::SetWin386Cursor(const bool value)
{
	SET_BOOL(is_win386_cursor, value);
}

bool MouseDriverState::IsEnabled() const
{
	return GET_BOOL(is_enabled);
}

void MouseDriverState::SetEnabled(const bool value)
{
	SET_BOOL(is_enabled, value);
}

uint8_t MouseDriverState::GetWheelApi() const
{
	return GET_UINT8(wheel_api);
}

void MouseDriverState::SetWheelApi(const uint8_t value)
{
	SET_UINT8(wheel_api, value);
}

float MouseDriverState::GetPosX() const
{
	return GET_FLOAT(pos_x);
}

float MouseDriverState::GetPosY() const
{
	return GET_FLOAT(pos_y);
}

void MouseDriverState::SetPosX(const float value)
{
	SET_FLOAT(pos_x, value);
}

void MouseDriverState::SetPosY(const float value)
{
	SET_FLOAT(pos_y, value);
}

int8_t MouseDriverState::GetCounterWheel() const
{
	return GET_INT8(counter_wheel);
}

void MouseDriverState::SetCounterWheel(const int8_t value)
{
	SET_INT8(counter_wheel, value);
}

MouseButtons12S MouseDriverState::GetButtons() const
{
	return GET_UINT8(buttons);
}

void MouseDriverState::SetButtons(const MouseButtons12S value)
{
	SET_UINT8(buttons, value._data);
}

uint16_t MouseDriverState::GetTimesPressed(const size_t index) const
{
	if (index >= MaxMouseButtons) {
		assert(false);
		return 0;
	}

	return GET_UINT16_ARRAY(times_pressed, index);
}

uint16_t MouseDriverState::GetTimesReleased(const size_t index) const
{
	if (index >= MaxMouseButtons) {
		assert(false);
		return 0;
	}

	return GET_UINT16_ARRAY(times_released, index);
}

void MouseDriverState::SetTimesPressed(const size_t index, const uint16_t value)
{
	if (index >= MaxMouseButtons) {
		assert(false);
		return;
	}

	SET_UINT16_ARRAY(times_pressed, index, value);
}

void MouseDriverState::SetTimesReleased(const size_t index, const uint16_t value)
{
	if (index >= MaxMouseButtons) {
		assert(false);
		return;
	}

	SET_UINT16_ARRAY(times_released, index, value);
}

uint16_t MouseDriverState::GetLastReleasedX(const size_t index) const
{
	if (index >= MaxMouseButtons) {
		assert(false);
		return 0;
	}

	return GET_UINT16_ARRAY(last_released_x, index);
}

uint16_t MouseDriverState::GetLastReleasedY(const size_t index) const
{
	if (index >= MaxMouseButtons) {
		assert(false);
		return 0;
	}

	return GET_UINT16_ARRAY(last_released_y, index);
}

void MouseDriverState::SetLastReleasedX(const size_t index, const uint16_t value)
{
	if (index >= MaxMouseButtons) {
		assert(false);
		return;
	}

	SET_UINT16_ARRAY(last_released_x, index, value);
}

void MouseDriverState::SetLastReleasedY(const size_t index, const uint16_t value)
{
	if (index >= MaxMouseButtons) {
		assert(false);
		return;
	}

	SET_UINT16_ARRAY(last_released_y, index, value);
}

uint16_t MouseDriverState::GetLastPressedX(const size_t index) const
{
	if (index >= MaxMouseButtons) {
		assert(false);
		return 0;
	}

	return GET_UINT16_ARRAY(last_pressed_x, index);
}

uint16_t MouseDriverState::GetLastPressedY(const size_t index) const
{
	if (index >= MaxMouseButtons) {
		assert(false);
		return 0;
	}

	return GET_UINT16_ARRAY(last_pressed_y, index);
}

void MouseDriverState::SetLastPressedX(const size_t index, const uint16_t value)
{
	if (index >= MaxMouseButtons) {
		assert(false);
		return;
	}

	SET_UINT16_ARRAY(last_pressed_x, index, value);
}

void MouseDriverState::SetLastPressedY(const size_t index, const uint16_t value)
{
	if (index >= MaxMouseButtons) {
		assert(false);
		return;
	}

	SET_UINT16_ARRAY(last_pressed_y, index, value);
}

uint16_t MouseDriverState::GetLastWheelMovedX() const
{
	return GET_UINT16(last_wheel_moved_x);
}

uint16_t MouseDriverState::GetLastWheelMovedY() const
{
	return GET_UINT16(last_wheel_moved_y);
}

void MouseDriverState::SetLastWheelMovedX(const uint16_t value)
{
	SET_UINT16(last_wheel_moved_x, value);
}

void MouseDriverState::SetLastWheelMovedY(const uint16_t value)
{
	SET_UINT16(last_wheel_moved_y, value);
}

float MouseDriverState::GetPreciseMickeyCounterX() const
{
	return GET_FLOAT(precise_mickey_counter_x);
}

float MouseDriverState::GetPreciseMickeyCounterY() const
{
	return GET_FLOAT(precise_mickey_counter_y);
}

void MouseDriverState::SetPreciseMickeyCounterX(const float value)
{
	SET_FLOAT(precise_mickey_counter_x, value);
}

void MouseDriverState::SetPreciseMickeyCounterY(const float value)
{
	SET_FLOAT(precise_mickey_counter_y, value);
}

int16_t MouseDriverState::GetMickeyCounterX() const
{
	return GET_INT16(mickey_counter_x);
}

int16_t MouseDriverState::GetMickeyCounterY() const
{
	return GET_INT16(mickey_counter_y);
}

void MouseDriverState::SetMickeyCounterX(const int16_t value)
{
	SET_INT16(mickey_counter_x, value);
}

void MouseDriverState::SetMickeyCounterY(const int16_t value)
{
	SET_INT16(mickey_counter_y, value);
}

float MouseDriverState::GetMickeysPerPixelX() const
{
	return GET_FLOAT(mickeys_per_pixel_x);
}

float MouseDriverState::GetMickeysPerPixelY() const
{
	return GET_FLOAT(mickeys_per_pixel_y);
}

float MouseDriverState::GetPixelsPerMickeyX() const
{
	return GET_FLOAT(pixels_per_mickey_x);
}

float MouseDriverState::GetPixelsPerMickeyY() const
{
	return GET_FLOAT(pixels_per_mickey_y);
}

void MouseDriverState::SetMickeysPerPixelX(const float value)
{
	SET_FLOAT(mickeys_per_pixel_x, value);
}

void MouseDriverState::SetMickeysPerPixelY(const float value)
{
	SET_FLOAT(mickeys_per_pixel_y, value);
}

void MouseDriverState::SetPixelsPerMickeyX(const float value)
{
	SET_FLOAT(pixels_per_mickey_x, value);
}

void MouseDriverState::SetPixelsPerMickeyY(const float value)
{
	SET_FLOAT(pixels_per_mickey_y, value);
}

uint16_t MouseDriverState::GetDoubleSpeedThreshold() const
{
	return GET_UINT16(double_speed_threshold);
}

void MouseDriverState::SetDoubleSpeedThreshold(const uint16_t value)
{
	SET_UINT16(double_speed_threshold, value);
}

uint16_t MouseDriverState::GetGranularityX() const
{
	return GET_UINT16(granularity_x);
}

uint16_t MouseDriverState::GetGranularityY() const
{
	return GET_UINT16(granularity_y);
}

void MouseDriverState::SetGranularityX(const uint16_t value)
{
	SET_UINT16(granularity_x, value);
}

void MouseDriverState::SetGranularityY(const uint16_t value)
{
	SET_UINT16(granularity_y, value);
}

int16_t MouseDriverState::GetUpdateRegionX(const size_t index) const
{
	if (index >= MaxUpdateRegions) {
		assert(false);
		return 0;
	}

	return GET_INT16_ARRAY(update_region_x, index);
}

int16_t MouseDriverState::GetUpdateRegionY(const size_t index) const
{
	if (index >= MaxUpdateRegions) {
		assert(false);
		return 0;
	}

	return GET_INT16_ARRAY(update_region_y, index);
}

void MouseDriverState::SetUpdateRegionX(const size_t index, const int16_t value)
{
	if (index >= MaxUpdateRegions) {
		assert(false);
		return;
	}

	SET_INT16_ARRAY(update_region_x, index, value);
}

void MouseDriverState::SetUpdateRegionY(const size_t index, const int16_t value)
{
	if (index >= MaxUpdateRegions) {
		assert(false);
		return;
	}

	SET_INT16_ARRAY(update_region_y, index, value);
}

uint8_t MouseDriverState::GetBiosScreenMode() const
{
	return GET_UINT8(bios_screen_mode);
}

void MouseDriverState::SetBiosScreenMode(const uint8_t value)
{
	SET_UINT8(bios_screen_mode, value);
}

uint8_t MouseDriverState::GetSensitivityX() const
{
	return GET_UINT8(sensitivity_x);
}

uint8_t MouseDriverState::GetSensitivityY() const
{
	return GET_UINT8(sensitivity_y);
}

void MouseDriverState::SetSensitivityX(const uint8_t value)
{
	SET_UINT8(sensitivity_x, value);
}

void MouseDriverState::SetSensitivityY(const uint8_t value)
{
	SET_UINT8(sensitivity_y, value);
}

uint8_t MouseDriverState::GetUnknownValue01() const
{
	return GET_UINT8(unknown_01);
}

void MouseDriverState::SetUnknownValue01(const uint8_t value)
{
	SET_UINT8(unknown_01, value);
}

float MouseDriverState::GetSensitivityCoeffX() const
{
	return GET_FLOAT(sensitivity_coeff_x);
}

float MouseDriverState::GetSensitivityCoeffY() const
{
	return GET_FLOAT(sensitivity_coeff_y);
}

void MouseDriverState::SetSensitivityCoeffX(const float value)
{
	SET_FLOAT(sensitivity_coeff_x, value);
}

void MouseDriverState::SetSensitivityCoeffY(const float value)
{
	SET_FLOAT(sensitivity_coeff_y, value);
}

int16_t MouseDriverState::GetMinPosX() const
{
	return GET_INT16(minpos_x);
}

int16_t MouseDriverState::GetMinPosY() const
{
	return GET_INT16(minpos_y);
}

int16_t MouseDriverState::GetMaxPosX() const
{
	return GET_INT16(maxpos_x);
}

int16_t MouseDriverState::GetMaxPosY() const
{
	return GET_INT16(maxpos_y);
}

void MouseDriverState::SetMinPosX(const int16_t value)
{
	SET_INT16(minpos_x, value);
}

void MouseDriverState::SetMinPosY(const int16_t value)
{
	SET_INT16(minpos_y, value);
}

void MouseDriverState::SetMaxPosX(const int16_t value)
{
	SET_INT16(maxpos_x, value);
}

void MouseDriverState::SetMaxPosY(const int16_t value)
{
	SET_INT16(maxpos_y, value);
}

uint8_t MouseDriverState::GetPage() const
{
	return GET_UINT8(page);
}

void MouseDriverState::SetPage(const uint8_t value)
{
	SET_UINT8(page, value);
}

bool MouseDriverState::IsInhibitDraw() const
{
	return GET_BOOL(inhibit_draw);
}

void MouseDriverState::SetInhibitDraw(const bool value)
{
	SET_BOOL(inhibit_draw, value);
}

uint16_t MouseDriverState::GetHidden() const
{
	return GET_UINT16(hidden);
}

uint16_t MouseDriverState::GetOldHidden() const
{
	return GET_UINT16(old_hidden);
}

void MouseDriverState::SetHidden(const uint16_t value)
{
	SET_UINT16(hidden, value);
}

void MouseDriverState::SetOldHidden(const uint16_t value)
{
	SET_UINT16(old_hidden, value);
}

int16_t MouseDriverState::GetClipX() const
{
	return GET_INT16(clip_x);
}

int16_t MouseDriverState::GetClipY() const
{
	return GET_INT16(clip_y);
}

void MouseDriverState::SetClipX(const int16_t value)
{
	SET_INT16(clip_x, value);
}

void MouseDriverState::SetClipY(const int16_t value)
{
	SET_INT16(clip_y, value);
}

int16_t MouseDriverState::GetHotX() const
{
	return GET_INT16(hot_x);
}

int16_t MouseDriverState::GetHotY() const
{
	return GET_INT16(hot_y);
}

void MouseDriverState::SetHotX(const int16_t value)
{
	SET_INT16(hot_x, value);
}

void MouseDriverState::SetHotY(const int16_t value)
{
	SET_INT16(hot_y, value);
}

bool MouseDriverState::Background_IsEnabled() const
{
	return GET_BOOL(background.enabled);
}

void MouseDriverState::Background_SetEnabled(const bool value)
{
	SET_BOOL(background.enabled, value);
}

uint16_t MouseDriverState::Background_GetPosX() const
{
	return GET_UINT16(background.pos_x);
}

uint16_t MouseDriverState::Background_GetPosY() const
{
	return GET_UINT16(background.pos_y);
}

void MouseDriverState::Background_SetPosX(const uint16_t value)
{
	SET_UINT16(background.pos_x, value);
}

void MouseDriverState::Background_SetPosY(const uint16_t value)
{
	SET_UINT16(background.pos_y, value);
}

uint8_t MouseDriverState::Background_GetData(const size_t index) const
{
	if (index >= CursorSize * CursorSize) {
		assert(false);
		return 0;
	}

	return GET_UINT8_ARRAY(background.data, index);
}

void MouseDriverState::Background_SetData(const size_t index, const uint8_t value)
{
	if (index >= CursorSize * CursorSize) {
		assert(false);
		return;
	}

	SET_UINT8_ARRAY(background.data, index, value);
}

MouseCursor MouseDriverState::GetCursorType() const
{
	return std::bit_cast<MouseCursor>(GET_UINT8(cursor_type));
}

void MouseDriverState::SetCursorType(const MouseCursor value)
{
	const auto tmp = enum_val(value);
	SET_UINT8(cursor_type, tmp);
}

uint16_t MouseDriverState::GetTextAndMask() const
{
	return GET_UINT16(text_and_mask);
}

uint16_t MouseDriverState::GetTextXorMask() const
{
	return GET_UINT16(text_xor_mask);
}

void MouseDriverState::SetTextAndMask(const uint16_t value)
{
	SET_UINT16(text_and_mask, value);
}

void MouseDriverState::SetTextXorMask(const uint16_t value)
{
	SET_UINT16(text_xor_mask, value);
}

bool MouseDriverState::IsUserScreenMask() const
{
	return GET_BOOL(user_screen_mask);
}

bool MouseDriverState::IsUserCursorMask() const
{
	return GET_BOOL(user_cursor_mask);
}

void MouseDriverState::SetUserScreenMask(const bool value)
{
	SET_BOOL(user_screen_mask, value);
}

void MouseDriverState::SetUserCursorMask(const bool value)
{
	SET_BOOL(user_cursor_mask, value);
}

uint16_t MouseDriverState::GetUserDefScreenMask(const size_t index) const
{
	if (index >= CursorSize) {
		assert(false);
		return 0;
	}

	return GET_UINT16_ARRAY(user_def_screen_mask, index);
}

uint16_t MouseDriverState::GetUserDefCursorMask(const size_t index) const
{
	if (index >= CursorSize) {
		assert(false);
		return 0;
	}

	return GET_UINT16_ARRAY(user_def_cursor_mask, index);
}

void MouseDriverState::SetUserDefScreenMask(const size_t index, const uint16_t value)
{
	if (index >= CursorSize) {
		assert(false);
		return;
	}

	SET_UINT16_ARRAY(user_def_screen_mask, index, value);
}

void MouseDriverState::SetUserDefCursorMask(const size_t index, const uint16_t value)
{
	if (index >= CursorSize) {
		assert(false);
		return;
	}

	SET_UINT16_ARRAY(user_def_cursor_mask, index, value);
}

uint16_t MouseDriverState::GetUserCallbackMask() const
{
	return GET_UINT16(user_callback_mask);
}

uint16_t MouseDriverState::GetUserCallbackSegment() const
{
	return GET_UINT16(user_callback_segment);
}

uint16_t MouseDriverState::GetUserCallbackOffset() const
{
	return GET_UINT16(user_callback_offset);
}

void MouseDriverState::SetUserCallbackMask(const uint16_t value)
{
	SET_UINT16(user_callback_mask, value);
}

void MouseDriverState::SetUserCallbackSegment(const uint16_t value)
{
	SET_UINT16(user_callback_segment, value);
}

void MouseDriverState::SetUserCallbackOffset(const uint16_t value)
{
	SET_UINT16(user_callback_offset, value);
}
