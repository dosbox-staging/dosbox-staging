/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2025  The DOSBox Staging Team
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

#include <algorithm>

#include "bios.h"
#include "bitops.h"
#include "byteorder.h"
#include "callback.h"
#include "checks.h"
#include "config.h"
#include "cpu.h"
#include "dos_inc.h"
#include "math_utils.h"
#include "pic.h"
#include "regs.h"

#include "../../ints/int10.h"

CHECK_NARROWING();

// This file implements the DOS mouse driver interface,
// using host system events

// Reference:
// - Ralf Brown's Interrupt List
// - WHEELAPI.TXT, INT10.LST, and INT33.LST from CuteMouse driver
// - https://www.stanislavs.org/helppc/int_33.html
// - http://www2.ift.ulaval.ca/~marchand/ift17583/dosints.pdf
// - https://github.com/FDOS/mouse/blob/master/int33.lst
// - https://www.fysnet.net/faq.htm

// Versions are stored in BCD code - 0x09 = version 9, 0x10 = version 10, etc.
static constexpr uint8_t driver_version_major = 0x08;
static constexpr uint8_t driver_version_minor = 0x05;

static constexpr uint8_t  cursor_size_x  = 16;
static constexpr uint8_t  cursor_size_y  = 16;
static constexpr uint16_t cursor_size_xy = cursor_size_x * cursor_size_y;

static constexpr auto MaxButtons = 3;

enum class MouseCursor : uint8_t { Software = 0, Hardware = 1, Text = 2 };

// This enum has to be compatible with mask in DOS driver function 0x0c
enum class MouseEventId : uint8_t {
	NotDosEvent    = 0,
	MouseHasMoved  = 1 << 0,
	PressedLeft    = 1 << 1,
	ReleasedLeft   = 1 << 2,
	PressedRight   = 1 << 3,
	ReleasedRight  = 1 << 4,
	PressedMiddle  = 1 << 5,
	ReleasedMiddle = 1 << 6,
	WheelHasMoved  = 1 << 7,
};

// Pending (usually delayed) events

// delay to enforce between callbacks, in milliseconds
static uint8_t delay_ms = 5;

// true = delay timer is in progress
static bool delay_running  = false;
// true = delay timer expired, event can be sent immediately
static bool delay_finished = true;

static bool pending_moved  = false;
static bool pending_button = false;
static bool pending_wheel  = false;

static MouseButtons12S pending_button_state = 0;

// These values represent 'hardware' state, not driver state

static MouseButtons12S buttons = 0;
static float pos_x = 0.0f;
static float pos_y = 0.0f;
static int8_t counter_w = 0; // wheel counter

// true = ignore absolute mouse position
static bool use_relative = true;
// true = no host mouse acceleration pre-applied
static bool is_input_raw = true;

// true = rate was set by DOS application
static bool rate_is_set     = false;
static uint16_t rate_hz     = 0;
static uint16_t min_rate_hz = 0;

// Data from mouse events which were already received,
// but not necessary visible to the application

static struct {
	// Mouse movement
	float x_rel = 0.0f;
	float y_rel = 0.0f;
	float x_abs = 0.0f;
	float y_abs = 0.0f;

	// Wheel movement
	float delta_wheel = 0.0f;

	void Reset()
	{
		x_rel = 0.0f;
		y_rel = 0.0f;
		delta_wheel = 0.0f;
	}
} pending;

static struct { // DOS driver state

	// Structure containing (only!) data which should be
	// saved/restored during task switching

	// DANGER, WILL ROBINSON!
	//
	// This whole structure can be read or written from the guest side
	// via virtual DOS driver, functions 0x15 / 0x16 / 0x17.
	// Do not put here any array indices, pointers, or anything that
	// can crash the emulator if filled-in incorrectly, or that can
	// be used by malicious code to escape from emulation!

	bool enabled   = false; // TODO: make use of this
	bool wheel_api = false; // CuteMouse compatible WheelAPI v1.0 extension

	uint16_t times_pressed[MaxButtons]   = {0};
	uint16_t times_released[MaxButtons]  = {0};
	uint16_t last_released_x[MaxButtons] = {0};
	uint16_t last_released_y[MaxButtons] = {0};
	uint16_t last_pressed_x[MaxButtons]  = {0};
	uint16_t last_pressed_y[MaxButtons]  = {0};
	uint16_t last_wheel_moved_x           = 0;
	uint16_t last_wheel_moved_y           = 0;

	float mickey_counter_x = 0.0f;
	float mickey_counter_y = 0.0f;

	float mickeys_per_pixel_x = 0.0f;
	float mickeys_per_pixel_y = 0.0f;
	float pixels_per_mickey_x = 0.0f;
	float pixels_per_mickey_y = 0.0f;

	uint16_t double_speed_threshold = 0; // in mickeys/s
	                                     // TODO: should affect movement

	uint16_t granularity_x = 0; // mask
	uint16_t granularity_y = 0;

	int16_t update_region_x[2] = {0};
	int16_t update_region_y[2] = {0};

	uint16_t language = 0; // language for driver messages, unused
	uint8_t bios_screen_mode = 0;

	// sensitivity
	uint8_t sensitivity_x = 0;
	uint8_t sensitivity_y = 0;
	// TODO: find out what it is for (acceleration?), for now
	// just set it to default value on startup
	uint8_t unknown_01 = 50;

	float sensitivity_coeff_x = 0;
	float sensitivity_coeff_y = 0;

	// mouse position allowed range
	int16_t minpos_x = 0;
	int16_t maxpos_x = 0;
	int16_t minpos_y = 0;
	int16_t maxpos_y = 0;

	// mouse cursor
	uint8_t page       = 0; // cursor display page number
	bool inhibit_draw  = false;
	uint16_t hidden    = 0;
	uint16_t oldhidden = 0;
	int16_t clipx      = 0;
	int16_t clipy      = 0;
	int16_t hot_x      = 0; // cursor hot spot, horizontal
	int16_t hot_y      = 0; // cursor hot spot, vertical

	struct {
		bool enabled                 = false;
		uint16_t pos_x               = 0;
		uint16_t pos_y               = 0;
		uint8_t data[cursor_size_xy] = {0};

	} background = {};

	MouseCursor cursor_type = MouseCursor::Software;

	// cursor shape definition
	uint16_t text_and_mask                       = 0;
	uint16_t text_xor_mask                       = 0;
	bool user_screen_mask                        = false;
	bool user_cursor_mask                        = false;
	uint16_t user_def_screen_mask[cursor_size_x] = {0};
	uint16_t user_def_cursor_mask[cursor_size_y] = {0};

	// user callback
	uint16_t user_callback_mask    = 0;
	uint16_t user_callback_segment = 0;
	uint16_t user_callback_offset  = 0;

} state;

// Guest-side pointers to various driver information
static uint16_t info_segment          = 0;
static uint16_t info_offset_ini_file  = 0;
static uint16_t info_offset_version   = 0;
static uint16_t info_offset_copyright = 0;

static RealPt user_callback;

// ***************************************************************************
// Model capabilities support
// ***************************************************************************

static uint8_t get_num_buttons()
{
	using enum MouseModelDos;

	switch (mouse_config.model_dos) {
	case TwoButton:
		return 2;
	case ThreeButton:
	case Wheel:
		return 3;
	default:
		assertm(false, "unknown mouse model (DOS)");
		return 2;
	}
}

static uint8_t get_button_mask()
{
	MouseButtonsAll button_mask = {};

	button_mask.left  = 1;
	button_mask.right = 1;

	if (get_num_buttons() >= 3) {
		button_mask.middle = 1;
	}

	return button_mask._data;
}

static bool has_wheel()
{
	return mouse_config.model_dos == MouseModelDos::Wheel;
}

// ***************************************************************************
// Delayed event support
// ***************************************************************************

static bool has_pending_event()
{
	return pending_moved || pending_button || pending_wheel;
}

static void maybe_trigger_event(); // forward declaration

static void delay_handler(uint32_t /*val*/)
{
	delay_running  = false;
	delay_finished = true;

	maybe_trigger_event();
}

static void maybe_start_delay_timer(const uint8_t timer_delay_ms)
{
	if (delay_running) {
		return;
	}
	PIC_AddEvent(delay_handler, timer_delay_ms);
	delay_running  = true;
	delay_finished = false;
}

static void maybe_trigger_event()
{
	if (!delay_finished) {
		maybe_start_delay_timer(delay_ms);
		return;
	}

	if (!has_pending_event()) {
		return;
	}

	maybe_start_delay_timer(delay_ms);
	PIC_ActivateIRQ(mouse_predefined.IRQ_PS2);
}

static void clear_pending_events()
{
	if (delay_running) {
		PIC_RemoveEvents(delay_handler);
		delay_running = false;
	}

	pending_moved  = false;
	pending_button = pending_button_state._data;
	pending_wheel  = false;
	maybe_start_delay_timer(delay_ms);
}

// ***************************************************************************
// Common helper routines
// ***************************************************************************

static uint8_t signed_to_reg8(const int8_t x)
{
	return static_cast<uint8_t>(x);
}

static uint16_t signed_to_reg16(const int16_t x)
{
	return static_cast<uint16_t>(x);
}

static int16_t reg_to_signed16(const uint16_t x)
{
	return static_cast<int16_t>(x);
}

static uint16_t get_pos_x()
{
	return static_cast<uint16_t>(std::lround(pos_x)) & state.granularity_x;
}

static uint16_t get_pos_y()
{
	return static_cast<uint16_t>(std::lround(pos_y)) & state.granularity_y;
}

static uint16_t mickey_counter_to_reg16(const float x)
{
	return static_cast<uint16_t>(std::lround(x));
}

// ***************************************************************************
// Data - default cursor/mask
// ***************************************************************************

static constexpr uint16_t default_text_and_mask = 0x77ff;
static constexpr uint16_t default_text_xor_mask = 0x7700;

// clang-format off

