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

#include "tui_texteditor.h"
#include "tui_application.h"

#include "checks.h"
#include "clipboard.h"
#include "logging.h"
#include "math_utils.h"

#include <cassert>

#include "string_utils.h"

CHECK_NARROWING();

constexpr size_t max_file_size   = 16 * 1024 * 1024;
constexpr size_t max_lines       = UINT16_MAX;
constexpr size_t max_line_length = UINT16_MAX;


TuiTextEditor::TuiTextEditor(TuiApplication &application) :
	TuiAbstractWidget(application)
{
	SetMinSizeXY({40, 10});

	widget_title = Add<TuiLabel>();
	widget_title->SetMarginLeftRight(1);
	widget_title->SetAttributes(TuiColor::Black + TuiBgColor::White);

	widget_read_only = Add<TuiLabel>();
	widget_read_only->SetMarginLeftRight(1);
	widget_read_only->SetAttributes(TuiColor::Black + TuiBgColor::White);

	widget_scroll_bar_h = Add<TuiScrollBarH>();
	widget_scroll_bar_v = Add<TuiScrollBarV>();

	widget_status = Add<TuiLabel>();
	widget_status->SetMarginLeftRight(1);
	widget_status->SetAttributes(TuiColor::Black + TuiBgColor::White);
	
	widget_num_lock = Add<TuiLabel>();
	widget_num_lock->SetText("NUM");
	widget_num_lock->SetMarginLeftRight(1);
	widget_num_lock->SetAttributes(TuiColor::Black + TuiBgColor::White);
	
	widget_caps_lock = Add<TuiLabel>();
	widget_caps_lock->SetText("CAPS");
	widget_caps_lock->SetMarginLeftRight(1);
	widget_caps_lock->SetAttributes(TuiColor::Black + TuiBgColor::White);
}

TuiTextEditor::~TuiTextEditor()
{
}

void TuiTextEditor::OnInit()
{
	const std::string src = "┌─┐│┘─└│";
	utf8_to_dos(src, border, UnicodeFallback::Box);
	assert(border.size() == src.size());

	string_line   = MSG_Get("TUI_TEXTEDITOR_LINE");
	string_column = MSG_Get("TUI_TEXTEDITOR_COLUMN");

	UpdateStatusWidget();

	// XXX temporary
	widget_title->SetText(std::string("<") + MSG_Get("TUI_TEXTEDITOR_UNTITLED") + std::string(" 1>"));
	widget_read_only->SetText(MSG_Get("TUI_TEXTEDITOR_READ_ONLY"));
}

void TuiTextEditor::OnResize()
{
	// SetSizeX(40); // XXX check with this size!

	uint8_t tmp = 0;
	const uint8_t bottom_edge = static_cast<uint8_t>(GetSizeY() - 1);
	const uint8_t left_edge = static_cast<uint8_t>(GetSizeX() - 1);

	// XXX resize scroll bars!
	widget_scroll_bar_h->SetScrollBarSize(20);
	widget_scroll_bar_v->SetScrollBarSize(20);

	widget_scroll_bar_h->SetPositionLeftFrom(left_edge, 2);
	widget_scroll_bar_h->SetPositionY(bottom_edge);
	widget_scroll_bar_v->SetPositionXY({left_edge, 2});

	widget_read_only->SetPositionLeftFrom(left_edge, 2);
	widget_read_only->SetPositionY(0);

	widget_title->SetPositionX(31); // XXX center!
	widget_title->SetPositionY(0);

	widget_status->SetPositionXY({3, bottom_edge});

	widget_caps_lock->SetPositionLeftFrom(*widget_scroll_bar_h, 2);
	widget_caps_lock->SetPositionY(bottom_edge);
	widget_num_lock->SetPositionLeftFrom(*widget_caps_lock, 1);
	widget_num_lock->SetPositionY(bottom_edge);

	view_size_x = GetSizeX() - 2;
	view_size_y = GetSizeY() - 2;
}

void TuiTextEditor::OnRedraw()
{
	const auto attributes = application.GetAttributesDefault(); // XXX

	constexpr uint8_t min_x = 0;
	constexpr uint8_t min_y = 0;
	const uint8_t max_x = GetSizeX() - 1;
	const uint8_t max_y = GetSizeY() - 1;

	const uint8_t len_x = max_x - 1; // XXX check range
	const uint8_t len_y = max_y - 1; // XXX check range

	assert(border.size() == 8);
	for (size_t idx = 0; idx < border.size(); ++idx) {
		TuiCell cell = { border[idx], TuiColor::White + TuiBgColor::Black };
		switch (idx) {
		case 0:
			SetCell({min_x, min_y}, cell);
			break;
		case 1:
			SetCells({min_x + 1, min_y}, {len_x, 1}, cell);
			break;
		case 2:
			SetCell({max_x, min_y}, cell);
			break;		
		case 3:
			SetCells({max_x, min_y + 1}, {1, len_y}, cell);
			break;
		case 4:
			SetCell({max_x, max_y}, cell);
			break;	
		case 5:
			SetCells({min_x + 1, max_y}, {len_x, 1}, cell);
			break;
		case 6:
			SetCell({min_x, max_y}, cell);
			break;	
		case 7:
			SetCells({min_x, min_y + 1}, {1, len_y}, cell);
			break;
		default:
			assert(false);
			break;
		}
	}

	RedrawDocument();
	UpdateCursorShape();
}

