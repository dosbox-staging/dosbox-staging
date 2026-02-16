// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MOUSE_H
#define DOSBOX_MOUSE_H

#include <array>
#include <regex>
#include <string>
#include <vector>

#include "config/config.h"
#include "dosbox.h"
#include "utils/rect.h"

// ***************************************************************************
// Initialization, configuration
// ***************************************************************************

void MOUSE_AddConfigSection(const ConfigPtr &);
void MOUSE_Init();

// ***************************************************************************
// Data types
// ***************************************************************************

enum class MouseInterfaceId {
	DOS,  // emulated DOS mouse driver
	PS2,  // PS/2 mouse (this includes VMware and VirtualBox protocols)
	COM1, // serial mouse
	COM2,
	COM3,
	COM4,
};

constexpr std::array<MouseInterfaceId, 6> AllMouseInterfaceIds = {
	MouseInterfaceId::DOS,
	MouseInterfaceId::PS2,
	MouseInterfaceId::COM1,
	MouseInterfaceId::COM2,
	MouseInterfaceId::COM3,
	MouseInterfaceId::COM4,
};

enum class MouseMapStatus {
	HostPointer,
	Mapped,       // single physical mouse mapped to emulated port
	Disconnected, // physical mouse used to be mapped, but got unplugged
	Disabled
};

// Each mouse button has a corresponding fixed identifying value, similar to
// keyboard scan codes.
enum class MouseButtonId : uint8_t {
	Left   = 0,
	Right  = 1,
	Middle = 2,
	Extra1 = 3,
	Extra2 = 4,

	// Aliases
	First = Left,
	Last  = Extra2,
	None  = UINT8_MAX,
};

enum class MouseHint {
	None,                    // no hint to display
	CapturedHotkey,          // captured, hotkey to release
	CapturedHotkeyMiddle,    // captured, hotkey or middle-click release
	ReleasedHotkey,          // released, hotkey to capture
	ReleasedHotkeyMiddle,    // released, hotkey or middle-click to capture
	ReleasedHotkeyAnyButton, // released, hotkey or any click to capture
	SeamlessHotkey,          // seamless, hotkey to capture
	SeamlessHotkeyMiddle,    // seamless, hotkey or middle-click to capture
};

// ***************************************************************************
// Notifications from external subsystems - all should go via these methods
// ***************************************************************************

void MOUSE_EventMoved(const float x_rel, const float y_rel,
                      const float x_abs, const float y_abs);
void MOUSE_EventMoved(const float x_rel, const float y_rel,
                      const MouseInterfaceId device_id);

void MOUSE_EventButton(const MouseButtonId button_id, const bool pressed);
void MOUSE_EventButton(const MouseButtonId button_id, const bool pressed,
                       const MouseInterfaceId device_id);

void MOUSE_EventWheel(const float w_rel);
void MOUSE_EventWheel(const int16_t w_rel, const MouseInterfaceId device_id);

// Notify that guest OS is being booted, so that certain
// parts of the emulation (like DOS driver) should be disabled
void MOUSE_NotifyBooting();

// Notify that the translation language has changed
void MOUSE_NotifyLanguageChanged();

// Notify that GFX subsystem (currently SDL) is started
// and can accept requests from mouse emulation module
void MOUSE_NotifyReadyGFX();

// Notify whether emulator window is active, this tells the mouse
// emulation code if it should process mouse events or ignore them
void MOUSE_NotifyWindowActive(const bool is_active);

// A GUI has to use this function to tell when it takes over or releases
// the mouse; this will change various settings like raw input (we don't
// want it for the GUI) or cursor visibility (we want the host cursor
// visible while a GUI is running)
void MOUSE_NotifyTakeOver(const bool gui_has_taken_over);

struct MouseScreenParams {
	// The draw rectangle in logical units. Note the (x1,y1) upper-left
	// coordinates can be negative if we're "zooming into" the DOS content
	// (e.g., in 'relative' viewport mode), in which case the draw rect
	// extends beyond the dimensions of the screen/window.
	DosBox::Rect draw_rect = {};

	// New absolute mouse cursor position in logical units
	float x_abs = 0;
	float y_abs = 0;

	// Whether the new mode is fullscreen or windowed
	bool is_fullscreen = false;

	// Whether more than one display was detected
	bool is_multi_display = false;
};

