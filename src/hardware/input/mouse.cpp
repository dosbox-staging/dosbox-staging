// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mouse.h"

#include "private/mouse_config.h"
#include "private/mouse_interfaces.h"
#include "private/mouse_manymouse.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "gui/common.h"
#include "hardware/pic.h"
#include "misc/video.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

CHECK_NARROWING();

static callback_number_t int74_ret_callback = 0;

static ManyMouseGlue &manymouse = ManyMouseGlue::GetInstance();

// ***************************************************************************
// GFX-related decision making
// ***************************************************************************

static struct {
	bool is_fullscreen    = false; // if full screen mode is active
	bool is_multi_display = false; // if host system has more than 1 display

	// The draw rectangle in logical units. Note the (x1,y1) upper-left
	// coordinates can be negative if we're "zooming into" the DOS content
	// (e.g., in 'relative' viewport mode), in which case the draw rect
	// extends beyond the dimensions of the screen/window.
	DosBox::Rect draw_rect = {};

	// Absolute position from start of drawing area in logical units
	float cursor_x_abs = 0.0f;
	float cursor_y_abs = 0.0f;

	// If mouse cursor is outside of drawing area
	bool cursor_is_outside = false;

	bool is_window_active       = false; // if our window is active (has focus)
	bool gui_has_taken_over     = false; // if a GUI requested to take over the mouse
	bool is_mapping_in_progress = false; // if interactive mapping is running
	bool capture_was_requested  = false; // if user requested mouse to be captured
	bool vmm_wants_pointer      = false; // if virtual machine guest addons wants us
	                                     // to show the host pointer

	// if we have a desktop environment, then we can support uncaptured and seamless modes
	const bool have_desktop_environment = GFX_HaveDesktopEnvironment();

	bool is_captured  = false; // if GFX was requested to capture mouse
	bool is_visible   = false; // if GFX was requested to make cursor visible
	bool is_raw_input = false; // if GFX was requested to provide raw movements
	bool is_seamless  = false; // if seamless mouse integration is in effect

	// If mouse events should be ignored, except button release
	bool should_drop_events = true;

	bool should_capture_on_click  = false; // if any button click should capture the mouse
	bool should_capture_on_middle = false; // if middle button press should capture the mouse
	bool should_release_on_middle = false; // if middle button press should release the mouse
	bool should_toggle_on_hotkey  = false; // if hotkey should toggle mouse capture

	MouseHint hint_id = MouseHint::None; // hint to be displayed on title bar

} state;

static void update_cursor_absolute_position(const float x_abs, const float y_abs)
{
	state.cursor_is_outside = false;

	auto calc_pos = [&](const float pos,
	                    const int draw_start_pos,
	                    const int draw_end_pos) {
		assert(draw_end_pos - draw_start_pos > 1);
		constexpr float MinPos = 0.0f;

		if (pos < MinPos || pos < static_cast<float>(draw_start_pos)) {
			// Cursor is before the top or left of the draw area
			state.cursor_is_outside = !state.is_captured;
			return MinPos;

		} else if (pos >= static_cast<float>(draw_end_pos)) {
			// Cursor is after the bottom or right of the draw area
			state.cursor_is_outside = !state.is_captured;
			return static_cast<float>(draw_end_pos - draw_start_pos - 1);

		} else {
			return pos - static_cast<float>(draw_start_pos);
		}
	};

	const auto x1 = iroundf(state.draw_rect.x1());
	const auto y1 = iroundf(state.draw_rect.y1());
	const auto x2 = x1 + check_cast<int>(mouse_shared.resolution_x);
	const auto y2 = y1 + check_cast<int>(mouse_shared.resolution_y);

	state.cursor_x_abs = calc_pos(x_abs, x1, x2);
	state.cursor_y_abs = calc_pos(y_abs, y1, y2);
}

static void update_cursor_visibility()
{
	// If mouse subsystem not started yet, do nothing
	if (!mouse_shared.started) {
		return;
	}

	static bool first_time = true;

	// Store internally old settings, to avoid unnecessary GFX calls
	const auto old_is_visible = state.is_visible;

	if (!state.is_window_active) {

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
		// Or if:
		// - virtual machine guest addons wants us to show the pointer
		state.is_visible = !(state.is_captured || state.is_seamless) ||
		                   (state.is_seamless && state.cursor_is_outside) ||
		                   state.vmm_wants_pointer;
	}

	// Apply calculated settings if changed or if this is the first run
	if (first_time || old_is_visible != state.is_visible) {
		GFX_SetMouseVisibility(state.is_visible);
	}

	// And take a note that this is no longer the first run
	first_time = false;
}