void TuiTextEditor::MaybeMarkSelectionStart(const bool has_shift)
{
	if (!is_selection_in_progres && has_shift) {
		is_content_selected     = true;
		is_selection_in_progres = true;
		is_selection_empty      = true;
		
		selection_start_x = logical_cursor_x;
		selection_start_y = logical_cursor_y;
	}
}

bool TuiTextEditor::MaybeMarkSelectionEnd()
{
	if (!is_selection_in_progres) {
		return false;
	}

	// XXX clamp logical cursor x and y

	if (logical_cursor_x == selection_start_x &&
	    logical_cursor_y == selection_start_y) {
		is_selection_empty = true;
		return true;
	}

	is_selection_empty = false;

	if ((logical_cursor_y == selection_start_y &&
	     logical_cursor_x < selection_start_x) ||
	    (logical_cursor_y < selection_start_y)) {
	    	// Cursor is located BEFORE the point where selection started
		selection_begin_x = logical_cursor_x;
		selection_begin_y = logical_cursor_y;
	    	if (selection_start_x == 0) {
	    		selection_end_x = max_line_length;
	    		selection_end_y = selection_start_y - 1;
	    	} else {
	    		selection_end_x = selection_start_x - 1;
	    		selection_end_y = selection_start_y;
	    	}
		return true;
	}

	if ((logical_cursor_y == selection_start_y &&
	     logical_cursor_x >= selection_start_x) ||
	    (logical_cursor_y > selection_start_y)) {
	    	// Cursor is located AFTER the point where selection started
	    	selection_begin_x = selection_start_x;
	    	selection_begin_y = selection_start_y;
	    	if (logical_cursor_x == 0) {
			selection_end_x = max_line_length;
			selection_end_y = logical_cursor_y - 1;
	    	} else {
			selection_end_x = logical_cursor_x - 1;
			selection_end_y = logical_cursor_y;
	    	}
		return true;
	}

	return true;
}

void TuiTextEditor::StopSelection()
{
	if (is_selection_in_progres) {
		is_selection_in_progres = false;
		if (is_selection_empty) {
			is_content_selected = false;
		}
	}
}

void TuiTextEditor::UnselectContent()
{
	is_content_selected = false;
	is_selection_empty  = true;
}

bool TuiTextEditor::IsInSelection(const size_t x, const size_t y) const
{
	if (!is_content_selected || is_selection_empty) {
		return false;
	}

	if (y > selection_begin_y && y < selection_end_y) {
		return true;
	}

	if (selection_begin_y == selection_end_y) {
		return (y == selection_begin_y) &&
	               (x >= selection_begin_x) && (x <= selection_end_x);
	}

	if (y == selection_begin_y && x >= selection_begin_x) {
		return true;
	}

	if (y == selection_end_y && x <= selection_end_x) {
		return true;
	}

	return false;
}

void TuiTextEditor::RedrawDocument(const bool only_cursor_moved)
{
	bool should_redraw_content = !only_cursor_moved;

	// Adapt cursor position

	logical_cursor_x = std::min(logical_cursor_x, max_line_length);
	logical_cursor_y = std::min(logical_cursor_y, content.size());

	if (logical_cursor_x != old_logical_cursor_x ||
	    logical_cursor_y != old_logical_cursor_y) {
	    	old_logical_cursor_x = logical_cursor_x;
	    	old_logical_cursor_y = logical_cursor_y;
		UpdateStatusWidget();
	}

	// Update cursor position

	const TuiCoordinates dimension_correction = {3, 3};
	const TuiCoordinates position_correction  = {1, 1};

	const auto dimensions = GetSizeXY() - dimension_correction;

	auto adapt_offset = [&](size_t &content_offset,
	                       const size_t logical_cursor_position,
	                       const uint8_t dimension) {
		if (logical_cursor_position < content_offset) {
			content_offset = logical_cursor_position;
			should_redraw_content = true;
		} else if (logical_cursor_position - content_offset > dimension) {
			content_offset = logical_cursor_position - dimension;
			should_redraw_content = true;
		}
	};

	adapt_offset(content_offset_x, logical_cursor_x, dimensions.x);
	adapt_offset(content_offset_y, logical_cursor_y, dimensions.y);

	TuiCoordinates position = { logical_cursor_x - content_offset_x,
	                            logical_cursor_y - content_offset_y };

	SetCursorPositionXY(position + position_correction);

	// Update scroll bars

	const auto update_scroll_bar = [](TuiAbstractScrollBar &widget,
	                                  const size_t content_size,
	                                  const size_t content_offset,
	                                  const size_t view_size,
	                                  const size_t cursor_position) {
		if (content_offset != 0 || content_size > view_size) {
			widget.Show();
			const auto total = std::max(content_size, cursor_position);
			widget.SetScrollBarParams(total, view_size, content_offset);
		} else {
			widget.Hide();
		}
	};

	update_scroll_bar(*widget_scroll_bar_h, content_width, content_offset_x,
	                  view_size_x, logical_cursor_x);
	update_scroll_bar(*widget_scroll_bar_v, content.size(), content_offset_y,
	                  view_size_y, logical_cursor_y);

	// Redraw document content if needed

	if (!should_redraw_content) {
		return;
	}

	// XXX get these from artwork class
	const auto color_normal   = TuiColor::White + TuiBgColor::Blue;
	const auto color_selected = TuiColor::Black + TuiBgColor::White;	
	TuiCell cell = {};
	for (uint16_t x = 1; x < GetSizeX() - 1; x++) {
		for (uint16_t y = 1; y < GetSizeY() - 1; y++) {
			const size_t index_x = x - 1 + content_offset_x;
			const size_t index_y = y - 1 + content_offset_y;
			cell.attributes = IsInSelection(index_x, index_y) ?
				color_selected : color_normal;
			if (index_y >= content.size() ||
			    index_x >= content[index_y].size()) {
			    	cell.screen_code = ' ';
			} else {
				cell.screen_code = content[index_y][index_x];
			}
			SetCell({x, y}, cell);
		}
	}
}

