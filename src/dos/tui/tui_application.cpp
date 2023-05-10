/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#include "tui_application.h"

#include "tui_screen.h"

#include "bios.h"
#include "callback.h"
#include "checks.h"
#include "logging.h"
#include "mem.h"
#include "pic.h"
#include "regs.h"

#include "../../ints/int10.h"

using namespace bit::literals;

CHECK_NARROWING();


constexpr uint8_t min_resolution_x = 80;
constexpr uint8_t min_resolution_y = 25;
constexpr uint8_t max_resolution   = 250;


TuiApplication::TuiApplication(Program &program) :
	program(program)
{
	is_init_needed = true;

	is_blackwhite = IsGraphicsMonochrome(); // XXX IsGraphicsBlackWhite(); ???
}

TuiApplication::~TuiApplication()
{
}

void TuiApplication::Run(std::shared_ptr<TuiScreen> screen)
{
	assert(screen);

	FlushKeyboard();
	if (!EnforceMinResolution(*screen)) {
		// XXX error message
		return;
	}

	ReadKeyboardStatus();

	while (!shutdown_requested && !is_quit_requested) {
		if (is_init_needed) {
			// XXX mark every widget as needing init?
			is_init_needed = false;
		}
		screen->Refresh();

		CALLBACK_Idle();

		HandleKeyboardEvents(screen);
		HandleMouseEvents(screen);
	}

	FlushKeyboard();
}

bool TuiApplication::EnforceMinResolution(TuiScreen &screen)
{
	uint8_t resolution_x = 0;
	uint8_t resolution_y = 0;

	auto check_resolution = [&] {
		resolution_x = check_cast<uint8_t>(INT10_GetTextColumns());
		resolution_y = check_cast<uint8_t>(INT10_GetTextRows());

		return (resolution_x >= min_resolution_x) &&
		       (resolution_y >= min_resolution_y) &&
		       (resolution_x <= max_resolution) &&
		       (resolution_y <= max_resolution);
	};

	if (!check_resolution()) {
		// Try to set proper video mode
		reg_ah = 0x00; // set video mode
		reg_al = (machine < MCH_CGA) ? 0x07 : 0x03; // 80x25 mode
		CALLBACK_RunRealInt(0x10);
		if (!check_resolution()) {
			return false;
		}
	}

	screen.SetResolution({resolution_x, resolution_y});
	return true;
}

void TuiApplication::HandleKeyboardEvents(std::shared_ptr<TuiScreen> screen)
{
	// Check for changed keyboard status flags
	const auto old_status = keyboard_status;
	ReadKeyboardStatus();

	if (keyboard_status != old_status) {

		if (keyboard_status.IsShiftPressed() !=
		    old_status.IsShiftPressed()) {
			screen->PassShiftKeyEvent();
		}

		if (keyboard_status.IsControlPressed() !=
		    old_status.IsControlPressed()) {
			screen->PassControlKeyEvent();
		}

		if (keyboard_status.IsAltPressed() !=
		    old_status.IsAltPressed()) {
			screen->PassAltKeyEvent();
		}

		if (keyboard_status.IsNumLockActive() !=
                    old_status.IsNumLockActive()) {
			screen->PassNumLockKeyEvent();
		}

		if (keyboard_status.IsCapsLockActive() !=
		    old_status.IsCapsLockActive()) {
			screen->PassCapsLockKeyEvent();
		}
	}

	// Check keyboard type
	const auto flags = real_readb(BIOSMEM_SEG, BIOSMEM_KBD_FLAGS3);
	const bool is_keyboard_extended = bit::is(flags, b4);

	// Check for key stroke
	reg_ah = is_keyboard_extended ? 0x11 : 0x01;
	CALLBACK_RunRealInt(0x16);
	if (GETFLAG(ZF)) {
		// Nothing available to read
		return;
	}

	// Fetch the key
	reg_ah = is_keyboard_extended ? 0x10 : 0x00;
	CALLBACK_RunRealInt(0x16);
	if (reg_ax != 0) {
		screen->PassInputEvent({reg_ah, reg_al, keyboard_status});
	}
}

void TuiApplication::HandleMouseEvents(std::shared_ptr<TuiScreen> screen)
{
	// XXX
}

void TuiApplication::ReadKeyboardStatus()
{
	reg_ah = 0x02; // compatible with all keyboards
	CALLBACK_RunRealInt(0x16);

	keyboard_status = reg_al;
}

bool TuiApplication::IsGraphicsMonochrome()
{
	return machine < MCH_CGA;
}

void TuiApplication::SetBlackWhite(const bool new_is_blackwhite)
{
	if (!IsGraphicsMonochrome() && new_is_blackwhite != is_blackwhite) {
		is_blackwhite  = new_is_blackwhite;
		is_init_needed = true;
	}
}

uint8_t TuiApplication::GetAttributesDefault() const
{
	return TuiColor::Black + TuiBgColor::White;
}

uint8_t TuiApplication::GetAttributesReverse() const
{
	return TuiColor::White + TuiBgColor::Black;
}

void TuiApplication::FlushKeyboard()
{
	// XXX
}