static uint16_t default_screen_mask[cursor_size_y] = {
	0x3fff, 0x1fff, 0x0fff, 0x07ff, 0x03ff, 0x01ff, 0x00ff, 0x007f,
	0x003f, 0x001f, 0x01ff, 0x00ff, 0x30ff, 0xf87f, 0xf87f, 0xfcff
};

static uint16_t default_cursor_mask[cursor_size_y] = {
	0x0000, 0x4000, 0x6000, 0x7000, 0x7800, 0x7c00, 0x7e00, 0x7f00,
	0x7f80, 0x7c00, 0x6c00, 0x4600, 0x0600, 0x0300, 0x0300, 0x0000
};

// clang-format on

// ***************************************************************************
// Text mode cursor
// ***************************************************************************

// Write and read directly to the screen. Do no use int_setcursorpos (LOTUS123)
extern void WriteChar(uint16_t col, uint16_t row, uint8_t page, uint8_t chr,
                      uint8_t attr, bool useattr);
extern void ReadCharAttr(uint16_t col, uint16_t row, uint8_t page, uint16_t *result);

static void restore_cursor_background_text()
{
	if (state.hidden || state.inhibit_draw)
		return;

	if (state.background.enabled) {
		WriteChar(state.background.pos_x,
		          state.background.pos_y,
		          real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE),
		          state.background.data[0],
		          state.background.data[1],
		          true);
		state.background.enabled = false;
	}
}

static void draw_cursor_text()
{
	// Restore Background
	restore_cursor_background_text();

	// Check if cursor in update region
	auto x = get_pos_x();
	auto y = get_pos_y();
	if ((y <= state.update_region_y[1]) && (y >= state.update_region_y[0]) &&
	    (x <= state.update_region_x[1]) && (x >= state.update_region_x[0])) {
		return;
	}

	// Save Background
	state.background.pos_x = static_cast<uint16_t>(x / 8);
	state.background.pos_y = static_cast<uint16_t>(y / 8);
	if (state.bios_screen_mode < 2) {
		state.background.pos_x = state.background.pos_x / 2;
	}

	// use current page (CV program)
	const uint8_t page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);

	if (state.cursor_type == MouseCursor::Software ||
	    state.cursor_type == MouseCursor::Text) { // needed by MS Word 5.5
		uint16_t result = 0;
		ReadCharAttr(state.background.pos_x,
		             state.background.pos_y,
		             page,
		             &result); // result is in native/host-endian format
		state.background.data[0] = read_low_byte(result);
		state.background.data[1] = read_high_byte(result);
		state.background.enabled = true;

		// Write Cursor
		result = result & state.text_and_mask;
		result = result ^ state.text_xor_mask;

		WriteChar(state.background.pos_x,
		          state.background.pos_y,
		          page,
		          read_low_byte(result),
		          read_high_byte(result),
		          true);
	} else {
		uint16_t address = static_cast<uint16_t>(
		        page * real_readw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE));
		address = static_cast<uint16_t>(
		        address + (state.background.pos_y *
		                           real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) +
		                   state.background.pos_x) *
		                          2);
		address /= 2;
		const uint16_t cr = real_readw(BIOSMEM_SEG, BIOSMEM_CRTC_ADDRESS);
		IO_Write(cr, 0xe);
		IO_Write(static_cast<io_port_t>(cr + 1),
		         static_cast<uint8_t>((address >> 8) & 0xff));
		IO_Write(cr, 0xf);
		IO_Write(static_cast<io_port_t>(cr + 1),
		         static_cast<uint8_t>(address & 0xff));
	}
}

// ***************************************************************************
// Graphic mode cursor
// ***************************************************************************

static struct {
	uint8_t sequ_address    = 0;
	uint8_t sequ_data       = 0;
	uint8_t grdc_address[9] = {0};

} vga_regs;

static void save_vga_registers()
{
	if (IS_VGA_ARCH) {
		for (uint8_t i = 0; i < 9; i++) {
			IO_Write(VGAREG_GRDC_ADDRESS, i);
			vga_regs.grdc_address[i] = IO_Read(VGAREG_GRDC_DATA);
		}
		// Setup some default values in GFX regs that should work
		IO_Write(VGAREG_GRDC_ADDRESS, 3);
		IO_Write(VGAREG_GRDC_DATA, 0); // disable rotate and operation
		IO_Write(VGAREG_GRDC_ADDRESS, 5);
		IO_Write(VGAREG_GRDC_DATA,
		         vga_regs.grdc_address[5] & 0xf0); // Force read/write
		                                           // mode 0

		// Set Map to all planes. Celtic Tales
		vga_regs.sequ_address = IO_Read(VGAREG_SEQU_ADDRESS);
		IO_Write(VGAREG_SEQU_ADDRESS, 2);
		vga_regs.sequ_data = IO_Read(VGAREG_SEQU_DATA);
		IO_Write(VGAREG_SEQU_DATA, 0xF);
	} else if (machine == MCH_EGA) {
		// Set Map to all planes.
		IO_Write(VGAREG_SEQU_ADDRESS, 2);
		IO_Write(VGAREG_SEQU_DATA, 0xF);
	}
}

static void restore_vga_registers()
{
	if (IS_VGA_ARCH) {
		for (uint8_t i = 0; i < 9; i++) {
			IO_Write(VGAREG_GRDC_ADDRESS, i);
			IO_Write(VGAREG_GRDC_DATA, vga_regs.grdc_address[i]);
		}

		IO_Write(VGAREG_SEQU_ADDRESS, 2);
		IO_Write(VGAREG_SEQU_DATA, vga_regs.sequ_data);
		IO_Write(VGAREG_SEQU_ADDRESS, vga_regs.sequ_address);
	}
}

static void clip_cursor_area(int16_t &x1, int16_t &x2, int16_t &y1, int16_t &y2,
                             uint16_t &addx1, uint16_t &addx2, uint16_t &addy)
{
	addx1 = 0;
	addx2 = 0;
	addy  = 0;
	// Clip up
	if (y1 < 0) {
		addy = static_cast<uint16_t>(addy - y1);
		y1   = 0;
	}
	// Clip down
	if (y2 > state.clipy) {
		y2 = state.clipy;
	};
	// Clip left
	if (x1 < 0) {
		addx1 = static_cast<uint16_t>(addx1 - x1);
		x1    = 0;
	};
	// Clip right
	if (x2 > state.clipx) {
		addx2 = static_cast<uint16_t>(x2 - state.clipx);
		x2    = state.clipx;
	};
}

static void restore_cursor_background()
{
	if (state.hidden || state.inhibit_draw || !state.background.enabled) {
		return;
	}

	save_vga_registers();

	// Restore background
	uint16_t addx1, addx2, addy;
	uint16_t data_pos = 0;
	int16_t x1        = static_cast<int16_t>(state.background.pos_x);
	int16_t y1        = static_cast<int16_t>(state.background.pos_y);
	int16_t x2        = static_cast<int16_t>(x1 + cursor_size_x - 1);
	int16_t y2        = static_cast<int16_t>(y1 + cursor_size_y - 1);

	clip_cursor_area(x1, x2, y1, y2, addx1, addx2, addy);

	data_pos = static_cast<uint16_t>(addy * cursor_size_x);
	for (int16_t y = y1; y <= y2; y++) {
		data_pos = static_cast<uint16_t>(data_pos + addx1);
		for (int16_t x = x1; x <= x2; x++) {
			INT10_PutPixel(static_cast<uint16_t>(x),
			               static_cast<uint16_t>(y),
			               state.page,
			               state.background.data[data_pos++]);
		};
		data_pos = static_cast<uint16_t>(data_pos + addx2);
	};
	state.background.enabled = false;

	restore_vga_registers();
}

static void draw_cursor()
{
	if (state.hidden || state.inhibit_draw) {
		return;
	}

	INT10_SetCurMode();

	// In Textmode ?
	if (INT10_IsTextMode(*CurMode)) {
		draw_cursor_text();
		return;
	}

	// Check video page. Seems to be ignored for text mode.
	// hence the text mode handled above this
	// >>> removed because BIOS page is not actual page in some cases, e.g.
	// QQP games
	//    if (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE) != state.page)
	//    return;

	// Check if cursor in update region
	/*    if ((get_pos_x() >= state.update_region_x[0]) && (get_pos_y() <=
	   state.update_region_x[1]) && (get_pos_y() >= state.update_region_y[0])
	   && (GETPOS_Y <= state.update_region_y[1])) { if
	   (CurMode->type==M_TEXT16) restore_cursor_background_text(); else
	            restore_cursor_background();
	        --mouse.shown;
	        return;
	    }
	   */ /*Not sure yet what to do update region should be set to ??? */

	// Get Clipping ranges

	state.clipx = static_cast<int16_t>((Bits)CurMode->swidth - 1); // Get from
	                                                               // BIOS?
	state.clipy = static_cast<int16_t>((Bits)CurMode->sheight - 1);

	// might be vidmode == 0x13?2:1
	int16_t xratio = 640;
	if (CurMode->swidth > 0)
		xratio = static_cast<int16_t>(xratio / CurMode->swidth);
	if (xratio == 0)
		xratio = 1;

	restore_cursor_background();

	save_vga_registers();

	// Save Background
	uint16_t addx1, addx2, addy;
	uint16_t data_pos = 0;
	int16_t x1 = static_cast<int16_t>(get_pos_x() / xratio - state.hot_x);
	int16_t y1 = static_cast<int16_t>(get_pos_y() - state.hot_y);
	int16_t x2 = static_cast<int16_t>(x1 + cursor_size_x - 1);
	int16_t y2 = static_cast<int16_t>(y1 + cursor_size_y - 1);

	clip_cursor_area(x1, x2, y1, y2, addx1, addx2, addy);

	data_pos = static_cast<uint16_t>(addy * cursor_size_x);
	for (int16_t y = y1; y <= y2; y++) {
		data_pos = static_cast<uint16_t>(data_pos + addx1);
		for (int16_t x = x1; x <= x2; x++) {
			INT10_GetPixel(static_cast<uint16_t>(x),
			               static_cast<uint16_t>(y),
			               state.page,
			               &state.background.data[data_pos++]);
		};
		data_pos = static_cast<uint16_t>(data_pos + addx2);
	};
	state.background.enabled = true;
	state.background.pos_x   = static_cast<uint16_t>(get_pos_x() / xratio -
                                                       state.hot_x);
	state.background.pos_y = static_cast<uint16_t>(get_pos_y() - state.hot_y);

	// Draw Mousecursor
	data_pos               = static_cast<uint16_t>(addy * cursor_size_x);
	const auto screen_mask = state.user_screen_mask ? state.user_def_screen_mask
	                                                : default_screen_mask;
	const auto cursor_mask = state.user_cursor_mask ? state.user_def_cursor_mask
	                                                : default_cursor_mask;
	for (int16_t y = y1; y <= y2; y++) {
		uint16_t sc_mask = screen_mask[addy + y - y1];
		uint16_t cu_mask = cursor_mask[addy + y - y1];
		if (addx1 > 0) {
			sc_mask  = static_cast<uint16_t>(sc_mask << addx1);
			cu_mask  = static_cast<uint16_t>(cu_mask << addx1);
			data_pos = static_cast<uint16_t>(data_pos + addx1);
		};
		for (int16_t x = x1; x <= x2; x++) {
			constexpr auto highest_bit = (1 << (cursor_size_x - 1));
			uint8_t pixel              = 0;
			// ScreenMask
			if (sc_mask & highest_bit)
				pixel = state.background.data[data_pos];
			// CursorMask
			if (cu_mask & highest_bit)
				pixel = pixel ^ 0x0f;
			sc_mask = static_cast<uint16_t>(sc_mask << 1);
			cu_mask = static_cast<uint16_t>(cu_mask << 1);
			// Set Pixel
			INT10_PutPixel(static_cast<uint16_t>(x),
			               static_cast<uint16_t>(y),
			               state.page,
			               pixel);
			++data_pos;
		};
		data_pos = static_cast<uint16_t>(addx2 + data_pos);
	};

	restore_vga_registers();
}