void TuiTextEditor::RecalculateContentWidthSize()
{
	content_width = 0;
	content_size  = content.size() * 2; // 2 bytes for each new line
	if (content.empty()) {
		return;
	}

	// Calculate content width
	const auto it = std::max_element(content.begin(), content.end(),
        	[](const auto &a, const auto &b) { return a.size() < b.size(); });
	content_width = it->size();

	// Calculate content size, in bytes
	for (const auto &line : content) {
		content_size += line.size();
	}
}

void TuiTextEditor::UpdateCursorShape()
{
	SetCursorShape(is_insert_mode ? TuiCursor::Normal : TuiCursor::Block);
}

void TuiTextEditor::UpdateStatusWidget()
{
	auto adapt_digits = [](uint8_t &display_digits,
                               const bool min_3_digits,
                               const size_t value) {
		assert(value < 100000);
		if (value >= 10000 && display_digits < 5) {
			display_digits = 5;
		} else if (value < 9000 && display_digits > 4) {
			display_digits = 4; // hysteresis
		} else if (value >= 1000 && display_digits < 4) {
			display_digits = 4;
		} else if (value < 900 && display_digits > 3) {
			display_digits = 3; // hysteresis
		} else if (min_3_digits) {
			if (display_digits < 3) {
				display_digits = 3; // minimum number of digits
			}
		} else {
			if (value >= 100 && display_digits < 3) {
				display_digits = 3;
			} else if (value < 90 && display_digits > 2) {
				display_digits = 2; // hysteresis
			} else if (display_digits < 2) {
				display_digits = 2; // minimum number of digits
			}
		}
	};

	bool min_3_digits = true;
	adapt_digits(display_digits_line, min_3_digits,
	             std::max(logical_cursor_y, content.size()) + 1);
	min_3_digits = false;
	adapt_digits(display_digits_column, min_3_digits,
	             std::max(logical_cursor_x, content_width) + 1);

	std::string line_num_str   = std::to_string(logical_cursor_y + 1);
	std::string column_num_str = std::to_string(logical_cursor_x + 1);

	while (line_num_str.size() < display_digits_line) {
		line_num_str = std::string("0") + line_num_str;
	}
	while (column_num_str.size() < display_digits_column) {
		column_num_str = std::string("0") + column_num_str;
	}

	static const std::string separator1 = ": ";
	static const std::string separator2 = " ";

	line_num_str   = string_line + separator1 + line_num_str;
	column_num_str = string_column + separator1 + column_num_str;

	widget_status->SetText(line_num_str + separator2 + column_num_str);
}

std::vector<std::string> TuiTextEditor::InputToContent(std::string string) const
{
	// XXX handle tabulation

	constexpr char char_cr = 0x0d; // carriage return
	constexpr char char_lf = 0x0a; // line feed

	std::vector<std::string> output = {};

	std::string line = {};

	bool ignore_next_cr = false;
	bool ignore_next_lf = false;
	bool last_was_new_file = false;
	for (const auto character : string) {
		// Ignore second part of the newline charatcer
		if (ignore_next_cr && character == char_cr ||
		    ignore_next_lf && character == char_lf) {
		    	ignore_next_cr = false;
		        ignore_next_lf = false;
		        continue;
		}

		// Handle newline character
		if (character == char_cr || character == char_lf) {
			if (character == char_cr) {
				ignore_next_lf = true;
			} else {
				ignore_next_cr = true;
			}

			output.push_back(line);
			line.clear();
			last_was_new_file = true;
			continue;
		}

		last_was_new_file = false;

		// XXX temporary solution
		if (!is_extended_printable_ascii(character)) {
			line.push_back('?');
			continue;
		}

		// A regular, printable character
		line.push_back(character);
	}

	if (!line.empty()) {
		output.push_back(line);
	} else if (last_was_new_file) {
		output.push_back(""); // XXX skip if loading file?
	}

	return output;
}

