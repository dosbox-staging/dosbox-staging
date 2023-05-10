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

#ifndef DOSBOX_TUI_ABSTRACT_WIDGET_H
#define DOSBOX_TUI_ABSTRACT_WIDGET_H

#include "tui_object.h"
#include "tui_types.h"

#include <optional>

class TuiApplication;
class TuiScreen;


class TuiAbstractWidget : public TuiObject
{
	friend class TuiApplication;
	friend class TuiScreen;

public:
	TuiAbstractWidget(TuiApplication &application);
	~TuiAbstractWidget() override;

	template<class T, typename... Params>
	std::shared_ptr<T> Add(Params... params) {
		auto widget = std::make_shared<T>(application, params...);
		Add(widget);
		return widget;
	}

	void SetFocus(const std::shared_ptr<TuiAbstractWidget> focus);

	TuiAbstractWidget *GetParent() const { return parent; }

	TuiKeyboardStatus KeyboardStatus() const;
	void RequestBeep();

	// Called on initialization, screen resolution or code page change, etc.
	virtual void OnInit();
	// XXX
	virtual void OnMove();
	// XXX
	virtual void OnResize();
	// Called when widget is requested to redraw it's whole content
	virtual void OnRedraw() = 0;

	virtual void OnInputEvent(const TuiScanCode scan_code);
	virtual void OnInputEventShift();
	virtual void OnInputEventControl();
	virtual void OnInputEventAlt();
	virtual void OnInputEventCapsLock();
	virtual void OnInputEventNumLock();

	void Show();
	void Hide();
	bool IsVisible() const { return is_widget_visible; }

	void SetCell(const TuiCoordinates position, const TuiCell &content);
	void SetCells(const TuiCoordinates position, const TuiCoordinates width,
	              const TuiCell &content);

	void SetPositionXY(const TuiCoordinates position);
	void SetPositionX(const uint8_t position);
	void SetPositionY(const uint8_t position);
	TuiCoordinates GetPositionXY() const { return position; }
	uint8_t GetPositionX() const { return position.x; }
	uint8_t GetPositionY() const { return position.y; }

	void SetPositionLeftFrom(const TuiAbstractWidget &other,
	                         const uint8_t margin = 0);
	void SetPositionLeftFrom(const uint8_t position,
	                         const uint8_t margin = 0);
	void SetPositionRightFrom(const TuiAbstractWidget &other,
	                          const uint8_t margin = 0);
	void SetPositionRightFrom(const uint8_t position,
	                          const uint8_t margin = 0);
	void SetPositionTopFrom(const TuiAbstractWidget &other,
	                        const uint8_t margin = 0);
	void SetPositionTopFrom(const uint8_t position,
	                        const uint8_t margin = 0);
	void SetPositionBottomFrom(const TuiAbstractWidget &other,
	                           const uint8_t margin = 0);
	void SetPositionBottomFrom(const uint8_t position,
	                           const uint8_t margin = 0);

	void SetSizeXY(const TuiCoordinates size);
	void SetSizeX(const uint8_t size);
	void SetSizeY(const uint8_t size);
	TuiCoordinates GetSizeXY() const { return size; }
	uint8_t GetSizeX() const { return size.x; }
	uint8_t GetSizeY() const { return size.y; }

	void SetCursorShape(const TuiCursor cursor_shape);
	TuiCursor GetCursorShape() const { return cursor_shape; }

	void SetCursorPositionXY(const TuiCoordinates position);
	void SetCursorPositionX(const uint8_t position);
	void SetCursorPositionY(const uint8_t position);
	TuiCoordinates GetCursorPositionXY() const { return cursor_position; }
	uint8_t GetCursorPositionX() const { return cursor_position.x; }
	uint8_t GetCursorPositionY() const { return cursor_position.y; }

protected:
	void MarkNeedsCallOnResize();
	void MarkNeedsCallOnMove();
	void MarkNeedsCallOnRedraw();

	void SetMinSizeXY(const TuiCoordinates size);  // 0 = no limit
	void SetMinSizeX(const uint8_t size);
	void SetMinSizeY(const uint8_t size);

	void SetMaxSizeXY(const TuiCoordinates size);  // 0 = no limit
	void SetMaxSizeX(const uint8_t size);
	void SetMaxSizeY(const uint8_t size);

private:
	TuiAbstractWidget(const TuiAbstractWidget&) = delete;
	void operator=(const TuiAbstractWidget&) = delete;

	void Add(std::shared_ptr<TuiAbstractWidget> child);
	void SetParent(TuiAbstractWidget *parent);

	void Update();

	TuiCoordinates GetAdaptedSizeXY(const TuiCoordinates size) const;
	uint8_t GetAdaptedSizeX(const uint8_t size) const;
	uint8_t GetAdaptedSizeY(const uint8_t size) const;

	void MarkSurfaceDirty();
	void MarkDescendantDirty();
	void MarkTreeClean();
	void MarkTreeDirty();

	void MarkDescendantOutdated();

	void PassInputEvent(const TuiScanCode scan_code);
	void PassShiftKeyEvent();
	void PassControlKeyEvent();
	void PassAltKeyEvent();
	void PassCapsLockKeyEvent();
	void PassNumLockKeyEvent();

	std::optional<TuiCell> CalculateCell(const TuiCoordinates position) const;
	TuiCursor CalculateCursorShape() const;
	TuiCoordinates CalculateCursorPosition() const;

	void SetSizeCommon();
	void SetSizePositionVisibilityCommon();
	void SearchTreeForOutdatedWidgets();

	TuiAbstractWidget *parent = nullptr;
	std::list<std::shared_ptr<TuiAbstractWidget>> children = {};
	std::shared_ptr<TuiAbstractWidget> focus = {};

	TuiCoordinates cursor_position = {};
	TuiCursor cursor_shape = TuiCursor::Hidden;

	bool is_widget_visible = true;

	bool needs_call_on_init   = true;
	bool needs_call_on_move   = true;
	bool needs_call_on_resize = true;
	bool needs_call_on_redraw = true;
	// True = at least one descendant needs one of the 'On...()' calls
	bool has_outdated_descendant = false;

	bool is_surface_dirty     = false;
	bool has_dirty_descendant = false;

	std::vector<std::vector<TuiCell>> surface = {};

	TuiCoordinates min_size = {};
	TuiCoordinates max_size = {}; // coordinate == 0 -> no limit
	TuiCoordinates size     = {};
	TuiCoordinates position = {};
};

#endif // DOSBOX_TUI_ABSTRACT_WIDGET_H