// ***************************************************************************
// DOS driver interface implementation
// ***************************************************************************

static void maybe_log_mouse_model()
{
	using enum MouseModelDos;

	if (!mouse_config.dos_driver_enabled) {
		return;
	}

	static bool first_time = true;
	static MouseModelDos last_logged = {};

	if (!first_time && mouse_config.model_dos == last_logged) {
		return;
	}

	std::string model_name = {};
	switch (mouse_config.model_dos) {
	case TwoButton:
		model_name = "2 buttons";
		break;
	case ThreeButton:
		model_name = "3 buttons";
		break;
	case Wheel:
		model_name = "3 buttons + wheel";
		break;
	default:
		assertm(false, "unknown mouse model (DOS)");
		break;
	}

	if (!model_name.empty()) {
		LOG_INFO("MOUSE (DOS): Built-in driver is simulating a %s model",
			 model_name.c_str());
	}

	first_time  = false;
	last_logged = mouse_config.model_dos;
}

static void update_driver_active()
{
	mouse_shared.active_dos = (state.user_callback_mask != 0);
	MOUSE_UpdateGFX();
}

static uint8_t get_reset_wheel_8bit()
{
	if (!state.wheel_api || !has_wheel()) {
		return 0;
	}

	const auto tmp = counter_w;
	counter_w = 0; // reading always clears the counter

	// 0xff for -1, 0xfe for -2, etc.
	return signed_to_reg8(tmp);
}

static uint16_t get_reset_wheel_16bit()
{
	if (!state.wheel_api || !has_wheel()) {
		return 0;
	}

	const int16_t tmp = counter_w;
	counter_w = 0; // reading always clears the counter

	return signed_to_reg16(tmp);
}

static void set_mickey_pixel_rate(const int16_t ratio_x, const int16_t ratio_y)
{
	// According to https://www.stanislavs.org/helppc/int_33-f.html
	// the values should be non-negative (highest bit not set)

	if ((ratio_x > 0) && (ratio_y > 0)) {
		// ratio = number of mickeys per 8 pixels
		constexpr auto pixels     = 8.0f;
		state.mickeys_per_pixel_x = static_cast<float>(ratio_x) / pixels;
		state.mickeys_per_pixel_y = static_cast<float>(ratio_y) / pixels;
		state.pixels_per_mickey_x = pixels / static_cast<float>(ratio_x);
		state.pixels_per_mickey_y = pixels / static_cast<float>(ratio_y);
	}
}

static void set_double_speed_threshold(const uint16_t threshold)
{
	if (threshold) {
		state.double_speed_threshold = threshold;
	} else {
		state.double_speed_threshold = 64; // default value
	}
}

static void set_sensitivity(const uint16_t sensitivity_x,
                            const uint16_t sensitivity_y, const uint16_t unknown)
{
	const auto tmp_x = std::min(static_cast<uint16_t>(100), sensitivity_x);
	const auto tmp_y = std::min(static_cast<uint16_t>(100), sensitivity_y);
	const auto tmp_u = std::min(static_cast<uint16_t>(100), unknown);

	state.sensitivity_x = static_cast<uint8_t>(tmp_x);
	state.sensitivity_y = static_cast<uint8_t>(tmp_y);
	state.unknown_01    = static_cast<uint8_t>(tmp_u);

	// Inspired by CuteMouse, although their cursor
	// update routine is far more complex then ours
	auto calculate_coeff = [](const uint16_t value) {
		// Checked with original Microsoft mouse driver,
		// setting sensitivity to 0 stops cursor movement
		if (value == 0) {
			return 0.0f;
		}

		auto tmp = static_cast<float>(value - 1);
		return (tmp * tmp) / 3600.0f + 1.0f / 3.0f;
	};

	state.sensitivity_coeff_x = calculate_coeff(tmp_x);
	state.sensitivity_coeff_y = calculate_coeff(tmp_y);
}

static void notify_interface_rate()
{
	// Real mouse drivers set the PS/2 mouse sampling rate
	// to the following rates:
	// - A4 Pointing Device 8.04A   100 Hz
	// - CuteMouse 2.1b4            100 Hz
	// - Genius Dynamic Mouse 9.20   60 Hz
	// - Microsoft Mouse 8.20        60 Hz
	// - Mouse Systems 8.00         100 Hz
	// and the most common serial mice were 1200 bauds, which gives
	// approx. 40 Hz sampling rate limit due to COM port bandwidth.

	// Original DOSBox uses 200 Hz for callbacks, but the internal
	// states (buttons, mickey counters) are updated in realtime.
	// This is too much (at least Ultima Underworld I and II do not
	// like this).

	// Set default value to 200 Hz (which is the maximum setting for
	// PS/2 mice - and hopefully this is safe (if it's not, user can
	// always adjust it in configuration file or with MOUSECTL.COM).

	constexpr uint16_t rate_default_hz = 200;

	if (rate_is_set) {
		// Rate was set by guest application - use this value. The
		// minimum will be enforced by MouseInterface nevertheless
		MouseInterface::GetDOS()->NotifyInterfaceRate(rate_hz);
	} else if (min_rate_hz) {
		// If user set the minimum mouse rate - follow it
		MouseInterface::GetDOS()->NotifyInterfaceRate(min_rate_hz);
	} else {
		// No user setting in effect - use default value
		MouseInterface::GetDOS()->NotifyInterfaceRate(rate_default_hz);
	}
}

static void set_interrupt_rate(const uint16_t rate_id)
{
	uint16_t val_hz;

	switch (rate_id) {
	case 0:  val_hz = 0;   break; // no events, TODO: this should be simulated
	case 1:  val_hz = 30;  break;
	case 2:  val_hz = 50;  break;
	case 3:  val_hz = 100; break;
	default: val_hz = 200; break; // above 4 is not suported, set max
	}

	if (val_hz) {
		rate_is_set = true;
		rate_hz     = val_hz;
		notify_interface_rate();
	}
}

static uint8_t get_interrupt_rate()
{
	uint16_t rate_to_report = 0;
	if (rate_is_set) {
		// Rate was set by the application - report what was requested
		rate_to_report = rate_hz;
	} else {
		// Raate wasn't set - report the value closest to the real rate
		rate_to_report = MouseInterface::GetDOS()->GetRate();
	}

	if (rate_to_report == 0) {
		return 0;
	} else if (rate_to_report < (30 + 50) / 2) {
		return 1; // report 30 Hz
	} else if (rate_to_report < (50 + 100) / 2) {
		return 2; // report 50 Hz
	} else if (rate_to_report < (100 + 200) / 2) {
		return 3; // report 100 Hz
	}

	return 4; // report 200 Hz
}

static void reset_hardware()
{
	// Resetting the wheel API status in reset() might seem to be a more
	// logical approach, but this is clearly not what CuteMouse does;
	// if this is done in reset(), the DN2 is unable to use mouse wheel
	state.wheel_api = false;
	counter_w       = 0;

	PIC_SetIRQMask(mouse_predefined.IRQ_PS2, false); // lower IRQ line

	// Reset mouse refresh rate
	rate_is_set = false;
	notify_interface_rate();
}

void MOUSEDOS_NotifyMinRate(const uint16_t value_hz)
{
	min_rate_hz = value_hz;

	// If rate was set by a DOS application, don't change it
	if (rate_is_set) {
		return;
	}

	notify_interface_rate();
}

void MOUSEDOS_BeforeNewVideoMode()
{
	if (INT10_IsTextMode(*CurMode)) {
		restore_cursor_background_text();
	} else {
		restore_cursor_background();
	}

	state.hidden             = 1;
	state.oldhidden          = 1;
	state.background.enabled = false;
}