// To be called when screen mode changes, emulator window gets resized, etc.
void MOUSE_NewScreenParams(const MouseScreenParams &params);

// Notification that user pressed/released the hotkey combination
// to capture/release the mouse
void MOUSE_ToggleUserCapture(const bool pressed);

// ***************************************************************************
// BIOS mouse interface for PS/2 mouse
// ***************************************************************************

void MOUSEBIOS_Subfunction_C2();

// ***************************************************************************
// Register-level interface for PS/2 mouse
// ***************************************************************************

void MOUSEPS2_FlushBuffer();
bool MOUSEPS2_SendPacket();

// ***************************************************************************
// DOS mouse driver
// ***************************************************************************

bool MOUSEDOS_NeedsAutoexecEntry();

bool MOUSEDOS_IsDriverStarted();
bool MOUSEDOS_StartDriver(const bool force_low_memory = false);

void MOUSEDOS_BeforeNewVideoMode();
void MOUSEDOS_AfterNewVideoMode(const bool is_mode_changing);

// Handle communication of the simulated DOS driver with Windows via multiplex
// functions 0x1605/0x1606//0x1607
void MOUSEDOS_HandleWindowsStartup();
void MOUSEDOS_HandleWindowsShutdown();
void MOUSEDOS_HandleWindowsCallout();

// Notify if Windows goes to the background, DOS instance goes fullscreen
void MOUSEDOS_NotifyWindowsBackground();

// Notify if Windows goes to the foreground back again
void MOUSEDOS_NotifyWindowsForeground();

// ***************************************************************************
// Virtual Machine Manager (VMware/VirtualBox) PS/2 mouse protocol extensions
// ***************************************************************************

enum class MouseVmmProtocol : uint8_t {
	VirtualBox,
	VmWare,
};

struct MouseVirtualBoxPointerStatus {
	uint16_t absolute_x = 0;
	uint16_t absolute_y = 0;
};

struct MouseVmWarePointerStatus {
	uint16_t absolute_x = 0;
	uint16_t absolute_y = 0;

	uint8_t buttons       = 0;
	uint8_t wheel_counter = 0;
};

bool MOUSEVMM_IsSupported(const MouseVmmProtocol protocol);

void MOUSEVMM_EnableImmediateInterrupts(const bool enable);

void MOUSEVMM_Activate(const MouseVmmProtocol protocol);
void MOUSEVMM_Deactivate(const MouseVmmProtocol protocol);
void MOUSEVMM_DeactivateAll();

void MOUSEVMM_GetPointerStatus(MouseVirtualBoxPointerStatus& status);
void MOUSEVMM_GetPointerStatus(MouseVmWarePointerStatus& status);

void MOUSEVMM_SetPointerVisible_VirtualBox(const bool is_visible);
bool MOUSEVMM_CheckIfUpdated_VmWare();

// ***************************************************************************
// Serial port mouse device interface
// ***************************************************************************

class CSerialMouse;

enum class MouseModelCom {
	NoMouse, // dummy value or no mouse
	Microsoft,
	Logitech,
	Wheel,
	MouseSystems
};

// Configured default serial mouse model
MouseModelCom MOUSECOM_GetConfiguredModel();
// Whether the serial mouse should auto-switch to Mouse Systems mouse emulation
bool MOUSECOM_GetConfiguredAutoMsm();

bool MOUSECOM_ParseComModel(const std::string& model_str,
                            MouseModelCom& model,
                            bool& auto_msm);

void MOUSECOM_RegisterListener(const MouseInterfaceId interface_id,
                               CSerialMouse& listener);
void MOUSECOM_UnregisterListener(const MouseInterfaceId interface_id);

void MOUSECOM_NotifyInterfaceRate(const MouseInterfaceId interface_id,
                                  const uint16_t rate_hz);

// ***************************************************************************
// Generic utility functions, made public for serial port mouse
// ***************************************************************************

float MOUSE_ClampRelativeMovement(const float rel);
float MOUSE_ClampWheelMovement(const float rel);
uint16_t MOUSE_ClampRateHz(const uint16_t rate_hz);

bool MOUSE_HasAccumulatedInt(const float delta);
int8_t MOUSE_ConsumeInt8(float& delta, const bool skip_delta_update = false);
int16_t MOUSE_ConsumeInt16(float& delta, const bool skip_delta_update = false);

