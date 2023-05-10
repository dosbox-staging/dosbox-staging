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

#ifndef DOSBOX_TUI_APPLICATION_H
#define DOSBOX_TUI_APPLICATION_H

#include "programs.h"

#include "tui_types.h"

class TuiScreen;


class TuiApplication final
{
public:
	template<class T, typename... Params>
	static void Run(Program &program, Params... params) {
		TuiApplication application(program);
		application.Run(std::make_shared<T>(application, params...));
	}

	void FlushKeyboard();
	void RequestQuit() { is_quit_requested = true; }

	TuiKeyboardStatus KeyboardStatus() const { return keyboard_status; }

	// XXX create separate class for artwork
	void SetBlackWhite(const bool is_blackwhite);
	bool GetBlackWhite() const { return is_blackwhite; }
	// XXX create separate class for artwork
	uint8_t GetAttributesDefault() const;
	uint8_t GetAttributesReverse() const;

private:
	TuiApplication() = delete;
	TuiApplication(const TuiApplication&) = delete;
	void operator=(const TuiApplication&) = delete;

	TuiApplication(Program &program);
	~TuiApplication();

	void Run(std::shared_ptr<TuiScreen> screen);

	bool EnforceMinResolution(TuiScreen &screen);

	static bool IsGraphicsMonochrome();
	void HandleKeyboardEvents(std::shared_ptr<TuiScreen> screen);
	void HandleMouseEvents(std::shared_ptr<TuiScreen> screen);

	void ReadKeyboardStatus();

	Program &program;
	bool is_init_needed    = true;
	bool is_quit_requested = false;

	bool is_blackwhite = false;

	TuiKeyboardStatus keyboard_status = {};
};

#endif // DOSBOX_TUI_APPLICATION_H