void MOUSEDOS_AfterNewVideoMode(const bool is_mode_changing)
{
	constexpr uint8_t last_non_svga_mode = 0x13;

	// Gather screen mode information

	const uint8_t bios_screen_mode = mem_readb(BIOS_VIDEO_MODE);

	const bool is_svga_mode = IS_VGA_ARCH &&
	                          (bios_screen_mode > last_non_svga_mode);
	const bool is_svga_text = is_svga_mode && INT10_IsTextMode(*CurMode);

	// Perform common actions - clear pending mouse events, etc.

	clear_pending_events();

	state.bios_screen_mode   = bios_screen_mode;
	state.granularity_x      = 0xffff;
	state.granularity_y      = 0xffff;
	state.hot_x              = 0;
	state.hot_y              = 0;
	state.user_screen_mask   = false;
	state.user_cursor_mask   = false;
	state.text_and_mask      = default_text_and_mask;
	state.text_xor_mask      = default_text_xor_mask;
	state.page               = 0;
	state.update_region_y[1] = -1; // offscreen
	state.cursor_type        = MouseCursor::Software;
	state.enabled            = true;
	state.inhibit_draw       = false;

	// Some software (like 'Down by the Laituri' game) is known to first set
	// the min/max mouse cursor position and then set VESA mode, therefore
	// (unless this is a driver reset) skip setting min/max position and
	// granularity for SVGA graphic modes

	if (is_mode_changing && is_svga_mode && !is_svga_text) {
		return;
	}

	// Helper lambda for setting text mode max position x/y

	auto set_maxpos_text = [&]() {
		constexpr uint16_t threshold_lo = 1;
		constexpr uint16_t threshold_hi = 250;

		constexpr uint16_t default_rows    = 25;
		constexpr uint16_t default_columns = 80;

		uint16_t columns = INT10_GetTextColumns();
		uint16_t rows    = INT10_GetTextRows();

		if (rows < threshold_lo || rows > threshold_hi) {
			rows = default_rows;
		}
		if (columns < threshold_lo || columns > threshold_hi) {
			columns = default_columns;
		}

		state.maxpos_x = static_cast<int16_t>(8 * columns - 1);
		state.maxpos_y = static_cast<int16_t>(8 * rows - 1);
	};

	// Set min/max position - same for all the video modes

	state.minpos_x = 0;
	state.minpos_y = 0;

	// Apply settings depending on video mode

	switch (bios_screen_mode) {
	case 0x00: // text, 40x25, black/white        (CGA, EGA, MCGA, VGA)
	case 0x01: // text, 40x25, 16 colors          (CGA, EGA, MCGA, VGA)
		state.granularity_x = 0xfff0;
		state.granularity_y = 0xfff8;
		set_maxpos_text();
		// Apply correction due to different x axis granularity
		state.maxpos_x = static_cast<int16_t>(state.maxpos_x * 2 + 1);
		break;
	case 0x02: // text, 80x25, 16 shades of gray  (CGA, EGA, MCGA, VGA)
	case 0x03: // text, 80x25, 16 colors          (CGA, EGA, MCGA, VGA)
	case 0x07: // text, 80x25, monochrome         (MDA, HERC, EGA, VGA)
		state.granularity_x = 0xfff8;
		state.granularity_y = 0xfff8;
		set_maxpos_text();
		break;
	case 0x0d: // 320x200, 16 colors    (EGA, VGA)
	case 0x13: // 320x200, 256 colors   (MCGA, VGA)
		state.granularity_x = 0xfffe;
		state.maxpos_x = 639;
		state.maxpos_y = 199;
		break;
	case 0x04: // 320x200, 4 colors     (CGA, EGA, MCGA, VGA)
	case 0x05: // 320x200, 4 colors     (CGA, EGA, MCGA, VGA)
	case 0x06: // 640x200, black/white  (CGA, EGA, MCGA, VGA)
	case 0x08: // 160x200, 16 colors    (PCjr)
	case 0x09: // 320x200, 16 colors    (PCjr)
	case 0x0a: // 640x200, 4 colors     (PCjr)
	case 0x0e: // 640x200, 16 colors    (EGA, VGA)
		// Note: Setting true horizontal resolution for <640 modes
		// can break some games, like 'Life & Death' - be careful here!
		state.maxpos_x = 639;
		state.maxpos_y = 199;
		break;
	case 0x0f: // 640x350, monochrome   (EGA, VGA)
	case 0x10: // 640x350, 16 colors    (EGA 128K, VGA)
	           // 640x350, 4 colors     (EGA 64K)
		state.maxpos_x = 639;
		state.maxpos_y = 349;
		break;
	case 0x11: // 640x480, black/white  (MCGA, VGA)
	case 0x12: // 640x480, 16 colors    (VGA)
		state.maxpos_x = 639;
		state.maxpos_y = 479;
		break;
	default: // other modes, most likely SVGA
		if (!is_svga_mode) {
			// Unsupported mode, this should probably never happen
			LOG_WARNING("MOUSE (DOS): Unknown video mode 0x%02x",
			            bios_screen_mode);
			// Try to set some sane parameters, do not draw cursor
			state.inhibit_draw = true;
			state.maxpos_x = 639;
			state.maxpos_y = 479;
		} else if (is_svga_text) {
			// SVGA text mode
			state.granularity_x = 0xfff8;
			state.granularity_y = 0xfff8;
			set_maxpos_text();
		} else {
			// SVGA graphic mode
			state.maxpos_x = static_cast<int16_t>(CurMode->swidth - 1);
			state.maxpos_y = static_cast<int16_t>(CurMode->sheight - 1);
		}
		break;
	}
}

static void reset()
{
	// Although these do not belong to the driver state,
	// reset them too to avoid any possible problems
	counter_w = 0;
	pending.Reset();

	MOUSEDOS_BeforeNewVideoMode();
	constexpr bool is_mode_changing = false;
	MOUSEDOS_AfterNewVideoMode(is_mode_changing);

	set_mickey_pixel_rate(8, 16);
	set_double_speed_threshold(0); // set default value

	state.enabled = true;

	pos_x = static_cast<float>((state.maxpos_x + 1) / 2);
	pos_y = static_cast<float>((state.maxpos_y + 1) / 2);

	state.mickey_counter_x = 0.0f;
	state.mickey_counter_y = 0.0f;

	state.last_wheel_moved_x = 0;
	state.last_wheel_moved_y = 0;

	for (uint16_t idx = 0; idx < MaxButtons; idx++) {
		state.times_pressed[idx]   = 0;
		state.times_released[idx]  = 0;
		state.last_pressed_x[idx]  = 0;
		state.last_pressed_y[idx]  = 0;
		state.last_released_x[idx] = 0;
		state.last_released_y[idx] = 0;
	}

	state.user_callback_mask    = 0;
	mouse_shared.dos_cb_running = false;

	update_driver_active();
	clear_pending_events();
}

static void limit_coordinates()
{
	auto limit = [](float &pos, const int16_t minpos, const int16_t maxpos) {
		const float min = static_cast<float>(minpos);
		const float max = static_cast<float>(maxpos);

		pos = std::clamp(pos, min, max);
	};

	limit(pos_x, state.minpos_x, state.maxpos_x);
	limit(pos_y, state.minpos_y, state.maxpos_y);
}

static void update_mickeys_on_move(float& dx, float& dy,
                                   const float x_rel,
                                   const float y_rel)
{
	auto calculate_d = [](const float rel,
	                      const float pixel_per_mickey,
	                      const float sensitivity_coeff) {
		float d = rel * pixel_per_mickey;
		// Apply the DOSBox mouse acceleration only in case
		// of raw input - avoid double acceleration (host OS
		// and DOSBox), as the results would be unpredictable.
		if (!is_input_raw ||
		    (fabs(rel) > 1.0f) ||
		    (sensitivity_coeff < 1.0f)) {
			d *= sensitivity_coeff;
		}
		// TODO: add an alternative calculation (configurable),
		// reuse MOUSE_GetBallisticsCoeff for DOS driver
		return d;
	};

	auto update_mickey = [](float& mickey, const float d,
	                        const float mickeys_per_pixel) {
        mickey += d * mickeys_per_pixel;
        if (mickey > 32767.5f || mickey < -32768.5f) {
		    mickey -= std::copysign(65536.0f, mickey);
		}
    };

	// Calculate cursor displacement
	dx = calculate_d(x_rel, state.pixels_per_mickey_x, state.sensitivity_coeff_x);
	dy = calculate_d(y_rel, state.pixels_per_mickey_y, state.sensitivity_coeff_y);

	// Update mickey counters
	update_mickey(state.mickey_counter_x, dx, state.mickeys_per_pixel_x);
	update_mickey(state.mickey_counter_y, dy, state.mickeys_per_pixel_y);
}

static void move_cursor_captured(const float x_rel, const float y_rel)
{
	// Update mickey counters
	float dx = 0.0f;
	float dy = 0.0f;
	update_mickeys_on_move(dx, dy, x_rel, y_rel);

	// Apply mouse movement according to our acceleration model
	pos_x += dx;
	pos_y += dy;
}

static void move_cursor_seamless(const float x_rel, const float y_rel,
                                 const float x_abs, const float y_abs)
{
	// Update mickey counters
	float dx = 0.0f;
	float dy = 0.0f;
	update_mickeys_on_move(dx, dy, x_rel, y_rel);

	auto calculate = [](const float absolute, const uint32_t resolution) {
		assert(resolution > 1u);
		return absolute / static_cast<float>(resolution - 1);
	};

	// Apply mouse movement to mimic host OS
	const float x = calculate(x_abs, mouse_shared.resolution_x);
	const float y = calculate(y_abs, mouse_shared.resolution_y);

	// TODO: this is probably overcomplicated, especially
	// the usage of relative movement - to be investigated
	if (INT10_IsTextMode(*CurMode)) {
		pos_x = x * 8;
		pos_x *= INT10_GetTextColumns();
		pos_y = y * 8;
		pos_y *= INT10_GetTextRows();
	} else if ((state.maxpos_x < 2048) || (state.maxpos_y < 2048) ||
	           (state.maxpos_x != state.maxpos_y)) {
		if ((state.maxpos_x > 0) && (state.maxpos_y > 0)) {
			pos_x = x * state.maxpos_x;
			pos_y = y * state.maxpos_y;
		} else {
			pos_x += x_rel;
			pos_y += y_rel;
		}
	} else {
		// Fake relative movement through absolute coordinates
		pos_x += x_rel;
		pos_y += y_rel;
	}
}