static void update_state() // updates whole 'state' structure, except cursor visibility
{
	// If mouse subsystem not started yet, do nothing
	if (!mouse_shared.started) {
		return;
	}

	const bool is_config_on_start = (mouse_config.capture == MouseCapture::OnStart);
	const bool is_config_on_click = (mouse_config.capture == MouseCapture::OnClick);
	const bool is_config_no_mouse = (mouse_config.capture == MouseCapture::NoMouse);

	// Only consider multi-display mode if enabled in the configuration!
	const bool is_window_or_multi_display = !state.is_fullscreen ||
		(state.is_multi_display && mouse_config.multi_display_aware);

	// If running for the first time, capture the mouse if this was configured
	static bool first_time = true;
	if (first_time && is_config_on_start) {
		state.capture_was_requested = true;
	}

	// Virtual machine manager wants us to show mouse pointer if:
	// - virtual machine guest addons are running and
	// - they requested to show host mouse pointer
	state.vmm_wants_pointer = mouse_shared.active_vmm &&
	                          mouse_shared.vmm_wants_pointer;

	// Discard previous mouse capture request if:
	// - virtual machine guest addons wants us to show the pointer
	if (state.vmm_wants_pointer) {
		state.capture_was_requested = false;
	}

	// We are running in seamless mode:
	// - we have a desktop environment, and
	// - we are in windowed or multi-display mode, or
	//   if virtual machine guest addons wants us to show the pointer, and
	// - NoMouse is not configured, and
	// - seamless driver is running or Seamless capture is configured
	const bool is_seamless_config = (mouse_config.capture == MouseCapture::Seamless);
	const bool is_seamless_driver = mouse_shared.active_vmm;

	state.is_seamless = state.have_desktop_environment &&
	                    (is_window_or_multi_display || state.vmm_wants_pointer) &&
	                    !is_config_no_mouse &&
	                    (is_seamless_driver || is_seamless_config);

	// Due to ManyMouse API limitation, we are unable to support seamless
	// integration if mapping is in effect
	const bool is_mapping = state.is_mapping_in_progress ||
	                        manymouse.IsMappingInEffect();
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
	const auto old_is_raw_input = state.is_raw_input;
	const auto old_hint_id      = state.hint_id;

	// Raw input depends on the user configuration
	state.is_raw_input = mouse_config.raw_input;

	if (state.gui_has_taken_over) {
		state.is_captured = false;

		// Override user configuration, for the GUI we want
		// host OS mouse acceleration applied
		state.is_raw_input = false;

	} else if (is_config_no_mouse) { // NoMouse is configured

		// Capture mouse cursor if:
		// - we are in fullscreen mode and not in multi-display mode
		state.is_captured = !is_window_or_multi_display;

		// Drop the user capture request, otherwise runtime mouse
		// capture configuration change (for example to OnClick) could
		// have caused the mouse cursor to suddenly disappear
		state.capture_was_requested = false;

	} else if (state.is_window_active) { // window has focus, no GUI running

		// Capture mouse cursor if any of:
		// - we lack a desktop environment,
		// - we are in fullscreen mode and not in multi-display mode and
		//   virtual machine guest addons did not request us to show
		//   the mouse cursor, and
		// - user asked to capture the mouse
		state.is_captured = !state.have_desktop_environment ||
		                    (!is_window_or_multi_display && !state.vmm_wants_pointer) ||
		                    state.capture_was_requested;
	}

#if defined(WIN32)
	// Disable raw mouse input if:
	// - this is a Windows build, and
	// - mapping is in effect
	if (is_mapping) {
		// It was discovered that ManyMouse library does not function
		// properly in this case - it stops working as soon as the user
		// switches DOSBox to windowed mode. Workaround: do not allow
		// RAW mouse input in SDL API if mapping is in effect.
		state.is_raw_input = false;
	}
#endif

	// Drop mouse events (except for button release) if any of:
	// - GUI has taken over the mouse
	// - capture type is NoMouse
	state.should_drop_events = state.gui_has_taken_over ||
	                           is_config_no_mouse;
	if (!state.is_seamless) {

		// If not Seamless mode, also drop events if any of:
		// - mouse is not captured
		// - emulator window is not active (has no focus)
		state.should_drop_events = state.should_drop_events ||
		                           !state.is_captured ||
		                           !state.is_window_active;
	}

	// Use a hotkey to toggle mouse capture if:
	// - we have a desktop environment, and
	// - we are in windowed or multi-display mode, and
	// - capture type is different than NoMouse
	state.should_toggle_on_hotkey = state.have_desktop_environment &&
	                                is_window_or_multi_display &&
	                                !is_config_no_mouse;

	// Use any mouse click to capture the mouse if:
	// - we have a desktop environment, and
	// - we are in windowed or multi-display mode, and
	// - virtual machine guest addons did not request us to show
	//   the mouse cursor, and
	// - mouse is not captured, and
	// - we are not in seamless mode, and
	// - no GUI has taken over the mouse, and
	// - capture type is different than NoMouse, and
	// - capture on start/click was configured or mapping is in effect
	state.should_capture_on_click = state.have_desktop_environment &&
	                                is_window_or_multi_display &&
	                                !state.vmm_wants_pointer &&
	                                !state.is_captured &&
	                                !state.is_seamless &&
	                                !state.gui_has_taken_over &&
	                                !is_config_no_mouse &&
	                                (is_config_on_start || is_config_on_click || is_mapping);

	// Use a middle click to capture the mouse if:
	// - we have a desktop environment, and
	// - we are in windowed or multi-display mode, and
	// - virtual machine guest addons did not request us to show
	//   the mous cursor, and
	// - mouse is not captured, and
	// - no GUI has taken over the mouse, and
	// - capture type is different than NoMouse, and
	// - seamless mode is in effect, and
	// - middle release was configured
	state.should_capture_on_middle = state.have_desktop_environment &&
	                                 is_window_or_multi_display &&
	                                 !state.vmm_wants_pointer &&
	                                 !state.is_captured &&
	                                 !state.gui_has_taken_over &&
	                                 !is_config_no_mouse &&
	                                 state.is_seamless &&
	                                 mouse_config.middle_release;

	// Use a middle click to release the mouse if:
	// - we have a desktop environment, and
	// - we are in windowed or multi-display mode, and
	// - mouse is captured, and
	// - release by middle button was configured
	state.should_release_on_middle = state.have_desktop_environment &&
	                                 is_window_or_multi_display &&
	                                 state.is_captured &&
	                                 mouse_config.middle_release;

	// Note: it would make sense to block capture/release on any mouse click
	// while 'state.is_mapping_in_progress' - unfortunately this would lead
	// to a race condition between events from SDL and ManyMouse at the end
	// of mapping process, leading to ununiform (random) user experience.
	// TODO: if SDL gets expanded to include ManyMouse, change the behavior!

	// Select hint to be displayed on a title bar
	if (!state.have_desktop_environment || !is_window_or_multi_display ||
	    state.gui_has_taken_over || !state.is_window_active ||
	    is_config_no_mouse) {
		state.hint_id = MouseHint::None;
	} else if (state.is_captured && state.should_release_on_middle) {
		state.hint_id = MouseHint::CapturedHotkeyMiddle;
	} else if (state.is_captured) {
		state.hint_id = MouseHint::CapturedHotkey;
	} else if (state.should_capture_on_click) {
		state.hint_id = MouseHint::ReleasedHotkeyAnyButton;
	} else if (state.should_capture_on_middle) {
		state.hint_id = state.is_seamless
	                    ? MouseHint::SeamlessHotkeyMiddle
	                    : MouseHint::ReleasedHotkeyMiddle;
	} else {
		state.hint_id = state.is_seamless
	                    ? MouseHint::SeamlessHotkey
	                    : MouseHint::ReleasedHotkey;
	}

	// Center the mouse cursor if:
	// - this is not the first run, and
	// - seamless mode is not in effect, and
	// - we are going to release the captured mouse
	if (!first_time && !state.is_seamless &&
	    !state.is_captured && old_is_captured) {
		GFX_CenterMouse();
	}

	// Apply calculated settings if changed or if this is the first run
	if (first_time || old_is_captured != state.is_captured) {
		GFX_SetMouseCapture(state.is_captured);
	}
	if (first_time || old_is_raw_input != state.is_raw_input) {
		GFX_SetMouseRawInput(state.is_raw_input);
	}
	if (first_time || old_hint_id != state.hint_id) {
		GFX_SetMouseHint(state.hint_id);
	}

	for (const auto interface_id : AllMouseInterfaceIds) {
		auto& interface = MouseInterface::GetInstance(interface_id);
		interface.UpdateInputType();
	}

	// And take a note that this is no longer the first run
	first_time = false;
}