void TuiTextEditor::OnInputEvent(const TuiScanCode scan_code)
{
	const auto control_key = scan_code.GetControlKey();
	const auto hot_key     = scan_code.GetHotKey();
	const bool has_shift   = scan_code.HasShift();

	if ((!has_shift || control_key == TuiControlKey::ShiftTabulation) &&
	    is_selection_in_progres) {
		StopSelection();
	}

	if (scan_code.IsPrintable()) {
		KeyPrintable(scan_code.GetPrintableChar());
		return;
	}

	// XXX Control_Q, F - find
	// XXX Control_Q, A - find and replace
	// XXX Control_Q, Y - cut to the end of the line
	// XXX Control_Q, E - move cursor to top of window
	// XXX Control_Q, X - move cursor to bottom of window
	// XXX Control_K, 0-3 - set bookmark
	// XXX Control_Q, 0-3 - go to bookmark
	// XXX Home,Ctrl+N - insert blank line above cursor position

	// XXX reorder as in .h file

	switch(control_key) {
	case TuiControlKey::Enter:              KeyEnter();              return;
	case TuiControlKey::Tabulation:         KeyTabulation();         return;
	case TuiControlKey::ShiftTabulation:    KeyShiftTabulation();    return;
	case TuiControlKey::Backspace:          KeyBackspace();          return;
	case TuiControlKey::Escape:             KeyEscape();             return;
	case TuiControlKey::CursorUp:
	case TuiControlKey::ShiftCursorUp:      KeyCursorUp(has_shift);    return;
	case TuiControlKey::CursorDown:
	case TuiControlKey::ShiftCursorDown:    KeyCursorDown(has_shift);  return;
	case TuiControlKey::CursorLeft:
	case TuiControlKey::ShiftCursorLeft:    KeyCursorLeft(has_shift);  return;
	case TuiControlKey::CursorRight:
	case TuiControlKey::ShiftCursorRight:   KeyCursorRight(has_shift); return;
	case TuiControlKey::ControlCursorUp:
	case TuiControlKey::ShiftControlCursorUp:    KeyControlCursorUp(has_shift);    return;
	case TuiControlKey::ControlCursorDown:
	case TuiControlKey::ShiftControlCursorDown:  KeyControlCursorDown(has_shift);  return;
	case TuiControlKey::ControlCursorLeft:
	case TuiControlKey::ShiftControlCursorLeft:  KeyControlCursorLeft(has_shift);  return;
	case TuiControlKey::ControlCursorRight:
	case TuiControlKey::ShiftControlCursorRight: KeyControlCursorRight(has_shift); return;

	case TuiControlKey::Insert:             KeyInsert();             return;
	case TuiControlKey::Delete:             KeyDelete();             return;
	case TuiControlKey::Home:
	case TuiControlKey::ShiftHome:          KeyHome(has_shift);      return;
	case TuiControlKey::End:
	case TuiControlKey::ShiftEnd:           KeyEnd(has_shift);       return;
	case TuiControlKey::PageUp:
	case TuiControlKey::ShiftPageUp:        KeyPageUp(has_shift);    return;
	case TuiControlKey::PageDown:
	case TuiControlKey::ShiftPageDown:      KeyPageDown(has_shift);  return;
	case TuiControlKey::ControlHome:
	case TuiControlKey::ShiftControlHome:   KeyControlHome(has_shift);   return;
	case TuiControlKey::ControlEnd:
	case TuiControlKey::ShiftControlEnd:    KeyControlEnd(has_shift);    return;
	case TuiControlKey::ControlPageUp:
	case TuiControlKey::ShiftControlPageUp:   KeyControlPageUp(has_shift);   return;
	case TuiControlKey::ControlPageDown:
	case TuiControlKey::ShiftControlPageDown: KeyControlPageDown(has_shift); return;

	// Shortcuts with duplicated functionalities
	case TuiControlKey::ShiftInsert:    KeyControlC();  return;
	case TuiControlKey::ControlInsert:  KeyControlV();  return;
	case TuiControlKey::ShiftDelete:    KeyControlX();  return;

	default:
		if (is_selection_in_progres) {
			StopSelection();
		}
		break;
	}

	switch(hot_key) {
	case TuiHotKey::Control_C:  KeyControlC();  return;
	case TuiHotKey::Control_F:  KeyControlF();  return;
	case TuiHotKey::Control_P:  KeyControlP();  return;
	case TuiHotKey::Control_R:  KeyControlR();  return;
	case TuiHotKey::Control_T:  KeyControlT();  return;
	case TuiHotKey::Control_V:  KeyControlV();  return;
	case TuiHotKey::Control_X:  KeyControlX();  return;
	case TuiHotKey::Control_Y:  KeyControlY();  return;

	case TuiHotKey::F1:         KeyF1();         return;
	case TuiHotKey::F3:         KeyF3();         return;
	case TuiHotKey::F6:         KeyF6();         return;
	case TuiHotKey::F8:         KeyF8();         return;
	case TuiHotKey::Control_F4: KeyControlF4();  return;
	case TuiHotKey::Control_F6: KeyControlF6();  return;
	case TuiHotKey::Control_F8: KeyControlF8();  return;

	case TuiHotKey::Alt_Plus:   KeyAltPlus();    return;
	case TuiHotKey::Alt_Minus:  KeyAltMinus();   return;

	// Shortcuts with duplicated functionalities
	case TuiHotKey::Control_G:  KeyDelete();    return;
	case TuiHotKey::Control_H:  KeyBackspace(); return;
	case TuiHotKey::Control_K:  KeyF3();        return;

	default: break;
	}
}