static uint8_t move_cursor()
{
	const auto old_pos_x = get_pos_x();
	const auto old_pos_y = get_pos_y();

	const auto old_mickey_x = static_cast<int16_t>(state.mickey_counter_x);
	const auto old_mickey_y = static_cast<int16_t>(state.mickey_counter_y);

	if (use_relative) {
		move_cursor_captured(MOUSE_ClampRelativeMovement(pending.x_rel),
		                     MOUSE_ClampRelativeMovement(pending.y_rel));

	} else {
		move_cursor_seamless(pending.x_rel,
		                     pending.y_rel,
		                     pending.x_abs,
		                     pending.y_abs);
	}

	// Pending relative movement is now consumed
	pending.x_rel = 0.0f;
	pending.y_rel = 0.0f;

	// Make sure cursor stays in the range defined by application
	limit_coordinates();

	// Filter out unneeded events (like sub-pixel mouse movements,
	// which won't change guest side mouse state)
	const bool abs_changed = (old_pos_x != get_pos_x()) ||
	                         (old_pos_y != get_pos_y());
	const bool rel_changed = (old_mickey_x != state.mickey_counter_x) ||
	                         (old_mickey_y != state.mickey_counter_y);

	if (abs_changed || rel_changed) {
		return static_cast<uint8_t>(MouseEventId::MouseHasMoved);
	} else {
		return 0;
	}
}

static uint8_t update_moved()
{
	if (mouse_config.dos_driver_immediate) {
		return static_cast<uint8_t>(MouseEventId::MouseHasMoved);
	} else {
		return move_cursor();
	}
}

static uint8_t update_buttons(const MouseButtons12S new_buttons_12S)
{
	if (buttons._data == new_buttons_12S._data) {
		return 0;
	}

	auto mark_pressed = [](const uint8_t idx) {
		state.last_pressed_x[idx] = get_pos_x();
		state.last_pressed_y[idx] = get_pos_y();
		++state.times_pressed[idx];
	};

	auto mark_released = [](const uint8_t idx) {
		state.last_released_x[idx] = get_pos_x();
		state.last_released_y[idx] = get_pos_y();
		++state.times_released[idx];
	};

	uint8_t mask = 0;
	if (new_buttons_12S.left && !buttons.left) {
		mark_pressed(0);
		mask |= static_cast<uint8_t>(MouseEventId::PressedLeft);
	} else if (!new_buttons_12S.left && buttons.left) {
		mark_released(0);
		mask |= static_cast<uint8_t>(MouseEventId::ReleasedLeft);
	}

	if (new_buttons_12S.right && !buttons.right) {
		mark_pressed(1);
		mask |= static_cast<uint8_t>(MouseEventId::PressedRight);
	} else if (!new_buttons_12S.right && buttons.right) {
		mark_released(1);
		mask |= static_cast<uint8_t>(MouseEventId::ReleasedRight);
	}

	if (new_buttons_12S.middle && !buttons.middle) {
		mark_pressed(2);
		mask |= static_cast<uint8_t>(MouseEventId::PressedMiddle);
	} else if (!new_buttons_12S.middle && buttons.middle) {
		mark_released(2);
		mask |= static_cast<uint8_t>(MouseEventId::ReleasedMiddle);
	}

	buttons = new_buttons_12S;
	return mask;
}

static uint8_t move_wheel()
{
	const auto consumed = MOUSE_ConsumeInt8(pending.delta_wheel);
	counter_w           = clamp_to_int8(counter_w + consumed);

	state.last_wheel_moved_x = get_pos_x();
	state.last_wheel_moved_y = get_pos_y();

	if (counter_w != 0) {
		return static_cast<uint8_t>(MouseEventId::WheelHasMoved);
	}
	return 0;
}

static uint8_t update_wheel()
{
	if (mouse_config.dos_driver_immediate) {
		return static_cast<uint8_t>(MouseEventId::WheelHasMoved);
	}
	return move_wheel();
}

void MOUSEDOS_NotifyMoved(const float x_rel, const float y_rel,
                          const float x_abs, const float y_abs)
{
	bool event_needed = false;

	if (use_relative) {
		// Uses relative mouse movements - processing is too complicated
		// to easily predict whether the event can be safely omitted
		event_needed = true;
		// TODO: this can be done, but requires refactoring
	} else {
		// Uses absolute mouse position (seamless mode), relative
		// movements can wait to be reported - they are completely
		// unreliable anyway
		constexpr float Epsilon = 0.5f;
		if (std::lround(pending.x_abs / Epsilon) != std::lround(x_abs / Epsilon) ||
		    std::lround(pending.y_abs / Epsilon) != std::lround(y_abs / Epsilon)) {
			event_needed = true;
		}
		// TODO: Consider introducing some kind of sensitivity to avoid
		// unnecessary events, for example calculated using
		// 'state.maxpos_*' and 'mouse_shared.resolution_*' values.
		// Problem: when the mouse is moved really fast and it leaves
		// the window, the guest cursor is sometimes left like 30 pixels
		// from the window's edge; we need a mitigation mechanism here
	}

	// Update values to be consumed when the event arrives
	pending.x_rel = MOUSE_ClampRelativeMovement(pending.x_rel + x_rel);
	pending.y_rel = MOUSE_ClampRelativeMovement(pending.y_rel + y_rel);
	pending.x_abs = x_abs;
	pending.y_abs = y_abs;

	// NOTES:
	//
	// It might be tempting to optimize the flow here, by skipping
	// the whole event-queue-callback flow if there is no callback
	// registered, no graphic cursor to draw, etc. Don't do this - there
	// is at least one game (Master of Orion II), which performs INT 0x33
	// calls with 0x0f parameter (changing the callback settings)
	// constantly (don't ask me, why) - doing too much optimization
	// can cause the game to skip mouse events.

	if (event_needed && mouse_config.dos_driver_immediate) {
		event_needed = (move_cursor() != 0);
	}

	if (event_needed) {
		pending_moved = true;
		maybe_trigger_event();
	}
}

void MOUSEDOS_NotifyButton(const MouseButtons12S new_buttons_12S)
{
	auto new_button_state = new_buttons_12S;
	new_button_state._data &= get_button_mask();

	if (pending_button_state != new_button_state) {
		pending_button = true;
		pending_button_state = new_button_state;
		maybe_trigger_event();
	}
}

void MOUSEDOS_NotifyWheel(const float w_rel)
{
	if (!state.wheel_api || !has_wheel()) {
		return;
	}

	// Although in some places it is possible for the guest code to get
	// wheel counter in 16-bit format, scrolling hundreds of lines in one
	// go would be insane - thus, limit the wheel counter to 8 bits and
	// reuse the code written for other mouse modules
	pending.delta_wheel = MOUSE_ClampWheelMovement(pending.delta_wheel + w_rel);

	bool event_needed = MOUSE_HasAccumulatedInt(pending.delta_wheel);
	if (event_needed && mouse_config.dos_driver_immediate) {
		event_needed = (move_wheel() != 0);
	}

	if (event_needed) {
		pending_wheel = true;
		maybe_trigger_event();
	}
}

void MOUSEDOS_NotifyModelChanged()
{
	maybe_log_mouse_model();

	// If new mouse model has no wheel, disable the extension
	if (!has_wheel()) {
		state.wheel_api = false;
		counter_w       = 0;		
	}

	// Make sure button state has no buttons which are no longer present
	MOUSEDOS_NotifyButton(pending_button_state);
}

