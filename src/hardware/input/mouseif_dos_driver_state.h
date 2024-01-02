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

#ifndef DOSBOX_MOUSEIF_DOS_DRIVER_STATE_H
#define DOSBOX_MOUSEIF_DOS_DRIVER_STATE_H

#include "mem.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

// This module has just one purpose - to provide virtual DOS mouse driver
// with data storage, which is fully accessible from the guest side and can be
// paged by environment like Enhanced mode Windows 3.x


enum class MouseCursorType : uint8_t { Software = 0, Hardware = 1, Text = 2 };


static constexpr uint8_t num_buttons = 3;
static constexpr uint8_t cursor_size = 16;


class MouseDriverState {
public:
	static bool Initialize();
	static void ClearWindowsStruct();
	static RealPt SetupWindowsStruct(const RealPt link_pointer);

	static void SetWin386Running(const bool value);
	static bool IsWin386Running();

	static void SetWin386DrawingCursor(const bool value);
	static bool IsWin386DrawingCursor();

	static void SetMickeysPerPixelX(const float value);
	static void SetMickeysPerPixelY(const float value);
	static float GetMickeysPerPixelX();
	static float GetMickeysPerPixelY();

	static void SetPixelsPerMickeyX(const float value);
	static void SetPixelsPerMickeyY(const float value);
	static float GetPixelsPerMickeyX();
	static float GetPixelsPerMickeyY();

	static void SetSensitivityCoeffX(const float value);
	static void SetSensitivityCoeffY(const float value);
	static float GetSensitivityCoeffX();
	static float GetSensitivityCoeffY();

	static void SetAbsoluteX(const float value);
	static void SetAbsoluteY(const float value);
	static float GetAbsoluteX();
	static float GetAbsoluteY();

	static void SetMickeyCounterX(const float value);
	static void SetMickeyCounterY(const float value);
	static float GetMickeyCounterX();
	static float GetMickeyCounterY();

	static void SetTimesPressed(const size_t idx, const uint16_t value);
	static void SetTimesReleased(const size_t idx, const uint16_t value);
	static void SetLastPressedX(const size_t idx, const uint16_t value);
	static void SetLastPressedY(const size_t idx, const uint16_t value);
	static void SetLastReleasedX(const size_t idx, const uint16_t value);
	static void SetLastReleasedY(const size_t idx, const uint16_t value);
	static void SetLastWheelMovedX(const uint16_t value);
	static void SetLastWheelMovedY(const uint16_t value);
	static uint16_t GetTimesPressed(const size_t idx);
	static uint16_t GetTimesReleased(const size_t idx);
	static uint16_t GetLastPressedX(const size_t idx);
	static uint16_t GetLastPressedY(const size_t idx);
	static uint16_t GetLastReleasedX(const size_t idx);
	static uint16_t GetLastReleasedY(const size_t idx);
	static uint16_t GetLastWheelMovedX();
	static uint16_t GetLastWheelMovedY();

	static void SetEnabled(const bool value);
	static bool IsEnabled();

	static void SetWheelApi(const bool value);
	static bool IsWheelApi();

	static void SetDoubleSpeedThreshold(const uint16_t value);
	static uint16_t GetDoubleSpeedThreshold();

	static void SetGranularityX(const uint16_t value);
	static void SetGranularityY(const uint16_t value);
	static uint16_t GetGranularityX();
	static uint16_t GetGranularityY();

	static void SetUpdateRegionX1(const int16_t value);
	static void SetUpdateRegionY1(const int16_t value);
	static void SetUpdateRegionX2(const int16_t value);
	static void SetUpdateRegionY2(const int16_t value);
	static int16_t GetUpdateRegionX1();
	static int16_t GetUpdateRegionY1();
	static int16_t GetUpdateRegionX2();
	static int16_t GetUpdateRegionY2();

	static void SetLanguage(const uint16_t value);
	static uint16_t GetLanguage();

	static void SetBiosScreenMode(const uint8_t value);
	static uint8_t GetBiosScreenMode();

	static void SetSensitivityX(const uint8_t value);
	static void SetSensitivityY(const uint8_t value);
	static uint8_t GetSensitivityX();
	static uint8_t GetSensitivityY();

	static void SetUnknown01(const uint8_t value);
	static uint8_t GetUnknown01();

	static void SetMinPosX(const int16_t value);
	static void SetMinPosY(const int16_t value);
	static void SetMaxPosX(const int16_t value);
	static void SetMaxPosY(const int16_t value);
	static int16_t GetMinPosX();
	static int16_t GetMinPosY();
	static int16_t GetMaxPosX();
	static int16_t GetMaxPosY();

	static void SetPage(const uint8_t value);
	static uint8_t GetPage();

	static void SetInhibitDraw(const bool value);
	static bool IsInhibitDraw();

	static void SetHidden(const uint16_t value);
	static uint16_t GetHidden();
	static void SetOldHidden(const uint16_t value);
	static uint16_t GetOldHidden();

	static void SetClipX(const int16_t value);
	static void SetClipY(const int16_t value);
	static int16_t GetClipX();
	static int16_t GetClipY();

	static void SetHotX(const int16_t value);
	static void SetHotY(const int16_t value);
	static int16_t GetHotX();
	static int16_t GetHotY();

	static void SetBackgroundEnabled(const bool value);
	static bool IsBackgroundEnabled();

	static void SetBackgroundX(const uint16_t value);
	static void SetBackgroundY(const uint16_t value);
	static uint16_t GetBackgroundX();
	static uint16_t GetBackgroundY();

	static void SetBackgroundData(const size_t idx, const uint8_t value);
	static uint8_t GetBackgroundData(const size_t idx);

	static void SetCursorType(const MouseCursorType value);
	static MouseCursorType GetCursorType();

	static void SetTextMaskAnd(const uint16_t value);
	static void SetTextMaskXor(const uint16_t value);
	static uint16_t GetTextMaskAnd();
	static uint16_t GetTextMaskXor();

	static void SetUserScreenMask(const bool value);
	static void SetUserCursorMask(const bool value);
	static bool IsUserScreenMask();
	static bool IsUserCursorMask();

	static void SetUserScreenMaskData(const std::array<uint16_t, cursor_size> &value);
	static void SetUserCursorMaskData(const std::array<uint16_t, cursor_size> &value);
	static std::array<uint16_t, cursor_size> GetUserScreenMaskData();
	static std::array<uint16_t, cursor_size> GetUserCursorMaskData();

	static void SetCallbackReturn(const uint16_t segment,
		                       const uint16_t offset);
	static uint16_t GetCallbackReturnSegment();
	static uint16_t GetCallbackReturnOffset();

	static void SetUserCallback(const uint16_t segment,
		                    const uint16_t offset);
	static uint16_t GetUserCallbackSegment();
	static uint16_t GetUserCallbackOffset();

	static void SetUserCallbackMask(const uint16_t value);
	static uint16_t GetUserCallbackMask();

private:
	static uint16_t guest_data_segment;

	static uint8_t ReadB(const size_t offset);
	static uint16_t ReadW(const size_t offset);
	static uint32_t ReadD(const size_t offset);
	static void WriteB(const size_t offset, const  uint8_t value);
	static void WriteW(const size_t offset, const  uint16_t value);
	static void WriteD(const size_t offset, const  uint32_t value);
};

#endif // DOSBOX_MOUSEIF_DOS_DRIVER_STATE_H