void TuiTextEditor::OnInputEventShift()
{
	if (!KeyboardStatus().IsShiftPressed()) {
		StopSelection();
	}
}

void TuiTextEditor::OnInputEventControl()
{
	StopSelection();
}

void TuiTextEditor::OnInputEventAlt()
{
	StopSelection();
}

void TuiTextEditor::OnInputEventCapsLock()
{
	if (KeyboardStatus().IsCapsLockActive()) {
		widget_caps_lock->Show();
	} else {
		widget_caps_lock->Hide();
	}
}

void TuiTextEditor::OnInputEventNumLock()
{
	if (KeyboardStatus().IsNumLockActive()) {
		widget_num_lock->Show();
	} else {
		widget_num_lock->Hide();
	}
}

bool TuiTextEditor::CheckFreeSpace(const size_t bytes)
{
	if (content_size + bytes > max_file_size) {
		RequestBeep();
		return false;
	}

	return true;
}

bool TuiTextEditor::CheckFreeSpace(const size_t position_y,
                                   const size_t bytes)
{
	// Check maximum number of lines in the document

	if (position_y >= max_lines) {
		RequestBeep();
		return false;
	}

	// Check overall document size

	size_t needed_size = content_size + bytes;
	if (position_y >= content.size()) {
		// Extra new line characters will have to be added
		needed_size += 2 * (content.size() - position_y + 1);
	}
	if (needed_size > max_file_size) {
		RequestBeep();
		return false;
	}

	// Check maximum line length

	if (position_y >= content.size()) {
		if (bytes > max_line_length) {
			RequestBeep();
			return false;
		}
	} else {
		if (bytes + content[position_y].size() > max_line_length) {
			RequestBeep();
			return false;
		}
	}

	return true;
}

void TuiTextEditor::FillLineToCursor()
{
	while (logical_cursor_y >= content.size()) {
		content.push_back("");
		content_size += 2; // new-line is 2 characters long
	}

	auto &line = content[logical_cursor_y];
	if (logical_cursor_x > line.size()) {
		// If cursor is too far away, insert spaces
		const size_t num_spaces = logical_cursor_x - line.size();
		line.insert(line.size(), num_spaces, ' ');
		content_size += num_spaces;
	}
}

// ***************************************************************************
// Keyboard input - editing
// ***************************************************************************

void TuiTextEditor::KeyPrintable(const char character)
{
	// Check if enough space is left in the file
	size_t needed_size = 1;
	if (!is_insert_mode && (logical_cursor_y < content.size()) &&
	    (logical_cursor_x < content[logical_cursor_y].size())) {
	    	needed_size = 0;
	}
	if (!CheckFreeSpace(logical_cursor_y, needed_size)) {
		return;
	}

	// Add new line if needed
	if (logical_cursor_y == content.size()) {
		content.push_back({});
		content_size += 2; // new-line is 2 characters long
	}

	// Write character
	FillLineToCursor();
	auto &line = content[logical_cursor_y];
	if (logical_cursor_x < line.size() && !is_insert_mode) {
		line[logical_cursor_x] = character;
	} else {
		if (is_content_selected) {
			// Update selection if needed
			auto update_1 = [&](size_t &value_x, size_t &value_y) {
				// for cursor at the first line of selection
				if ((logical_cursor_y == value_y) &&
				    (logical_cursor_x < value_x)) {
				    	++value_x;
				}
			};
			auto update_2 = [&](size_t &value_x, size_t &value_y) {
				// for cursor at the last line of selection
				if ((logical_cursor_y == value_y) &&
				    (logical_cursor_x <= value_x)) {
				    	++value_x;
				}
			};
			update_1(selection_start_x, selection_start_y);
			update_1(selection_begin_x, selection_begin_y);
			update_2(selection_end_x, selection_end_y);
		}
		line.insert(logical_cursor_x, 1, character);
		++content_size;
	}
	++logical_cursor_x;

	// Update view end exit
	content_width = std::max(content_width, line.size());
	RedrawDocument();
}