static Bitu int33_handler()
{
	using namespace bit::literals;

	switch (reg_ax) {
	// MS MOUSE v1.0+ - reset driver and read status
	case 0x00:
		reset_hardware();
		[[fallthrough]];
	// MS MOUSE v6.0+ - software reset
	case 0x21:
		reg_ax = 0xffff; // mouse driver installed
		reg_bx = (get_num_buttons() == 2) ? 0xffff : get_num_buttons();
		reset();
		break;
	// MS MOUSE v1.0+ - show mouse cursor
	case 0x01:
		if (state.hidden) {
			--state.hidden;
		}
		state.update_region_y[1] = -1; // offscreen
		draw_cursor();
		break;
	// MS MOUSE v1.0+ - hide mouse cursor
	case 0x02:
		if (INT10_IsTextMode(*CurMode)) {
			restore_cursor_background_text();
		} else {
			restore_cursor_background();
		}
		++state.hidden;
		break;
	// MS MOUSE v1.0+ / WheelAPI v1.0+ - get position and button state
	case 0x03:
		reg_bl = buttons._data;
		reg_bh = get_reset_wheel_8bit(); // CuteMouse clears wheel
		                                 // counter too
		reg_cx = get_pos_x();
		reg_dx = get_pos_y();
		break;
	// MS MOUSE v1.0+ - position mouse cursor
	case 0x04:
	{
		// If position isn't different from current position, don't
		// change it. (position is rounded so numbers get lost when the
		// rounded number is set) (arena/simulation Wolf)
		if (reg_to_signed16(reg_cx) != get_pos_x()) {
			pos_x = static_cast<float>(reg_cx);
		}
		if (reg_to_signed16(reg_dx) != get_pos_y()) {
			pos_y = static_cast<float>(reg_dx);
		}
		limit_coordinates();
		draw_cursor();
		break;
	}
	// MS MOUSE v1.0+ / WheelAPI v1.0+ - get button press / wheel data
	case 0x05:
	{
		const uint16_t idx = reg_bx; // button index
		if (idx == 0xffff && state.wheel_api && has_wheel()) {
			// 'magic' index for checking wheel instead of button
			reg_bx = get_reset_wheel_16bit();
			reg_cx = state.last_wheel_moved_x;
			reg_dx = state.last_wheel_moved_y;
		} else if (idx < get_num_buttons()) {
			reg_ax = buttons._data;
			reg_bx = state.times_pressed[idx];
			reg_cx = state.last_pressed_x[idx];
			reg_dx = state.last_pressed_y[idx];

			state.times_pressed[idx] = 0;
		} else {
			// unsupported - try to do something sane
			// TODO: Check the real driver behavior
			reg_ax = buttons._data;
			reg_bx = 0;
			reg_cx = 0;
			reg_dx = 0;
		}
		break;
	}
	// MS MOUSE v1.0+ / WheelAPI v1.0+ - get button release / wheel data
	case 0x06:
	{
		const uint16_t idx = reg_bx; // button index
		if (idx == 0xffff && state.wheel_api && has_wheel()) {
			// 'magic' index for checking wheel instead of button
			reg_bx = get_reset_wheel_16bit();
			reg_cx = state.last_wheel_moved_x;
			reg_dx = state.last_wheel_moved_y;
		} else if (idx < get_num_buttons()) {
			reg_ax = buttons._data;
			reg_bx = state.times_released[idx];
			reg_cx = state.last_released_x[idx];
			reg_dx = state.last_released_y[idx];

			state.times_released[idx] = 0;
		} else {
			// unsupported - try to do something sane
			// TODO: Check the real driver behavior
			reg_ax = buttons._data;
			reg_bx = 0;
			reg_cx = 0;
			reg_dx = 0;
		}
		break;
	}
	// MS MOUSE v1.0+ - define horizontal cursor range
	case 0x07:
		// Lemmings set 1-640 and wants that. Iron Seed set 0-640. but
		// doesn't like 640. Iron Seed works if newvideo mode with mode
		// 13 sets 0-639. Larry 6 actually wants newvideo mode with mode
		// 13 to set it to 0-319.
		state.minpos_x = std::min(reg_to_signed16(reg_cx), reg_to_signed16(reg_dx));
		state.maxpos_x = std::max(reg_to_signed16(reg_cx), reg_to_signed16(reg_dx));
		// Battle Chess wants this
		pos_x = std::clamp(pos_x,
		                   static_cast<float>(state.minpos_x),
		                   static_cast<float>(state.maxpos_x));
		// Or alternatively this:
		// pos_x = (state.maxpos_x - state.minpos_x + 1) / 2;
		break;
	// MS MOUSE v1.0+ - define vertical cursor range
	case 0x08:
		// not sure what to take instead of the CurMode (see case 0x07
		// as well) especially the cases where sheight= 400 and we set
		// it with the mouse_reset to 200 disabled it at the moment.
		// Seems to break syndicate who want 400 in mode 13
		state.minpos_y = std::min(reg_to_signed16(reg_cx),
		                          reg_to_signed16(reg_dx));
		state.maxpos_y = std::max(reg_to_signed16(reg_cx),
		                          reg_to_signed16(reg_dx));
		// Battle Chess wants this
		pos_y = std::clamp(pos_y,
		                   static_cast<float>(state.minpos_y),
		                   static_cast<float>(state.maxpos_y));
		// Or alternatively this:
		// pos_y = (state.maxpos_y - state.minpos_y + 1) / 2;
		break;
	// MS MOUSE v3.0+ - define GFX cursor
	case 0x09:
	{
		auto clamp_hot = [](const uint16_t reg, const int cursor_size) {
			return std::clamp(reg_to_signed16(reg),
			                  static_cast<int16_t>(-cursor_size),
			                  static_cast<int16_t>(cursor_size));
		};

		PhysPt src = SegPhys(es) + reg_dx;
		MEM_BlockRead(src, state.user_def_screen_mask, cursor_size_y * 2);
		src += cursor_size_y * 2;
		MEM_BlockRead(src, state.user_def_cursor_mask, cursor_size_y * 2);
		state.user_screen_mask = true;
		state.user_cursor_mask = true;
		state.hot_x            = clamp_hot(reg_bx, cursor_size_x);
		state.hot_y            = clamp_hot(reg_cx, cursor_size_y);
		state.cursor_type      = MouseCursor::Text;
		draw_cursor();
		break;
	}
	// MS MOUSE v3.0+ - define text cursor
	case 0x0a:
		// TODO: shouldn't we use MouseCursor::Text, not
		// MouseCursor::Software?
		state.cursor_type   = (reg_bx ? MouseCursor::Hardware
		                              : MouseCursor::Software);
		state.text_and_mask = reg_cx;
		state.text_xor_mask = reg_dx;
		if (reg_bx) {
			INT10_SetCursorShape(reg_cl, reg_dl);
		}
		draw_cursor();
		break;
	// MS MOUSE v7.01+ - get screen/cursor masks and mickey counts
	case 0x27:
		reg_ax = state.text_and_mask;
		reg_bx = state.text_xor_mask;
		[[fallthrough]];
	// MS MOUSE v1.0+ - read motion data
	case 0x0b:
		reg_cx = mickey_counter_to_reg16(state.mickey_counter_x);
		reg_dx = mickey_counter_to_reg16(state.mickey_counter_y);
		// TODO: We might be losing partial mickeys, to be investigated
		state.mickey_counter_x = 0;
		state.mickey_counter_y = 0;
		break;
	// MS MOUSE v1.0+ - define user callback parameters
	case 0x0c:
		state.user_callback_mask    = reg_cx;
		state.user_callback_segment = SegValue(es);
		state.user_callback_offset  = reg_dx;
		update_driver_active();
		break;
	// MS MOUSE v1.0+ - light pen emulation on
	case 0x0d:
		// Both buttons down = pen pressed, otherwise pen considered
		// off-screen
		// TODO: maybe implement light pen using SDL touch events?
		LOG_WARNING("MOUSE (DOS): Light pen emulation not implemented");
		break;
	// MS MOUSE v1.0+ - light pen emulation off
	case 0x0e:
		// Although light pen emulation is not implemented, it is OK for
		// the application to only disable it (like 'The Settlers' game
		// is doing during initialization)
		break;
	// MS MOUSE v1.0+ - define mickey/pixel rate
	case 0x0f:
		set_mickey_pixel_rate(reg_to_signed16(reg_cx),
		                      reg_to_signed16(reg_dx));
		break;
	// MS MOUSE v1.0+ - define screen region for updating
	case 0x10:
		state.update_region_x[0] = reg_to_signed16(reg_cx);
		state.update_region_y[0] = reg_to_signed16(reg_dx);
		state.update_region_x[1] = reg_to_signed16(reg_si);
		state.update_region_y[1] = reg_to_signed16(reg_di);
		draw_cursor();
		break;
	// WheelAPI v1.0+ / Genius Mouse - get mouse capabilities
	case 0x11:
		if (has_wheel()) {
			// WheelAPI implementation
			// GTEST.COM from the Genius mouse driver package
			// reports 3 buttons if it sees this extension
			reg_ax = 0x574d; // Identifier for detection purposes
			reg_bx = 0;      // Reserved capabilities flags
			reg_cx = has_wheel() ? 1 : 0;
			// This call enables the WheelAPI extensions
			state.wheel_api = true;
			counter_w       = 0;
		} else {
			// Genius Mouse 9.06 API implementation
			reg_ax = 0xffff;
			reg_bx = get_num_buttons();		
		}
		break;
	// MS MOUSE - set large graphics cursor block
	case 0x12:
		LOG_WARNING("MOUSE (DOS): Large graphics cursor block not implemented");
		break;
	// MS MOUSE v5.0+ - set double-speed threshold
	case 0x13:
		set_double_speed_threshold(reg_bx);
		break;
	// MS MOUSE v3.0+ - exchange event-handler
	case 0x14:
	{
		const auto old_segment = state.user_callback_segment;
		const auto old_offset  = state.user_callback_offset;
		const auto old_mask    = state.user_callback_mask;
		// Set new values
		state.user_callback_mask    = reg_cx;
		state.user_callback_segment = SegValue(es);
		state.user_callback_offset  = reg_dx;
		update_driver_active();
		// Return old values
		reg_cx = old_mask;
		reg_dx = old_offset;
		SegSet16(es, old_segment);
		break;
	}
	// MS MOUSE v6.0+ - get driver storage space requirements
	case 0x15:
		reg_bx = sizeof(state);
		break;
	// MS MOUSE v6.0+ - save driver state
	case 0x16:
		MEM_BlockWrite(SegPhys(es) + reg_dx, &state, sizeof(state));
		break;
	// MS MOUSE v6.0+ - load driver state
	case 0x17:
		MEM_BlockRead(SegPhys(es) + reg_dx, &state, sizeof(state));
		pending.Reset();
		update_driver_active();
		set_sensitivity(state.sensitivity_x,
		                state.sensitivity_y,
		                state.unknown_01);
		// TODO: we should probably also fake an event for mouse
		// movement, redraw cursor, etc.
		break;
	// MS MOUSE v6.0+ - set alternate mouse user handler
	case 0x18:
	case 0x19:
		LOG_WARNING("MOUSE (DOS): Alternate mouse user handler not implemented");
		break;
	// MS MOUSE v6.0+ - set mouse sensitivity
	case 0x1a:
		// NOTE: Ralf Brown Interrupt List (and some other sources)
		// claim, that this should duplicate functions 0x0f and 0x13 -
		// this is not true at least for Mouse Systems driver v8.00 and
		// IBM/Microsoft driver v8.20
		set_sensitivity(reg_bx, reg_cx, reg_dx);
		break;
	//  MS MOUSE v6.0+ - get mouse sensitivity
	case 0x1b:
		reg_bx = state.sensitivity_x;
		reg_cx = state.sensitivity_y;
		reg_dx = state.unknown_01;
		break;
	// MS MOUSE v6.0+ - set interrupt rate
	case 0x1c:
		set_interrupt_rate(reg_bx);
		break;
	// MS MOUSE v6.0+ - set display page number
	case 0x1d:
		state.page = reg_bl;
		break;
	// MS MOUSE v6.0+ - get display page number
	case 0x1e:
		reg_bx = state.page;
		break;
	// MS MOUSE v6.0+ - disable mouse driver
	case 0x1f:
		// ES:BX old mouse driver Zero at the moment TODO
		reg_bx = 0;
		SegSet16(es, 0);
		state.enabled   = false;
		state.oldhidden = state.hidden;
		state.hidden    = 1;
		// According to Ralf Brown Interrupt List it returns 0x20 if
		// success,  but CuteMouse source code claims the code for
		// success is 0x1f. Both agree that 0xffff means failure.
		// Since reg_ax is 0x1f here, no need to change anything.
		// [FeralChild64] My results:
		// - MS driver 6.24 always returns 0xffff
		// - MS driver 8.20 returns 0xffff if 'state.enabled == false'
		// - 3rd party drivers I tested (A4Tech 8.04a, Genius 9.20,
		//   Mouse Systems 8.00, DR-DOS driver 1.1) never return anything
		break;
	// MS MOUSE v6.0+ - enable mouse driver
	case 0x20:
		state.enabled = true;
		state.hidden  = state.oldhidden;
		if (mouse_config.dos_driver_modern) {
			// Checked that MS driver alters AX this way starting
			// from version 7.
			reg_ax = 0xffff;
		}
		break;
	// MS MOUSE v6.0+ - set language for messages
	case 0x22:
		// 00h = English, 01h = French, 02h = Dutch, 03h = German, 04h =
		// Swedish 05h = Finnish, 06h = Spanish, 07h = Portugese, 08h =
		// Italian
		state.language = reg_bx;
		break;
	// MS MOUSE v6.0+ - get language for messages
	case 0x23:
		reg_bx = state.language;
		break;
	// MS MOUSE v6.26+ - get software version, mouse type, and IRQ number
	case 0x24:
		reg_bh = driver_version_major;
		reg_bl = driver_version_minor;
		// 1 = bus, 2 = serial, 3 = inport, 4 = PS/2, 5 = HP
		reg_ch = 0x04; // PS/2
		reg_cl = 0; // PS/2 mouse; for others it would be an IRQ number
		break;
	// MS MOUSE v6.26+ - get general driver information
	case 0x25:
	{
		// See https://github.com/FDOS/mouse/blob/master/int33.lst
		// AL = count of currently-active Mouse Display Drivers (MDDs)
		reg_al = 1;
		// AH - bits 0-3: interrupt rate
		//    - bits 4-5: current cursor type
		//    - bit 6: 1 = driver is newer integrated type
		//    - bit 7: 1 = loaded as device driver rather than TSR
		constexpr auto integrated_driver = (1 << 6);
		const auto cursor_type = static_cast<uint8_t>(state.cursor_type) << 4;
		reg_ah = static_cast<uint8_t>(integrated_driver | cursor_type |
		                              get_interrupt_rate());
		// BX - cursor lock flag for OS/2 to prevent reentrancy problems
		// CX - mouse code active flag (for OS/2)
		// DX - mouse driver busy flag (for OS/2)
		reg_bx = 0;
		reg_cx = 0;
		reg_dx = 0;
		break;
	}
	// MS MOUSE v6.26+ - get maximum virtual coordinates
	case 0x26:
		reg_bx = (state.enabled ? 0x0000 : 0xffff);
		reg_cx = signed_to_reg16(state.maxpos_x);
		reg_dx = signed_to_reg16(state.maxpos_y);
		break;
	// MS MOUSE v7.0+ - set video mode
	case 0x28:
		// TODO: According to PC sourcebook
		//       Entry:
		//       CX = Requested video mode
		//       DX = Font size, 0 for default
		//       Returns:
		//       DX = 0 on success, nonzero (requested video mode) if not
		LOG_WARNING("MOUSE (DOS): Set video mode not implemented");
		// TODO: once implemented, update function 0x32
		break;
	// MS MOUSE v7.0+ - enumerate video modes
	case 0x29:
		// TODO: According to PC sourcebook
		//       Entry:
		//       CX = 0 for first, != 0 for next
		//       Exit:
		//       BX:DX = named string far ptr
		//       CX = video mode number
		LOG_WARNING("MOUSE (DOS): Enumerate video modes not implemented");
		// TODO: once implemented, update function 0x32
		break;
	// MS MOUSE v7.01+ - get cursor hot spot
	case 0x2a:
		// Microsoft uses a negative byte counter
		// for cursor visibility
		reg_al = static_cast<uint8_t>(-state.hidden);
		reg_bx = signed_to_reg16(state.hot_x);
		reg_cx = signed_to_reg16(state.hot_y);
		reg_dx = 0x04; // PS/2 mouse type
		break;
	// MS MOUSE v7.0+ - load acceleration profiles
	case 0x2b:
	// MS MOUSE v7.0+ - get acceleration profiles
	case 0x2c:
	// MS MOUSE v7.0+ - select acceleration profile
	case 0x2d:
	// MS MOUSE v8.10+ - set acceleration profile names
	case 0x2e:
	// MS MOUSE v7.05+ - get/switch accelleration profile
	case 0x33:
		// Input: CX = buffer length, ES:DX = buffer address
		// Output: CX = bytes in buffer; buffer content:
		//     offset 0x00 - mouse type and port
		//     offset 0x01 - language
		//     offset 0x02 - horizontal sensitivity
		//     offset 0x03 - vertical sensitivity
		//     offset 0x04 - double speed threshold
		//     offset 0x05 - ballistic curve
		//     offset 0x06 - interrupt rate
		//     offset 0x07 - cursor mask
		//     offset 0x08 - laptop adjustment
		//     offset 0x09 - memory type
		//     offset 0x0a - super VGA flag
		//     offset 0x0b - rotation angle (2 bytes)
		//     offset 0x0d - primary button
		//     offset 0x0e - secondary button
		//     offset 0x0f - click lock enabled
		//     offset 0x10 - acceleration curves tables (324 bytes)
		LOG_WARNING("MOUSE (DOS): Custom acceleration profiles not implemented");
		// TODO: once implemented, update function 0x32
		break;
	// MS MOUSE v7.02+ - mouse hardware reset
	case 0x2f:
		LOG_WARNING("MOUSE (DOS): Hardware reset not implemented");
		// TODO: once implemented, update function 0x32
		break;
	// MS MOUSE v7.04+ - get/set BallPoint information
	case 0x30:
		LOG_WARNING("MOUSE (DOS): Get/set BallPoint information not implemented");
		// TODO: once implemented, update function 0x32
		break;
	// MS MOUSE v7.05+ - get current min/max virtual coordinates
	case 0x31:
		reg_ax = signed_to_reg16(state.minpos_x);
		reg_bx = signed_to_reg16(state.minpos_y);
		reg_cx = signed_to_reg16(state.maxpos_x);
		reg_dx = signed_to_reg16(state.maxpos_y);
		break;
	// MS MOUSE v7.05+ - get active advanced functions
	case 0x32:
		reg_ax = 0;
		reg_bx = 0; // unused
		reg_cx = 0; // unused
		reg_dx = 0; // unused
		// AL bit 0 - false; although function 0x34 is implemented, the
		//            actual MOUSE.INI file does not exists; so we
		//            should discourage calling it by the guest software
		// AL bit 1 - false, function 0x33 not supported
		bit::set(reg_al, b2); // function 0x32 supported (this one!)
		bit::set(reg_al, b3); // function 0x31 supported
		// AL bit 4 - false, function 0x30 not supported
		// AL bit 5 - false, function 0x2f not supported
		// AL bit 6 - false, function 0x2e not supported
		// AL bit 7 - false, function 0x2d not supported
		// AH bit 0 - false, function 0x2c not supported
		// AH bit 1 - false, function 0x2b not supported
		bit::set(reg_ah, b2); // function 0x2a supported
		// AH bit 3 - false, function 0x29 not supported
		// AH bit 4 - false, function 0x28 not supported
		bit::set(reg_ah, b5); // function 0x27 supported
		bit::set(reg_ah, b6); // function 0x26 supported
		bit::set(reg_ah, b7); // function 0x25 supported
		break;
	// MS MOUSE v8.0+ - get initialization file
	case 0x34:
		SegSet16(es, info_segment);
		reg_dx = info_offset_ini_file;
		break;
	// MS MOUSE v8.10+ - LCD screen large pointer support
	case 0x35:
		LOG_WARNING("MOUSE (DOS): LCD screen large pointer support not implemented");
		break;
	// MS MOUSE - return pointer to copyright string
	case 0x4d:
		SegSet16(es, info_segment);
		reg_di = info_offset_copyright;
		break;
	// MS MOUSE - get version string
	case 0x6d:
		SegSet16(es, info_segment);
		reg_di = info_offset_version;
		break;
	// Mouse Systems - installation check
	case 0x70:
	// Mouse Systems 7.01+ - unknown functionality
	// Genius Mouse 9.06+  - unknown functionality
	case 0x72:
	// Mouse Systems 7.01+ - get button assignments
	// VBADOS driver       - get driver info
	case 0x73:
	// Logitech CyberMan - get 3D position, orientation, and button status
	case 0x5301:
	// Logitech CyberMan - generate tactile feedback
	case 0x5330:
	// Logitech CyberMan - exchange event handlers
	case 0x53c0:
	// Logitech CyberMan - get static device data and driver support status
	case 0x53c1:		
	// Logitech CyberMan - get dynamic device data
	case 0x53c2:
		// Do not print out any warnings for known 3rd party oem driver
		// extensions - every software (except the one bound to the
		// particular driver) should continue working correctly even if
		// we completely ignore the call
		break;
	default:
		LOG_WARNING("MOUSE (DOS): Function 0x%04x not implemented", reg_ax);
		break;
	}
	return CBRET_NONE;
}

