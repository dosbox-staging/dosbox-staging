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

#ifndef DOSBOX_TUI_LABEL_H
#define DOSBOX_TUI_LABEL_H

#include "tui_abstractwidget.h"

#include <string>

class TuiLabel : public TuiAbstractWidget
{
public:
	TuiLabel(TuiApplication &application);
	~TuiLabel() override;

	void SetText(const std::string &text);
	void SetAttributes(const uint8_t attributes);

	void SetMarginLeft(const uint8_t margin);
	void SetMarginRight(const uint8_t margin);
	void SetMarginLeftRight(const uint8_t margin);

	void OnRedraw() override;

private:
	void SetLabelCommon();

	std::string text = {};
	uint8_t attributes = 0;

	uint8_t margin_left  = 0;
	uint8_t margin_right = 0;
};

#endif // DOSBOX_TUI_LABEL_H