void TuiTextEditor::KeyEnter()
{
	// Insert new line or split current one in two parts

	if (logical_cursor_y >= content.size()) {
		// Cursor is below the last line of the document - add a line
		if (!CheckFreeSpace(logical_cursor_y, 2)) {
			return;
		}
		content.push_back({});
		content_size += 2;
		logical_cursor_x = 0;
		++logical_cursor_y;

		constexpr bool only_cursor_moved = true;
		RedrawDocument(only_cursor_moved);
	} else {
		auto &line = content[logical_cursor_y];
		if (logical_cursor_y == (content.size() - 1) &&
		    logical_cursor_x >= line.size()) {
			// Cursor at the end of the last line of the document
			// do not add a new line, just move the cursor to the
			// beginning of the  next line.
			// Note: original EDIT.COM does not seem to have a
			// special support for this case, which can lead to
			// inserting unnecessary empty lines at the end of file
			logical_cursor_x = 0;
			++logical_cursor_y;

			constexpr bool only_cursor_moved = true;
			RedrawDocument(only_cursor_moved);
		} else {
			// XXX enforce maximum sizes / update sizes

			// Either add a new line in the middle of the document
			// or split the current line into two
			bool is_width_outdated = false;
			std::string new_line = "";
			if (logical_cursor_x < line.size()) {
				// Split the current line into two
				is_width_outdated = (line.size() == content_width);
				new_line = line.substr(logical_cursor_x);
				line = line.substr(0, logical_cursor_x);
			}
			if (is_content_selected) {
				// Update selection if needed
				auto update = [&](size_t &value_x, size_t &value_y) {
					if (logical_cursor_y == value_y) {
						if (logical_cursor_x <= value_x) {
							value_x -= logical_cursor_x;
							++value_y;
						}

					} else if (logical_cursor_y < value_y) {
						++value_y;
					} 
				};
				update(selection_start_x, selection_start_y);
				update(selection_begin_x, selection_begin_y);
				update(selection_end_x, selection_end_y);
			}
			logical_cursor_x = 0;
			++logical_cursor_y;
			content.insert(content.begin() + logical_cursor_y, new_line);

			if (is_width_outdated) {
				RecalculateContentWidthSize();
			}
			RedrawDocument();
		}
	}
}

void TuiTextEditor::KeyTabulation()
{
	// Move the cursor to the next tabulation stop / insert tabulation space
	// XXX Indent selection

	// XXX add spaces if in the middle of the line
	++logical_cursor_x;
	const auto remainder = logical_cursor_x % tabulation_size;
	if (remainder != 0) {
		logical_cursor_x += tabulation_size - remainder;
	}

	constexpr bool only_cursor_moved = true;
	RedrawDocument(only_cursor_moved);
}

void TuiTextEditor::KeyShiftTabulation()
{
	// Unindent selection

	// XXX
}

void TuiTextEditor::KeyBackspace()
{
	// Delete character left to the cursor

	// XXX handle selection update

	if (logical_cursor_y == 0 && logical_cursor_x == 0) {
		// we are at the beginning of the document - nothing to do
		return;
	}

	if (logical_cursor_y == content.size()) {
		// We are at the end of the document
		if (logical_cursor_x > 0) {
			--logical_cursor_x;
		} else {
			--logical_cursor_y;
			logical_cursor_x = content[logical_cursor_y].size();
		}

		constexpr bool only_cursor_moved = true;
		RedrawDocument(only_cursor_moved);
	} else if (logical_cursor_x == 0) {
		// Join two lines
		// XXX check for max line length
		logical_cursor_x = content[logical_cursor_y - 1].size();
		content[logical_cursor_y - 1] += content[logical_cursor_y];
		content.erase(content.begin() + logical_cursor_y);
		--logical_cursor_y;

		content_width = std::max(content_width,
			                 content[logical_cursor_y].size());
		RedrawDocument();
	} else if (content[logical_cursor_y].size() < logical_cursor_x) {
		// Cursor is over line length
		--logical_cursor_x;

		constexpr bool only_cursor_moved = true;
		RedrawDocument(only_cursor_moved);
	} else {
		// Delete single character
		auto &line = content[logical_cursor_y];
		const bool is_width_outdated = (line.size() == content_width);
		line.erase(logical_cursor_x - 1, 1);
		--logical_cursor_x;

		if (is_width_outdated) {
			RecalculateContentWidthSize();
		}
		RedrawDocument();
	}
}

void TuiTextEditor::KeyInsert()
{
	// Switch between inserting and overwriting

	is_insert_mode = !is_insert_mode;
	UpdateCursorShape();
}

void TuiTextEditor::KeyDelete()
{
	// Delete character under cursor
	// Delete selection

	// XXX
}

// ***************************************************************************
// Keyboard input - cursor movement / selection
// ***************************************************************************

void TuiTextEditor::KeyCursorUp(const bool has_shift)
{
	// Cursor movement (without shift) / text selection (with shift)

	if (logical_cursor_y != 0) {
		MaybeMarkSelectionStart(has_shift);
		--logical_cursor_y;
		const bool only_cursor_moved = !MaybeMarkSelectionEnd();

		RedrawDocument(only_cursor_moved);
	}
}

