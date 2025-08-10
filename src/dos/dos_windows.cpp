// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos_windows.h"

#include "callback.h"
#include "util/checks.h"
#include "cpu.h"
#include "logging.h"
#include "mouse.h"
#include "regs.h"

CHECK_NARROWING();

// Reference:
// - Microsoft Windows 3.1 Device Development Kit documentation

static bool is_windows_started = false;
static bool is_enhanced_mode   = false;

static uint8_t windows_version_major = 0;
static uint8_t windows_version_minor = 0;

// Helper class for storing and restoring the CPU registers - to ensure that
// Windows can be called any time, without damaging data stored in the regs
class RegStorage {
public:
	RegStorage()
	{
		old_eax = reg_eax;
		old_ebx = reg_ebx;
		old_ecx = reg_ecx;
		old_edx = reg_edx;

		old_edi = reg_edi;

		old_es = SegValue(es);
		old_ds = SegValue(ds);

		old_flags = reg_flags;
	}

	~RegStorage()
	{
		reg_eax = old_eax;
		reg_ebx = old_ebx;
		reg_ecx = old_ecx;
		reg_edx = old_edx;

		reg_edi = old_edi;

		if (SegValue(es) != old_es) {
			CPU_SetSegGeneral(es, old_es);
		}
		if (SegValue(ds) != old_ds) {
			CPU_SetSegGeneral(ds, old_ds);
		}

		reg_flags = old_flags;
	}

	RegStorage(const RegStorage& other)            = delete;
	RegStorage& operator=(const RegStorage& other) = delete;

private:
	uint32_t old_eax = 0;
	uint32_t old_ebx = 0;
	uint32_t old_ecx = 0;
	uint32_t old_edx = 0;

	uint32_t old_edi = 0;

	uint16_t old_es = 0;
	uint16_t old_ds = 0;

	uint32_t old_flags = 0;
};

bool WINDOWS_IsStarted()
{
	return is_windows_started;
}

std::pair<uint8_t, uint8_t> WINDOWS_GetVersion()
{
	if (!is_windows_started) {
		return {0, 0};
	}

	return {windows_version_major, windows_version_minor};
}

bool WINDOWS_IsEnhancedMode()
{
	return is_windows_started && is_enhanced_mode;
}

PhysAddress WINDOWS_GetEnhancedModeEntryPoint()
{
	if (!is_windows_started) {
		return {0, 0};
	}

	RegStorage storage = {};

	reg_ax = 0x1602;
	CALLBACK_RunRealInt(0x2f);

	return {SegValue(es), reg_di};
}

void WINDOWS_ReleaseTimeSlice()
{
	if (!is_windows_started) {
		return;
	}

	RegStorage storage = {};

	reg_ax = 0x1680;
	CALLBACK_RunRealInt(0x2f);
}

void WINDOWS_BeginCriticalSection()
{
	if (!is_windows_started) {
		return;
	}

	RegStorage storage = {};

	reg_ax = 0x1681;
	CALLBACK_RunRealInt(0x2f);
}

void WINDOWS_EndCriticalSection()
{
	if (!is_windows_started) {
		return;
	}

	RegStorage storage = {};

	reg_ax = 0x1682;
	CALLBACK_RunRealInt(0x2f);
}

WindowsVmId WINDOWS_GetVmId()
{
	if (!is_windows_started) {
		return 0;
	}

	RegStorage storage = {};

	reg_ax = 0x1683;
	CALLBACK_RunRealInt(0x2f);
	return reg_bx;
}

PhysAddress WINDOWS_GetDeviceEntryPoint()
{
	if (!is_windows_started) {
		return {0, 0};
	}

	RegStorage storage = {};

	// Before Windows 3.0 we are required to set ES:DI to 0 before the call
	if (windows_version_major < 3) {
		CPU_SetSegGeneral(es, 0);
		reg_di = 0;
	}

	reg_ax = 0x1684;
	CALLBACK_RunRealInt(0x2f);

	return {SegValue(es), reg_di};
}