static bool should_drop_move()
{
	return state.should_drop_events ||
	       (state.cursor_is_outside && !state.is_seamless);
}

static bool should_drop_press_or_wheel()
{
	return state.should_drop_events ||
	       state.cursor_is_outside;
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

bool MOUSE_IsRawInput()
{
	return state.is_raw_input;
}

bool MOUSE_IsProbeForMappingAllowed()
{
	// Conditions to be met to accept mouse clicks for interactive mapping:
	// - window is active (we have a focus)
	// - no GUI has taken over the mouse
	return state.is_window_active && !state.gui_has_taken_over;
}

// ***************************************************************************
// Interrupt 74 implementation
// ***************************************************************************

static Bitu int74_exit()
{
	const auto real_pt = CALLBACK_RealPointer(int74_ret_callback);
	SegSet16(cs, RealSegment(real_pt));
	reg_ip = RealOffset(real_pt);

	return CBRET_NONE;
}

static Bitu int74_handler()
{
	// Try BIOS events (from Intel 8042 controller)
	if (MOUSEBIOS_CheckCallback()) {
		CPU_Push16(RealSegment(CALLBACK_RealPointer(int74_ret_callback)));
		CPU_Push16(RealOffset(CALLBACK_RealPointer(int74_ret_callback)));
		MOUSEBIOS_DoCallback();
		// TODO: Handle both BIOS and DOS callback within
		// in a single interrupt
		return CBRET_NONE;
	}

	// Try DOS driver events
	if (!mouse_shared.dos_cb_running) {
		const auto mask = MOUSEDOS_DoInterrupt();
		if (mask) {
			const auto real_pt = CALLBACK_RealPointer(int74_ret_callback);
			CPU_Push16(RealSegment(real_pt));
			CPU_Push16(RealOffset(static_cast<RealPt>(real_pt) + 7));

			MOUSEDOS_DoCallback(mask);
			return CBRET_NONE;
		}
	}

	// No mouse emulation module is interested in the event
	return int74_exit();
}

Bitu int74_ret_handler()
{
	MOUSEBIOS_FinalizeInterrupt();
	MOUSEDOS_FinalizeInterrupt();
	MOUSEBIOS_FinalizeInterrupt();
	return CBRET_NONE;
}

// ***************************************************************************
// External notifications
// ***************************************************************************

void MOUSE_NewScreenParams(const MouseScreenParams &params)
{
	state.draw_rect = params.draw_rect;

	// Protection against strange window sizes,
	// to prevent division by 0 in some places
	constexpr auto min = 2;

	mouse_shared.resolution_x = check_cast<uint32_t>(
	        std::max(iroundf(params.draw_rect.w), min));

	mouse_shared.resolution_y = check_cast<uint32_t>(
	        std::max(iroundf(params.draw_rect.h), min));

	// If we are switching back from fullscreen,
	// clear the user capture request
	if (state.is_fullscreen && !params.is_fullscreen) {
		state.capture_was_requested = false;
	}

	state.is_fullscreen    = params.is_fullscreen;
	state.is_multi_display = params.is_multi_display;

	update_cursor_absolute_position(params.x_abs, params.y_abs);

	MOUSE_UpdateGFX();
	MOUSEVMM_NewScreenParams(state.cursor_x_abs, state.cursor_y_abs);
}

void MOUSE_ToggleUserCapture(const bool pressed)
{
	if (!pressed || !state.should_toggle_on_hotkey || state.vmm_wants_pointer) {
		return;
	}

	state.capture_was_requested = !state.capture_was_requested;
	MOUSE_UpdateGFX();
}

void MOUSE_NotifyTakeOver(const bool gui_has_taken_over)
{
	state.gui_has_taken_over = gui_has_taken_over;
	MOUSE_UpdateGFX();
}

void MOUSE_NotifyWindowActive(const bool is_active)
{
	state.is_window_active = is_active;
	MOUSE_UpdateGFX();
}

void MOUSE_NotifyDisconnect(const MouseInterfaceId interface_id)
{
	auto& interface = MouseInterface::GetInstance(interface_id);
	interface.NotifyDisconnect();
}

void MOUSE_NotifyBooting()
{
	for (const auto interface_id : AllMouseInterfaceIds) {
		auto& interface = MouseInterface::GetInstance(interface_id);
		interface.NotifyBooting();
	}
}

void MOUSE_EventMoved(const float x_rel, const float y_rel,
                      const float x_abs, const float y_abs)
{
	// Event from GFX

	// Update cursor position and visibility
	update_cursor_absolute_position(x_abs, y_abs);
	update_cursor_visibility();

	// Drop unneeded events
	if (should_drop_move()) {
		return;
	}

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
	const float x_scaled = x_rel * mouse_config.sensitivity_coeff_x;
	const float y_scaled = y_rel * mouse_config.sensitivity_coeff_y;
	for (const auto interface_id : AllMouseInterfaceIds) {
		auto& interface = MouseInterface::GetInstance(interface_id);
		if (interface.IsUsingHostPointer()) {
			interface.NotifyMoved(x_scaled,
			                      y_scaled,
			                      state.cursor_x_abs,
			                      state.cursor_y_abs);
		}
	}
}

void MOUSE_EventMoved(const float x_rel, const float y_rel,
                      const MouseInterfaceId interface_id)
{
	// Event from ManyMouse

	// Drop unneeded events
	if (should_drop_move()) {
		return;
	}

	// Notify mouse interface
	auto& interface = MouseInterface::GetInstance(interface_id);
	if (interface.IsUsingEvents()) {
		const float x_scaled = x_rel * mouse_config.sensitivity_coeff_x;
		const float y_scaled = y_rel * mouse_config.sensitivity_coeff_y;
		interface.NotifyMoved(x_scaled, y_scaled, 0, 0);
	}
}

void MOUSE_EventButton(const MouseButtonId button_id, const bool pressed)
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

		const auto is_middle = (button_id == MouseButtonId::Middle);

		// Handle mouse capture toggle by middle click
		if (is_middle && state.should_capture_on_middle) {
			state.capture_was_requested = true;
			MOUSE_UpdateGFX();
			return;
		}
		if (is_middle && state.should_release_on_middle) {
			state.capture_was_requested = false;
			MOUSE_UpdateGFX();
			return;
		}

		// Drop unneeded events
		if (should_drop_press_or_wheel()) {
			return;
		}
	}

	// Notify mouse interfaces
	for (const auto interface_id : AllMouseInterfaceIds) {
		auto& interface = MouseInterface::GetInstance(interface_id);
		if (interface.IsUsingHostPointer()) {
			interface.NotifyButton(button_id, pressed);
		}
	}
}

