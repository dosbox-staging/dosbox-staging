// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mouse.h"

#include "private/mouse_config.h"
#include "private/mouse_interfaces.h"
#include "private/mouseif_dos_driver_state.h"

#include <algorithm>

#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "dos/dos.h"
#include "dosbox_config.h"
#include "hardware/pic.h"
#include "ints/bios.h"
#include "ints/int10.h"
#include "misc/host_locale.h"
#include "misc/iso_locale_codes.h"
#include "misc/messages.h"
#include "utils/bitops.h"
#include "utils/byteorder.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

CHECK_NARROWING();

// This file implements the DOS mouse driver interface,
// using host system events

// Reference:
// - Ralf Brown's Interrupt List
// - Microsoft Windows 3.1 Device Development Kit documentation
// - WHEELAPI.TXT, INT10.LST, and INT33.LST from CuteMouse driver
// - https://www.stanislavs.org/helppc/int_33.html
// - http://www2.ift.ulaval.ca/~marchand/ift17583/dosints.pdf
// - https://github.com/FDOS/mouse/blob/master/int33.lst
// - https://www.fysnet.net/faq.htm

// Versions are stored in BCD code - 0x09 = version 9, 0x10 = version 10, etc.
static constexpr uint8_t DriverVersionMajor = 0x08;
static constexpr uint8_t DriverVersionMinor = 0x05;

// Mouse driver languages known by 'msd.exe' (the Microsoft Diagnostics tool)
static const std::unordered_map<std::string, uint16_t> LanguageCodes = {
        {Iso639::English,    0x00},
        {Iso639::French,     0x01},
        {Iso639::Dutch,      0x02},
        {Iso639::German,     0x03},
        {Iso639::LowGerman,  0x03}, // a German dialect
        {Iso639::Swedish,    0x04},
        {Iso639::Finnish,    0x05},
        {Iso639::Spanish,    0x06},
        {Iso639::Portuguese, 0x07},
        {Iso639::Italian,    0x08},
};

static constexpr auto CharToPixelRatio = 8;

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

// Callback identifiers
namespace CallbackIDs {

static callback_number_t int33    = {};
static callback_number_t mouse_bd = {};
static callback_number_t user     = {};
static callback_number_t win386   = {};

} // namespace CallbackIDs

// If the driver is running in the Windows/386 compatibility mode
static bool is_win386_mode = false;
// If Windows notified us that it goes foreground
static bool is_win386_foreground = false;

// Pending (usually delayed) events

// delay to enforce between callbacks, in milliseconds
static uint8_t delay_ms = 5;

// true = delay timer is in progress
static bool delay_running  = false;
// true = delay timer expired, event can be sent immediately
static bool delay_finished = true;

// These values represent 'hardware' state, not driver state

// true = ignore absolute mouse position
static bool use_relative = true;
// true = no host mouse acceleration pre-applied
static bool is_input_raw = true;

// true = rate was set by DOS application
static bool rate_is_set     = false;
static uint16_t rate_hz     = 0;
static uint16_t min_rate_hz = 0;

// Language of messages displayed by the driver
static uint16_t driver_language = 0;

// Data from mouse events which were already received,
// but not necessary visible to the application

static struct {
	bool has_mouse_moved    = false;
	bool has_button_changed = false;
	bool has_wheel_moved    = false;

	MouseButtons12S button_state = 0;

	// If set, disable the wheel API during the next interrupt
	bool disable_wheel_api = false;

	// Mouse movement
	float x_rel = 0.0f;
	float y_rel = 0.0f;
	float x_abs = 0.0f;
	float y_abs = 0.0f;

	// Wheel movement
	float delta_wheel = 0.0f;

	void ResetCounters()
	{
		x_rel = 0.0f;
		y_rel = 0.0f;
		delta_wheel = 0.0f;
	}

	void ResetPendingEvents()
	{
		has_mouse_moved    = false;
		has_button_changed = false;
		has_wheel_moved    = false;
	}
} pending;

// Driver data DOS memory segment; 0 = data stored outside of guest memory
static std::optional<uint16_t> state_segment = {};

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

static void maybe_disable_wheel_api()
{
	if (pending.disable_wheel_api) {
		MouseDriverState state(*state_segment);
		state.SetWheelApi(0);
		state.SetCounterWheel(0);

		pending.disable_wheel_api = false;
	}
}

// ***************************************************************************
// Delayed event support
// ***************************************************************************

static bool is_immediate_mode()
{
	// If configured by user, we can update the counters immediately after
	// receiving notificaion, without event delay - but this is not
	// compatible with Windows running as a guest sysystem, as we can only
	// update the mouse driver state in a proper Windows VM context
	return mouse_config.dos_driver_immediate && !is_win386_mode;
}

static bool has_pending_event()
{
	if (is_win386_foreground) {
		MouseDriverState state(*state_segment);

		return state.Win386Pending_IsCursorMoved() ||
		       state.Win386Pending_IsButtonChanged();
	} else {
		return pending.has_mouse_moved ||
		       pending.has_button_changed ||
		       pending.has_wheel_moved;
	}
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
	PIC_ActivateIRQ(Mouse::IrqPs2);
}

static void clear_pending_events()
{
	if (delay_running) {
		PIC_RemoveEvents(delay_handler);
		delay_running = false;
	}

	pending.has_mouse_moved    = false;
	pending.has_button_changed = pending.button_state._data;
	pending.has_wheel_moved    = false;
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
	MouseDriverState state(*state_segment);

	const auto pos_x = static_cast<uint16_t>(std::lround(state.GetPosX()));
	if (mouse_config.dos_driver_no_granularity) {
		return pos_x;
	}
	return pos_x & state.GetGranularityX();
}

static uint16_t get_pos_y()
{
	MouseDriverState state(*state_segment);

	const auto pos_y = static_cast<uint16_t>(std::lround(state.GetPosY()));
	if (mouse_config.dos_driver_no_granularity) {
		return pos_y;
	}
	return pos_y & state.GetGranularityY();
}

// ***************************************************************************
// Data - default cursor/mask
// ***************************************************************************

static constexpr uint16_t DefaultTextAndMask = 0x77ff;
static constexpr uint16_t DefaultTextXorMask = 0x7700;

// clang-format off

static std::array<uint16_t, CursorSize> DefaultScreenMask = {
	0x3fff, 0x1fff, 0x0fff, 0x07ff, 0x03ff, 0x01ff, 0x00ff, 0x007f,
	0x003f, 0x001f, 0x01ff, 0x00ff, 0x30ff, 0xf87f, 0xf87f, 0xfcff
};

static std::array<uint16_t, CursorSize> DefaultCursorMask = {
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
	MouseDriverState state(*state_segment);

	if (state.GetHidden() || state.IsInhibitDraw()) {
		return;
	}

	if (state.Background_IsEnabled()) {
		WriteChar(state.Background_GetPosX(),
		          state.Background_GetPosY(),
		          real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE),
		          state.Background_GetData(0),
		          state.Background_GetData(1),
		          true);
		state.Background_SetEnabled(false);
	}
}

static void draw_cursor_text()
{
	MouseDriverState state(*state_segment);

	// Restore Background
	restore_cursor_background_text();

	// Check if cursor in update region
	auto x = get_pos_x();
	auto y = get_pos_y();
	if ((y <= state.GetUpdateRegionY(1)) && (y >= state.GetUpdateRegionY(0)) &&
	    (x <= state.GetUpdateRegionX(1)) && (x >= state.GetUpdateRegionX(0))) {
		return;
	}

	// Save Background
	state.Background_SetPosX(static_cast<uint16_t>(x / 8));
	state.Background_SetPosY(static_cast<uint16_t>(y / 8));
	if (state.GetBiosScreenMode() < 2) {
		state.Background_SetPosX(state.Background_GetPosX() / 2);
	}

	// use current page (CV program)
	const uint8_t page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);

	const auto pos_x = state.Background_GetPosX();
	const auto pos_y = state.Background_GetPosY();

	const auto cursor_type = state.GetCursorType();
	if (cursor_type == MouseCursor::Software ||
	    cursor_type == MouseCursor::Text) { // needed by MS Word 5.5
		uint16_t result = 0;
		ReadCharAttr(pos_x, pos_y, page, &result);
		// result is in native/host-endian format
		state.Background_SetData(0, read_low_byte(result));
		state.Background_SetData(1, read_high_byte(result));
		state.Background_SetEnabled(true);

		// Write Cursor
		result = result & state.GetTextAndMask();
		result = result ^ state.GetTextXorMask();

		WriteChar(pos_x,
		          pos_y,
		          page,
		          read_low_byte(result),
		          read_high_byte(result),
		          true);
	} else {
		auto address = static_cast<uint16_t>(
		        page * real_readw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE));
		address = static_cast<uint16_t>(
		        address +
		        (pos_y * real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) + pos_x) * 2);
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
	if (is_machine_vga_or_better()) {
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
	} else if (is_machine_ega()) {
		// Set Map to all planes.
		IO_Write(VGAREG_SEQU_ADDRESS, 2);
		IO_Write(VGAREG_SEQU_DATA, 0xF);
	}
}

