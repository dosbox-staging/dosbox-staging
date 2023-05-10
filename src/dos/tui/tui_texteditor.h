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

#ifndef DOSBOX_TUI_TEXTEDITOR_H
#define DOSBOX_TUI_TEXTEDITOR_H

#include "tui_abstractwidget.h"

#include "tui_label.h"
#include "tui_scrollbar.h"

#include <string>

class TuiTextEditor final : public TuiAbstractWidget
{
public:
	TuiTextEditor(TuiApplication &application);
	~TuiTextEditor() override;

	static void AddMessages();

	void OnInit() override;
	void OnResize() override;
	void OnRedraw() override;

	void OnInputEvent(const TuiScanCode scan_code) override;
	void OnInputEventShift() override;
	void OnInputEventControl() override;
	void OnInputEventAlt() override;
	void OnInputEventCapsLock() override;
	void OnInputEventNumLock() override;

private:
	std::shared_ptr<TuiLabel>      widget_title = {};
	std::shared_ptr<TuiScrollBarH> widget_scroll_bar_h = {};
	std::shared_ptr<TuiScrollBarV> widget_scroll_bar_v = {};
	std::shared_ptr<TuiLabel>      widget_status = {};
	std::shared_ptr<TuiLabel>      widget_num_lock = {};
	std::shared_ptr<TuiLabel>      widget_caps_lock = {};
	std::shared_ptr<TuiLabel>      widget_read_only= {};

	void MaybeMarkSelectionStart(const bool has_shift);
	bool MaybeMarkSelectionEnd();
	void StopSelection();
	void UnselectContent();
	bool IsInSelection(const size_t x, const size_t y) const;

	void RedrawDocument(const bool only_cursor_moved = false);
	void RecalculateContentWidthSize();
	void UpdateCursorShape();
	void UpdateStatusWidget();

	// Adapts input (from the file or host clipboard) to teh form applicable
	// to be put into the editor; handles control codes, splits into several
	// lines, etc.
	std::vector<std::string> InputToContent(std::string string) const;

	// Checks if adding more bytes to the document content won't exceed
	// maximum document size or maximum length of given line; requests beep
	// if check failed
	bool CheckFreeSpace(const size_t bytes);
	bool CheckFreeSpace(const size_t position_y,
	                    const size_t bytes);

	// If cursor is positioned after the line content, fill the line with
	// spaces to reach the cursor
	void FillLineToCursor();

	// Keyboard input - editing

	void KeyPrintable(const char character);
	void KeyEnter();
	void KeyTabulation();
	void KeyShiftTabulation();
	void KeyBackspace();
	void KeyInsert();
	void KeyDelete();

	// Keyboard input - cursor movement / selection

	void KeyCursorUp(const bool has_shift);
	void KeyCursorDown(const bool has_shift);
	void KeyCursorLeft(const bool has_shift);
	void KeyCursorRight(const bool has_shift);
	void KeyControlCursorUp(const bool has_shift);
	void KeyControlCursorDown(const bool has_shift);
	void KeyControlCursorLeft(const bool has_shift);
	void KeyControlCursorRight(const bool has_shift);

	void KeyHome(const bool has_shift);
	void KeyEnd(const bool has_shift);
	void KeyPageUp(const bool has_shift);
	void KeyPageDown(const bool has_shift);
	void KeyControlHome(const bool has_shift);
	void KeyControlEnd(const bool has_shift);
	void KeyControlPageUp(const bool has_shift);
	void KeyControlPageDown(const bool has_shift);

	// Keyboard input - clipboard support

	void KeyControlC();
	void KeyControlV();
	void KeyControlX();

	// XXX create appropriate section

	void KeyEscape();

	void KeyControlF();
	void KeyControlP();
	void KeyControlR();
	void KeyControlT();
	void KeyControlY();

	void KeyF1();
	void KeyF3();
	void KeyF6();
	void KeyF8();
	void KeyControlF4();
	void KeyControlF6();
	void KeyControlF8();

	void KeyAltPlus();
	void KeyAltMinus();

	uint8_t tabulation_size = 8; // XXX add function to set it

	size_t logical_cursor_x = 0;
	size_t logical_cursor_y = 0;

	size_t old_logical_cursor_x = UINT32_MAX;
	size_t old_logical_cursor_y = UINT32_MAX;

	bool is_insert_mode = true;

	std::vector<std::string> content = {};
	size_t content_width    = 0;
	size_t content_size     = 0; // in bytes
	size_t content_offset_x = 0;
	size_t content_offset_y = 0;

	bool is_content_selected     = false;
	bool is_selection_in_progres = false;
	bool is_selection_empty      = true;
	size_t selection_start_x = 0; // cursor position when selection started
	size_t selection_start_y = 0;
	size_t selection_begin_x = 0;
	size_t selection_begin_y = 0;
	size_t selection_end_x = 0;
	size_t selection_end_y = 0;

	size_t view_size_x = 0;
	size_t view_size_y = 0;

	uint8_t display_digits_line   = 0;
	uint8_t display_digits_column = 0;

	std::string border = {};

	std::string string_line   = {};
	std::string string_column = {};
};

#endif // DOSBOX_TUI_TEXTEDITOR_H