void MOUSE_EventButton(const MouseButtonId button_id, const bool pressed,
                       const MouseInterfaceId interface_id)
{
	// Event from ManyMouse

	// Drop unneeded events - but never drop any button
	// releases events; pass them to concrete interfaces,
	// they will decide whether to ignore them or not.
	if (pressed && should_drop_press_or_wheel()) {
		return;
	}

	// Notify mouse interface
	auto& interface = MouseInterface::GetInstance(interface_id);
	if (interface.IsUsingEvents()) {
		interface.NotifyButton(button_id, pressed);
	}
}

void MOUSE_EventWheel(const float w_rel)
{
	// Event from GFX

	// Drop unneeded events
	if (should_drop_press_or_wheel()) {
		return;
	}

	// Notify mouse interfaces
	for (const auto interface_id : AllMouseInterfaceIds) {
		auto& interface = MouseInterface::GetInstance(interface_id);
		if (interface.IsUsingHostPointer()) {
			interface.NotifyWheel(w_rel);
		}
	}
}

void MOUSE_EventWheel(const int16_t w_rel, const MouseInterfaceId interface_id)
{
	// Event from ManyMouse

	// Drop unneeded events
	if (should_drop_press_or_wheel()) {
		return;
	}

	// Notify mouse interface
	auto& interface = MouseInterface::GetInstance(interface_id);
	if (interface.IsUsingEvents()) {
		interface.NotifyWheel(w_rel);
	}
}

