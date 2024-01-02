/*
 *  Copyright (C) 2022-2024  The DOSBox Staging Team
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
#include "mouseif_dos_driver_state.h"

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

// XXX add comment
using Instance = MouseDriverState;

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
static int8_t counter_w        = 0; // wheel counter // XXX this should go to driver state

static bool use_relative = true; // true = ignore absolute mouse position, use relative
static bool is_input_raw = true; // true = no host mouse acceleration pre-applied

static bool rate_is_set     = false; // true = rate was set by DOS application
static uint16_t rate_hz     = 0;
static uint16_t min_rate_hz = 0;

// Data from mouse events which were already received,
// but not necessary visible on the guest side

static struct {
	// Mouse movement
	float x_rel    = 0.0f;
	float y_rel    = 0.0f;
	uint32_t x_abs = 0;
	uint32_t y_abs = 0;

	// Wheel movement
	int16_t w_rel = 0;

	void Reset()
	{
		x_rel = 0.0f;
		y_rel = 0.0f;
		w_rel = 0;
	}
} pending;

static struct {

	bool xxx; // XXX get rid of this structure, fix data storage/restoration INT33 calls

} state;

// XXX add explanation
static struct
{
	// XXX cleanup variable names, synchronize naming with BIOS
	callback_number_t win386_callout  = 0;
	callback_number_t call_int33      = 0;
	callback_number_t call_mouse_bd   = 0;
	callback_number_t callback_return = 0;

} callback_id;

// Guest-side pointers to various driver information
static uint16_t info_segment          = 0;
static uint16_t info_offset_ini_file  = 0;
static uint16_t info_offset_version   = 0;
static uint16_t info_offset_copyright = 0;

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
	const auto absolute_x    = Instance::GetAbsoluteX();
	const auto granularity_x = Instance::GetGranularityX();

	return static_cast<uint16_t>(std::lround(absolute_x)) & granularity_x;
}

static uint16_t get_pos_y()
{
	const auto absolute_y    = Instance::GetAbsoluteY();
	const auto granularity_y = Instance::GetGranularityY();

	return static_cast<uint16_t>(std::lround(absolute_y)) & granularity_y;
}

static uint16_t mickey_counter_to_reg16(const float x)
{
	return static_cast<uint16_t>(std::lround(x));
}

// ***************************************************************************
// Data - default cursor/mask
// ***************************************************************************

static constexpr uint16_t default_text_mask_and = 0x77ff;
static constexpr uint16_t default_text_mask_xor = 0x7700;

// clang-format off

static const std::array<uint16_t, cursor_size> default_screen_mask = {
	0x3fff, 0x1fff, 0x0fff, 0x07ff, 0x03ff, 0x01ff, 0x00ff, 0x007f,
	0x003f, 0x001f, 0x01ff, 0x00ff, 0x30ff, 0xf87f, 0xf87f, 0xfcff
};

static const std::array<uint16_t, cursor_size> default_cursor_mask = {
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
	if (Instance::GetHidden() || Instance::IsInhibitDraw()) {
		return;
	}

	if (Instance::IsBackgroundEnabled()) {
		WriteChar(Instance::GetBackgroundX(),
		          Instance::GetBackgroundY(),
		          real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE),
		          Instance::GetBackgroundData(0),
		          Instance::GetBackgroundData(1),
		          true);
		Instance::SetBackgroundEnabled(false);
	}
}

static void draw_cursor_text()
{
	// Restore Background
	restore_cursor_background_text();

	// Check if cursor in update region
	auto x = get_pos_x();
	auto y = get_pos_y();
	if ((x >= Instance::GetUpdateRegionX1()) &&
	    (x <= Instance::GetUpdateRegionX2()) &&
	    (y >= Instance::GetUpdateRegionY1()) &&
	    (y <= Instance::GetUpdateRegionY2())) {
		return;
	}

	// Save Background
	auto background_x = static_cast<uint16_t>(x / 8); // XXX rename variables, maybe background_start_x?
	auto background_y = static_cast<uint16_t>(y / 8);
	if (Instance::GetBiosScreenMode() < 2) {
		background_x = background_x / 2;
	}

	Instance::SetBackgroundX(background_x);
	Instance::SetBackgroundY(background_y);

	// use current page (CV program)
	const uint8_t page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);

	const auto cursor_type = Instance::GetCursorType();
	if (cursor_type == MouseCursorType::Software ||
	    cursor_type == MouseCursorType::Text) { // needed by MS Word 5.5
		uint16_t result = 0;
		ReadCharAttr(background_x,
		             background_y,
		             page,
		             &result); // result is in native/host-endian format
		Instance::SetBackgroundData(0, read_low_byte(result));
		Instance::SetBackgroundData(1, read_high_byte(result));
		Instance::SetBackgroundEnabled(true);

		// Write Cursor
		result = result & Instance::GetTextMaskAnd();
		result = result ^ Instance::GetTextMaskXor();

		WriteChar(background_x,
		          background_y,
		          page,
		          read_low_byte(result),
		          read_high_byte(result),
		          true);
	} else {
		uint16_t address = static_cast<uint16_t>(
		        page * real_readw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE));
		address = static_cast<uint16_t>(address + (background_x * real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) + background_y) * 2);
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
	const auto clipx = Instance::GetClipX();
	const auto clipy = Instance::GetClipY();

	addx1 = 0;
	addx2 = 0;
	addy  = 0;
	// Clip up
	if (y1 < 0) {
		addy = static_cast<uint16_t>(addy - y1);
		y1   = 0;
	}
	// Clip down
	if (y2 > clipy) {
		y2 = clipy;
	};
	// Clip left
	if (x1 < 0) {
		addx1 = static_cast<uint16_t>(addx1 - x1);
		x1    = 0;
	};
	// Clip right
	if (x2 > clipx) {
		addx2 = static_cast<uint16_t>(x2 - clipx);
		x2    = clipx;
	};
}

static void restore_cursor_background()
{
	if (Instance::GetHidden() || Instance::IsInhibitDraw() || !Instance::IsBackgroundEnabled()) {
		return;
	}

	save_vga_registers();

	const auto page = Instance::GetPage();

	// Restore background
	uint16_t addx1, addx2, addy;
	uint16_t data_pos = 0;

	int16_t x1 = static_cast<int16_t>(Instance::GetBackgroundX());
	int16_t y1 = static_cast<int16_t>(Instance::GetBackgroundY());
	int16_t x2 = static_cast<int16_t>(x1 + cursor_size - 1);
	int16_t y2 = static_cast<int16_t>(y1 + cursor_size - 1);

	clip_cursor_area(x1, x2, y1, y2, addx1, addx2, addy);

	data_pos = static_cast<uint16_t>(addy * cursor_size);
	for (int16_t y = y1; y <= y2; y++) {
		data_pos = static_cast<uint16_t>(data_pos + addx1);
		for (int16_t x = x1; x <= x2; x++) {
			INT10_PutPixel(static_cast<uint16_t>(x),
			               static_cast<uint16_t>(y),
			               page,
			               Instance::GetBackgroundData(data_pos++));
		};
		data_pos = static_cast<uint16_t>(data_pos + addx2);
	};
	Instance::SetBackgroundEnabled(false);

	restore_vga_registers();
}

static void draw_cursor()
{
	if (Instance::GetHidden() || Instance::IsInhibitDraw()) {
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
	//    if (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE) != Instance::GetPage())
	//    return;

	// Get Clipping ranges
	Instance::SetClipX(static_cast<int16_t>((Bits)CurMode->swidth - 1)); // Get from BIOS?
	Instance::SetClipY(static_cast<int16_t>((Bits)CurMode->sheight - 1));

	// might be vidmode == 0x13?2:1
	int16_t xratio = 640;
	if (CurMode->swidth > 0) {
		xratio = static_cast<int16_t>(xratio / CurMode->swidth);
	}
	if (xratio == 0) {
		xratio = 1;
	}

	restore_cursor_background();

	save_vga_registers();

	const auto hotx = Instance::GetHotX();
	const auto hoty = Instance::GetHotY();
	const auto page = Instance::GetPage();

	// Save Background
	uint16_t addx1, addx2, addy;
	uint16_t data_pos = 0;
	int16_t x1 = static_cast<int16_t>(get_pos_x() / xratio - hotx);
	int16_t y1 = static_cast<int16_t>(get_pos_y() - hoty);
	int16_t x2 = static_cast<int16_t>(x1 + cursor_size - 1);
	int16_t y2 = static_cast<int16_t>(y1 + cursor_size - 1);

	clip_cursor_area(x1, x2, y1, y2, addx1, addx2, addy);

	data_pos = static_cast<uint16_t>(addy * cursor_size);
	for (int16_t y = y1; y <= y2; y++) {
		data_pos = static_cast<uint16_t>(data_pos + addx1);
		for (int16_t x = x1; x <= x2; x++) {
			uint8_t value = 0;
			INT10_GetPixel(static_cast<uint16_t>(x),
			               static_cast<uint16_t>(y),
			               page,
			               &value);
			Instance::SetBackgroundData(data_pos++, value);
		};
		data_pos = static_cast<uint16_t>(data_pos + addx2);
	};

	Instance::SetBackgroundEnabled(true);
	Instance::SetBackgroundX(static_cast<uint16_t>(get_pos_x() / xratio - hotx));
	Instance::SetBackgroundY(static_cast<uint16_t>(get_pos_y() - hoty));

	// Draw Mousecursor
	data_pos               = static_cast<uint16_t>(addy * cursor_size);
	const auto screen_mask = Instance::IsUserScreenMask() ? Instance::GetUserScreenMaskData()
	                                                      : default_screen_mask;
	const auto cursor_mask = Instance::IsUserCursorMask() ? Instance::GetUserCursorMaskData()
	                                                      : default_cursor_mask;
	for (int16_t y = y1; y <= y2; y++) {
		uint16_t sc_mask = screen_mask[check_cast<uint8_t>(addy + y - y1)];
		uint16_t cu_mask = cursor_mask[check_cast<uint8_t>(addy + y - y1)];
		if (addx1 > 0) {
			sc_mask  = static_cast<uint16_t>(sc_mask << addx1);
			cu_mask  = static_cast<uint16_t>(cu_mask << addx1);
			data_pos = static_cast<uint16_t>(data_pos + addx1);
		};
		for (int16_t x = x1; x <= x2; x++) {
			constexpr auto highest_bit = (1 << (cursor_size - 1));
			uint8_t pixel              = 0;
			// ScreenMask
			if (sc_mask & highest_bit)
				pixel = Instance::GetBackgroundData(data_pos);
			// CursorMask
			if (cu_mask & highest_bit)
				pixel = pixel ^ 0x0f;
			sc_mask = static_cast<uint16_t>(sc_mask << 1);
			cu_mask = static_cast<uint16_t>(cu_mask << 1);
			// Set Pixel
			INT10_PutPixel(static_cast<uint16_t>(x),
			               static_cast<uint16_t>(y),
			               page,
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

static uint8_t get_reset_wheel_8bit()
{
	if (!Instance::IsWheelApi()) {
		return 0;
	}

	const auto tmp = counter_w;
	counter_w      = 0; // reading always clears the counter

	// 0xff for -1, 0xfe for -2, etc.
	return signed_to_reg8(tmp);
}

static uint16_t get_reset_wheel_16bit()
{
	if (!Instance::IsWheelApi()) {
		return 0;
	}

	const int16_t tmp = counter_w;
	counter_w         = 0; // reading always clears the counter

	return signed_to_reg16(tmp);
}

static void set_mickey_pixel_rate(const int16_t ratio_x, const int16_t ratio_y)
{
	// According to https://www.stanislavs.org/helppc/int_33-f.html
	// the values should be non-negative (highest bit not set)

	if ((ratio_x > 0) && (ratio_y > 0)) {
		// ratio = number of mickeys per 8 pixels
		constexpr auto pixels     = 8.0f;
		Instance::SetMickeysPerPixelX(static_cast<float>(ratio_x) / pixels);
		Instance::SetMickeysPerPixelY(static_cast<float>(ratio_y) / pixels);
		Instance::SetPixelsPerMickeyX(pixels / static_cast<float>(ratio_x));
		Instance::SetPixelsPerMickeyY(pixels / static_cast<float>(ratio_y));
	}
}

static void set_double_speed_threshold(const uint16_t threshold)
{
	if (threshold) {
		Instance::SetDoubleSpeedThreshold(threshold);
	} else {
		Instance::SetDoubleSpeedThreshold(64); // default value XXX dedicated set default
	}
}

static void set_sensitivity(const uint16_t sensitivity_x,
                            const uint16_t sensitivity_y, const uint16_t unknown)
{
	const auto tmp_x = std::min(static_cast<uint16_t>(100), sensitivity_x);
	const auto tmp_y = std::min(static_cast<uint16_t>(100), sensitivity_y);
	const auto tmp_u = std::min(static_cast<uint16_t>(100), unknown);

	Instance::SetSensitivityX(static_cast<uint8_t>(tmp_x));
	Instance::SetSensitivityY(static_cast<uint8_t>(tmp_y));
	Instance::SetUnknown01(static_cast<uint8_t>(tmp_u));

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

	Instance::SetSensitivityCoeffX(calculate_coeff(tmp_x));
	Instance::SetSensitivityCoeffY(calculate_coeff(tmp_y));
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
	counter_w = 0;
	Instance::SetWheelApi(false);

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

	Instance::SetHidden(1);
	Instance::SetOldHidden(1);
	Instance::SetBackgroundEnabled(false);
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

	// XXX provide functions to set the defaults
	Instance::SetBiosScreenMode(bios_screen_mode);
	Instance::SetGranularityX(0xffff);
	Instance::SetGranularityY(0xffff);
	Instance::SetHotX(0);
	Instance::SetHotY(0);
	Instance::SetUserScreenMask(false);
	Instance::SetUserCursorMask(false);
	Instance::SetTextMaskAnd(default_text_mask_and);
	Instance::SetTextMaskXor(default_text_mask_xor);
	Instance::SetPage(0);
	Instance::SetUpdateRegionY2(-1); // offscreen
	Instance::SetCursorType(MouseCursorType::Software);
	Instance::SetEnabled(true);
	Instance::SetInhibitDraw(false);

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

		Instance::SetMaxPosX(static_cast<int16_t>(8 * columns - 1));
		Instance::SetMaxPosY(static_cast<int16_t>(8 * rows - 1));
	};

	// Set min/max position - same for all the video modes

	Instance::SetMinPosX(0);
	Instance::SetMinPosY(0);

	// Apply settings depending on video mode

	switch (bios_screen_mode) {
	case 0x00: // text, 40x25, black/white        (CGA, EGA, MCGA, VGA)
	case 0x01: // text, 40x25, 16 colors          (CGA, EGA, MCGA, VGA)
		Instance::SetGranularityX(0xfff0);
		Instance::SetGranularityY(0xfff8);
		set_maxpos_text();
		// Apply correction due to different x axis granularity
		Instance::SetMaxPosX(static_cast<int16_t>(Instance::GetMaxPosX() * 2 + 1));
		break;
	case 0x02: // text, 80x25, 16 shades of gray  (CGA, EGA, MCGA, VGA)
	case 0x03: // text, 80x25, 16 colors          (CGA, EGA, MCGA, VGA)
	case 0x07: // text, 80x25, monochrome         (MDA, HERC, EGA, VGA)
		Instance::SetGranularityX(0xfff8);
		Instance::SetGranularityY(0xfff8);
		set_maxpos_text();
		break;
	case 0x0d: // 320x200, 16 colors    (EGA, VGA)
	case 0x13: // 320x200, 256 colors   (MCGA, VGA)
		Instance::SetGranularityX(0xfffe);
		Instance::SetMaxPosX(639);
		Instance::SetMaxPosY(199);
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
		Instance::SetMaxPosX(639);
		Instance::SetMaxPosY(199);
		break;
	case 0x0f: // 640x350, monochrome   (EGA, VGA)
	case 0x10: // 640x350, 16 colors    (EGA 128K, VGA)
	           // 640x350, 4 colors     (EGA 64K)
		Instance::SetMaxPosX(639);
		Instance::SetMaxPosY(349);
		break;
	case 0x11: // 640x480, black/white  (MCGA, VGA)
	case 0x12: // 640x480, 16 colors    (VGA)
		Instance::SetMaxPosX(639);
		Instance::SetMaxPosY(479);
		break;
	default: // other modes, most likely SVGA
		if (!is_svga_mode) {
			// Unsupported mode, this should probably never happen
			LOG_WARNING("MOUSE (DOS): Unknown video mode 0x%02x",
			            bios_screen_mode);
			// Try to set some sane parameters, do not draw cursor
			Instance::SetInhibitDraw(true);
			Instance::SetMaxPosX(639);
			Instance::SetMaxPosY(479);
		} else if (is_svga_text) {
			// SVGA text mode
			Instance::SetGranularityX(0xfff8);
			Instance::SetGranularityY(0xfff8);
			set_maxpos_text();
		} else {
			// SVGA graphic mode
			Instance::SetMaxPosX(static_cast<int16_t>(CurMode->swidth - 1));
			Instance::SetMaxPosY(static_cast<int16_t>(CurMode->sheight - 1));
		}
		break;
	}
}

static void reset_driver()
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

	Instance::SetEnabled(true);

	Instance::SetAbsoluteX(static_cast<float>((Instance::GetMaxPosX() + 1) / 2));
	Instance::SetAbsoluteY(static_cast<float>((Instance::GetMaxPosY() + 1) / 2));

	Instance::SetMickeyCounterX(0.0f); // XXX provide functions to set default values
	Instance::SetMickeyCounterY(0.0f);

	for (uint8_t idx = 0; idx < num_buttons; idx++) {
		Instance::SetTimesPressed(idx, 0);
		Instance::SetTimesReleased(idx, 0);
		Instance::SetLastPressedX(idx, 0);
		Instance::SetLastPressedY(idx, 0);
		Instance::SetLastReleasedX(idx, 0);
		Instance::SetLastReleasedY(idx, 0);
	}
	Instance::SetLastWheelMovedX(0);
	Instance::SetLastWheelMovedY(0);

	Instance::SetUserCallbackMask(0); // XXX this should probably be avoided...

	mouse_shared.dos_cb_running = false; // XXX dangerous! move this to driver data?
	clear_pending_events(); // XXX dangerous!
}

static void limit_coordinates()
{
	auto limit = [](float &pos, const int16_t minpos, const int16_t maxpos) {
		const float min = static_cast<float>(minpos);
		const float max = static_cast<float>(maxpos);

		pos = std::clamp(pos, min, max);
	};

	auto absolute_x = Instance::GetAbsoluteX();
	auto absolute_y = Instance::GetAbsoluteY();

	limit(absolute_x, Instance::GetMinPosX(), Instance::GetMaxPosX());
	limit(absolute_y, Instance::GetMinPosY(), Instance::GetMaxPosY());

	Instance::SetAbsoluteX(absolute_x);
	Instance::SetAbsoluteY(absolute_y);
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
	dx = calculate_d(x_rel, Instance::GetPixelsPerMickeyX(), Instance::GetSensitivityCoeffX());
	dy = calculate_d(y_rel, Instance::GetPixelsPerMickeyY(), Instance::GetSensitivityCoeffY());

	// Update mickey counters

	auto mickey_counter_x = Instance::GetMickeyCounterX();
	auto mickey_counter_y = Instance::GetMickeyCounterY();
	update_mickey(mickey_counter_x, dx, Instance::GetMickeysPerPixelX());
	update_mickey(mickey_counter_y, dy, Instance::GetMickeysPerPixelY());
	Instance::SetMickeyCounterX(mickey_counter_x);
	Instance::SetMickeyCounterY(mickey_counter_y);
}

static void move_cursor_captured(const float x_rel, const float y_rel)
{
	// Update mickey counters
	float dx = 0.0f;
	float dy = 0.0f;
	update_mickeys_on_move(dx, dy, x_rel, y_rel);

	// Apply mouse movement according to our acceleration model
	Instance::SetAbsoluteX(Instance::GetAbsoluteX() + dx);
	Instance::SetAbsoluteY(Instance::GetAbsoluteY() + dy);
}

static void move_cursor_seamless(const float x_rel, const float y_rel,
                                 const uint32_t x_abs, const uint32_t y_abs)
{
	// Update mickey counters
	float dx = 0.0f;
	float dy = 0.0f;
	update_mickeys_on_move(dx, dy, x_rel, y_rel);

	auto calculate = [](const uint32_t absolute,
	                    const uint32_t resolution) {
		assert(resolution > 1u);
		return static_cast<float>(absolute) /
		       static_cast<float>(resolution - 1);
	};

	// Apply mouse movement to mimic host OS
	const float x = calculate(x_abs, mouse_shared.resolution_x);
	const float y = calculate(y_abs, mouse_shared.resolution_y);

	// TODO: this is probably overcomplicated, especially
	// the usage of relative movement - to be investigated
	auto absolute_x  = Instance::GetAbsoluteX();
	auto absolute_y  = Instance::GetAbsoluteY();
	const auto max_x = Instance::GetMaxPosX();
	const auto max_y = Instance::GetMaxPosY();
	if (INT10_IsTextMode(*CurMode)) {
		absolute_x = x * 8 * INT10_GetTextColumns();
		absolute_y = y * 8 * INT10_GetTextRows();
	} else if ((max_x < 2048) || (max_y < 2048) || (max_x != max_y)) {
		if ((max_x > 0) && (max_y > 0)) {
			absolute_x = x * max_x;
			absolute_y = y * max_y;
		} else {
			absolute_x += x_rel;
			absolute_y += y_rel;
		}
	} else {
		absolute_x += x_rel;
		absolute_y += y_rel;
	}
	Instance::SetAbsoluteX(absolute_x);
	Instance::SetAbsoluteY(absolute_y);
}

static uint8_t move_cursor()
{
	const auto old_pos_x = get_pos_x();
	const auto old_pos_y = get_pos_y();

	// XXX this cast-detect is possibly too imprecise
	const auto old_mickey_x = static_cast<int16_t>(Instance::GetMickeyCounterX());
	const auto old_mickey_y = static_cast<int16_t>(Instance::GetMickeyCounterY());

	if (use_relative) {
		move_cursor_captured(MOUSE_ClampRelativeMovement(pending.x_rel),
		                     MOUSE_ClampRelativeMovement(pending.y_rel));
	} else {
		move_cursor_seamless(pending.x_rel,
		                     pending.y_rel,
		                     pending.x_abs,
		                     pending.y_abs);
	}

	// Pending relative movement is now consummed
	pending.x_rel = 0.0f;
	pending.y_rel = 0.0f;

	// Make sure cursor stays in the range defined by application
	limit_coordinates();

	// Filter out unneeded events (like sub-pixel mouse movements,
	// which won't change guest side mouse state)
	const bool abs_changed = (old_pos_x != get_pos_x()) ||
	                         (old_pos_y != get_pos_y());
	const bool rel_changed = (old_mickey_x != Instance::GetMickeyCounterX()) ||
	                         (old_mickey_y != Instance::GetMickeyCounterY());

	if (abs_changed || rel_changed) {
		return static_cast<uint8_t>(MouseEventId::MouseHasMoved);
	} else {
		return 0;
	}
}

static uint8_t update_moved()
{
	if (mouse_config.dos_immediate) {
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
		Instance::SetLastPressedX(idx, get_pos_x());
		Instance::SetLastPressedY(idx, get_pos_y());
		const auto times_pressed = Instance::GetTimesPressed(idx);
		Instance::SetTimesPressed(idx, times_pressed + 1);
	};

	auto mark_released = [](const uint8_t idx) {
		Instance::SetLastReleasedX(idx, get_pos_x());
		Instance::SetLastReleasedY(idx, get_pos_y());
		const auto times_released = Instance::GetTimesReleased(idx);
		Instance::SetTimesReleased(idx, times_released + 1);
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
	counter_w = clamp_to_int8(static_cast<int32_t>(counter_w + pending.w_rel));

	// Pending wheel scroll is now consummed
	pending.w_rel = 0;

	Instance::SetLastWheelMovedX(get_pos_x());
	Instance::SetLastWheelMovedY(get_pos_y());

	if (counter_w != 0) {
		return static_cast<uint8_t>(MouseEventId::WheelHasMoved);
	} else {
		return 0;
	}
}

static uint8_t update_wheel()
{
	if (mouse_config.dos_immediate) {
		return static_cast<uint8_t>(MouseEventId::WheelHasMoved);
	} else {
		return move_wheel();
	}
}

void MOUSEDOS_NotifyMoved(const float x_rel, const float y_rel,
                          const uint32_t x_abs, const uint32_t y_abs)
{
	bool event_needed = false;

	if (use_relative) {
		// Uses relative mouse movements - processing is too complicated
		// to easily predict whether the event can be safely omitted
		event_needed = true;
		// TODO: this can be done, but requyires refactoring
	} else {
		// Uses absolute mouse position (seamless mode), relative
		// movements can wait to be reported - they are completely
		// unreliable anyway
		if (pending.x_abs != x_abs || pending.y_abs != y_abs)
			event_needed = true;
	}

	// Update values to be consummed when the event arrives
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

	if (event_needed && mouse_config.dos_immediate) {
		event_needed = (move_cursor() != 0);
	}

	if (event_needed) {
		pending_moved = true;
		maybe_trigger_event();
	}
}

void MOUSEDOS_NotifyButton(const MouseButtons12S new_buttons_12S)
{
	pending_button = true;
	pending_button_state = new_buttons_12S;
	maybe_trigger_event();
}

void MOUSEDOS_NotifyWheel(const int16_t w_rel)
{
	if (!Instance::IsWheelApi()) { // XXX is it safe to skip?
		return;
	}

	// Although in some places it is possible for the guest code to get
	// wheel counter in 16-bit format, scrolling hundreds of lines in one
	// go would be insane - thus, limit the wheel counter to 8 bits and
	// reuse the code written for other mouse modules
	pending.w_rel = clamp_to_int8(pending.w_rel + w_rel);

	bool event_needed = (pending.w_rel != 0);
	if (event_needed && mouse_config.dos_immediate) {
		event_needed = (move_wheel() != 0);
	}

	if (event_needed) {
		pending_wheel = true;
		maybe_trigger_event();
	}
}

static bool int2f_handler()
{
	// XXX check if interface is emulated?
	constexpr uint16_t vmd_mouse_device_id = 0x0c;
	// Returning false prevents further multiplex handlers from being called

	// XXX check https://git.javispedro.com/cgit/vbados.git/tree/mousetsr.c?id=4091173cceea8f2c3b0cdbc19a1adb6bd87c0a94#n1413
	switch (reg_ax) {
	case 0x1605: // Windows startup
	{
		const uint8_t major = static_cast<uint8_t>(reg_di >> 8);
		const uint8_t minor = static_cast<uint8_t>(reg_di & 0xff);
		LOG_INFO("MOUSE (DOS): Starting Windows %d.%d", major, minor);
		const auto link_ptr = Instance::SetupWindowsStruct(RealMake(SegValue(es), reg_bx));
		SegSet16(es, RealSegment(link_ptr));
		reg_bx = RealOffset(link_ptr);
		Instance::SetWin386Running(true);
		Instance::SetWin386DrawingCursor(false); // XXX call set default
		break;
	}
	case 0x1606: // Windows shutdown
		LOG_INFO("MOUSE (DOS): Shutting down Windows");
		Instance::ClearWindowsStruct();
		Instance::SetWin386Running(false);
		Instance::SetWin386DrawingCursor(false); // XXX call set default
		// XXX refresh cursor
		break;
	case 0x1607: // Windows device callout
		if (reg_bx == vmd_mouse_device_id) {
			LOG_ERR("XXX device callout, 0x%04x", reg_cx);
			switch (reg_cx) {
			case 0x00: // callout test
				reg_cx = 1; // we are available
				break;
			case 0x01: // mouse callout address request
			{
				const auto farptr = CALLBACK_RealPointer(callback_id.win386_callout);
				SegSet16(ds, RealSegment(farptr));
				reg_si = RealOffset(farptr);
				reg_ax = 0; // we are available
				break;
			}
			default:
				// XXX warning log
				break;
			}
		}
		break;
	case 0x4001: // switch task to background
	case 0x4002: // switch task to foreground
		break;
	default:
		break;
	}

	return true;
}

static Bitu win386_callout_handler()
{
	reg_bp = reg_sp;

	switch (reg_ax) {
	case 1:
		LOG_ERR("XXX mouse event");
		// XXX bx = x, cx = y, dx = buttons, si = events
		break;
	case 2:
		Instance::SetWin386DrawingCursor(true);
		LOG_ERR("XXX hide cursor");
		// XXX refresh cursor
		break;
	case 3:
		Instance::SetWin386DrawingCursor(false);
		LOG_ERR("XXX show cursor");
		// XXX refresh cursor
		break;
	default:
		LOG_WARNING("MOUSE (DOS): Windows callout function 0x%04x not implemented",
		            reg_ax);
		break;
	}

	return CBRET_NONE;
}

static Bitu int33_handler()
{
	using namespace bit::literals;

	switch (reg_ax) {
	case 0x00: // MS MOUSE - reset driver and read status
		reset_hardware();
		[[fallthrough]];
	case 0x21: // MS MOUSE v6.0+ - software reset
		reg_ax = 0xffff; // mouse driver installed
		reg_bx = 3;      // for 2 buttons return 0xffff
		reset_driver();
		break;
	case 0x01: // MS MOUSE v1.0+ - show mouse cursor
	{
		const auto hidden = Instance::GetHidden();
		if (hidden) {
			Instance::SetHidden(hidden - 1);
		}
		Instance::SetUpdateRegionY2(-1); // offscreen
		draw_cursor();
		break;
	}
	case 0x02: // MS MOUSE v1.0+ - hide mouse cursor
	{
		if (INT10_IsTextMode(*CurMode)) {
			restore_cursor_background_text();
		} else {
			restore_cursor_background();
		}
		const auto hidden = Instance::GetHidden();
		Instance::SetHidden(hidden + 1);
		break;
	}
	case 0x03: // MS MOUSE v1.0+ / WheelAPI v1.0+ - get position and button
	           // status
		reg_bl = buttons._data;
		reg_bh = get_reset_wheel_8bit(); // CuteMouse clears wheel
		                                 // counter too
		reg_cx = get_pos_x();
		reg_dx = get_pos_y();
		break;
	case 0x04: // MS MOUSE v1.0+ - position mouse cursor
	{
		// If position isn't different from current position, don't
		// change it. (position is rounded so numbers get lost when the
		// rounded number is set) (arena/simulation Wolf)
		if (reg_to_signed16(reg_cx) != get_pos_x()) {
			Instance::SetAbsoluteX(static_cast<float>(reg_cx));
		}
		if (reg_to_signed16(reg_dx) != get_pos_y()) {
			Instance::SetAbsoluteY(static_cast<float>(reg_dx));
		}
		limit_coordinates();
		draw_cursor();
		break;
	}
	case 0x05: // MS MOUSE v1.0+ / WheelAPI v1.0+ - get button press / wheel
	           // data
	{
		const uint16_t idx = reg_bx; // button index
		if (idx == 0xffff && Instance::IsWheelApi()) {
			// 'magic' index for checking wheel instead of button
			reg_bx = get_reset_wheel_16bit();
			reg_cx = Instance::GetLastWheelMovedX();
			reg_dx = Instance::GetLastWheelMovedY();
		} else if (idx < num_buttons) {
			reg_ax = buttons._data;
			reg_bx = Instance::GetTimesPressed(idx);
			reg_cx = Instance::GetLastPressedX(idx);
			reg_dx = Instance::GetLastPressedY(idx);
			Instance::SetTimesPressed(idx, 0);
		} else {
			// unsupported - try to do something same
			reg_ax = buttons._data;
			reg_bx = 0;
			reg_cx = 0;
			reg_dx = 0;
		}
		break;
	}
	case 0x06: // MS MOUSE v1.0+ / WheelAPI v1.0+ - get button release data
	           // / mouse wheel data
	{
		const uint16_t idx = reg_bx; // button index
		if (idx == 0xffff && Instance::IsWheelApi()) {
			// 'magic' index for checking wheel instead of button
			reg_bx = get_reset_wheel_16bit();
			reg_cx = Instance::GetLastWheelMovedX();
			reg_dx = Instance::GetLastWheelMovedY();
		} else if (idx < num_buttons) {
			reg_ax = buttons._data;
			reg_bx = Instance::GetTimesReleased(idx);
			reg_cx = Instance::GetLastReleasedX(idx);
			reg_dx = Instance::GetLastReleasedY(idx);
			Instance::SetTimesReleased(idx, 0);
		} else {
			// unsupported - try to do something same
			reg_ax = buttons._data;
			reg_bx = 0;
			reg_cx = 0;
			reg_dx = 0;
		}
		break;
	}
	case 0x07: // MS MOUSE v1.0+ - define horizontal cursor range
	{
		// Lemmings set 1-640 and wants that. Iron Seed set 0-640. but
		// doesn't like 640. Iron Seed works if newvideo mode with mode
		// 13 sets 0-639. Larry 6 actually wants newvideo mode with mode
		// 13 to set it to 0-319.
		const auto min = std::min(reg_to_signed16(reg_cx), reg_to_signed16(reg_dx));
		const auto max = std::max(reg_to_signed16(reg_cx), reg_to_signed16(reg_dx));
		Instance::SetMinPosX(min);
		Instance::SetMaxPosX(max);
		// Battle Chess wants this
		auto absolute = Instance::GetAbsoluteX();
		absolute = std::clamp(absolute, static_cast<float>(min), static_cast<float>(max));
		Instance::SetAbsoluteX(absolute);
		// Or alternatively this:
		// (Instance::GetMaxPosX() - Instance::GetMinPosX() + 1) / 2;
		break;
	}
	case 0x08: // MS MOUSE v1.0+ - define vertical cursor range
	{
		// not sure what to take instead of the CurMode (see case 0x07
		// as well) especially the cases where sheight= 400 and we set
		// it with the mouse_reset to 200 disabled it at the moment.
		// Seems to break syndicate who want 400 in mode 13
		const auto min = std::min(reg_to_signed16(reg_cx), reg_to_signed16(reg_dx));
		const auto max = std::max(reg_to_signed16(reg_cx), reg_to_signed16(reg_dx));
		Instance::SetMinPosY(min);
		Instance::SetMaxPosY(max);
		// Battle Chess wants this
		auto absolute = Instance::GetAbsoluteY();
		absolute = std::clamp(absolute, static_cast<float>(min), static_cast<float>(max));
		Instance::SetAbsoluteY(absolute);
		// Or alternatively this:
		// (Instance::GetMaxPosY() - Instance::GetMinPosY() + 1) / 2;
		break;
	}
	case 0x09: // MS MOUSE v3.0+ - define GFX cursor
	{
		auto clamp_hot = [](const uint16_t reg, const int cursor_size) {
			return std::clamp(reg_to_signed16(reg),
			                  static_cast<int16_t>(-cursor_size),
			                  static_cast<int16_t>(cursor_size));
		};

		PhysPt src = SegPhys(es) + reg_dx;
		std::array<uint16_t, cursor_size> screen_mask_data = {0};
		std::array<uint16_t, cursor_size> cursor_mask_data = {0};
		MEM_BlockRead(src, screen_mask_data.data(), cursor_size * 2);
		src += cursor_size * 2;
		MEM_BlockRead(src, cursor_mask_data.data(), cursor_size * 2);
		Instance::SetUserScreenMaskData(screen_mask_data);
		Instance::SetUserCursorMaskData(screen_mask_data);
		Instance::SetUserScreenMask(true);
		Instance::SetUserCursorMask(true);
		Instance::SetHotX(clamp_hot(reg_bx, cursor_size));
		Instance::SetHotY(clamp_hot(reg_cx, cursor_size));
		Instance::SetCursorType(MouseCursorType::Text);
		draw_cursor();
		break;
	}
	case 0x0a: // MS MOUSE v3.0+ - define text cursor
		// TODO: shouldn't we use MouseCursorType::Text, not
		// MouseCursorType::Software?
		Instance::SetCursorType(reg_bx ? MouseCursorType::Hardware
		                               : MouseCursorType::Software);
		Instance::SetTextMaskAnd(reg_cx);
		Instance::SetTextMaskXor(reg_dx);
		if (reg_bx) {
			INT10_SetCursorShape(reg_cl, reg_dl);
		}
		draw_cursor();
		break;
	case 0x27: // MS MOUSE v7.01+ - get screen/cursor masks and mickey counts
		reg_ax = Instance::GetTextMaskAnd();
		reg_bx = Instance::GetTextMaskXor();
		[[fallthrough]];
	case 0x0b: // MS MOUSE v1.0+ - read motion data
		reg_cx = mickey_counter_to_reg16(Instance::GetMickeyCounterX());
		reg_dx = mickey_counter_to_reg16(Instance::GetMickeyCounterY());
		Instance::SetMickeyCounterX(0.0f);
		Instance::SetMickeyCounterY(0.0f);
		break;
	case 0x0c: // MS MOUSE v1.0+ - define user callback parameters
		Instance::SetUserCallbackMask(reg_cx);
		Instance::SetUserCallback(SegValue(es), reg_dx);
		break;
	case 0x0d: // MS MOUSE v1.0+ - light pen emulation on
		// Both buttons down = pen pressed, otherwise pen considered
		// off-screen
		// TODO: maybe implement light pen using SDL touch events?
		LOG_WARNING("MOUSE (DOS): Light pen emulation not implemented");
		break;
	case 0x0e: // MS MOUSE v1.0+ - light pen emulation off
		// Although light pen emulation is not implemented, it is OK for
		// the application to only disable it (like 'The Settlers' game
		// is doing during initialization)
		break;
	case 0x0f: // MS MOUSE v1.0+ - define mickey/pixel rate
		set_mickey_pixel_rate(reg_to_signed16(reg_cx),
		                      reg_to_signed16(reg_dx));
		break;
	case 0x10: // MS MOUSE v1.0+ - define screen region for updating
		Instance::SetUpdateRegionX1(reg_to_signed16(reg_cx));
		Instance::SetUpdateRegionY1(reg_to_signed16(reg_dx));
		Instance::SetUpdateRegionX2(reg_to_signed16(reg_si));
		Instance::SetUpdateRegionY2(reg_to_signed16(reg_di));
		draw_cursor();
		break;
	case 0x11: // WheelAPI v1.0+ - get mouse capabilities
		reg_ax    = 0x574d; // Identifier for detection purposes
		reg_bx    = 0;      // Reserved capabilities flags
		reg_cx    = 1;      // Wheel is supported
		counter_w = 0;
		Instance::SetWheelApi(true); // Enable mouse wheel API

		// Previous implementation provided Genius Mouse 9.06 function
		// to get number of buttons
		// (https://sourceforge.net/p/dosbox/patches/32/), it was
		// returning 0xffff in reg_ax and number of buttons in reg_bx; I
		// suppose the WheelAPI extensions are more useful
		break;
	case 0x12: // MS MOUSE - set large graphics cursor block
		LOG_WARNING("MOUSE (DOS): Large graphics cursor block not implemented");
		break;
	case 0x13: // MS MOUSE v5.0+ - set double-speed threshold
		set_double_speed_threshold(reg_bx);
		break;
	case 0x14: // MS MOUSE v3.0+ - exchange event-handler
	{
		const auto old_mask    = Instance::GetUserCallbackMask();
		const auto old_segment = Instance::GetUserCallbackSegment();
		const auto old_offset  = Instance::GetUserCallbackOffset();
		// Set new values
		Instance::SetUserCallbackMask(reg_cx);
		Instance::SetUserCallback(SegValue(es), reg_dx);
		// Return old values
		reg_cx = old_mask;
		reg_dx = old_offset;
		SegSet16(es, old_segment);
		break;
	}
	case 0x15: // MS MOUSE v6.0+ - get driver storage space requirements
		reg_bx = sizeof(state);
		break;
	case 0x16: // MS MOUSE v6.0+ - save driver state
		MEM_BlockWrite(SegPhys(es) + reg_dx, &state, sizeof(state));
		break;
	case 0x17: // MS MOUSE v6.0+ - load driver state
		MEM_BlockRead(SegPhys(es) + reg_dx, &state, sizeof(state));
		pending.Reset();
		set_sensitivity(Instance::GetSensitivityX(),
		                Instance::GetSensitivityY(),
		                Instance::GetUnknown01());
		// TODO: we should probably also fake an event for mouse
		// movement, redraw cursor, etc.
		break;
	case 0x18: // MS MOUSE v6.0+ - set alternate mouse user handler
	case 0x19: // MS MOUSE v6.0+ - set alternate mouse user handler
		LOG_WARNING("MOUSE (DOS): Alternate mouse user handler not implemented");
		break;
	case 0x1a: // MS MOUSE v6.0+ - set mouse sensitivity
		// NOTE: Ralf Brown Interrupt List (and some other sources)
		// claim, that this should duplicate functions 0x0f and 0x13 -
		// this is not true at least for Mouse Systems driver v8.00 and
		// IBM/Microsoft driver v8.20
		set_sensitivity(reg_bx, reg_cx, reg_dx);
		break;
	case 0x1b: //  MS MOUSE v6.0+ - get mouse sensitivity
		reg_bx = Instance::GetSensitivityX();
		reg_cx = Instance::GetSensitivityY();
		reg_dx = Instance::GetUnknown01();
		break;
	case 0x1c: // MS MOUSE v6.0+ - set interrupt rate
		set_interrupt_rate(reg_bx);
		break;
	case 0x1d: // MS MOUSE v6.0+ - set display page number
		Instance::SetPage(reg_bl);
		break;
	case 0x1e: // MS MOUSE v6.0+ - get display page number
		reg_bx = Instance::GetPage();
		break;
	case 0x1f: // MS MOUSE v6.0+ - disable mouse driver
		// ES:BX old mouse driver Zero at the moment TODO
		reg_bx = 0;
		SegSet16(es, 0);
		Instance::SetEnabled(false);
		Instance::SetOldHidden(Instance::GetHidden());
		Instance::SetHidden(1);
		// According to Ralf Brown Interrupt List it returns 0x20 if
		// success,  but CuteMouse source code claims the code for
		// success if 0x1f. Both agree that 0xffff means failure.
		// Since reg_ax is 0x1f here, no need to change anything.
		break;
	case 0x20: // MS MOUSE v6.0+ - enable mouse driver
		Instance::SetEnabled(true);
		Instance::SetHidden(Instance::GetOldHidden());
		break;
	case 0x22: // MS MOUSE v6.0+ - set language for messages
		// 00h = English, 01h = French, 02h = Dutch, 03h = German, 04h =
		// Swedish 05h = Finnish, 06h = Spanish, 07h = Portugese, 08h =
		// Italian
		Instance::SetLanguage(reg_bx);
		break;
	case 0x23: // MS MOUSE v6.0+ - get language for messages
		Instance::GetLanguage();
		break;
	case 0x24: // MS MOUSE v6.26+ - get Software version, mouse type, and
	           // IRQ number
		reg_bh = driver_version_major;
		reg_bl = driver_version_minor;
		// 1 = bus, 2 = serial, 3 = inport, 4 = PS/2, 5 = HP
		reg_ch = 0x04; // PS/2
		reg_cl = 0; // PS/2 mouse; for others it would be an IRQ number
		break;
	case 0x25: // MS MOUSE v6.26+ - get general driver information
	{
		// See https://github.com/FDOS/mouse/blob/master/int33.lst
		// AL = count of currently-active Mouse Display Drivers (MDDs)
		reg_al = 1;
		// AH - bits 0-3: interrupt rate
		//    - bits 4-5: current cursor type
		//    - bit 6: 1 = driver is newer integrated type
		//    - bit 7: 1 = loaded as device driver rather than TSR
		constexpr auto integrated_driver = (1 << 6);
		const auto cursor_type = static_cast<uint8_t>(Instance::GetCursorType()) << 4;
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
	case 0x26: // MS MOUSE v6.26+ - get maximum virtual coordinates
		reg_bx = (Instance::IsEnabled() ? 0x0000 : 0xffff);
		reg_cx = signed_to_reg16(Instance::GetMaxPosX());
		reg_dx = signed_to_reg16(Instance::GetMaxPosY());
		break;
	case 0x28: // MS MOUSE v7.0+ - set video mode
		// TODO: According to PC sourcebook
		//       Entry:
		//       CX = Requested video mode
		//       DX = Font size, 0 for default
		//       Returns:
		//       DX = 0 on success, nonzero (requested video mode) if not
		LOG_WARNING("MOUSE (DOS): Set video mode not implemented");
		// TODO: once implemented, update function 0x32
		break;
	case 0x29: // MS MOUSE v7.0+ - enumerate video modes
		// TODO: According to PC sourcebook
		//       Entry:
		//       CX = 0 for first, != 0 for next
		//       Exit:
		//       BX:DX = named string far ptr
		//       CX = video mode number
		LOG_WARNING("MOUSE (DOS): Enumerate video modes not implemented");
		// TODO: once implemented, update function 0x32
		break;
	case 0x2a: // MS MOUSE v7.01+ - get cursor hot spot
		// Microsoft uses a negative byte counter
		// for cursor visibility
		reg_al = static_cast<uint8_t>(-Instance::GetHidden());
		reg_bx = signed_to_reg16(Instance::GetHotX());
		reg_cx = signed_to_reg16(Instance::GetHotY());
		reg_dx = 0x04; // PS/2 mouse type
		break;
	case 0x2b: // MS MOUSE v7.0+ - load acceleration profiles
	case 0x2c: // MS MOUSE v7.0+ - get acceleration profiles
	case 0x2d: // MS MOUSE v7.0+ - select acceleration profile
	case 0x2e: // MS MOUSE v8.10+ - set acceleration profile names
	case 0x33: // MS MOUSE v7.05+ - get/switch accelleration profile
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
	case 0x2f: // MS MOUSE v7.02+ - mouse hardware reset
		LOG_WARNING("MOUSE (DOS): Hardware reset not implemented");
		// TODO: once implemented, update function 0x32
		break;
	case 0x30: // MS MOUSE v7.04+ - get/set BallPoint information
		LOG_WARNING("MOUSE (DOS): Get/set BallPoint information not implemented");
		// TODO: once implemented, update function 0x32
		break;
	case 0x31: // MS MOUSE v7.05+ - get current min/max virtual coordinates
		reg_ax = signed_to_reg16(Instance::GetMinPosX());
		reg_bx = signed_to_reg16(Instance::GetMinPosY());
		reg_cx = signed_to_reg16(Instance::GetMaxPosX());
		reg_dx = signed_to_reg16(Instance::GetMaxPosY());
		break;
	case 0x32: // MS MOUSE v7.05+ - get active advanced functions
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
	case 0x34: // MS MOUSE v8.0+ - get initialization file
		SegSet16(es, info_segment);
		reg_dx = info_offset_ini_file;
		break;
	case 0x35: // MS MOUSE v8.10+ - LCD screen large pointer support
		LOG_WARNING("MOUSE (DOS): LCD screen large pointer support not implemented");
		break;
	case 0x4d: // MS MOUSE - return pointer to copyright string
		SegSet16(es, info_segment);
		reg_di = info_offset_copyright;
		break;
	case 0x6d: // MS MOUSE - get version string
		SegSet16(es, info_segment);
		reg_di = info_offset_version;
		break;
	case 0x70:   // Mouse Systems       - installation check
	case 0x72:   // Mouse Systems 7.01+ - unknown functionality
	             // Genius Mouse 9.06+  - unknown functionality
	case 0x73:   // Mouse Systems 7.01+ - get button assignments
	             // VBADOS              - get driver info
	case 0x53c1: // Logitech CyberMan   - unknown functionality
		// Do not print out any warnings for known 3rd party oem driver
		// extensions - every software (except the one bound to the
		// particular driver) should continue working correctly even if
		// we completely ignore the call
		break;
	default:
		LOG_WARNING("MOUSE (DOS): Driver function 0x%04x not implemented",
		            reg_ax);
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
		if (reg_bx != 0) {
			SegSet16(es, reg_bx);
		} else {
			SegSet16(es, SegValue(ds));
		}
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

static Bitu callback_return_handler()
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

	if (!(Instance::GetUserCallbackMask() & mask)) {
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
	reg_si = mickey_counter_to_reg16(Instance::GetMickeyCounterX());
	reg_di = mickey_counter_to_reg16(Instance::GetMickeyCounterY());

	CPU_Push16(Instance::GetCallbackReturnSegment());
	CPU_Push16(Instance::GetCallbackReturnOffset());
	CPU_Push16(Instance::GetUserCallbackSegment());
	CPU_Push16(Instance::GetUserCallbackOffset());
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

void DOS_StartMouseDriver()
{
	if (!MouseInterface::GetDOS()->IsEmulated()) {
		return; // DOS driver not emulated, nothing to do
	}

	prepare_driver_info(); // XXX move to DOS_StartMouseDriver

	// Initialize driver instance data
	if (!Instance::Initialize()) {  // XXX move to DOS_StartMouseDriver
		assert(false);
		return;
	}

	// Windows mouse callout
	CALLBACK_Setup(callback_id.win386_callout, win386_callout_handler, CB_RETF, "Windows mouse callout");

	// Callback for mouse interrupt 0x33
	const auto tmp_pt = static_cast<uint16_t>(DOS_GetMemory(0x1) - 1);
	const auto int33_location = RealMake(tmp_pt, 0x10);
	CALLBACK_Setup(callback_id.call_int33,
	               &int33_handler,
	               CB_MOUSE,
	               RealToPhysical(int33_location),
	               "Mouse");
	// Wasteland needs low(seg(int33))!=0 and low(ofs(int33))!=0
	real_writed(0, 0x33 << 2, int33_location);

	const auto tmp_offs = static_cast<uint16_t>(RealOffset(int33_location) + 2);
	CALLBACK_Setup(callback_id.call_mouse_bd,
	               &mouse_bd_handler,
	               CB_RETF8,
	               PhysicalMake(RealSegment(int33_location), tmp_offs),
	               "MouseBD");

	// Callback for mouse user routine return
	CALLBACK_Setup(callback_id.callback_return, &callback_return_handler, CB_RETF_CLI, "mouse user ret");

	const auto callback_return_farptr = CALLBACK_RealPointer(callback_id.callback_return);
	Instance::SetCallbackReturn(RealSegment(callback_return_farptr),
	                            RealOffset(callback_return_farptr));
	Instance::SetUserCallback(0x6362, 0x00); // magic value

	// XXX these should probably be the defaults
	Instance::SetHidden(1); // hide cursor on startup
	Instance::SetBiosScreenMode(UINT8_MAX); // non-existing mode

	set_sensitivity(50, 50, 50);
	reset_hardware();
	reset_driver();

	MOUSEDOS_NotifyMinRate(MouseInterface::GetDOS()->GetMinRate());
}

void MOUSEDOS_Init()
{
	// Allocate callback IDs
	callback_id.win386_callout  = CALLBACK_Allocate();
	callback_id.call_int33      = CALLBACK_Allocate();
	callback_id.call_mouse_bd   = CALLBACK_Allocate();
	callback_id.callback_return = CALLBACK_Allocate();

	// Mouse part of interrupt 0x2f
	DOS_AddMultiplexHandler(int2f_handler);
}
