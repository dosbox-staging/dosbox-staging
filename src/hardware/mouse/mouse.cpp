/*
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "mouse.h"
#include "mouse_config.h"
#include "mouse_interfaces.h"
#include "mouse_manymouse.h"
#include "mouse_queue.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include "callback.h"
#include "checks.h"
#include "cpu.h"
#include "pic.h"
#include "video.h"

CHECK_NARROWING();

static Bitu int74_ret_callback = 0;

static MouseQueue &mouse_queue  = MouseQueue::GetInstance();
static ManyMouseGlue &manymouse = ManyMouseGlue::GetInstance();

// ***************************************************************************
// GFX-related decision making
// ***************************************************************************

static struct {
	bool is_fullscreen = false; // if full screen mode is active
	uint32_t clip_x    = 0;     // clipping = size of black border (one side)
	uint32_t clip_y    = 0;

	uint32_t cursor_x_abs  = 0;    // absolute position from start of drawing area
	uint32_t cursor_y_abs  = 0;
	bool cursor_is_outside = true; // if mouse cursor is outside of drawing area

	bool has_focus             = false; // if our window has focus
	bool gui_has_taken_over    = false; // if a GUI requested to take over the mouse
	bool capture_was_requested = false; // if user requested mouse to be captured

	bool is_captured  = false; // if GFX was requested to capture mouse
	bool is_visible   = false; // if GFX was requested to make cursor visible
	bool is_input_raw = false; // if GFX was requested to provide raw movements
	bool is_seamless  = false; // if seamless mouse integration is in effect

	bool should_drop_events       = true;  // if we should drop mouse events
	bool should_capture_on_click  = false; // if any button click should capture the mouse
	bool should_capture_on_middle = false; // if middle button press should capture the mouse
	bool should_release_on_middle = false; // if middle button press should release the mouse
	bool should_toggle_on_hotkey  = false; // if hotkey should toggle mouse capture

	MouseHint hint_id = MouseHint::None; // hint to be displayed on title bar

} state;

static void update_cursor_absolute_position(const int32_t x_abs, const int32_t y_abs)
{
	state.cursor_is_outside = false;

	auto calculate = [&](const int32_t absolute,
	                     const uint32_t clipping,
	                     const uint32_t resolution) -> uint32_t {
		assert(resolution > 1u);
		assert(clipping * 2 < resolution);

		if (absolute < 0 || static_cast<uint32_t>(absolute) < clipping) {
			// cursor is over the top or left black bar
			state.cursor_is_outside = true;
			return 0;
		} else if (static_cast<uint32_t>(absolute) >= resolution + clipping) {
			// cursor is over the bottom or right black bar
			state.cursor_is_outside = true;
			return check_cast<uint32_t>(resolution - 1);
		}

		const auto result = static_cast<uint32_t>(absolute) - clipping;
		return static_cast<uint32_t>(result);
	};

	auto &x = state.cursor_x_abs;
	auto &y = state.cursor_y_abs;

	x = calculate(x_abs, state.clip_x, mouse_shared.resolution_x);
	y = calculate(y_abs, state.clip_y, mouse_shared.resolution_y);
}

static void update_cursor_visibility()
{
	// If mouse subsystem not started yet, do nothing
	if (!mouse_shared.started)
		return;

	static bool first_time = true;

	// Store internally old settings, to avoid unnecessary GFX calls
	const auto old_is_visible = state.is_visible;

	if (!state.has_focus) {

		// No change to cursor visibility

	} else if (state.gui_has_taken_over) {

		state.is_visible = true;

	} else { // Window has focus, no GUI running

		// Host cursor should be hidden if any of:
		// - mouse cursor is captured, for any reason
		// - seamless integration is in effect
		// But show it nevertheless if:
		// - seamless integration is in effect and
		// - cursor is outside of drawing area
		state.is_visible = !(state.is_captured || state.is_seamless) ||
		                   (state.is_seamless && state.cursor_is_outside);
	}

	// Apply calculated settings if changed or if this is the first run
	if (first_time || old_is_visible != state.is_visible)
		GFX_SetMouseVisibility(state.is_visible);

	// And take a note that this is no longer the first run
	first_time = false;
}

static void update_state() // updates whole 'state' structure, except cursor visibility
{
	// If mouse subsystem not started yet, do nothing
	if (!mouse_shared.started)
		return;

	const bool is_config_on_start = (mouse_config.capture == MouseCapture::OnStart);
	const bool is_config_on_click = (mouse_config.capture == MouseCapture::OnClick);
	const bool is_config_no_mouse = (mouse_config.capture == MouseCapture::NoMouse);

	// If running for the first time, capture the mouse if this was configured
	static bool first_time = true;
	if (first_time && is_config_on_start)
		state.capture_was_requested = true;

	// We are running in seamless mode:
	// - we are not in windowed mode, and
	// - NoMouse is not configured, and
	// - seamless driver is running or Seamless capture is configured
	const bool is_seamless_config = (mouse_config.capture == MouseCapture::Seamless);
	const bool is_seamless_driver = mouse_shared.active_vmm;
	state.is_seamless = !state.is_fullscreen &&
	                    !is_config_no_mouse &&
	                    (is_seamless_driver || is_seamless_config);

	// Due to ManyMouse API limitation, we are unable to support seamless
	// integration if mapping is in effect
	const bool is_mapping = manymouse.IsMappingInEffect();
	if (state.is_seamless && is_mapping) {
		state.is_seamless = false;
		static bool already_warned = false;
		if (!already_warned) {
			LOG_WARNING("MOUSE: Mapping disables seamless pointer integration");
			already_warned = true;
		}
	}

	// Store internally old settings, to avoid unnecessary GFX calls
	const auto old_is_captured  = state.is_captured;
	const auto old_is_input_raw = state.is_input_raw;
	const auto old_hint_id      = state.hint_id;

	// Raw input depends on the user configuration
	state.is_input_raw = mouse_config.raw_input;

	if (!state.has_focus) {

		state.should_drop_events = true;

		// No change to:
		// - state.is_captured

	} else if (state.gui_has_taken_over) {

		state.is_captured = false;
		state.should_drop_events = true;

		// Override user configuration, for the GUI we want
		// host OS mouse acceleration applied
		state.is_input_raw = false;

	} else { // Window has focus, no GUI running

		// Capture mouse cursor if any of:
		// - we are in fullscreen mode
		// - user asked to capture the mouse
		state.is_captured = state.is_fullscreen ||
	                        state.capture_was_requested;

		// Drop mouse events if NoMouse is configured
		state.should_drop_events = is_config_no_mouse;
		// Also drop events if:
		// - mouse not captured, and
		// - mouse not in seamless mode (due to user setting or seamless driver)
		if (!state.is_captured && !state.is_seamless)
			state.should_drop_events = true;
	}

	// Use a hotkey to toggle mouse capture if:
	// - windowed mode, and
	// - capture type is different than NoMouse
	state.should_toggle_on_hotkey = !state.is_fullscreen &&
	                                !is_config_no_mouse;

	// Use any mouse click to capture the mouse if:
	// - windowed mode, and
	// - mouse is not captured, and
	// - we are not in seamless mode, and
	// - no GUI has taken over the mouse, and
	// - no NoMouse mode is in effect, and
	// - capture on start/click was configured or mapping is in effect
	state.should_capture_on_click = !state.is_fullscreen &&
	                                !state.is_captured &&
	                                !state.is_seamless &&
	                                !state.gui_has_taken_over &&
	                                !is_config_no_mouse &&
	                                (is_config_on_start || is_config_on_click || is_mapping);

	// Use a middle click to capture the mouse if:
	// - windowed mode, and
	// - mouse is not captured, and
	// - no GUI has taken over the mouse, and
	// - no NoMouse mode is in effect, and
	// - seamless mode is in effect, and
	// - middle release was configured
	state.should_capture_on_middle = !state.is_fullscreen &&
	                                 !state.is_captured &&
	                                 !state.gui_has_taken_over &&
	                                 !is_config_no_mouse &&
	                                 state.is_seamless &&
	                                 mouse_config.middle_release;

	// Use a middle click to release the mouse if:
	// - windowed mode, and
	// - mouse is captured, and
	// - release by middle button was configured
	state.should_release_on_middle = !state.is_fullscreen &&
	                                 state.is_captured &&
	                                 mouse_config.middle_release;

	// Select hint to be displayed on a title bar
	if (state.is_fullscreen || state.gui_has_taken_over || !state.has_focus)
		state.hint_id = MouseHint::None;
	else if (is_config_no_mouse)
		state.hint_id = MouseHint::NoMouse;
	else if (state.is_captured && state.should_release_on_middle)
		state.hint_id = MouseHint::CapturedHotkeyMiddle;
	else if (state.is_captured)
		state.hint_id = MouseHint::CapturedHotkey;
	else if (state.should_capture_on_click)
		state.hint_id = MouseHint::ReleasedHotkeyAnyButton;
	else if (state.should_capture_on_middle)
		state.hint_id = state.is_seamless
	                    ? MouseHint::SeamlessHotkeyMiddle
	                    : MouseHint::ReleasedHotkeyMiddle;
	else
		state.hint_id = state.is_seamless
	                    ? MouseHint::SeamlessHotkey
	                    : MouseHint::ReleasedHotkey;

	// Apply calculated settings if changed or if this is the first run
	if (first_time || old_is_captured != state.is_captured)
		GFX_SetMouseCapture(state.is_captured);
	if (first_time || old_is_input_raw != state.is_input_raw)
		GFX_SetMouseRawInput(state.is_input_raw);
	if (first_time || old_hint_id != state.hint_id)
		GFX_SetMouseHint(state.hint_id);

	for (auto &interface : mouse_interfaces)
		interface->UpdateInputType();

	// And take a note that this is no longer the first run
	first_time = false;
}

static bool should_drop_event()
{
	// Decide whether to drop mouse events, depending on both
	// mouse cursor position and general event dropping policy
	return (state.is_seamless && state.cursor_is_outside) ||
	       state.should_drop_events;
}

void MOUSE_UpdateGFX()
{
	update_state();
	update_cursor_visibility();
}

bool MOUSE_IsCaptured()
{
	return state.is_captured;
}

// ***************************************************************************
// Interrupt 74 implementation
// ***************************************************************************

static Bitu int74_exit()
{
	SegSet16(cs, RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
	reg_ip = RealOff(CALLBACK_RealPointer(int74_ret_callback));

	return CBRET_NONE;
}

static Bitu int74_handler()
{
	MouseEvent ev;
	mouse_queue.FetchEvent(ev);

	// Handle DOS events
	if (ev.request_dos) {
		uint8_t mask = 0;
		if (ev.dos_moved) {
			mask = MOUSEDOS_UpdateMoved();

			// Taken from DOSBox X: HERE within the IRQ 12 handler
			// is the appropriate place to redraw the cursor. OSes
			// like Windows 3.1 expect real-mode code to do it in
			// response to IRQ 12, not "out of the blue" from the
			// SDL event handler like the original DOSBox code did
			// it. Doing this allows the INT 33h emulation to draw
			// the cursor while not causing Windows 3.1 to crash or
			// behave erratically.
			if (mask)
				MOUSEDOS_DrawCursor();
		}
		if (ev.dos_button)
			mask = static_cast<uint8_t>(
			        mask | MOUSEDOS_UpdateButtons(ev.dos_buttons));
		if (ev.dos_wheel)
			mask = static_cast<uint8_t>(mask | MOUSEDOS_UpdateWheel());

		// If DOS driver's client is not interested in this particular
		// type of event - skip it
		if (!MOUSEDOS_HasCallback(mask))
			return int74_exit();

		CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
		CPU_Push16(RealOff(static_cast<RealPt>(CALLBACK_RealPointer(
		                           int74_ret_callback)) +
		                   7));

		return MOUSEDOS_DoCallback(mask, ev.dos_buttons);
	}

	// Handle PS/2 and BIOS mouse events
	if (ev.request_ps2 && mouse_shared.active_bios) {
		CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
		CPU_Push16(RealOff(CALLBACK_RealPointer(int74_ret_callback)));

		MOUSEPS2_UpdatePacket();
		return MOUSEBIOS_DoCallback();
	}

	// No mouse emulation module is interested in event
	return int74_exit();
}

Bitu int74_ret_handler()
{
	mouse_queue.StartTimerIfNeeded();
	return CBRET_NONE;
}

// ***************************************************************************
// External notifications
// ***************************************************************************

void MOUSE_NewScreenParams(const uint32_t clip_x, const uint32_t clip_y,
                           const uint32_t res_x, const uint32_t res_y,
                           const int32_t x_abs, const int32_t y_abs,
                           const bool is_fullscreen)
{
	assert(clip_x <= INT32_MAX);
	assert(clip_y <= INT32_MAX);
	assert(res_x <= INT32_MAX);
	assert(res_y <= INT32_MAX);

	state.clip_x = clip_x;
	state.clip_y = clip_y;

	// Protection against strange window sizes,
	// to prevent division by 0 in some places
	constexpr uint32_t min = 2;
	mouse_shared.resolution_x = std::max(res_x, min);
	mouse_shared.resolution_y = std::max(res_y, min);

	// If we are switching back from fullscreen,
	// clear the user capture request
	if (state.is_fullscreen && !is_fullscreen)
		state.capture_was_requested = false;

	state.is_fullscreen = is_fullscreen;

	update_cursor_absolute_position(x_abs, y_abs);

	MOUSE_UpdateGFX();
	MOUSEVMM_NewScreenParams(state.cursor_x_abs, state.cursor_y_abs);
}

void MOUSE_ToggleUserCapture(const bool pressed)
{
	if (!pressed || !state.should_toggle_on_hotkey)
		return;

	state.capture_was_requested = !state.capture_was_requested;
	MOUSE_UpdateGFX();
}

void MOUSE_NotifyTakeOver(const bool gui_has_taken_over)
{
	state.gui_has_taken_over = gui_has_taken_over;
	MOUSE_UpdateGFX();
}

void MOUSE_NotifyHasFocus(const bool has_focus)
{
	state.has_focus = has_focus;
	MOUSE_UpdateGFX();
}

void MOUSE_NotifyResetDOS()
{
	mouse_queue.ClearEventsDOS();
}

void MOUSE_NotifyDisconnect(const MouseInterfaceId interface_id)
{
	auto interface = MouseInterface::Get(interface_id);
	if (interface)
		interface->NotifyDisconnect();
}

void MOUSE_NotifyFakePS2()
{
	const auto interface = MouseInterface::GetPS2();

	if (interface && interface->IsUsingEvents()) {
		MouseEvent ev;
		ev.request_ps2 = true;
		mouse_queue.AddEvent(ev);
	}
}

void MOUSE_NotifyBooting()
{
	for (auto &interface : mouse_interfaces)
		interface->NotifyBooting();
}

void MOUSE_EventMoved(const float x_rel, const float y_rel,
                      const int32_t x_abs, const int32_t y_abs)
{
	// Event from GFX

	// Update cursor position and visibility
	update_cursor_absolute_position(x_abs, y_abs);
	update_cursor_visibility();

	// Drop unneeded events
	if (should_drop_event())
		return;

	// From the GUI we are getting mouse movement data in two
	// distinct formats:
	//
	// - relative; this one has a chance to be raw movements,
	//   it has to be fed to PS/2 mouse emulation, serial port
	//   mouse emulation, etc.; any guest side software accessing
	//   these mouse interfaces will most likely implement it's
	//   own mouse acceleration/smoothing/etc.
	// - absolute; this follows host OS mouse behavior and should
	//   be fed to VMware seamless mouse emulation and similar
	//   interfaces
	//
	// Our DOS mouse driver (INT 33h) is a bit special, as it can
	// act both ways (seamless and non-seamless mouse pointer),
	// so it needs data in both formats.

	// Notify mouse interfaces

	MouseEvent ev;
	for (auto &interface : mouse_interfaces)
		if (interface->IsUsingHostPointer())
			interface->NotifyMoved(ev, x_rel, y_rel,
			                       state.cursor_x_abs,
			                       state.cursor_y_abs);
	mouse_queue.AddEvent(ev);
}

void MOUSE_EventMoved(const float x_rel, const float y_rel,
                      const MouseInterfaceId interface_id)
{
	// Event from ManyMouse

	// Drop unneeded events
	if (should_drop_event())
		return;

	auto interface = MouseInterface::Get(interface_id);
	if (interface && interface->IsUsingEvents()) {
		MouseEvent ev;
		interface->NotifyMoved(ev, x_rel, y_rel, 0, 0);
		mouse_queue.AddEvent(ev);
	}
}

void MOUSE_EventButton(const uint8_t idx, const bool pressed)
{
	// Event from GFX

	// Never ignore any button releases - always pass them
	// to concrete interfaces, they will decide whether to
	// ignore them or not.
	if (pressed) {
		// Handle mouse capture by button click
		if (state.should_capture_on_click) {
			state.capture_was_requested = true;
			MOUSE_UpdateGFX();
			return;
		}

		// Handle mouse capture toggle by middle click
		constexpr uint8_t idx_middle = 2;
		if (idx == idx_middle && state.should_capture_on_middle) {
			state.capture_was_requested = true;
			MOUSE_UpdateGFX();
			return;
		}
		if (idx == idx_middle && state.should_release_on_middle) {
			state.capture_was_requested = false;
			MOUSE_UpdateGFX();
			return;
		}

		/// Drop unneeded events
		if (should_drop_event())
			return;
	}

	MouseEvent ev;
	for (auto &interface : mouse_interfaces)
		if (interface->IsUsingHostPointer())
			interface->NotifyButton(ev, idx, pressed);
	mouse_queue.AddEvent(ev);
}

void MOUSE_EventButton(const uint8_t idx, const bool pressed,
                       const MouseInterfaceId interface_id)
{
	// Event from ManyMouse

	// Drop unneeded events - but never drop any button
	// releases events; pass them to concrete interfaces,
	// they will decide whether to ignore them or not.
	if (pressed && should_drop_event())
		return;

	auto interface = MouseInterface::Get(interface_id);
	if (interface && interface->IsUsingEvents()) {
		MouseEvent ev;
		interface->NotifyButton(ev, idx, pressed);
		mouse_queue.AddEvent(ev);
	}
}

void MOUSE_EventWheel(const int16_t w_rel)
{
	// Event from GFX

	// Drop unneeded events
	if (should_drop_event())
		return;

	MouseEvent ev;
	for (auto &interface : mouse_interfaces)
		if (interface->IsUsingHostPointer())
			interface->NotifyWheel(ev, w_rel);
	mouse_queue.AddEvent(ev);
}

void MOUSE_EventWheel(const int16_t w_rel, const MouseInterfaceId interface_id)
{
	// Event from ManyMouse

	// Drop unneeded events
	if (state.should_drop_events)
		return;

	auto interface = MouseInterface::Get(interface_id);
	if (interface && interface->IsUsingEvents()) {
		MouseEvent ev;
		interface->NotifyWheel(ev, w_rel);
		mouse_queue.AddEvent(ev);
	}
}

// ***************************************************************************
// MOUSECTL.COM / GUI configurator interface
// ***************************************************************************

static std::vector<MouseInterface *> get_relevant_interfaces(
        const std::vector<MouseInterfaceId> &list_ids)
{
	std::vector<MouseInterface *> list_tmp = {};

	if (list_ids.empty())
		// If command does not specify interfaces,
		// assume we are interested in all of them
		list_tmp = mouse_interfaces;
	else
		for (const auto &interface_id : list_ids) {
			auto interface = MouseInterface::Get(interface_id);
			if (interface)
				list_tmp.push_back(interface);
		}

	// Filter out not emulated ones
	std::vector<MouseInterface *> list_out = {};
	for (const auto &interface : list_tmp)
		if (interface->IsEmulated())
			list_out.push_back(interface);

	return list_out;
}

MouseControlAPI::MouseControlAPI()
{
	manymouse.StartConfigAPI();
}

MouseControlAPI::~MouseControlAPI()
{
	manymouse.StopConfigAPI();
	MOUSE_UpdateGFX();
}

bool MouseControlAPI::IsNoMouseMode()
{
	return mouse_config.capture == MouseCapture::NoMouse;
}

const std::vector<MouseInterfaceInfoEntry> &MouseControlAPI::GetInfoInterfaces() const
{
	return mouse_info.interfaces;
}

const std::vector<MousePhysicalInfoEntry> &MouseControlAPI::GetInfoPhysical()
{
	manymouse.RescanIfSafe();
	return mouse_info.physical;
}

bool MouseControlAPI::CheckInterfaces(const MouseControlAPI::ListIDs &list_ids)
{
	const auto list = get_relevant_interfaces(list_ids);

	if (list_ids.empty() && list.empty())
		return false; // no emulated mouse interfaces
	if (list_ids.empty())
		return true; // OK, requested all emulated interfaces
	if (list_ids.size() != list.size())
		return false; // at least one requested interface is not emulated

	return true;
}

bool MouseControlAPI::PatternToRegex(const std::string &pattern, std::regex &regex)
{
	// Convert DOS wildcard pattern to a regular expression
	std::stringstream pattern_regex;
	pattern_regex << std::hex;
	for (const auto character : pattern) {
		if (character < 0x20 || character > 0x7E) {
			return false;
		}
		if (character == '?')
			pattern_regex << ".";
		else if (character == '*')
			pattern_regex << ".*";
		else if (std::isalnum(character))
			pattern_regex << character;
		else
			pattern_regex << "\\x" << static_cast<int>(character);
	}

	// Return a case-insensitive regular expression
	regex = std::regex(pattern_regex.str(), std::regex_constants::icase);
	return true;
}

bool MouseControlAPI::ProbeForMapping(uint8_t &device_id)
{
	if (IsNoMouseMode())
		return false;

	manymouse.RescanIfSafe();
	return manymouse.ProbeForMapping(device_id);
}

bool MouseControlAPI::Map(const MouseInterfaceId interface_id, const uint8_t device_idx)
{
	if (IsNoMouseMode())
		return false;

	auto mouse_interface = MouseInterface::Get(interface_id);
	if (!mouse_interface)
		return false;

	return mouse_interface->ConfigMap(device_idx);
}

bool MouseControlAPI::Map(const MouseInterfaceId interface_id, const std::regex &regex)
{
	if (IsNoMouseMode())
		return false;

	manymouse.RescanIfSafe();
	const auto idx = manymouse.GetIdx(regex);
	if (idx >= mouse_info.physical.size())
		return false;
	const auto result = Map(interface_id, idx);

	MOUSE_UpdateGFX();
	return result;
}

bool MouseControlAPI::UnMap(const MouseControlAPI::ListIDs &list_ids)
{
	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigUnMap();

	MOUSE_UpdateGFX();
	return !list.empty();
}

bool MouseControlAPI::OnOff(const MouseControlAPI::ListIDs &list_ids, const bool enable)
{
	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigOnOff(enable);

	return !list.empty();
}

bool MouseControlAPI::Reset(const MouseControlAPI::ListIDs &list_ids)
{
	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigReset();

	MOUSE_UpdateGFX();
	return !list.empty();
}

bool MouseControlAPI::SetSensitivity(const MouseControlAPI::ListIDs &list_ids,
                                     const int16_t sensitivity_x,
                                     const int16_t sensitivity_y)
{
	if (sensitivity_x > mouse_predefined.sensitivity_user_max ||
	    sensitivity_x < -mouse_predefined.sensitivity_user_max ||
	    sensitivity_y > mouse_predefined.sensitivity_user_max ||
	    sensitivity_y < -mouse_predefined.sensitivity_user_max)
		return false;

	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigSetSensitivity(sensitivity_x, sensitivity_y);

	return !list.empty();
}

bool MouseControlAPI::SetSensitivityX(const MouseControlAPI::ListIDs &list_ids,
                                      const int16_t sensitivity_x)
{
	if (sensitivity_x > mouse_predefined.sensitivity_user_max ||
	    sensitivity_x < -mouse_predefined.sensitivity_user_max)
		return false;

	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigSetSensitivityX(sensitivity_x);

	return !list.empty();
}

bool MouseControlAPI::SetSensitivityY(const MouseControlAPI::ListIDs &list_ids,
                                      const int16_t sensitivity_y)
{
	if (sensitivity_y > mouse_predefined.sensitivity_user_max ||
	    sensitivity_y < -mouse_predefined.sensitivity_user_max)
		return false;

	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigSetSensitivityY(sensitivity_y);

	return !list.empty();
}

bool MouseControlAPI::ResetSensitivity(const MouseControlAPI::ListIDs &list_ids)
{
	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigResetSensitivity();

	return !list.empty();
}

bool MouseControlAPI::ResetSensitivityX(const MouseControlAPI::ListIDs &list_ids)
{
	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigResetSensitivityX();

	return !list.empty();
}

bool MouseControlAPI::ResetSensitivityY(const MouseControlAPI::ListIDs &list_ids)
{
	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigResetSensitivityY();

	return !list.empty();
}

const std::vector<uint16_t> &MouseControlAPI::GetValidMinRateList()
{
	return MouseConfig::GetValidMinRateList();
}

const std::string &MouseControlAPI::GetValidMinRateStr()
{
	static std::string out_str = "";

	if (out_str.empty()) {
		const auto &valid_list = GetValidMinRateList();

		bool first = true;
		for (const auto &rate : valid_list) {
			if (first)
				first = false;
			else
				out_str += std::string(", ");
			out_str += std::to_string(rate);
		}
	}

	return out_str;
}

std::string MouseControlAPI::GetInterfaceNameStr(const MouseInterfaceId interface_id)
{
	switch (interface_id) {
	case MouseInterfaceId::DOS:  return "DOS";
	case MouseInterfaceId::PS2:  return "PS/2";
	case MouseInterfaceId::COM1: return "COM1";
	case MouseInterfaceId::COM2: return "COM2";
	case MouseInterfaceId::COM3: return "COM3";
	case MouseInterfaceId::COM4: return "COM4";
	case MouseInterfaceId::None: return "";
	default:
		assert(false); // missing implementation
		return nullptr;
	}
}

bool MouseControlAPI::SetMinRate(const MouseControlAPI::ListIDs &list_ids,
                                 const uint16_t value_hz)
{
	const auto &valid_list = GetValidMinRateList();
	if (!contains(valid_list, value_hz))
		return false; // invalid value

	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigSetMinRate(value_hz);

	return !list.empty();
}

bool MouseControlAPI::ResetMinRate(const MouseControlAPI::ListIDs &list_ids)
{
	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigResetMinRate();

	return !list.empty();
}

// ***************************************************************************
// Initialization
// ***************************************************************************

void MOUSE_StartupIfReady()
{
	if (mouse_shared.started ||
	    !mouse_shared.ready_init ||
	    !mouse_shared.ready_config ||
	    !mouse_shared.ready_gfx)
		return;

	switch (mouse_config.capture) {
	case MouseCapture::Seamless:
		LOG_MSG("MOUSE: Will move seamlessly: left and right button clicks won't capture the mouse");
		break;
	case MouseCapture::OnClick:
		LOG_MSG("MOUSE: Will be captured after the first left or right button click");
		break;
	case MouseCapture::OnStart:
		LOG_MSG("MOUSE: Will be captured immediately on start");
		break;
	case MouseCapture::NoMouse:
		LOG_MSG("MOUSE: Control is disabled");
		break;
	default: assert(false); break;
	}

	if (mouse_config.capture != MouseCapture::NoMouse) {
		LOG_MSG("MOUSE: Middle button will %s",
		        mouse_config.middle_release
		                ? "capture/release the mouse (clicks not sent to the game/program)"
		                : "be sent to the game/program (clicks not used to capture/release)");
	}

	// Callback for ps2 irq
	auto call_int74 = CALLBACK_Allocate();
	CALLBACK_Setup(call_int74, &int74_handler, CB_IRQ12, "int 74");
	// pseudocode for CB_IRQ12:
	//    sti
	//    push ds
	//    push es
	//    pushad
	//    callback int74_handler
	//        ps2 or user callback if requested
	//        otherwise jumps to CB_IRQ12_RET
	//    push ax
	//    mov al, 0x20
	//    out 0xa0, al
	//    out 0x20, al
	//    pop    ax
	//    cld
	//    retf

	int74_ret_callback = CALLBACK_Allocate();
	CALLBACK_Setup(int74_ret_callback, &int74_ret_handler, CB_IRQ12_RET, "int 74 ret");
	// pseudocode for CB_IRQ12_RET:
	//    cli
	//    mov al, 0x20
	//    out 0xa0, al
	//    out 0x20, al
	//    callback int74_ret_handler
	//    popad
	//    pop es
	//    pop ds
	//    iret

	// (MOUSE_IRQ > 7) ? (0x70 + MOUSE_IRQ - 8) : (0x8 + MOUSE_IRQ);
	RealSetVec(0x74, CALLBACK_RealPointer(call_int74));

	MouseInterface::InitAllInstances();
	mouse_shared.started = true;

	MOUSE_UpdateGFX();
}

void MOUSE_NotifyReadyGFX()
{
	mouse_shared.ready_gfx = true;
	MOUSE_StartupIfReady();
}

void MOUSE_Init(Section * /*sec*/)
{
	// Start mouse emulation if ready
	mouse_shared.ready_init = true;
	MOUSE_StartupIfReady();
}