// ***************************************************************************
// MOUSECTL.COM / GUI configurator interface
// ***************************************************************************

static std::vector<MouseInterface *> get_relevant_interfaces(
        const std::vector<MouseInterfaceId> &list_ids)
{
	std::vector<MouseInterface *> list_tmp = {};

	if (list_ids.empty()) {
		// If command does not specify interfaces,
		// assume we are interested in all of them
		for (const auto interface_id : AllMouseInterfaceIds) {
			auto& interface = MouseInterface::GetInstance(interface_id);
			list_tmp.push_back(&interface);
		}
	} else {
		for (const auto& interface_id : list_ids) {
			auto& interface = MouseInterface::GetInstance(interface_id);
			list_tmp.push_back(&interface);
		}
	}

	// Filter out not emulated ones
	std::vector<MouseInterface *> list_out = {};
	for (const auto &interface : list_tmp) {
		if (interface->IsEmulated()) {
			list_out.push_back(interface);
		}
	}

	return list_out;
}

MouseControlAPI::MouseControlAPI()
{
	manymouse.StartConfigAPI();
}

MouseControlAPI::~MouseControlAPI()
{
	manymouse.StopConfigAPI();
	if (was_interactive_mapping_started)
		state.is_mapping_in_progress = false;
	MOUSE_UpdateGFX();
}

