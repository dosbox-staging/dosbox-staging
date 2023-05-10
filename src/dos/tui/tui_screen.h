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

#ifndef DOSBOX_TUI_SCREEN_H
#define DOSBOX_TUI_SCREEN_H

#include "tui_abstractwidget.h"

#include <vector>

class TuiScreen : public TuiAbstractWidget
{
public:
	TuiScreen(TuiApplication &application);
	~TuiScreen() override;

	void Refresh();
	void SetResolution(const TuiCoordinates resolution);

	void OnInit() override;
	void OnRedraw() override;

private:

	const struct ScreenStorage {
		ScreenStorage();
		~ScreenStorage();

		uint8_t page = 0;
		TuiCoordinates size = {};

		uint16_t cursor_shape = 0;
		TuiCoordinates cursor_position = {};

		std::vector<uint16_t> content = {};

	} screen_storage = {};

	void RefreshCursor();

	void ApplyCursorShape(const TuiCursor shape);
	void ApplyCursorPosition(const TuiCoordinates position);
	void ApplyCellContent(const TuiCoordinates position, const TuiCell cell);

	TuiCursor screen_cursor_shape    = {};
	TuiCoordinates screen_cursor_position = {};

	TuiCell background = {};
};

#endif // DOSBOX_TUI_SCREEN_H