static void restore_vga_registers()
{
	if (is_machine_vga_or_better()) {
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
	MouseDriverState state(*state_segment);

	const auto clip_x = state.GetClipX();
	const auto clip_y = state.GetClipY();

	addx1 = 0;
	addx2 = 0;
	addy  = 0;
	// Clip up
	if (y1 < 0) {
		addy = static_cast<uint16_t>(addy - y1);
		y1   = 0;
	}
	// Clip down
	if (y2 > clip_y) {
		y2 = clip_y;
	};
	// Clip left
	if (x1 < 0) {
		addx1 = static_cast<uint16_t>(addx1 - x1);
		x1    = 0;
	};
	// Clip right
	if (x2 > clip_x) {
		addx2 = static_cast<uint16_t>(x2 - clip_x);
		x2    = clip_x;
	};
}

static void restore_cursor_background_gfx()
{
	MouseDriverState state(*state_segment);

	if (state.GetHidden() || state.IsInhibitDraw() ||
	    !state.Background_IsEnabled()) {
		return;
	}

	save_vga_registers();

	// Restore background
	uint16_t addx1, addx2, addy;
	uint16_t data_pos = 0;

	auto x1 = static_cast<int16_t>(state.Background_GetPosX());
	auto y1 = static_cast<int16_t>(state.Background_GetPosY());
	auto x2 = static_cast<int16_t>(x1 + CursorSize - 1);
	auto y2 = static_cast<int16_t>(y1 + CursorSize - 1);

	clip_cursor_area(x1, x2, y1, y2, addx1, addx2, addy);

	data_pos = static_cast<uint16_t>(addy * CursorSize);
	for (int16_t y = y1; y <= y2; y++) {
		data_pos = static_cast<uint16_t>(data_pos + addx1);
		for (int16_t x = x1; x <= x2; x++) {
			INT10_PutPixel(static_cast<uint16_t>(x),
			               static_cast<uint16_t>(y),
			               state.GetPage(),
			               state.Background_GetData(data_pos++));
		};
		data_pos = static_cast<uint16_t>(data_pos + addx2);
	};
	state.Background_SetEnabled(false);

	restore_vga_registers();
}

static void restore_cursor_background()
{
	if (INT10_IsTextMode(*CurMode)) {
		restore_cursor_background_text();
	} else {
		restore_cursor_background_gfx();
	}
}

static void draw_cursor()
{
	MouseDriverState state(*state_segment);
	if (state.GetHidden() || state.IsInhibitDraw() || state.IsWin386Cursor()) {
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
	//    if (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE) !=
	//    state.GetPage()) return;

	// Check if cursor in update region
	/*    if ((get_pos_x() >= state.GetUpdateRegionX(0) && (get_pos_y() <=
	   state.GetUpdateRegionX(1)) && (get_pos_y() >=  state.GetUpdateRegionY(0))
	   && (get_pos_y() <= state.GetUpdateRegionY(1))) { if
	   (CurMode->type==M_TEXT16) restore_cursor_background_text(); else
	            restore_cursor_background_gfx();
	        --mouse.shown;
	        return;
	    }
	   */ /*Not sure yet what to do update region should be set to ??? */

	// Calculate clipping ranges
	state.SetClipX(static_cast<int16_t>((Bits)CurMode->swidth - 1));
	state.SetClipY(static_cast<int16_t>((Bits)CurMode->sheight - 1));

	// might be vidmode == 0x13?2:1
	int16_t xratio = 640;
	if (CurMode->swidth > 0)
		xratio = static_cast<int16_t>(xratio / CurMode->swidth);
	if (xratio == 0)
		xratio = 1;

	restore_cursor_background_gfx();

	save_vga_registers();

	// Save Background
	uint16_t addx1, addx2, addy;
	uint16_t data_pos = 0;

	const auto hot_x = state.GetHotX();
	const auto hot_y = state.GetHotY();

	auto x1 = static_cast<int16_t>(get_pos_x() / xratio - hot_x);
	auto y1 = static_cast<int16_t>(get_pos_y() - hot_y);
	auto x2 = static_cast<int16_t>(x1 + CursorSize - 1);
	auto y2 = static_cast<int16_t>(y1 + CursorSize - 1);

	clip_cursor_area(x1, x2, y1, y2, addx1, addx2, addy);

	data_pos = static_cast<uint16_t>(addy * CursorSize);
	for (int16_t y = y1; y <= y2; y++) {
		data_pos = static_cast<uint16_t>(data_pos + addx1);
		for (int16_t x = x1; x <= x2; x++) {
			uint8_t color = 0;
			INT10_GetPixel(static_cast<uint16_t>(x),
			               static_cast<uint16_t>(y),
			               state.GetPage(),
			               &color);
			state.Background_SetData(data_pos++, color);
		};
		data_pos = static_cast<uint16_t>(data_pos + addx2);
	};

	state.Background_SetEnabled(true);

	state.Background_SetPosX(static_cast<uint16_t>(get_pos_x() / xratio - hot_x));
	state.Background_SetPosY(static_cast<uint16_t>(get_pos_y() - hot_y));

	// Draw mouse cursor
	data_pos = static_cast<uint16_t>(addy * CursorSize);

	const auto is_user_screen_mask = state.IsUserScreenMask();
	const auto is_user_cursor_mask = state.IsUserCursorMask();

	for (int16_t y = y1; y <= y2; y++) {
		const auto idx = addy + y - y1;

		uint16_t sc_mask = is_user_screen_mask
		                         ? state.GetUserDefScreenMask(idx)
		                         : DefaultScreenMask[idx];

		uint16_t cu_mask = is_user_cursor_mask
		                         ? state.GetUserDefCursorMask(idx)
		                         : DefaultCursorMask[idx];

		if (addx1 > 0) {
			sc_mask  = static_cast<uint16_t>(sc_mask << addx1);
			cu_mask  = static_cast<uint16_t>(cu_mask << addx1);
			data_pos = static_cast<uint16_t>(data_pos + addx1);
		};
		for (int16_t x = x1; x <= x2; x++) {
			constexpr auto HighestBit = (1 << (CursorSize - 1));
			uint8_t pixel = 0;
			// ScreenMask
			if (sc_mask & HighestBit) {
				pixel = state.Background_GetData(data_pos);
			}
			// CursorMask
			if (cu_mask & HighestBit) {
				pixel = pixel ^ 0x0f;
			}
			sc_mask = static_cast<uint16_t>(sc_mask << 1);
			cu_mask = static_cast<uint16_t>(cu_mask << 1);
			// Set Pixel
			INT10_PutPixel(static_cast<uint16_t>(x),
			               static_cast<uint16_t>(y),
			               state.GetPage(),
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

	if (!state_segment.has_value()) {
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

static void reset_pending_events()
{
	if (!is_win386_mode) {
		MouseDriverState state(*state_segment);

		state.Win386Pending_SetCursorMoved(false);
		state.Win386Pending_SetButtonChanged(false);
	}

	pending.ResetCounters();
	pending.ResetPendingEvents();
}

static void update_driver_active()
{
	MouseDriverState state(*state_segment);

	mouse_shared.active_dos = (state.GetUserCallbackMask() != 0);
	MOUSE_UpdateGFX();
}

static uint8_t get_reset_wheel_8bit()
{
	MouseDriverState state(*state_segment);

	if (!state.GetWheelApi() || !has_wheel()) {
		return 0;
	}

	const auto tmp = state.GetCounterWheel();
	// Reading always clears the counter
	state.SetCounterWheel(0);

	// 0xff for -1, 0xfe for -2, etc.
	return signed_to_reg8(tmp);
}

static uint16_t get_reset_wheel_16bit()
{
	MouseDriverState state(*state_segment);

	if (!state.GetWheelApi() || !has_wheel()) {
		return 0;
	}

	const int16_t tmp = state.GetCounterWheel();
	// Reading always clears the counter
	state.SetCounterWheel(0);

	return signed_to_reg16(tmp);
}

static void set_mickey_pixel_rate(const int16_t ratio_x, const int16_t ratio_y)
{
	// According to https://www.stanislavs.org/helppc/int_33-f.html
	// the values should be non-negative (highest bit not set)

	if ((ratio_x > 0) && (ratio_y > 0)) {
		// ratio = number of mickeys per 8 pixels
		constexpr auto pixels     = 8.0f;

		MouseDriverState state(*state_segment);

		state.SetMickeysPerPixelX(static_cast<float>(ratio_x) / pixels);
		state.SetMickeysPerPixelY(static_cast<float>(ratio_y) / pixels);
		state.SetPixelsPerMickeyX(pixels / static_cast<float>(ratio_x));
		state.SetPixelsPerMickeyY(pixels / static_cast<float>(ratio_y));
	}
}

static void set_double_speed_threshold(const uint16_t threshold)
{
	MouseDriverState state(*state_segment);

	if (threshold) {
		state.SetDoubleSpeedThreshold(threshold);
	} else {
		state.SetDoubleSpeedThreshold(64); // default value
	}
}

static void set_sensitivity(const uint16_t sensitivity_x,
                            const uint16_t sensitivity_y, const uint16_t unknown)
{
	MouseDriverState state(*state_segment);

	const auto tmp_x = std::min(static_cast<uint16_t>(100), sensitivity_x);
	const auto tmp_y = std::min(static_cast<uint16_t>(100), sensitivity_y);
	const auto tmp_u = std::min(static_cast<uint16_t>(100), unknown);

	state.SetSensitivityX(static_cast<uint8_t>(tmp_x));
	state.SetSensitivityY(static_cast<uint8_t>(tmp_y));

	state.SetUnknownValue01(static_cast<uint8_t>(tmp_u));

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

	state.SetSensitivityCoeffX(calculate_coeff(tmp_x));
	state.SetSensitivityCoeffY(calculate_coeff(tmp_y));
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
	// always adjust it with the 'MOUSECTL.COM' tool).

	constexpr uint16_t DefaultRateHz = 200;

	auto& interface = MouseInterface::GetInstance(MouseInterfaceId::DOS);
	if (rate_is_set) {
		// Rate was set by guest application - use this value. The
		// minimum will be enforced by MouseInterface nevertheless
		interface.NotifyInterfaceRate(rate_hz);
	} else if (min_rate_hz) {
		// If user set the minimum mouse rate - follow it
		interface.NotifyInterfaceRate(min_rate_hz);
	} else {
		// No user setting in effect - use default value
		interface.NotifyInterfaceRate(DefaultRateHz);
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
		// Rate wasn't set - report the value closest to the real rate
		const auto& interface = MouseInterface::GetInstance(MouseInterfaceId::DOS);

		rate_to_report = interface.GetRate();
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

static void synchronize_driver_language()
{
	// Get the translation language
	const auto language = LanguageTerritory(MSG_GetLanguage()).GetIsoLanguageCode();

	// Find the mouse driver language code
	if (LanguageCodes.contains(language)) {
		driver_language = LanguageCodes.at(language);
	} else {
		// Couldn't match the language, set a dummy value (English)
		driver_language = 0;
	}
}

static void reset_hardware()
{
	MouseDriverState state(*state_segment);

	// Resetting the wheel API status in reset() might seem to be a more
	// logical approach, but this is clearly not what CuteMouse does;
	// if this is done in reset(), the DN2 is unable to use mouse wheel
	state.SetWheelApi(0);
	state.SetCounterWheel(0);

	// Lower the IRQ line
	PIC_SetIRQMask(Mouse::IrqPs2, false);

	// Reset mouse refresh rate
	rate_is_set = false;
	notify_interface_rate();
}

void MOUSE_NotifyLanguageChanged()
{
	synchronize_driver_language();
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
	if (!state_segment) {
		return;
	}

	if (is_win386_mode && WINDOWS_GetVmId() == WindowsKernelVmId) {
		// This is Windows switching the video mode, not the DOS VM
		return;
	}

	MouseDriverState state(*state_segment);

	restore_cursor_background();

	state.SetHidden(1);
	state.SetOldHidden(1);
	state.Background_SetEnabled(false);
}

void MOUSEDOS_AfterNewVideoMode(const bool is_mode_changing)
{
	if (!state_segment) {
		return;
	}

	if (is_win386_mode && WINDOWS_GetVmId() == WindowsKernelVmId) {
		// This is Windows switching the video mode, not the DOS VM
		return;
	}

	// Gather screen mode information

	const uint8_t bios_screen_mode = mem_readb(BIOS_VIDEO_MODE);

	const bool is_svga_mode = is_machine_vga_or_better() &&
	                          (bios_screen_mode > LastNonSvgaModeNumber);
	const bool is_svga_text = is_svga_mode && INT10_IsTextMode(*CurMode);

	// Perform common actions - clear pending mouse events, etc.

	clear_pending_events();

	MouseDriverState state(*state_segment);

	state.SetBiosScreenMode(bios_screen_mode);
	state.SetGranularityX(0xffff);
	state.SetGranularityY(0xffff);
	state.SetHotX(0);
	state.SetHotY(0);
	state.SetUserScreenMask(false);
	state.SetUserCursorMask(false);
	state.SetTextAndMask(DefaultTextAndMask);
	state.SetTextXorMask(DefaultTextXorMask);
	state.SetPage(0);
	state.SetUpdateRegionY(1, -1); // offscreen
	state.SetCursorType(MouseCursor::Software);
	state.SetEnabled(true);
	state.SetInhibitDraw(false);

	// Some software (like 'Down by the Laituri' game) is known to first set
	// the min/max mouse cursor position and then set VESA mode, therefore
	// (unless this is a driver reset) skip setting min/max position and
	// granularity for SVGA graphic modes

	if (is_mode_changing && is_svga_mode && !is_svga_text) {
		return;
	}

	// Helper lambda for setting text mode max position x/y

	auto set_maxpos_text = [&]() {
		constexpr uint16_t ThresholdLow  = 1;
		constexpr uint16_t ThresholdHigh = 250;

		constexpr uint16_t DefaultRows    = 25;
		constexpr uint16_t DefaultColumns = 80;

		uint16_t columns = INT10_GetTextColumns();
		uint16_t rows    = INT10_GetTextRows();

		if (rows < ThresholdLow || rows > ThresholdHigh) {
			rows = DefaultRows;
		}
		if (columns < ThresholdLow || columns > ThresholdHigh) {
			columns = DefaultColumns;
		}

		state.SetMaxPosX(static_cast<int16_t>(CharToPixelRatio * columns - 1));
		state.SetMaxPosY(static_cast<int16_t>(CharToPixelRatio * rows - 1));
	};

	// Set min/max position - same for all the video modes

	state.SetMinPosX(0);
	state.SetMinPosY(0);

	// Apply settings depending on video mode

	switch (bios_screen_mode) {
	case 0x00: // text, 40x25, black/white        (CGA, EGA, MCGA, VGA)
	case 0x01: // text, 40x25, 16 colors          (CGA, EGA, MCGA, VGA)
		state.SetGranularityX(0xfff0);
		state.SetGranularityY(0xfff8);
		set_maxpos_text();
		// Apply correction due to different x axis granularity
		state.SetMaxPosX(static_cast<int16_t>(state.GetMaxPosX() * 2 + 1));
		break;
	case 0x02: // text, 80x25, 16 shades of gray  (CGA, EGA, MCGA, VGA)
	case 0x03: // text, 80x25, 16 colors          (CGA, EGA, MCGA, VGA)
	case 0x07: // text, 80x25, monochrome         (MDA, HERC, EGA, VGA)
		state.SetGranularityX(0xfff8);
		state.SetGranularityY(0xfff8);
		set_maxpos_text();
		break;
	case 0x0d: // 320x200, 16 colors    (EGA, VGA)
	case 0x13: // 320x200, 256 colors   (MCGA, VGA)
		state.SetGranularityX(0xfffe);
		state.SetMaxPosX(639);
		state.SetMaxPosY(199);
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
		state.SetMaxPosX(639);
		state.SetMaxPosY(199);
		break;
	case 0x0f: // 640x350, monochrome   (EGA, VGA)
	case 0x10: // 640x350, 16 colors    (EGA 128K, VGA)
	           // 640x350, 4 colors     (EGA 64K)
		state.SetMaxPosX(639);
		state.SetMaxPosY(349);
		break;
	case 0x11: // 640x480, black/white  (MCGA, VGA)
	case 0x12: // 640x480, 16 colors    (VGA)
		state.SetMaxPosX(639);
		state.SetMaxPosY(479);
		break;
	default: // other modes, most likely SVGA
		if (!is_svga_mode) {
			// Unsupported mode, this should probably never happen
			LOG_WARNING("MOUSE (DOS): Unknown video mode 0x%02x",
			            bios_screen_mode);
			// Try to set some sane parameters, do not draw cursor
			state.SetInhibitDraw(true);
			state.SetMaxPosX(639);
			state.SetMaxPosY(479);
		} else if (is_svga_text) {
			// SVGA text mode
			state.SetGranularityX(0xfff8);
			state.SetGranularityY(0xfff8);
			set_maxpos_text();
		} else {
			// SVGA graphic mode
			state.SetMaxPosX(static_cast<int16_t>(CurMode->swidth - 1));
			state.SetMaxPosY(static_cast<int16_t>(CurMode->sheight - 1));
		}
		break;
	}
}

static void reset()
{
	MouseDriverState state(*state_segment);

	// Although these do not belong to the driver state,
	// reset them too to avoid any possible problems
	pending.ResetCounters();

	MOUSEDOS_BeforeNewVideoMode();
	constexpr bool IsModeChanging = false;
	MOUSEDOS_AfterNewVideoMode(IsModeChanging);

	set_mickey_pixel_rate(8, 16);
	set_double_speed_threshold(0); // set default value

	state.SetEnabled(true);

	state.SetPosX(static_cast<float>((state.GetMaxPosX() + 1) / 2));
	state.SetPosY(static_cast<float>((state.GetMaxPosY() + 1) / 2));

	state.SetPreciseMickeyCounterX(0.0f);
	state.SetPreciseMickeyCounterY(0.0f);
	state.SetMickeyCounterX(0);
	state.SetMickeyCounterY(0);
	state.SetCounterWheel(0);

	state.SetLastWheelMovedX(0);
	state.SetLastWheelMovedY(0);

	for (auto idx = 0; idx < MaxMouseButtons; idx++) {
		state.SetTimesPressed(idx, 0);
		state.SetTimesReleased(idx, 0);
		state.SetLastPressedX(idx, 0);
		state.SetLastPressedY(idx, 0);
		state.SetLastReleasedX(idx, 0);
		state.SetLastReleasedY(idx, 0);
	}

	state.SetUserCallbackMask(0);
	mouse_shared.dos_cb_running = false;

	update_driver_active();
	clear_pending_events();
}

static void limit_coordinates()
{
	MouseDriverState state(*state_segment);

	auto limit = [](float& pos, const int16_t minpos, const int16_t maxpos) {
		const auto min = static_cast<float>(minpos);
		const auto max = static_cast<float>(maxpos);

		pos = std::clamp(pos, min, max);
	};

	auto pos_x = state.GetPosX();
	auto pos_y = state.GetPosY();

	limit(pos_x, state.GetMinPosX(), state.GetMaxPosX());
	limit(pos_y, state.GetMinPosY(), state.GetMaxPosY());

	state.SetPosX(pos_x);
	state.SetPosY(pos_y);
}

static void update_mickeys_on_move(float& dx, float& dy,
                                   const float x_rel,
                                   const float y_rel)
{
	MouseDriverState state(*state_segment);

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

	auto update_mickey = [](int16_t& mickey,
	                        float& precise,
	                        const float displacement,
	                        const float mickeys_per_pixel,
	                        const float threshold) {
		precise += displacement * mickeys_per_pixel;
		if (std::fabs(precise) < threshold) {
			return;
		}

		mickey = clamp_to_int16(mickey + MOUSE_ConsumeInt16(precise));
	};

	// Calculate cursor displacement
	dx = calculate_d(x_rel,
	                 state.GetPixelsPerMickeyX(),
	                 state.GetSensitivityCoeffX());
	dy = calculate_d(y_rel,
	                 state.GetPixelsPerMickeyY(),
	                 state.GetSensitivityCoeffY());

	auto precise_counter_x = state.GetPreciseMickeyCounterX();
	auto precise_counter_y = state.GetPreciseMickeyCounterY();
	auto mickey_counter_x  = state.GetMickeyCounterX();
	auto mickey_counter_y  = state.GetMickeyCounterY();

	update_mickey(mickey_counter_x,
	              precise_counter_x,
	              dx,
	              state.GetMickeysPerPixelX(),
	              mouse_config.dos_driver_move_threshold_x);

	update_mickey(mickey_counter_y,
	              precise_counter_y,
	              dy,
	              state.GetMickeysPerPixelY(),
	              mouse_config.dos_driver_move_threshold_y);

	state.SetPreciseMickeyCounterX(precise_counter_x);
	state.SetPreciseMickeyCounterY(precise_counter_y);
	state.SetMickeyCounterX(mickey_counter_x);
	state.SetMickeyCounterY(mickey_counter_y);
}

static void move_cursor_captured(const float x_rel, const float y_rel)
{
	// Update mickey counters
	float dx = 0.0f;
	float dy = 0.0f;
	update_mickeys_on_move(dx, dy, x_rel, y_rel);

	// Apply mouse movement according to our acceleration model
	MouseDriverState state(*state_segment);
	state.SetPosX(state.GetPosX() + dx);
	state.SetPosY(state.GetPosY() + dy);
}

static void move_cursor_seamless(const float x_rel, const float y_rel,
                                 const float x_abs, const float y_abs)
{
	MouseDriverState state(*state_segment);

	// Update mickey counters
	float dx = 0.0f;
	float dy = 0.0f;
	update_mickeys_on_move(dx, dy, x_rel, y_rel);

	auto calculate = [](const float absolute, const uint32_t resolution) {
		assert(resolution > 1u);
		return absolute / static_cast<float>(resolution - 1);
	};

	const uint32_t resolution_x = is_win386_foreground
	                                    ? state.GetMaxPosX()
	                                    : mouse_shared.resolution_x;
	const uint32_t resolution_y = is_win386_foreground
	                                    ? state.GetMaxPosY()
	                                    : mouse_shared.resolution_y;

	// Apply mouse movement to mimic host OS
	const float x = calculate(x_abs, resolution_x);
	const float y = calculate(y_abs, resolution_y);

	// TODO: this is probably overcomplicated, especially
	// the usage of relative movement - to be investigated
	if (INT10_IsTextMode(*CurMode)) {
		state.SetPosX(x * CharToPixelRatio * INT10_GetTextColumns());
		state.SetPosY(y * CharToPixelRatio * INT10_GetTextRows());
	} else if ((state.GetMaxPosX() < 2048) || (state.GetMaxPosY() < 2048) ||
	           (state.GetMaxPosX() != state.GetMaxPosY())) {
		if ((state.GetMaxPosX() > 0) && (state.GetMaxPosY() > 0)) {
			state.SetPosX(x * state.GetMaxPosX());
			state.SetPosY(y * state.GetMaxPosY());
		} else {
			state.SetPosX(state.GetPosX() + x_rel);
			state.SetPosY(state.GetPosY() + y_rel);
		}
	} else {
		// Fake relative movement through absolute coordinates
		state.SetPosX(state.GetPosX() + x_rel);
		state.SetPosY(state.GetPosY() + y_rel);
	}
}

static uint8_t move_cursor()
{
	MouseDriverState state(*state_segment);

	const auto old_pos_x = get_pos_x();
	const auto old_pos_y = get_pos_y();

	const auto old_mickey_x = state.GetMickeyCounterX();
	const auto old_mickey_y = state.GetMickeyCounterY();

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
	const bool rel_changed = (old_mickey_x != state.GetMickeyCounterX()) ||
	                         (old_mickey_y != state.GetMickeyCounterY());

	if (abs_changed || rel_changed) {
		return enum_val(MouseEventId::MouseHasMoved);
	} else {
		return 0;
	}
}

static uint8_t update_moved()
{
	if (is_immediate_mode()) {
		return enum_val(MouseEventId::MouseHasMoved);
	} else {
		return move_cursor();
	}
}

static uint8_t update_moved_win386()
{
	MouseDriverState state(*state_segment);
	if (!state.Win386Pending_IsCursorMoved()) {
		return 0;
	}

	move_cursor_seamless(0, 0,
	                     state.Win386Pending_GetXAbs(),
	                     state.Win386Pending_GetYAbs());
	state.Win386Pending_SetCursorMoved(false);

	// Make sure cursor stays in the range defined by application
	limit_coordinates();

	return enum_val(MouseEventId::MouseHasMoved);
}

static uint8_t update_buttons(const MouseButtons12S new_buttons_12S)
{
	MouseDriverState state(*state_segment);

	const auto buttons = state.GetButtons();
	if (buttons._data == new_buttons_12S._data) {
		return 0;
	}

	auto mark_pressed = [&state](const uint8_t idx) {
		state.SetLastPressedX(idx, get_pos_x());
		state.SetLastPressedY(idx, get_pos_y());

		const auto times = state.GetTimesPressed(idx);
		state.SetTimesPressed(idx, times + 1);
	};

	auto mark_released = [&state](const uint8_t idx) {
		state.SetLastReleasedX(idx, get_pos_x());
		state.SetLastReleasedY(idx, get_pos_y());

		const auto times = state.GetTimesReleased(idx);
		state.SetTimesReleased(idx, times + 1);
	};

	uint8_t mask = 0;
	if (new_buttons_12S.left && !buttons.left) {
		mark_pressed(0);
		mask |= enum_val(MouseEventId::PressedLeft);
	} else if (!new_buttons_12S.left && buttons.left) {
		mark_released(0);
		mask |= enum_val(MouseEventId::ReleasedLeft);
	}

	if (new_buttons_12S.right && !buttons.right) {
		mark_pressed(1);
		mask |= enum_val(MouseEventId::PressedRight);
	} else if (!new_buttons_12S.right && buttons.right) {
		mark_released(1);
		mask |= enum_val(MouseEventId::ReleasedRight);
	}

	if (new_buttons_12S.middle && !buttons.middle) {
		mark_pressed(2);
		mask |= enum_val(MouseEventId::PressedMiddle);
	} else if (!new_buttons_12S.middle && buttons.middle) {
		mark_released(2);
		mask |= enum_val(MouseEventId::ReleasedMiddle);
	}

	state.SetButtons(new_buttons_12S);
	return mask;
}

static uint8_t move_wheel()
{
	MouseDriverState state(*state_segment);

	const auto consumed = MOUSE_ConsumeInt8(pending.delta_wheel);
	auto counter_wheel  = state.GetCounterWheel();

	counter_wheel = clamp_to_int8(counter_wheel + consumed);

	state.SetCounterWheel(counter_wheel);
	state.SetLastWheelMovedX(get_pos_x());
	state.SetLastWheelMovedY(get_pos_y());

	if (counter_wheel != 0) {
		return enum_val(MouseEventId::WheelHasMoved);
	}
	return 0;
}

static uint8_t update_wheel()
{
	if (is_immediate_mode()) {
		return enum_val(MouseEventId::WheelHasMoved);
	}
	return move_wheel();
}

void MOUSEDOS_NotifyMoved(const float x_rel, const float y_rel,
                          const float x_abs, const float y_abs)
{
	// Do not access 'state' here in Windows 386 Enhanced mode,
	// it might lead to crashes as the VM context is unspecified here!

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
		// 'state.GetMaxPos*()' and 'mouse_shared.resolution_*' values.
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

	if (event_needed && is_immediate_mode()) {
		event_needed = (move_cursor() != 0);
	}

	if (event_needed) {
		pending.has_mouse_moved = true;
		maybe_trigger_event();
	}
}

void MOUSEDOS_NotifyButton(const MouseButtons12S new_buttons_12S)
{
	// Do not access 'state' here in Windows 386 Enhanced mode,
	// it might lead to crashes as the VM context is unspecified here!

	auto new_button_state = new_buttons_12S;
	new_button_state._data &= get_button_mask();

	if (pending.button_state != new_button_state) {
		pending.has_button_changed = true;
		pending.button_state = new_button_state;
		maybe_trigger_event();
	}
}

void MOUSEDOS_NotifyWheel(const float w_rel)
{
	// Do not access 'state' here in Windows 386 Enhanced mode,
	// it might lead to crashes as the VM context is unspecified here!

	// Although in some places it is possible for the guest code to get
	// wheel counter in 16-bit format, scrolling hundreds of lines in one
	// go would be insane - thus, limit the wheel counter to 8 bits and
	// reuse the code written for other mouse modules
	pending.delta_wheel = MOUSE_ClampWheelMovement(pending.delta_wheel + w_rel);

	bool event_needed = MOUSE_HasAccumulatedInt(pending.delta_wheel);
	if (event_needed && is_immediate_mode()) {
		event_needed = (move_wheel() != 0);
	}

	if (event_needed) {
		pending.has_wheel_moved = true;
		maybe_trigger_event();
	}
}

void MOUSEDOS_NotifyModelChanged()
{
	// Do not access 'state' here in Windows 386 Enhanced mode,
	// it might lead to crashes as the VM context is unspecified here!

	maybe_log_mouse_model();

	if (!has_wheel()) {
		pending.has_wheel_moved = false;
		pending.delta_wheel    = 0;
	}

	pending.disable_wheel_api = !has_wheel();

	// Make sure button state has no buttons which are no longer present
	MOUSEDOS_NotifyButton(pending.button_state);
}

// Requires function parameters to be present in the CPU registers
static bool is_known_oem_function()
{
	// Reference:
	// -
	// https://mirror.math.princeton.edu/pub/oldlinux/Linux.old/docs/interrupts/int-html/int-33.htm

	if (reg_ax >= 0xffe6) {
		// Switch-It task switcher software
		return true;
	}

	if (reg_al == 0x6c && (reg_ah >= 0x13 && reg_ah <= 0x27)) {
		// Logitech Mouse function, some known functions:
		// 0x156c - get signature and version strings
		// 0x1d6c - get compass parameter
		// 0x1e6c - set compass parameter
		// 0x1f6c - get ballistics information
		// 0x206c - set left or right parameter
		// 0x216c - get left or right parameter
		// 0x226c - remove driver from memory
		// 0x236c - set ballistics information
		// 0x246c - get parameters and reset serial mouse
		// 0x256c - set parameters (serial mice only):
		//          BX = 0x0000 - set baud rate
		//          BX = 0x0001 - set emulation
		//          BX = 0x0002 - set report rate
		//          BX = 0x0003 - set mouse port
		//          BX = 0x0004 - set mouse logical buttons
		// 0x266c - get version (?)
		return true;
	}

	switch (reg_ax) {
	default: return false;
		// Do not silence out unknown functions up to 0x6f; we have no
		// information about possible extra functions available in the
		// Microsoft mouse driver 8.x-11.x; there is a chance that some
		// early OEM drivers have functions with a conflicting ID
	case 0x0070:
		// Mouse Systems - installation check
	case 0x0072:
		// Mouse Systems 7.01+ - unknown functionality
		// Genius Mouse 9.06+  - unknown functionality
	case 0x0073:
		// Mouse Systems 7.01+ - (BX = 0xabcd) get button assignments
		// VBADOS driver       - get driver info
	case 0x00a0:
		// TrueDOX Mouse driver - set PC mode (3 button)
	case 0x00a1:
		// TrueDOX Mouse driver - set MS mode (2 button)
	case 0x00a6:
		// TrueDOX Mouse driver - get resolution
	case 0x00b0:
		// LCS/Telegraphics Mouse Driver - unknown functionality
	case 0x00d6:
		// Twiddler TWMOUSE - get button/tilt state
	case 0x00f0:
	case 0x00f1:
	case 0x00f2:
	case 0x00f3:
		// LCS/Telegraphics Mouse Driver - unknown functionality
	case 0x0100:
		// GRT Mouse 1.00+ - installation check
	case 0x0101:
		// GRT Mouse 1.00+ - set mouse cursor shape
	case 0x0102:
		// GRT Mouse 1.00+ - get mouse cursor shape
	case 0x0103:
		// GRT Mouse 1.00+ - set active characters
	case 0x0104:
		// GRT Mouse 1.00+ - get active characters
	case 0x0666:
		// TrueDOX Mouse driver v4.01 - get copyright string
	case 0x3000:
		// Smooth Mouse Driver, PrecisePoint - installation check
	case 0x3001:
		// Smooth Mouse Driver, PrecisePoint - enable smooth mouse
	case 0x3002:
		// Smooth Mouse Driver, PrecisePoint - disable smooth mouse
	case 0x3003:
		// Smooth Mouse Driver, PrecisePoint - get information
	case 0x3004:
	case 0x3005:
		// Smooth Mouse Driver, PrecisePoint - reserved
	case 0x4f00:
	case 0x4f01:
		// Logitech Mouse 6.10+ - unknown functionality
	case 0x5301:
		// Logitech CyberMan - get 3D position, orientation, and button
		// status
	case 0x5330:
		// Logitech CyberMan - generate tactile feedback
	case 0x53c0:
		// Logitech CyberMan - exchange event handlers
	case 0x53c1:
		// Logitech CyberMan - get static device data and driver support
		// status
	case 0x53c2:
		// Logitech CyberMan - get dynamic device data
	case 0x6f00:
		// Hewlett Packard - driver installation check
	case 0x8800:
		// InfoTrack IMOUSE.COM - unhook mouse IRQ
		//                      - (BX = 0xffff) - get active IRQ
		return true;
	}
}

static Bitu int33_handler()
{
	maybe_disable_wheel_api();

	using namespace bit::literals;

	MouseDriverState state(*state_segment);

	switch (reg_ax) {
	case 0x00:
		// MS MOUSE v1.0+ - reset driver and read status
		reset_hardware();
		[[fallthrough]];
	case 0x21:
		// MS MOUSE v6.0+ - software reset
		reg_ax = 0xffff; // mouse driver installed
		reg_bx = (get_num_buttons() == 2) ? 0xffff : get_num_buttons();
		reset();
		break;
	case 0x01:
		// MS MOUSE v1.0+ - show mouse cursor
		{
			const auto hidden = state.GetHidden();
			if (hidden) {
				state.SetHidden(hidden - 1);
			}
			state.SetUpdateRegionY(1, -1); // offscreen
			draw_cursor();
			break;
		}
	case 0x02:
		// MS MOUSE v1.0+ - hide mouse cursor
		restore_cursor_background();
		state.SetHidden(state.GetHidden() + 1);
		break;
	case 0x03:
		// MS MOUSE v1.0+ / WheelAPI v1.0+ - get position and button state
		reg_bl = state.GetButtons()._data;
		// CuteMouse clears the wheel counter too
		reg_bh = get_reset_wheel_8bit();
		reg_cx = get_pos_x();
		reg_dx = get_pos_y();
		break;
	case 0x04:
		// MS MOUSE v1.0+ - position mouse cursor
		{
			// If position isn't different from current position,
			// don't change it.
			// Position is rounded so numbers get lost when the
			// rounded number is set (arena/simulation Wolf)
			if (reg_to_signed16(reg_cx) != get_pos_x()) {
				state.SetPosX(static_cast<float>(reg_cx));
			}
			if (reg_to_signed16(reg_dx) != get_pos_y()) {
				state.SetPosY(static_cast<float>(reg_dx));
			}
			limit_coordinates();
			draw_cursor();
			break;
		}
	case 0x05: {
		// MS MOUSE v1.0+ / WheelAPI v1.0+ - get button press / wheel data
		const uint16_t idx = reg_bx; // button index
		if (idx == 0xffff && state.GetWheelApi() && has_wheel()) {
			// 'magic' index for checking wheel instead of button
			reg_bx = get_reset_wheel_16bit();
			reg_cx = state.GetLastWheelMovedX();
			reg_dx = state.GetLastWheelMovedY();
		} else if (idx < get_num_buttons()) {
			reg_ax = state.GetButtons()._data;
			reg_bx = state.GetTimesPressed(idx);
			reg_cx = state.GetLastPressedX(idx);
			reg_dx = state.GetLastPressedY(idx);

			state.SetTimesPressed(idx, 0);
		} else {
			// unsupported - try to do something sane
			// TODO: Check the real driver behavior
			reg_ax = state.GetButtons()._data;
			reg_bx = 0;
			reg_cx = 0;
			reg_dx = 0;
		}
		break;
	}
	case 0x06:
		// MS MOUSE v1.0+ / WheelAPI v1.0+ - get button release / wheel data
		{
			const uint16_t idx = reg_bx; // button index
			if (idx == 0xffff && state.GetWheelApi() && has_wheel()) {
				// 'magic' index for checking wheel instead of button
				reg_bx = get_reset_wheel_16bit();
				reg_cx = state.GetLastWheelMovedX();
				reg_dx = state.GetLastWheelMovedY();
			} else if (idx < get_num_buttons()) {
				reg_ax = state.GetButtons()._data;
				reg_bx = state.GetTimesReleased(idx);
				reg_cx = state.GetLastReleasedX(idx);
				reg_dx = state.GetLastReleasedY(idx);

				state.SetTimesReleased(idx, 0);
			} else {
				// unsupported - try to do something sane
				// TODO: Check the real driver behavior
				reg_ax = state.GetButtons()._data;
				reg_bx = 0;
				reg_cx = 0;
				reg_dx = 0;
			}
			break;
		}
	case 0x07:
		// MS MOUSE v1.0+ - define horizontal cursor range
		{
			// Lemmings set 1-640 and wants that. Iron Seed set
			// 0-640, but doesn't like 640. Iron Seed works if new
			// video mode with mode 13 sets 0-639. Larry 6 actually
			// wants new video mode with mode 13 to set it to 0-319.
			const auto min = std::min(reg_to_signed16(reg_cx),
			                          reg_to_signed16(reg_dx));
			const auto max = std::max(reg_to_signed16(reg_cx),
			                          reg_to_signed16(reg_dx));

			state.SetMinPosX(min);
			state.SetMaxPosX(max);
			// Battle Chess wants this
			auto pos_x = state.GetPosX();
			pos_x = std::clamp(pos_x,
	                                   static_cast<float>(min),
	                                   static_cast<float>(max));
			// Or alternatively this:
			// pos_x = (max - min + 1) / 2;
			state.SetPosX(pos_x);
			break;
		}
	case 0x08:
		// MS MOUSE v1.0+ - define vertical cursor range
		{
			// Not sure what to take instead of the CurMode (see
			// case 0x07 as well) especially the cases where
			// sheight=400 and we set it with the mouse_reset to 200
			// disabled it at the moment.
			// Seems to break Syndicate who want 400 in mode 13
			const auto min = std::min(reg_to_signed16(reg_cx),
			                          reg_to_signed16(reg_dx));
			const auto max = std::max(reg_to_signed16(reg_cx),
			                          reg_to_signed16(reg_dx));

			state.SetMinPosY(min);
			state.SetMaxPosY(max);
			// Battle Chess wants this
			auto pos_y = state.GetPosY();
			pos_y = std::clamp(pos_y,
	                                   static_cast<float>(min),
	                                   static_cast<float>(max));
			// Or alternatively this:
			// pos_x = (max - min + 1) / 2;
			state.SetPosY(pos_y);
			break;
		}
	case 0x09: 
		// MS MOUSE v3.0+ - define GFX cursor
		{
			auto clamp_hot = [](const uint16_t reg, const int cursor_size) {
				return std::clamp(reg_to_signed16(reg),
				                  static_cast<int16_t>(-cursor_size),
				                  static_cast<int16_t>(cursor_size));
			};

			uint16_t tmp[CursorSize] = {0};

			PhysPt src = SegPhys(es) + reg_dx;
			MEM_BlockRead(src, tmp, CursorSize * 2);
			for (auto idx = 0; idx < CursorSize; ++idx) {
				state.SetUserDefScreenMask(idx, tmp[idx]);
			}

			src += CursorSize * 2;
			MEM_BlockRead(src, tmp, CursorSize * 2);
			for (auto idx = 0; idx < CursorSize; ++idx) {
				state.SetUserDefCursorMask(idx, tmp[idx]);
			}

			state.SetUserScreenMask(true);
			state.SetUserCursorMask(true);
			state.SetHotX(clamp_hot(reg_bx, CursorSize));
			state.SetHotY(clamp_hot(reg_cx, CursorSize));
			state.SetCursorType(MouseCursor::Text);

			draw_cursor();
			break;
		}
	case 0x0a:
		// MS MOUSE v3.0+ - define text cursor	
		//
		// TODO: shouldn't we use MouseCursor::Text, not
		// MouseCursor::Software?
		state.SetCursorType(reg_bx ? MouseCursor::Hardware
		                           : MouseCursor::Software);
		state.SetTextAndMask(reg_cx);
		state.SetTextXorMask(reg_dx);
		if (reg_bx) {
			INT10_SetCursorShape(reg_cl, reg_dl);
		}
		draw_cursor();
		break;
	case 0x27:
		// MS MOUSE v7.01+ - get screen/cursor masks and mickey counts
		reg_ax = state.GetTextAndMask();
		reg_bx = state.GetTextXorMask();
		[[fallthrough]];
	case 0x0b:
		// MS MOUSE v1.0+ - read motion data
		reg_cx = static_cast<uint16_t>(state.GetMickeyCounterX());
		reg_dx = static_cast<uint16_t>(state.GetMickeyCounterY());
		state.SetMickeyCounterX(0);
		state.SetMickeyCounterY(0);
		break;
	case 0x0c:
		// MS MOUSE v1.0+ - define user callback parameters
		state.SetUserCallbackMask(reg_cx);
		state.SetUserCallbackSegment(SegValue(es));
		state.SetUserCallbackOffset(reg_dx);
		update_driver_active();
		break;
	case 0x0d:
		// MS MOUSE v1.0+ - light pen emulation on
		//
		// Both buttons down = pen pressed, otherwise pen considered
		// off-screen
		// TODO: maybe implement light pen using SDL touch events?
		LOG_WARNING("MOUSE (DOS): Light pen emulation not implemented");
		break;
	case 0x0e:
		// MS MOUSE v1.0+ - light pen emulation off
		//
		// Although light pen emulation is not implemented, it is OK for
		// the application to only disable it (like 'The Settlers' game
		// is doing during initialization)
		break;
	case 0x0f:
		// MS MOUSE v1.0+ - define mickey/pixel rate
		set_mickey_pixel_rate(reg_to_signed16(reg_cx),
		                      reg_to_signed16(reg_dx));
		break;
	case 0x10:
		// MS MOUSE v1.0+ - define screen region for updating
		state.SetUpdateRegionX(0, reg_to_signed16(reg_cx));
		state.SetUpdateRegionY(0, reg_to_signed16(reg_dx));
		state.SetUpdateRegionX(1, reg_to_signed16(reg_si));
		state.SetUpdateRegionY(1, reg_to_signed16(reg_di));
		draw_cursor();
		break;
	case 0x11:
		// WheelAPI v1.0+ / Genius Mouse - get mouse capabilities
		if (is_win386_mode && WINDOWS_GetVmId() == WindowsKernelVmId) {
			// The only software, which probes for the wheel API
			// while running in the context of Microsoft Windows
			// kernel, is the VBADOS driver. Our Windows support is
			// not yet compatible with this driver, so switch back
			// to normal operation - this way at least there is
			// going to be a working mouse in the GUI (windowed
			// MS-DOS prompt won't work for now).
			MOUSEDOS_HandleWindowsShutdown();
			constexpr bool IsModeChanging = true;
			MOUSEDOS_BeforeNewVideoMode();
			MOUSEDOS_AfterNewVideoMode(IsModeChanging);
			// TODO: Use 'WINDOWS_SwitchVM' to pump the host mouse
			// events to the kernel context
		}
		if (has_wheel()) {
			// WheelAPI implementation
			// GTEST.COM from the Genius mouse driver package
			// reports 3 buttons if it sees this extension
			reg_ax = 0x574d; // Identifier for detection purposes
			reg_bx = 0;      // Reserved capabilities flags
			reg_cx = has_wheel() ? 1 : 0;
			// This call enables the WheelAPI extensions
			state.SetWheelApi(true);
			state.SetCounterWheel(0);
		} else {
			// Genius Mouse 9.06 API implementation
			reg_ax = 0xffff;
			reg_bx = get_num_buttons();
		}
		break;
	case 0x12:
		// MS MOUSE - set large graphics cursor block
		LOG_WARNING("MOUSE (DOS): Large graphics cursor block not implemented");
		break;
	case 0x13:
		// MS MOUSE v5.0+ - set double-speed threshold
		set_double_speed_threshold(reg_bx);
		break;
	case 0x14:
		// MS MOUSE v3.0+ - exchange event-handler
		{
			const auto old_segment = state.GetUserCallbackSegment();
			const auto old_offset  = state.GetUserCallbackOffset();
			const auto old_mask    = state.GetUserCallbackMask();
			// Set new values
			state.SetUserCallbackMask(reg_cx);
			state.SetUserCallbackSegment(SegValue(es));
			state.SetUserCallbackOffset(reg_dx);
			update_driver_active();
			// Return old values
			reg_cx = old_mask;
			reg_dx = old_offset;
			SegSet16(es, old_segment);
			break;
		}
	case 0x15:
		// MS MOUSE v6.0+ - get driver storage space requirements
		reg_bx = MouseDriverState::GetSize();
		break;
	case 0x16:
		// MS MOUSE v6.0+ - save driver state
		{
			const auto tmp = state.ReadBinaryData();
			MEM_BlockWrite(SegPhys(es) + reg_dx, tmp.data(), tmp.size());
			break;
		}
	case 0x17:
		// MS MOUSE v6.0+ - load driver state
		{
			std::vector<uint8_t> tmp = {};
			tmp.resize(MouseDriverState::GetSize());
			MEM_BlockRead(SegPhys(es) + reg_dx, tmp.data(), tmp.size());
			state.WriteBinaryData(tmp);

			pending.ResetCounters();
			update_driver_active();
			set_sensitivity(state.GetSensitivityX(),
			                state.GetSensitivityY(),
			                state.GetUnknownValue01());
			// TODO: we should probably also fake an event for mouse
			// movement, redraw cursor, etc.
			break;
		}
	case 0x18:
	case 0x19:
		// MS MOUSE v6.0+ - set alternate mouse user handler
		LOG_WARNING("MOUSE (DOS): Alternate mouse user handler not implemented");
		break;
	case 0x1a:
		// MS MOUSE v6.0+ - set mouse sensitivity
		//
		// NOTE: Ralf Brown Interrupt List (and some other sources)
		// claim, that this should duplicate functions 0x0f and 0x13 -
		// this is not true at least for Mouse Systems driver v8.00 and
		// IBM/Microsoft driver v8.20
		set_sensitivity(reg_bx, reg_cx, reg_dx);
		break;
	case 0x1b:
		//  MS MOUSE v6.0+ - get mouse sensitivity
		reg_bx = state.GetSensitivityX();
		reg_cx = state.GetSensitivityY();
		reg_dx = state.GetUnknownValue01();
		break;
	case 0x1c:
		// MS MOUSE v6.0+ - set interrupt rate
		set_interrupt_rate(reg_bx);
		break;
	case 0x1d:
		// MS MOUSE v6.0+ - set display page number
		state.SetPage(reg_bl);
		break;
	case 0x1e:
		// MS MOUSE v6.0+ - get display page number
		reg_bx = state.GetPage();
		break;
	case 0x1f:
		// MS MOUSE v6.0+ - disable mouse driver
		//
		// ES:BX old mouse driver Zero at the moment TODO
		reg_bx = 0;
		SegSet16(es, 0);
		state.SetEnabled(false);
		state.SetOldHidden(state.GetHidden());
		state.SetHidden(1);
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
	case 0x20:
		// MS MOUSE v6.0+ - enable mouse driver
		state.SetEnabled(true);
		state.SetOldHidden(state.GetHidden());
		if (mouse_config.dos_driver_modern) {
			// Checked that MS driver alters AX this way starting
			// from version 7.
			reg_ax = 0xffff;
		}
		break;
	case 0x22:
		// MS MOUSE v6.0+ - set language for messages
		//
		// 00h = English, 01h = French, 02h = Dutch, 03h = German, 04h =
		// Swedish 05h = Finnish, 06h = Spanish, 07h = Portugese, 08h =
		// Italian
		if (reg_bx != driver_language) {
			LOG_WARNING("MOUSE (DOS): Overriding the driver language not supported");
			driver_language = reg_bx;
		}
		break;
	case 0x23:
		// MS MOUSE v6.0+ - get language for messages
		reg_bx = driver_language;
		break;
	case 0x24:
		// MS MOUSE v6.26+ - get software version, mouse type, and IRQ
		// number
		reg_bh = DriverVersionMajor;
		reg_bl = DriverVersionMinor;
		// 1 = bus, 2 = serial, 3 = inport, 4 = PS/2, 5 = HP
		reg_ch = 0x04; // PS/2
		reg_cl = 0; // PS/2 mouse; for others it would be an IRQ number
		break;
	case 0x25:
		// MS MOUSE v6.26+ - get general driver information
		{
			// See https://github.com/FDOS/mouse/blob/master/int33.lst
			// AL = count of currently-active Mouse Display Drivers (MDDs)
			reg_al = 1;
			// AH - bits 0-3: interrupt rate
			//    - bits 4-5: current cursor type
			//    - bit 6: 1 = driver is newer integrated type
			//    - bit 7: 1 = loaded as device driver rather than TSR
			constexpr auto IntegratedDriver = (1 << 6);
			const auto cursor_type = enum_val(state.GetCursorType());
			reg_ah = static_cast<uint8_t>(IntegratedDriver | (cursor_type << 4) |
			                              get_interrupt_rate());
			// BX - cursor lock flag for OS/2 to prevent reentrancy problems
			// CX - mouse code active flag (for OS/2)
			// DX - mouse driver busy flag (for OS/2)
			reg_bx = 0;
			reg_cx = 0;
			reg_dx = 0;
			break;
		}
	case 0x26:
		// MS MOUSE v6.26+ - get maximum virtual coordinates
		reg_bx = (state.IsEnabled() ? 0x0000 : 0xffff);
		reg_cx = signed_to_reg16(state.GetMaxPosX());
		reg_dx = signed_to_reg16(state.GetMaxPosY());
		break;
	case 0x28:
		// MS MOUSE v7.0+ - set video mode
		//
		// TODO: According to PC sourcebook
		//       Entry:
		//       CX = Requested video mode
		//       DX = Font size, 0 for default
		//       Returns:
		//       DX = 0 on success, nonzero (requested video mode) if not
		LOG_WARNING("MOUSE (DOS): Set video mode not implemented");
		// TODO: once implemented, update function 0x32
		break;
	case 0x29:
		// MS MOUSE v7.0+ - enumerate video modes
		//
		// TODO: According to PC sourcebook
		//       Entry:
		//       CX = 0 for first, != 0 for next
		//       Exit:
		//       BX:DX = named string far ptr
		//       CX = video mode number
		LOG_WARNING("MOUSE (DOS): Enumerate video modes not implemented");
		// TODO: once implemented, update function 0x32
		break;
	case 0x2a:
		// MS MOUSE v7.01+ - get cursor hot spot
		//
		// Microsoft uses a negative byte counter
		// for cursor visibility
		reg_al = static_cast<uint8_t>(-state.GetHidden());
		reg_bx = signed_to_reg16(state.GetHotX());
		reg_cx = signed_to_reg16(state.GetHotY());
		reg_dx = 0x04; // PS/2 mouse type
		break;
	case 0x2b:
		// MS MOUSE v7.0+ - load acceleration profiles
	case 0x2c:
		// MS MOUSE v7.0+ - get acceleration profiles
	case 0x2d:
		// MS MOUSE v7.0+ - select acceleration profile
	case 0x2e:
		// MS MOUSE v8.10+ - set acceleration profile names
	case 0x33:
		// MS MOUSE v7.05+ - get/switch accelleration profile
		//
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
	case 0x2f:
		// MS MOUSE v7.02+ - mouse hardware reset
		LOG_WARNING("MOUSE (DOS): Hardware reset not implemented");
		// TODO: once implemented, update function 0x32
		break;
	case 0x30:
		// MS MOUSE v7.04+ - get/set BallPoint information
		LOG_WARNING("MOUSE (DOS): Get/Set BallPoint information not implemented");
		// TODO: once implemented, update function 0x32
		break;
	case 0x31:
		// MS MOUSE v7.05+ - get current min/max virtual coordinates
		reg_ax = signed_to_reg16(state.GetMinPosX());
		reg_bx = signed_to_reg16(state.GetMinPosY());
		reg_cx = signed_to_reg16(state.GetMaxPosX());
		reg_dx = signed_to_reg16(state.GetMaxPosY());
		break;
	case 0x32:
		// MS MOUSE v7.05+ - get active advanced functions
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
	case 0x34:
		// MS MOUSE v8.0+ - get initialization file
		SegSet16(es, info_segment);
		reg_dx = info_offset_ini_file;
		break;
	case 0x35:
		// MS MOUSE v8.10+ - LCD screen large pointer support
		LOG_WARNING("MOUSE (DOS): LCD screen large pointer support not implemented");
		break;
	case 0x4d:
		// MS MOUSE - return pointer to copyright string
		SegSet16(es, info_segment);
		reg_di = info_offset_copyright;
		break;
	case 0x6d:
		// MS MOUSE - get version string
		SegSet16(es, info_segment);
		reg_di = info_offset_version;
		break;
	default:
		// Do not print out any warnings for known 3rd party oem driver
		// extensions - every software (except the one bound to the
		// particular driver) should continue working correctly even if
		// we completely ignore the call
		if (!is_known_oem_function()) {
			// Unknown function
			LOG_WARNING("MOUSE (DOS): Interrupt 0x33 function 0x%04x not implemented",
			            reg_ax);
		}
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
	default:
		break;
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

	static_assert(low_nibble(DriverVersionMinor) <= 9);
	static_assert(low_nibble(DriverVersionMajor) <= 9);
	static_assert(high_nibble(DriverVersionMinor) <= 9);
	static_assert(high_nibble(DriverVersionMajor) <= 9);

	std::string str_version = "version ";
	if constexpr(high_nibble(DriverVersionMajor) > 0) {
		str_version = str_version + high_nibble_str(DriverVersionMajor);
	}

	str_version = str_version + low_nibble_str(DriverVersionMajor) +
	              std::string(".") + high_nibble_str(DriverVersionMinor) +
	              low_nibble_str(DriverVersionMinor);

	const size_t length_bytes = (str_version.length() + 1) +
	                            (str_copyright.length() + 1);
	assert(length_bytes <= UINT8_MAX);

	constexpr uint8_t BytesPerBlock = 0x10;
	auto length_blocks = static_cast<uint16_t>(length_bytes / BytesPerBlock);
	if (length_bytes % BytesPerBlock) {
		length_blocks = static_cast<uint16_t>(length_blocks + 1);
	}

	info_segment = DOS_GetMemory(length_blocks);

	// TODO: if 'MOUSE.INI' file gets implemented, INT 33 function 0x32
	// should be updated to indicate function 0x34 is supported
	std::string str_combined = str_version + '\0' + str_copyright + '\0';
	const size_t size = static_cast<size_t>(length_blocks) * BytesPerBlock;
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
	maybe_disable_wheel_api();

	if (!has_pending_event()) {
		return 0x00;
	}

	MouseDriverState state(*state_segment);

	uint8_t mask = 0x00;
	if (!is_win386_foreground && pending.has_mouse_moved) {
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
		pending.has_mouse_moved = false;
	}

	if (is_win386_foreground && state.Win386Pending_IsCursorMoved()) {
		mask = update_moved_win386();
		if (mask) {
			draw_cursor();
		}
		state.Win386Pending_SetCursorMoved(false);
	}

	if (!is_win386_foreground && pending.has_button_changed) {
		const auto result = update_buttons(pending.button_state);
		mask = static_cast<uint8_t>(mask | result);
		pending.has_button_changed = false;
	}

	if (is_win386_foreground && state.Win386Pending_IsButtonChanged()) {
		const auto result = update_buttons(state.Win386Pending_GetButtons());
		mask = static_cast<uint8_t>(mask | result);
		state.Win386Pending_SetButtonChanged(false);
	}

	if (!is_win386_foreground && pending.has_wheel_moved) {
		const auto result = update_wheel();
		mask = static_cast<uint8_t>(mask | result);
		pending.has_wheel_moved = false;
	}

	// If DOS driver's client is not interested in this particular
	// type of event - skip it
	if (!(state.GetUserCallbackMask() & mask)) {
		return 0x00;
	}

	return mask;
}

void MOUSEDOS_DoCallback(const uint8_t mask)
{
	MouseDriverState state(*state_segment);

	mouse_shared.dos_cb_running = true;
	const bool mouse_moved = mask & enum_val(MouseEventId::MouseHasMoved);
	const bool wheel_moved = mask & enum_val(MouseEventId::WheelHasMoved);

	// Extension for Windows mouse driver by javispedro:
	// - https://git.javispedro.com/cgit/vbados.git/about/
	// which allows seamless mouse integration. It is also included in
	// DOSBox-X and Dosemu2:
	// - https://github.com/joncampbell123/dosbox-x/pull/3424
	// - https://github.com/dosemu2/dosemu2/issues/1552#issuecomment-1100777880
	// - https://github.com/dosemu2/dosemu2/commit/cd9d2dbc8e3d58dc7cbc92f172c0d447881526be
	// - https://github.com/joncampbell123/dosbox-x/commit/aec29ce28eb4b520f21ead5b2debf370183b9f28
	if (WINDOWS_IsStarted() && !is_win386_mode) {
		// Windows is runnning, but due to VBADOS Int33 driver detected
		// we have shut down our Windows/386 compatibility mode
		reg_ah = (!use_relative && mouse_moved) ? 1 : 0;
	} else {
		// Do not manifest the extension:
		// - besides the VBADOS Int33 Windows driver nothing uses it
		// - setting any bit the game does not know about is always a
		//   slight risk of incompatibility
		reg_ah = 0;
	}

	reg_al = mask;
	reg_bl = state.GetButtons()._data;
	reg_bh = wheel_moved ? get_reset_wheel_8bit() : 0;
	reg_cx = get_pos_x();
	reg_dx = get_pos_y();
	reg_si = static_cast<uint16_t>(state.GetMickeyCounterX());
	reg_di = static_cast<uint16_t>(state.GetMickeyCounterY());

	CPU_Push16(RealSegment(user_callback));
	CPU_Push16(RealOffset(user_callback));
	CPU_Push16(state.GetUserCallbackSegment());
	CPU_Push16(state.GetUserCallbackOffset());
}

void MOUSEDOS_FinalizeInterrupt()
{
	// Just in case our interrupt was taken over by the PS/2 BIOS callback,
	// or if user interrupt handler did not finish yet

	if (has_pending_event()) {
		maybe_start_delay_timer(1);
	}
}

void MOUSEDOS_NotifyInputType(const bool new_use_relative, const bool new_is_input_raw)
{
	// Do not access 'state' here in Windows 386 Enhanced mode,
	// it might lead to crashes as the VM context is unspecified here!

	use_relative = new_use_relative;
	is_input_raw = new_is_input_raw;
}

void MOUSEDOS_SetDelay(const uint8_t new_delay_ms)
{
	// Do not access 'state' here in Windows 386 Enhanced mode,
	// it might lead to crashes as the VM context is unspecified here!

	delay_ms = new_delay_ms;
}

void MOUSEDOS_HandleWindowsStartup()
{
	if (is_win386_mode) {
		return;
	}

	// Function only supported if TSR emulation is enabled and Windows
	// is running in the 386 Enhanced mode
	if (!state_segment.has_value() || mouse_config.dos_driver_no_tsr ||
	    !WINDOWS_IsEnhancedMode()) {
		return;
	}

	// Check for Windows version at least version 3.1 - earlier releases
	// could not run the DOS prompt in a window and, since they don't inform
	// us when the Windows puts itself in the background, our Windows
	// compatibility mechanism only disrupts their DOS mode mouse support
	constexpr uint8_t MinMajor = 3;
	constexpr uint8_t MinMinor = 10;
	const auto [major, minor]  = WINDOWS_GetVersion();
	if (major < MinMajor || (major == MinMajor && minor < MinMinor)) {
		return;
	}

	is_win386_mode       = true;
	is_win386_foreground = true;

	MouseDriverState state(*state_segment);
	state.SetWin386Cursor(false);

	// Setup Windows/386 communication structures
	const auto startup_ptr   = RealMake(*state_segment,
                                            state.GetWin386StartupOffset());
	const auto instances_ptr = RealMake(*state_segment,
	                                    state.GetWin386InstancesOffset());

	state.Win386Startup_SetVersionMinor(0);
	state.Win386Startup_SetVersionMajor(3);
	state.Win386Startup_SetNextInfoPtr(RealMake(SegValue(es), reg_bx));
	state.Win386Startup_SetDeviceDriverPtr(0);
	state.Win386Startup_SetDeviceDriverDataPtr(0);
	state.Win386Startup_SetInstanceDataPtr(instances_ptr);

	state.Win386Instance_SetDataPtr(0, RealMake(*state_segment, 0));
	state.Win386Instance_SetSize(0, MouseDriverState::GetSize());
	state.Win386Instance_SetDataPtr(1, 0);
	state.Win386Instance_SetSize(1, 0);

	// Provide the startup structure to Windows
	SegSet16(es, RealSegment(startup_ptr));
	reg_bx = RealOffset(startup_ptr);
}

void MOUSEDOS_HandleWindowsShutdown()
{
	// Function only supported in TSR mode
	if (!state_segment.has_value() || mouse_config.dos_driver_no_tsr) {
		return;
	}

	is_win386_mode       = false;
	is_win386_foreground = false;

	MouseDriverState state(*state_segment);
	state.SetWin386Cursor(false);
}

void MOUSEDOS_HandleWindowsCallout()
{
	// Function only supported in TSR mode
	if (!state_segment.has_value() || mouse_config.dos_driver_no_tsr) {
		return;
	}

	MouseDriverState state(*state_segment);

	// Check if Windows is calling a mouse device driver
	constexpr uint16_t VmdMouseDeviceId = 0x000c;
	if (reg_bx != VmdMouseDeviceId) {
		return;
	}

	switch (reg_cx) {
	// Callout availability check
	case 0x00:
		// Confirm availability
		reg_cx = 1;
		break;
	// Callout address request
	case 0x01: {
		// Return callout handler address
		const auto ptr = CALLBACK_RealPointer(CallbackIDs::win386);
		SegSet16(ds, RealSegment(ptr));
		reg_si = RealOffset(ptr);

		// Confirm availability
		reg_ax = 0;
		break;
	}
	// Unknown function
	default:
		LOG_WARNING("MOUSE (DOS): Windows callout function 0x%04x not implemented",
		            reg_cx);
		break;
	}
}

void MOUSEDOS_NotifyWindowsBackground()
{
	if (!is_win386_mode) {
		return;
	}

	reset_pending_events();
	is_win386_foreground = false;
}

void MOUSEDOS_NotifyWindowsForeground()
{
	if (!is_win386_mode) {
		return;
	}

	reset_pending_events();
	is_win386_foreground = true;
}

static void win386_handle_mouse_event(const uint16_t win386_event,
                                      const uint16_t win386_buttons,
                                      const uint16_t win386_abs_x,
                                      const uint16_t win386_abs_y)
{
	constexpr uint16_t WinEventMouseMove = 1;

	constexpr uint16_t WinButtonLeft   = 1 << 0;
	constexpr uint16_t WinButtonRight  = 1 << 1;
	constexpr uint16_t WinButtonMiddle = 1 << 2;

	MouseDriverState state(*state_segment);

	if (win386_event == WinEventMouseMove) {
		state.Win386Pending_SetXAbs(win386_abs_x);
		state.Win386Pending_SetYAbs(win386_abs_y);
		state.Win386Pending_SetCursorMoved(true);
	} else {
		MouseButtons12S buttons = {};
		buttons.left   = (0 != (win386_buttons & WinButtonLeft));
		buttons.right  = (0 != (win386_buttons & WinButtonRight));
		buttons.middle = (0 != (win386_buttons & WinButtonMiddle));

		state.Win386Pending_SetButtons(buttons);
		state.Win386Pending_SetButtonChanged(true);
	}

	// TODO: Try to call the event directly from here
	maybe_trigger_event();
}

static Bitu win386_callout_handler()
{
	// Function only supported in TSR mode
	if (!state_segment.has_value() || mouse_config.dos_driver_no_tsr) {
		return CBRET_NONE;
	}

	MouseDriverState state(*state_segment);

	switch (reg_ax) {
	// Mouse event notification
	case 1:
		if (is_win386_foreground) {
			win386_handle_mouse_event(reg_si, reg_dx, reg_bx, reg_cx);
		}
		break;
	// Hide mouse cursor, will be displayed by Windows
	case 2:
		restore_cursor_background();
		state.SetWin386Cursor(true);
		break;
	// Show mouse cursor
	case 3:
		state.SetWin386Cursor(false);
		draw_cursor();
		break;
	// Unknown function
	default:
		LOG_WARNING("MOUSE (DOS): Windows callout function 0x%04x not implemented",
		            reg_ax);
		break;
	}

	return CBRET_NONE;
}

bool MOUSEDOS_NeedsAutoexecEntry()
{
	return mouse_config.dos_driver_autoexec;
}

bool MOUSEDOS_IsDriverStarted()
{
	return state_segment.has_value();
}

static void start_driver()
{
	// Callback for mouse interrupt 0x33
	const auto tmp_pt = static_cast<uint16_t>(DOS_GetMemory(0x1) - 1);
	const auto int33_location = RealMake(tmp_pt, 0x10);
	CALLBACK_Setup(CallbackIDs::int33,
	               &int33_handler,
	               CB_MOUSE,
	               RealToPhysical(int33_location),
	               "Mouse");
	// Wasteland needs low(seg(int33))!=0 and low(ofs(int33))!=0
	real_writed(0, 0x33 << 2, int33_location);

	const auto tmp_offs = static_cast<uint16_t>(RealOffset(int33_location) + 2);
	CALLBACK_Setup(CallbackIDs::mouse_bd,
	               &mouse_bd_handler,
	               CB_RETF8,
	               PhysicalMake(RealSegment(int33_location), tmp_offs),
	               "MouseBD");

	// Callback for mouse user routine return
	CALLBACK_Setup(CallbackIDs::user,
	               &user_callback_handler,
	               CB_RETF_CLI,
	               "mouse user ret");
	user_callback = CALLBACK_RealPointer(CallbackIDs::user);

	// Windows mouse callout
	CALLBACK_Setup(CallbackIDs::win386,
	               win386_callout_handler,
	               CB_RETF,
	               "Windows mouse callout");

	MouseDriverState state(*state_segment);

	maybe_log_mouse_model();

	state.SetUnknownValue01(50);
	state.SetUserCallbackSegment(0x6362); // magic value
	state.SetHidden(1);                   // hide cursor on startup
	state.SetBiosScreenMode(UINT8_MAX);   // non-existing mode

	set_sensitivity(50, 50, 50);
	reset_hardware();
	reset();

	synchronize_driver_language();

	auto& interface = MouseInterface::GetInstance(MouseInterfaceId::DOS);
	interface.NotifyDosDriverStartup();
}

bool MOUSEDOS_StartDriver(const bool force_low_memory)
{
	if (MOUSEDOS_IsDriverStarted()) {
		return false;
	}

	state_segment = DOS_CreateFakeTsrArea(MouseDriverState::GetSize(),
	                                      force_low_memory);
	if (!state_segment) {
		return false;
	}

	start_driver();

	return true;
}

void MOUSEDOS_Init()
{
	prepare_driver_info();

	// Allocate callbacks
	CallbackIDs::int33    = CALLBACK_Allocate();
	CallbackIDs::mouse_bd = CALLBACK_Allocate();
	CallbackIDs::user     = CALLBACK_Allocate();
	CallbackIDs::win386   = CALLBACK_Allocate();

	// Start the driver if virtual driver got selected
	if (mouse_config.dos_driver_no_tsr) {
		state_segment = 0;
		start_driver();
	}
}