bool MouseControlAPI::IsNoMouseMode()
{
	return mouse_config.capture == MouseCapture::NoMouse;
}

bool MouseControlAPI::IsMappingBlockedByDriver()
{
	return state.vmm_wants_pointer;
}

MouseControlAPI::MappingSupport MouseControlAPI::IsMappingSupported()
{
#ifndef C_MANYMOUSE
	return MappingSupport::NotCompiledIn;
#else
#if defined(WIN32)
	if (mouse_config.raw_input) {
		return MappingSupport::NotAvailableRawInput;
	}
#endif
	return MappingSupport::Supported;
#endif
}

const std::vector<MouseInterfaceInfoEntry>& MouseControlAPI::GetInfoInterfaces() const
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

bool MouseControlAPI::MapInteractively(const MouseInterfaceId interface_id,
                                       uint8_t &physical_device_idx)
{
	if (MappingSupport::Supported != IsMappingSupported() ||
	    IsNoMouseMode() || IsMappingBlockedByDriver()) {
		return false;
	}

	if (!was_interactive_mapping_started) {
		// Interactive mapping was started
		assert(!state.is_mapping_in_progress);
		// Capture the mouse, otherwise it might be confusing
		// for the user when it gets captured after he clicks
		// simply to select the mouse
		state.capture_was_requested     = true;
		// Tell the other code that mapping is in progress,
		// so that it can disable seamless mouse integration,
		// and possibly apply other changes to mouse behavior
		state.is_mapping_in_progress    = true;
		was_interactive_mapping_started = true;
		MOUSE_UpdateGFX();
	}

	manymouse.RescanIfSafe();
	if (!manymouse.ProbeForMapping(physical_device_idx)) {
		return false;
	}

	return Map(interface_id, physical_device_idx);
}

bool MouseControlAPI::Map(const MouseInterfaceId interface_id,\
                          const uint8_t physical_device_idx)
{
	if (MappingSupport::Supported != IsMappingSupported() ||
	    IsNoMouseMode() || IsMappingBlockedByDriver()) {
		return false;
	}

	auto& interface = MouseInterface::GetInstance(interface_id);
	return interface.ConfigMap(physical_device_idx);
}

bool MouseControlAPI::Map(const MouseInterfaceId interface_id, const std::regex &regex)
{
	if (MappingSupport::Supported != IsMappingSupported() ||
	    IsNoMouseMode() || IsMappingBlockedByDriver()) {
		return false;
	}

	manymouse.RescanIfSafe();
	const auto idx = manymouse.GetIdx(regex);
	if (idx >= mouse_info.physical.size()) {
		return false;
	}
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
	if (sensitivity_x < Mouse::MinSensitivity ||
	    sensitivity_x > Mouse::MaxSensitivity ||
	    sensitivity_y < Mouse::MinSensitivity ||
	    sensitivity_y > Mouse::MaxSensitivity) {

		return false;
	}

	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigSetSensitivity(sensitivity_x, sensitivity_y);

	return !list.empty();
}

bool MouseControlAPI::SetSensitivityX(const MouseControlAPI::ListIDs &list_ids,
                                      const int16_t sensitivity_x)
{
	if (sensitivity_x < Mouse::MinSensitivity ||
	    sensitivity_x > Mouse::MaxSensitivity) {
		return false;
	}

	auto list = get_relevant_interfaces(list_ids);
	for (auto &interface : list)
		interface->ConfigSetSensitivityX(sensitivity_x);

	return !list.empty();
}

bool MouseControlAPI::SetSensitivityY(const MouseControlAPI::ListIDs &list_ids,
                                      const int16_t sensitivity_y)
{
	if (sensitivity_y < Mouse::MinSensitivity ||
	    sensitivity_y > Mouse::MaxSensitivity) {
		return false;
	}

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
	default:
		assert(false); // missing implementation
		return {};
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

	// Callback for PS/2 BIOS or DOS driver IRQ
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

	MOUSEDOS_Init();
}

void MOUSE_NotifyReadyGFX()
{
	mouse_shared.ready_gfx = true;
	MOUSE_StartupIfReady();
}
