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

#ifndef DOSBOX_TUI_OBJECT_H
#define DOSBOX_TUI_OBJECT_H

#include "tui_types.h"

#include <list>
#include <memory>
#include <vector>

class TuiApplication;

class TuiObject
{
public:
	TuiObject(TuiApplication &application);
	virtual ~TuiObject();

protected:
	TuiKeyboardStatus KeyboardStatus() const;

	TuiApplication &application;

private:
	TuiObject() = delete;
};

#endif // DOSBOX_TUI_OBJECT_H