static Bitu mouse_bd_handler()
{
	// the stack contains offsets to register values
	const uint16_t raxpt = real_readw(SegValue(ss), static_cast<uint16_t>(reg_sp + 0x0a));
	const uint16_t rbxpt = real_readw(SegValue(ss), static_cast<uint16_t>(reg_sp + 0x08));
	const uint16_t rcxpt = real_readw(SegValue(ss), static_cast<uint16_t>(reg_sp + 0x06));
	const uint16_t rdxpt = real_readw(SegValue(ss), static_cast<uint16_t>(reg_sp + 0x04));

	// read out the actual values, registers ARE overwritten
	const uint16_t rax = real_readw(SegValue(ds), raxpt);
	reg_ax = rax;
	reg_bx = real_readw(SegValue(ds), rbxpt);
	reg_cx = real_readw(SegValue(ds), rcxpt);
	reg_dx = real_readw(SegValue(ds), rdxpt);

	// some functions are treated in a special way (additional registers)
	switch (rax) {
	case 0x09: // Define GFX Cursor
	case 0x16: // Save driver state
	case 0x17: // load driver state
		SegSet16(es, SegValue(ds));
		break;
	case 0x0c: // Define interrupt subroutine parameters
	case 0x14: // Exchange event-handler
		if (reg_bx != 0)
			SegSet16(es, reg_bx);
		else
			SegSet16(es, SegValue(ds));
		break;
	case 0x10: // Define screen region for updating
		reg_cx = real_readw(SegValue(ds), rdxpt);
		reg_dx = real_readw(SegValue(ds), static_cast<uint16_t>(rdxpt + 2));
		reg_si = real_readw(SegValue(ds), static_cast<uint16_t>(rdxpt + 4));
		reg_di = real_readw(SegValue(ds), static_cast<uint16_t>(rdxpt + 6));
		break;
	default: break;
	}

	int33_handler();

	// save back the registers, too
	real_writew(SegValue(ds), raxpt, reg_ax);
	real_writew(SegValue(ds), rbxpt, reg_bx);
	real_writew(SegValue(ds), rcxpt, reg_cx);
	real_writew(SegValue(ds), rdxpt, reg_dx);
	switch (rax) {
	case 0x1f: // Disable Mousedriver
		real_writew(SegValue(ds), rbxpt, SegValue(es));
		break;
	case 0x14: // Exchange event-handler
		real_writew(SegValue(ds), rcxpt, SegValue(es));
		break;
	default: break;
	}

	return CBRET_NONE;
}

