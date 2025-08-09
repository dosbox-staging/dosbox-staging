// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DOS_WINDOWS_H
#define DOSBOX_DOS_WINDOWS_H

#include <cstdint>
#include <utility>

#include "compiler.h"
#include "hardware/memory.h"

// Functions and definitions which can be used by programs and drivers to
// communicate with Microsoft Windows running on the simulated DOS.

// WARNING: Windows releases before 3.1 do not support all the APIs listed here,
// in particular Windows 1.0 does not support any of the calls.

using WindowsVmId = uint16_t;

// Virtual Machine ID used by the Windows kernel
constexpr WindowsVmId WindowsKernelVmId = 1;

// If Microsoft Windows is running (Windows 1.0 won't be detected).
bool WINDOWS_IsStarted();

// Get the major/minor version of the currently running Windows
std::pair<uint8_t, uint8_t> WINDOWS_GetVersion();

// Check if Windows is running in the 386 Enhanced mode
bool WINDOWS_IsEnhancedMode();

// Get address of the 386 Enhanced mode Windows entry point procedure. Only
// intended to be used under Windows 2.x, under Windows 3.x it is provided
// for compatibility with earlier software
PhysAddress WINDOWS_GetEnhancedModeEntryPoint();

// Stop running current VM, switch to the next one; call if program is idle
void WINDOWS_ReleaseTimeSlice();

// Critical section prevents content (VM) switching, except for serving
// hardware interrupts
void WINDOWS_BeginCriticalSection();
void WINDOWS_EndCriticalSection();

// Get the current Windows Virtual Machine ID; 0 if error
WindowsVmId WINDOWS_GetVmId();

// Retrieve entry point (segment and offset) of the virtual device driver
PhysAddress WINDOWS_GetDeviceEntryPoint();

// Check if Window provides interrupt 0x31 services
bool WINDOWS_HasInterrupt31();

// Virtual Machine priority boost
enum class WindowsSchedulerBoost : uint32_t {
	// Currently running VM priority
	CurrentlyRunningVm = 1 << 2,
	// For important events, which are not really time critical
	LowPriorityDevice  = 1 << 4,
	// For important, time critical events
	HighPriorityDevice = 1 << 12,
	// Priority of VMs within the critical section
	CriticalSection    = 1 << 20,
	// For emulating hardware interrupts, ignores critical sections
	TimeCritical       = 1 << 22,
	// Reserved values, do not use
	// - ReservedLow  = 1 << 0,
	// - ReservedHigh = 1 << 30,
};

enum class WindowsVmSwitchResult : uint16_t {
	OK                   = 0x0000,
	InvalidVmId          = 0x0001,
	InvalidPriorityBoost = 0x0002,
	InvalidFlags         = 0x0003,
	// DOSBox specific return code
	WindowsNotRunning = 0xFFFF,
};

// Switch the VM and run the given callback function
WindowsVmSwitchResult WINDOWS_SwitchVm(const WindowsVmId id,
                                       const WindowsSchedulerBoost priority_boost,
                                       const uint16_t callback_segment,
                                       const uint16_t callback_offset,
                                       const bool wait_until_interrupts_enabled,
                                       const bool wait_until_critical_section_released);

// Structs needed to register Windows-compatible DOS drivers

#ifdef _MSC_VER
#pragma pack(1)
#endif

struct Win386State {
	uint8_t running        = 0;
	uint8_t drawing_cursor = 0;
} GCC_ATTRIBUTE(packed);

struct Win386Startup {
	uint8_t version_minor         = 0;
	uint8_t version_major         = 0;
	RealPt next_info_ptr          = 0;
	RealPt device_driver_ptr      = 0;
	RealPt device_driver_data_ptr = 0;
	RealPt instance_data_ptr      = 0;
} GCC_ATTRIBUTE(packed);

struct Win386Instances {
	RealPt instance0_data_ptr = 0;
	uint16_t instance0_size   = 0;
	RealPt instance1_data_ptr = 0;
	uint16_t instance1_size   = 0;
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack()
#endif

// Multiplex handler for Windows interrupt 0x2F, do not call it directly
bool WINDOWS_Int2F_Handler();

// Notify the guest Windows subsystem that we are booting a real OS now
void WINDOWS_NotifyBooting();

#endif // DOSBOX_DOS_WINDOWS_H