void TuiTextEditor::KeyCursorDown(const bool has_shift)
{
	// Cursor movement (without shift) / text selection (with shift)

	if (logical_cursor_y < content.size()) {
		MaybeMarkSelectionStart(has_shift);
		++logical_cursor_y;
		const bool only_cursor_moved = !MaybeMarkSelectionEnd();

		RedrawDocument(only_cursor_moved);
	}
}

void TuiTextEditor::KeyCursorLeft(const bool has_shift)
{
	// Cursor movement (without shift) / text selection (with shift)

	if (logical_cursor_x > 0) {
		MaybeMarkSelectionStart(has_shift);
		--logical_cursor_x;
		const bool only_cursor_moved = !MaybeMarkSelectionEnd();

		RedrawDocument(only_cursor_moved);
	}
}

void TuiTextEditor::KeyCursorRight(const bool has_shift)
{
	// Cursor movement (without shift) / text selection (with shift)

	MaybeMarkSelectionStart(has_shift);
	++logical_cursor_x;
	const bool only_cursor_moved = !MaybeMarkSelectionEnd();

	RedrawDocument(only_cursor_moved);
}

void TuiTextEditor::KeyControlCursorUp(const bool has_shift)
{
	// Scroll up one line

	if (content_offset_y > 0) {
		MaybeMarkSelectionStart(has_shift);
		--content_offset_y;
		--logical_cursor_y;
		MaybeMarkSelectionEnd();

		// We moved both cursor and viewport
		RedrawDocument();
	} else if (has_shift) {
		KeyCursorUp(has_shift);
	}
}

void TuiTextEditor::KeyControlCursorDown(const bool has_shift)
{
	// Scroll down one line

	if (content_offset_y + view_size_y <= content.size()) {
		MaybeMarkSelectionStart(has_shift);
		++content_offset_y;
		++logical_cursor_y;
		MaybeMarkSelectionEnd();

		// We moved both cursor and viewport
		RedrawDocument();
	} else if (has_shift) {
		KeyCursorDown(has_shift);
	}
}

void TuiTextEditor::KeyControlCursorLeft(const bool has_shift)
{
	// Move cursor one word to the left
	// Select word to the left

	// XXX
}

void TuiTextEditor::KeyControlCursorRight(const bool has_shift)
{
	// Move cursor one word to the right
	// Select word to the right

	// XXX
}

void TuiTextEditor::KeyHome(const bool has_shift)
{
	// Move the cursor to the start of the current line

	MaybeMarkSelectionStart(has_shift);
	logical_cursor_x = 0;
	const bool only_cursor_moved = !MaybeMarkSelectionEnd();

	RedrawDocument(only_cursor_moved);
}

void TuiTextEditor::KeyEnd(const bool has_shift)
{
	// Move the cursor to the end of the current line

	MaybeMarkSelectionStart(has_shift);
	if (logical_cursor_y >= content.size()) {
		logical_cursor_x = 0;
	} else {
		logical_cursor_x = content[logical_cursor_y].size();
	}
	const bool only_cursor_moved = !MaybeMarkSelectionEnd();

	RedrawDocument(only_cursor_moved);
}

void TuiTextEditor::KeyPageUp(const bool has_shift)
{
	// Scroll document one screen upwards

	if (content_offset_y > 0) {
		MaybeMarkSelectionStart(has_shift);
		const auto shift = std::min(view_size_y, content_offset_y);
		content_offset_y -= shift;
		logical_cursor_y -= shift;
		MaybeMarkSelectionEnd();

		// We moved both cursor and viewport
		RedrawDocument();
	} else if (has_shift) {
		KeyControlHome(has_shift);
	}
}

void TuiTextEditor::KeyPageDown(const bool has_shift)
{
	// Scroll document one screen downwards

	if (content_offset_y + view_size_y <= content.size()) {
		MaybeMarkSelectionStart(has_shift);
		const auto remaining = content.size() - view_size_y - content_offset_y + 1;
		const auto shift = std::min(view_size_y, remaining);
		content_offset_y += shift;
		logical_cursor_y += shift;
		MaybeMarkSelectionEnd();

		// We moved both cursor and viewport
		RedrawDocument();
	} else if (has_shift) {
		KeyControlEnd(has_shift);
	}
}

void TuiTextEditor::KeyControlHome(const bool has_shift)
{
	// Move the cursor to the beginning of the document

	if (!content.empty()) {
		MaybeMarkSelectionStart(has_shift);
		logical_cursor_x = 0;
		logical_cursor_y = 0;
		const bool only_cursor_moved = !MaybeMarkSelectionEnd();

		RedrawDocument(only_cursor_moved);
	}
}

void TuiTextEditor::KeyControlEnd(const bool has_shift)
{
	// Move the cursor to the end of the document

	if (!content.empty()) {
		MaybeMarkSelectionStart(has_shift);
		logical_cursor_x = 0;
		logical_cursor_y = content.size();
		const bool only_cursor_moved = !MaybeMarkSelectionEnd();

		RedrawDocument(only_cursor_moved);
	}
}

