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

#include "tui_screen.h"

#include "tui_application.h"

#include "../ints/int10.h"
#include "byteorder.h"
#include "dosbox.h"
#include "checks.h"
#include "logging.h"
#include "string_utils.h"

CHECK_NARROWING();


TuiScreen::ScreenStorage::ScreenStorage()
{
	page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
	size = { check_cast<uint8_t>(INT10_GetTextColumns()),
                 check_cast<uint8_t>(INT10_GetTextRows()) };

	cursor_shape    = real_readw(BIOSMEM_SEG, BIOSMEM_CURSOR_TYPE);
	cursor_position = { CURSOR_POS_COL(page), CURSOR_POS_ROW(page) };

	content.resize(size.x * size.y);
	for (uint16_t x = 0; x < size.x; ++x) {
		for (uint16_t y = 0; y < size.y; ++y) {
			INT10_SetCursorPos(static_cast<uint8_t>(y),
			                   static_cast<uint8_t>(x),
			                   page);
			const auto idx = static_cast<uint16_t>(x + y * size.x);
			INT10_ReadCharAttr(&content[idx], page);
		}
	}

	// XXX for VGA, also use:
	// INT10_VideoState_GetSize, INT10_VideoState_Save, INT10_VideoState_Restore
	// DOS_AllocateMemory, DOS_FreeMemory

	// XXX AH=0Bh / BH = 00h - set background/border color  XXX - how to get it? 
	// XXX AH=0Bh / BH = 01h - set palette                  XXX - how to get it?

	// XXX store/restore screen mode
}

TuiScreen::ScreenStorage::~ScreenStorage()
{
	for (uint16_t x = 0; x < size.x; ++x) {
		for (uint16_t y = 0; y < size.y; ++y) {
			INT10_SetCursorPos(static_cast<uint8_t>(y),
			                   static_cast<uint8_t>(x),
			                   page);
			const auto idx = static_cast<uint16_t>(x + y * size.x);
			const auto &tmp = content[idx];
			constexpr uint16_t count = 1;
			constexpr bool showattr = true;
			INT10_WriteChar(read_low_byte(tmp), read_high_byte(tmp),
			                page, count, showattr);
		}
	}

	INT10_SetCursorPos(cursor_position.y, cursor_position.x, page);
	INT10_SetCursorShape(read_high_byte(cursor_shape),
	                     read_low_byte(cursor_shape));

	// XXX flush keyboard
}


TuiScreen::TuiScreen(TuiApplication &application) :
	TuiAbstractWidget(application)
{
	ApplyCursorShape(TuiCursor::Hidden);
	SetResolution(screen_storage.size); // XXX test with initial set to, for example, 40x20
}

TuiScreen::~TuiScreen()
{
}

void TuiScreen::SetResolution(const TuiCoordinates resolution)
{
	SetMinSizeXY(resolution);
	SetMaxSizeXY(resolution);
}

void TuiScreen::OnInit()
{
	std::string background_pattern = {};
	utf8_to_dos("░", background_pattern, UnicodeFallback::Null);
	assert(background_pattern.size() == 1);
	
	const auto attributes = TuiColor::White + TuiBgColor::Black;
	if (background_pattern[0] == 0) {
		background = { ' ', attributes };
	} else {
		background = { background_pattern[0], attributes };
	}
}

void TuiScreen::OnRedraw()
{
	SetCells({0, 0}, GetSizeXY(), background);
}

void TuiScreen::Refresh()
{
	Update();

	if (!is_widget_visible || (!is_surface_dirty && !has_dirty_descendant)) {
		RefreshCursor();
		return;
	}

	// Update screen content

	for (uint16_t idx_x = 0; idx_x < GetSizeX(); ++idx_x) {
		for (uint16_t idx_y = 0; idx_y < GetSizeY(); ++idx_y) {
			TuiCoordinates position = {idx_x, idx_y};
			const auto cell = CalculateCell(position);
			if (!cell) {
				continue;
			}

			ApplyCellContent(position, *cell);
		}
	}
	MarkTreeClean();

	RefreshCursor();
}

void TuiScreen::RefreshCursor()
{
	// XXX optimize: introduce cursor dirty flag

	// Update cursor shape

	bool is_cursor_chape_changed = false;
	const auto shape = CalculateCursorShape();
	if (shape != screen_cursor_shape) {
		ApplyCursorShape(shape);
		is_cursor_chape_changed = true;
	}

	// Update cursor position

	const auto position = CalculateCursorPosition();
	if (position != screen_cursor_position || is_cursor_chape_changed) {
	    	ApplyCursorPosition(position);
	}
}

void TuiScreen::ApplyCursorShape(const TuiCursor shape)
{
	uint8_t first_line = 0x20;
	uint8_t last_line  = 0x00;

	switch (shape) {
	case TuiCursor::Hidden:
		break;
	case TuiCursor::Normal:
		first_line = 0x06;
		last_line  = 0x07;
		break;
	case TuiCursor::Block:
		first_line = 0x00;
		last_line  = 0x07;
		break;
	default:
		assert(false);
		break;
	}

	INT10_SetCursorShape(first_line, last_line);
	screen_cursor_shape = shape;
}

void TuiScreen::ApplyCursorPosition(const TuiCoordinates position)
{
	INT10_SetCursorPos(position.y, position.x, screen_storage.page);
	screen_cursor_position = position;
}

void TuiScreen::ApplyCellContent(const TuiCoordinates position, const TuiCell cell)
{
	ApplyCursorPosition(position);

	constexpr uint16_t count = 1;
	constexpr bool showattr = true;

	INT10_WriteChar(cell.screen_code, cell.attributes,
	                screen_storage.page, count, showattr);
}