static Bitu user_callback_handler()
{
	mouse_shared.dos_cb_running = false;
	return CBRET_NONE;
}

static void prepare_driver_info()
{
	// Prepare information to be returned by DOS mouse driver functions
	// 0x34, 0x4d, and 0x6f

	if (info_segment) {
		assert(false);
		return;
	}

	const std::string str_copyright = DOSBOX_COPYRIGHT;

	auto low_nibble_str = [&](const uint8_t byte) {
		return std::to_string(low_nibble(byte));
	};
	auto high_nibble_str = [&](const uint8_t byte) {
		return std::to_string(high_nibble(byte));
	};

	static_assert(low_nibble(driver_version_minor) <= 9);
	static_assert(low_nibble(driver_version_major) <= 9);
	static_assert(high_nibble(driver_version_minor) <= 9);
	static_assert(high_nibble(driver_version_major) <= 9);

	std::string str_version = "version ";
	if (high_nibble(driver_version_major) > 0) {
		str_version = str_version + high_nibble_str(driver_version_major);
	}

	str_version = str_version + low_nibble_str(driver_version_major) +
	              std::string(".") + high_nibble_str(driver_version_minor) +
	              low_nibble_str(driver_version_minor);

	const size_t length_bytes = (str_version.length() + 1) +
	                            (str_copyright.length() + 1);
	assert(length_bytes <= UINT8_MAX);

	constexpr uint8_t bytes_per_block = 0x10;
	auto length_blocks = static_cast<uint16_t>(length_bytes / bytes_per_block);
	if (length_bytes % bytes_per_block) {
		length_blocks = static_cast<uint16_t>(length_blocks + 1);
	}

	info_segment = DOS_GetMemory(length_blocks);

	// TODO: if 'MOUSE.INI' file gets implemented, INT 33 function 0x32
	// should be updated to indicate function 0x34 is supported
	std::string str_combined = str_version + '\0' + str_copyright + '\0';
	const size_t size = static_cast<size_t>(length_blocks) * bytes_per_block;
	str_combined.resize(size, '\0');

	info_offset_ini_file  = check_cast<uint16_t>(str_version.length());
	info_offset_version   = 0;
	info_offset_copyright = check_cast<uint16_t>(str_version.length() + 1);

	MEM_BlockWrite(PhysicalMake(info_segment, 0),
	               str_combined.c_str(),
	               str_combined.size());
}

uint8_t MOUSEDOS_DoInterrupt()
{
	if (!has_pending_event()) {
		return 0x00;
	}

	uint8_t mask = 0x00;

	if (pending_moved) {
		mask = update_moved();

		// Taken from DOSBox X: HERE within the IRQ 12 handler
		// is the appropriate place to redraw the cursor. OSes
		// like Windows 3.1 expect real-mode code to do it in
		// response to IRQ 12, not "out of the blue" from the
		// SDL event handler like the original DOSBox code did
		// it. Doing this allows the INT 33h emulation to draw
		// the cursor while not causing Windows 3.1 to crash or
		// behave erratically.
		if (mask) {
			draw_cursor();
		}

		pending_moved = false;
	}

	if (pending_button) {
		const auto new_mask = mask | update_buttons(pending_button_state);
		mask = static_cast<uint8_t>(new_mask);

		pending_button = false;
	}

	if (pending_wheel) {
		const auto new_mask = mask | update_wheel();
		mask = static_cast<uint8_t>(new_mask);

		pending_wheel = false;
	}

	// If DOS driver's client is not interested in this particular
	// type of event - skip it

	if (!(state.user_callback_mask & mask)) {
		return 0x00;
	}

	return mask;
}

void MOUSEDOS_DoCallback(const uint8_t mask)
{
	mouse_shared.dos_cb_running = true;
	const bool mouse_moved = mask & static_cast<uint8_t>(MouseEventId::MouseHasMoved);
	const bool wheel_moved = mask & static_cast<uint8_t>(MouseEventId::WheelHasMoved);

	// Extension for Windows mouse driver by javispedro:
	// - https://git.javispedro.com/cgit/vbados.git/about/
	// which allows seamless mouse integration. It is also included in
	// DOSBox-X and Dosemu2:
	// - https://github.com/joncampbell123/dosbox-x/pull/3424
	// - https://github.com/dosemu2/dosemu2/issues/1552#issuecomment-1100777880
	// - https://github.com/dosemu2/dosemu2/commit/cd9d2dbc8e3d58dc7cbc92f172c0d447881526be
	// - https://github.com/joncampbell123/dosbox-x/commit/aec29ce28eb4b520f21ead5b2debf370183b9f28
	reg_ah = (!use_relative && mouse_moved) ? 1 : 0;

	reg_al = mask;
	reg_bl = buttons._data;
	reg_bh = wheel_moved ? get_reset_wheel_8bit() : 0;
	reg_cx = get_pos_x();
	reg_dx = get_pos_y();
	reg_si = mickey_counter_to_reg16(state.mickey_counter_x);
	reg_di = mickey_counter_to_reg16(state.mickey_counter_y);

	CPU_Push16(RealSegment(user_callback));
	CPU_Push16(RealOffset(user_callback));
	CPU_Push16(state.user_callback_segment);
	CPU_Push16(state.user_callback_offset);
}

void MOUSEDOS_FinalizeInterrupt()
{
	// Just in case our interrupt was taken over by
	// PS/2 BIOS callback, or if user interrupt
	// handler did not finish yet

	if (has_pending_event()) {
		maybe_start_delay_timer(1);
	}
}

void MOUSEDOS_NotifyInputType(const bool new_use_relative, const bool new_is_input_raw)
{
	use_relative = new_use_relative;
	is_input_raw = new_is_input_raw;
}

void MOUSEDOS_SetDelay(const uint8_t new_delay_ms)
{
	delay_ms = new_delay_ms;
}

void MOUSEDOS_Init()
{
	prepare_driver_info();

	// Callback for mouse interrupt 0x33
	const auto call_int33 = CALLBACK_Allocate();
	const auto tmp_pt = static_cast<uint16_t>(DOS_GetMemory(0x1) - 1);
	const auto int33_location = RealMake(tmp_pt, 0x10);
	CALLBACK_Setup(call_int33,
	               &int33_handler,
	               CB_MOUSE,
	               RealToPhysical(int33_location),
	               "Mouse");
	// Wasteland needs low(seg(int33))!=0 and low(ofs(int33))!=0
	real_writed(0, 0x33 << 2, int33_location);

	const auto call_mouse_bd = CALLBACK_Allocate();
	const auto tmp_offs = static_cast<uint16_t>(RealOffset(int33_location) + 2);
	CALLBACK_Setup(call_mouse_bd,
	               &mouse_bd_handler,
	               CB_RETF8,
	               PhysicalMake(RealSegment(int33_location), tmp_offs),
	               "MouseBD");
	// pseudocode for CB_MOUSE (including the special backdoor entry point):
	//    jump near i33hd
	//    callback mouse_bd_handler
	//    retf 8
	//  label i33hd:
	//    callback int33_handler
	//    iret

	// Callback for mouse user routine return
	const auto call_user = CALLBACK_Allocate();
	CALLBACK_Setup(call_user, &user_callback_handler, CB_RETF_CLI, "mouse user ret");
	user_callback = CALLBACK_RealPointer(call_user);

	state.user_callback_segment = 0x6362;    // magic value
	state.hidden                = 1;         // hide cursor on startup
	state.bios_screen_mode      = UINT8_MAX; // non-existing mode

	maybe_log_mouse_model();

	set_sensitivity(50, 50, 50);
	reset_hardware();
	reset();
}