void TuiTextEditor::KeyControlPageUp(const bool has_shift)
{
	// Scroll document one screen left

	if (content_offset_x > 0) {
		MaybeMarkSelectionStart(has_shift);
		const auto shift = std::min(view_size_x, content_offset_x);
		content_offset_x -= shift;
		logical_cursor_x -= shift;
		MaybeMarkSelectionEnd();

		// We moved both cursor and viewport
		RedrawDocument();
	} else if (has_shift) {
		KeyHome(has_shift);
	}
}

void TuiTextEditor::KeyControlPageDown(const bool has_shift)
{
	// Scroll document one screen right

	// XXX
}

// ***************************************************************************
// Keyboard input - clipboard support
// ***************************************************************************

void TuiTextEditor::KeyControlC()
{
	// Copy to clipboard

	if (!is_content_selected) {
		return;
	}

	// Prepare content to be copied
	std::string content_to_copy = {};
	const std::string newline = "\n";
	if (!is_selection_empty) {
		for (size_t index_y = selection_begin_y; index_y <= selection_end_y; ++index_y) {
			if (index_y >= content.size()) {
				content_to_copy += newline;
				break;
			}

			const auto &line = content[index_y];
			if (index_y == selection_begin_y && index_y == selection_end_y) {
				content_to_copy += line.substr(selection_begin_x, selection_end_x - selection_begin_x + 1);
			} else if (index_y == selection_begin_y) {
				content_to_copy += line.substr(selection_begin_x) + newline;
			} else if (index_y == selection_end_y) {
				if (selection_end_x == 0) {
					content_to_copy += newline;
				} else {
					content_to_copy += line.substr(0, selection_end_x + 1);
				}
			} else {
				content_to_copy += line + newline;
			}
		}
	}

	// Copy to clipboard, unselect content
	clipboard_copy_text_dos(content_to_copy);
	UnselectContent();

	RedrawDocument();
}

void TuiTextEditor::KeyControlV()
{
	// Paste from clipboard

	// XXX handle maximum sizes

	const std::string clipboard_text = clipboard_paste_text_dos();
	if (clipboard_text.empty()) {
		return;
	}

	auto tmp = InputToContent(clipboard_text);

	FillLineToCursor();

	if (logical_cursor_y >= content.size()) {
		// Cursor is below the last line of the document - append
		// clipboard content
		content.insert(content.end(), tmp.begin(), tmp.end());

		logical_cursor_x = 0;
		logical_cursor_y = content.size();
	} else {
		const auto &line = content[logical_cursor_y];
		tmp[0] = line.substr(0, logical_cursor_x) + tmp[0];
		tmp.back() += line.substr(logical_cursor_x);
		content.erase(content.begin() + logical_cursor_y);
		content.insert(content.begin() + logical_cursor_y,
		               tmp.begin(), tmp.end());

		// XXX set cursor position
	}

	RecalculateContentWidthSize();
	RedrawDocument();
}

void TuiTextEditor::KeyControlX()
{
	// XXX cut and move to clipboard
}

// ***************************************************************************
// Keyboard input - XXX create appropriate section
// ***************************************************************************

void TuiTextEditor::KeyEscape()
{
	// Cancel current action

	// XXX if, for example, CTRL+Q mode is entered - cancel it instead

	UnselectContent();
	RedrawDocument();	
}

void TuiTextEditor::KeyControlF()
{
	// XXX find (DOSBox Staging specific shortcut)
}

void TuiTextEditor::KeyControlP()
{
	// XXX insert special character
}

void TuiTextEditor::KeyControlR()
{
	// XXX replace (DOSBox Staging specific shortcut)
}

void TuiTextEditor::KeyControlT()
{
	// XXX delete the rest of the word cursor is on
}



void TuiTextEditor::KeyControlY()
{
	// XXX
}

void TuiTextEditor::KeyF1()
{
	// XXX help
}

void TuiTextEditor::KeyF3()
{
	// XXX find next
}

void TuiTextEditor::KeyF6()
{
	// XXX next window
}

void TuiTextEditor::KeyF8()
{
	// XXX next file
}

void TuiTextEditor::KeyControlF4()
{
	// XXX close second window
}

void TuiTextEditor::KeyControlF6()
{
	// XXX open second window
}

void TuiTextEditor::KeyControlF8()
{
	// XXX resize windows
}

void TuiTextEditor::KeyAltPlus()
{
	// XXX increase size of current window
}

void TuiTextEditor::KeyAltMinus()
{
	// XXX decrease size of current window
}

void TuiTextEditor::AddMessages()
{
	MSG_Add("TUI_TEXTEDITOR_UNTITLED", "UNTITLED");
	MSG_Add("TUI_TEXTEDITOR_READ_ONLY", "read-only");
	MSG_Add("TUI_TEXTEDITOR_LINE", "line");
	MSG_Add("TUI_TEXTEDITOR_COLUMN", "column");
	// XXX
}