// ***************************************************************************
// MOUSECTL.COM / GUI configurator interface
// ***************************************************************************

class MouseInterface;
class MousePhysical;

class MouseInterfaceInfoEntry final {
public:
	bool IsEmulated() const;
	bool IsMapped() const;
	bool IsMapped(const uint8_t physical_device_idx) const;
	bool IsMappedDeviceDisconnected() const;

	MouseInterfaceId GetInterfaceId() const;
	MouseMapStatus GetMapStatus() const;
	const std::string &GetMappedDeviceName() const;
	int16_t GetSensitivityX() const; // -999 to +999
	int16_t GetSensitivityY() const; // -999 to +999
	uint16_t GetMinRate() const;     // 10-500, 0 for none
	uint16_t GetRate() const;        // current rate, 10-500, 0 for N/A

private:
	friend class MouseInterface;
	MouseInterfaceInfoEntry(const MouseInterfaceId interface_id);

	const MouseInterfaceId interface_id;

	const MouseInterface &Interface() const;
	const MousePhysical &MappedPhysical() const;
};

class MousePhysicalInfoEntry final {
public:
	bool IsMapped() const;
	bool IsDeviceDisconnected() const;
	const std::string &GetDeviceName() const;

private:
	friend class ManyMouseGlue;
	MousePhysicalInfoEntry(const uint8_t idx);

	const uint8_t idx;
	const MousePhysical &Physical() const;
};

class MouseControlAPI final {
public:
	// Always destroy the object once it is not needed anymore
	// (configuration tool finishes it's job) and we are returning
	// to normal code execution!
	//
	// TODO consider using a singleton instead

	MouseControlAPI();
	~MouseControlAPI();

	// Empty list = performs operation on all emulated interfaces
	typedef std::vector<MouseInterfaceId> ListIDs;

	// Do not use the references after object gets destroyed
	const std::vector<MouseInterfaceInfoEntry> &GetInfoInterfaces() const;
	const std::vector<MousePhysicalInfoEntry> &GetInfoPhysical();

	static bool IsNoMouseMode();
	static bool IsMappingBlockedByDriver();

	using MappingSupport = enum {
		Supported,            // fully supported
		NotCompiledIn,        // ManyMouse not included in the build
		NotAvailableRawInput, // user has to disable 'mouse_raw_input'
	};
	static MappingSupport IsMappingSupported();

	static bool CheckInterfaces(const ListIDs& list_ids);
	static bool PatternToRegex(const std::string& pattern, std::regex& regex);

	// This one is ONLY for interactive mapping in MOUSECTL.COM!
	bool MapInteractively(const MouseInterfaceId interface_id,
	                      uint8_t &physical_device_idx);

	bool Map(const MouseInterfaceId interface_id,
	         const uint8_t physical_device_idx);
	bool Map(const MouseInterfaceId interface_id,
	         const std::regex &regex);
	bool UnMap(const ListIDs &list_ids);

	bool OnOff(const ListIDs &list_ids, const bool enable);
	bool Reset(const ListIDs &list_ids);

	// Valid sensitivity values are from -999 to +999
	bool SetSensitivity(const ListIDs &list_ids,
	                    const int16_t sensitivity_x,
	                    const int16_t sensitivity_y);
	bool SetSensitivityX(const ListIDs &list_ids, const int16_t sensitivity_x);
	bool SetSensitivityY(const ListIDs &list_ids, const int16_t sensitivity_y);

	bool ResetSensitivity(const ListIDs &list_ids);
	bool ResetSensitivityX(const ListIDs &list_ids);
	bool ResetSensitivityY(const ListIDs &list_ids);

	static const std::vector<uint16_t> &GetValidMinRateList();
	static const std::string &GetValidMinRateStr();
	static std::string GetInterfaceNameStr(const MouseInterfaceId interface_id);

	bool SetMinRate(const MouseControlAPI::ListIDs &list_ids,
	                const uint16_t value_hz);
	bool ResetMinRate(const MouseControlAPI::ListIDs &list_ids);

private:
	MouseControlAPI(const MouseControlAPI &)            = delete;
	MouseControlAPI &operator=(const MouseControlAPI &) = delete;

	bool was_interactive_mapping_started = false;
};

#endif // DOSBOX_MOUSE_H
