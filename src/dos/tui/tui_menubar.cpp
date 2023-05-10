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

#include "tui_menubar.h"

#include "checks.h"
#include "logging.h"

#include <cassert>

CHECK_NARROWING();


TuiMenuBar::TuiMenuBar(TuiApplication &application) :
	TuiAbstractWidget(application)
{
	SetMinSizeXY({80, 1});
	SetMaxSizeY(1);

	// XXX
	widget_title = Add<TuiLabel>();
	widget_title->SetText("File Editor");
	widget_title->SetAttributes(TuiColor::Red + TuiBgColor::White);
	widget_title->SetPositionXY({58, 0});

	// XXX
	menus.emplace_back(Add<TuiLabel>());
	menus.emplace_back(Add<TuiLabel>());
	menus.emplace_back(Add<TuiLabel>());
	menus.emplace_back(Add<TuiLabel>());
	menus[0]->SetText(" File ");
	menus[0]->SetAttributes(TuiColor::Black + TuiBgColor::White);
	menus[0]->SetPositionXY({1, 0});
	menus[1]->SetText(" Edit ");
	menus[1]->SetAttributes(TuiColor::Black + TuiBgColor::White);
	menus[1]->SetPositionXY({7, 0});
	menus[2]->SetText(" Search ");
	menus[2]->SetAttributes(TuiColor::Black + TuiBgColor::White);
	menus[2]->SetPositionXY({13, 0});
	menus[3]->SetText(" View ");
	menus[3]->SetAttributes(TuiColor::Black + TuiBgColor::White);
	menus[3]->SetPositionXY({21, 0});
}

TuiMenuBar::~TuiMenuBar()
{
}

void TuiMenuBar::OnInit()
{
	// XXX

	const auto parent = GetParent();
	assert(parent);
	SetSizeXY({parent->GetSizeX(), 1});
}

void TuiMenuBar::OnRedraw()
{
	TuiCell cell = { ' ', TuiColor::Black + TuiBgColor::White };

	for (uint16_t x = 0; x < GetSizeX(); x++) {
		SetCell({x, 0}, cell);
	}

	cell.screen_code = 0xb3; // XXX check if exists
	SetCell({56, 0}, cell); // xxx dynamic size
}