bool WINDOWS_HasInterrupt31()
{
	if (!is_windows_started) {
		return false;
	}

	RegStorage storage = {};

	reg_ax = 0x1686;
	CALLBACK_RunRealInt(0x2f);
	return (reg_ax == 0);
}

WindowsVmSwitchResult WINDOWS_SwitchVm(const WindowsVmId id,
                                       const WindowsSchedulerBoost priority_boost,
                                       const uint16_t callback_segment,
                                       const uint16_t callback_offset,
                                       const bool wait_until_interrupts_enabled,
                                       const bool wait_until_critical_section_released)
{
	if (!is_windows_started) {
		return WindowsVmSwitchResult::WindowsNotRunning;
	}

	RegStorage storage = {};

	reg_bx = id;

	reg_cx = 0;
	if (wait_until_interrupts_enabled) {
		reg_cx |= (1 << 0);
	}
	if (wait_until_critical_section_released) {
		reg_cx |= (1 << 1);
	}

	reg_dx = static_cast<uint16_t>(enum_val(priority_boost) >> 16);
	reg_si = static_cast<uint16_t>(enum_val(priority_boost) & UINT16_MAX);

	CPU_SetSegGeneral(es, callback_segment);
	reg_di = callback_offset;

	reg_ax = 0x1685;
	CALLBACK_RunRealInt(0x2f);

	if (reg_flags & FLAG_CF) {
		// Failed, return Windows error code
		return static_cast<WindowsVmSwitchResult>(reg_ax);
	}

	return WindowsVmSwitchResult::OK;
}

bool WINDOWS_Int2F_Handler()
{
	auto check_for_enhanced_mode = []() {
		RegStorage storage = {};

		reg_ax = 0x1600;
		CALLBACK_RunRealInt(0x2f);

		is_enhanced_mode = (reg_al != 0x00) && (reg_al != 0x80);
	};

	switch (reg_ax) {
	case 0x1605:
		// Windows startup initiated
		{
			windows_version_major = static_cast<uint8_t>(reg_di >> 8);
			windows_version_minor = static_cast<uint8_t>(reg_di & 0xff);
			LOG_INFO("DOS: Starting Microsoft Windows %d.%d",
			         windows_version_major,
			         windows_version_minor);
			is_windows_started = true;
			check_for_enhanced_mode();
			MOUSEDOS_HandleWindowsStartup();
			return false;
		}
	case 0x1608:
		// Windows startup complete
		// (it seems it is only called in 386 Enhanced mode)
		LOG_DEBUG("DOS: Microsoft Windows startup complete");
		// Looks like the enhanced mode is only enabled at this stage,
		// at least for the most common Windows versions
		check_for_enhanced_mode();
		MOUSEDOS_HandleWindowsStartup();
		return false;
	case 0x1609:
		// Windows shutdown initiated
		// (it seems it is only called in 386 Enhanced mode)
		LOG_DEBUG("DOS: Shutting down Microsoft Windows");
		check_for_enhanced_mode();
		return false;
	case 0x1606:
		// Windows shutdown complete
		LOG_INFO("DOS: Microsoft Windows shutdown complete");
		is_windows_started = false;
		is_enhanced_mode   = false;
		MOUSEDOS_HandleWindowsShutdown();
		return false;
	case 0x1607:
		// Windows device callout
		MOUSEDOS_HandleWindowsCallout();
		return false;
	case 0x4001:
		// Windows goes into the background
		LOG_DEBUG("DOS: Microsoft Windows going background");
		MOUSEDOS_NotifyWindowsBackground();
		return false;
	case 0x4002:
		// Windows returns to the foreground
		LOG_DEBUG("DOS: Microsoft Windows going foreground");
		MOUSEDOS_NotifyWindowsForeground();
		return false;
	default:
		return false;
	}
}

void WINDOWS_NotifyBooting()
{
	is_windows_started = false;
	is_enhanced_mode   = false;
}
