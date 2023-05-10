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

#include "tui_abstractwidget.h"

#include "tui_application.h"

#include "checks.h"
#include "logging.h"
#include "math_utils.h"

#include <algorithm>
#include <cassert>

CHECK_NARROWING();


TuiAbstractWidget::TuiAbstractWidget(TuiApplication &application) :
	TuiObject(application)
{
}

TuiAbstractWidget::~TuiAbstractWidget()
{
}

void TuiAbstractWidget::Add(std::shared_ptr<TuiAbstractWidget> child)
{
	children.push_front(child);
	child->SetParent(this);
}

void TuiAbstractWidget::SetParent(TuiAbstractWidget *new_parent)
{
	assert(!parent);

	parent = new_parent;
	parent->MarkDescendantDirty();
	parent->MarkDescendantOutdated();
}

void TuiAbstractWidget::SetFocus(const std::shared_ptr<TuiAbstractWidget> new_focus)
{
	// XXX mark 'OnFocus()' call needed, mark screen refresh needed

	if (new_focus == nullptr) {
		focus = new_focus;
		return;
	}

	if (std::find(children.begin(), children.end(), new_focus) == children.end()) {
		assert(false);
		focus = nullptr;
		return;
	}

	focus = new_focus;
}

void TuiAbstractWidget::Update()
{
	auto should_call_on_init = [this]() {
		return is_widget_visible && needs_call_on_init;
	};

	auto should_call_on_resize = [this]() {
		return is_widget_visible && needs_call_on_resize;
	};

	auto should_call_on_move = [this]() {
		return is_widget_visible && needs_call_on_move;
	};

	auto is_widget_update_needed = [&]() {
		return should_call_on_init() ||
		       should_call_on_resize() ||
	               should_call_on_move();
	};

	auto is_widget_redraw_needed = [this]() {
		return is_widget_visible && needs_call_on_redraw;
	};

	auto is_update_children_needed = [this]() {
		return is_widget_visible && has_outdated_descendant;
	};

	while (is_widget_update_needed() || is_widget_redraw_needed() ||
	       is_update_children_needed()) {

		// Update current widget

		if (should_call_on_init()) {
			needs_call_on_init = false;
			OnInit();
			// Dummy events, just to trigger posible actions
			OnInputEventCapsLock();
			OnInputEventNumLock();
		}

		if (should_call_on_move()) {
			needs_call_on_move = false;
			OnMove();
		}

		if (should_call_on_resize()) {
			needs_call_on_resize = false;
			OnResize();
		}

		// Do not go further if widget update is still needed
		if (is_widget_update_needed()) {
			continue;
		}

		// Update child widgets
		if (is_update_children_needed()) {
			has_outdated_descendant = false;
			for (auto &child : children) {
				assert(child);
				child->Update();
			}
			// Redraw is costly - check all the conditions once
			// again, to be sure
			continue;
		}

		// Redraw widget surface - last step
		if (is_widget_redraw_needed()) {
			needs_call_on_redraw = false;
			OnRedraw();
		}
	}
}

void TuiAbstractWidget::MarkSurfaceDirty()
{
	if (is_surface_dirty) {
		return;
	}

	is_surface_dirty = true;
	if (is_widget_visible && parent) {
		parent->MarkDescendantDirty();
	}
}

void TuiAbstractWidget::MarkDescendantDirty()
{
	if (!has_dirty_descendant && is_widget_visible) {
		has_dirty_descendant = true;
		if (parent) {
			parent->MarkDescendantDirty();
		}
	}
}

void TuiAbstractWidget::MarkTreeClean()
{
	if (is_widget_visible && (is_surface_dirty || has_dirty_descendant)) {
		is_surface_dirty = false;
		if (has_dirty_descendant) {
			for (const auto &child : children) {
				assert(child);
				child->MarkTreeClean();
			}
		}
		has_dirty_descendant = false;
	}
}

void TuiAbstractWidget::MarkTreeDirty()
{
	if (is_widget_visible) {
		MarkSurfaceDirty();
		has_dirty_descendant = !children.empty();
		for (const auto &child : children) {
			assert(child);
			child->MarkTreeDirty();
		}
	}
}

void TuiAbstractWidget::MarkNeedsCallOnResize()
{
	if (needs_call_on_resize) {
		return;
	}

	needs_call_on_resize = true;
	if (is_widget_visible && parent) {
		parent->MarkDescendantOutdated();
	}
}

void TuiAbstractWidget::MarkNeedsCallOnMove()
{
	if (needs_call_on_move) {
		return;
	}

	needs_call_on_move = true;
	if (is_widget_visible && parent) {
		parent->MarkDescendantOutdated();
	}
}

void TuiAbstractWidget::MarkNeedsCallOnRedraw()
{
	if (needs_call_on_redraw) {
		return;
	}

	needs_call_on_redraw = true;
	if (is_widget_visible && parent) {
		parent->MarkDescendantOutdated();
	}
}

void TuiAbstractWidget::MarkDescendantOutdated()
{
	if (!has_outdated_descendant && is_widget_visible) {
		has_outdated_descendant = true;
		if (parent) {
			parent->MarkDescendantOutdated();
		}
	}
}

void TuiAbstractWidget::PassInputEvent(const TuiScanCode scan_code)
{
	if (focus != nullptr) {
		focus->PassInputEvent(scan_code);
	} else {
		OnInputEvent(scan_code);
	}
}

void TuiAbstractWidget::PassShiftKeyEvent()
{
	OnInputEventShift();

	for (auto &child : children) {
		child->PassShiftKeyEvent();
	}
}

void TuiAbstractWidget::PassControlKeyEvent()
{
	OnInputEventControl();

	for (auto &child : children) {
		child->PassControlKeyEvent();
	}
}

void TuiAbstractWidget::PassAltKeyEvent()
{
	OnInputEventAlt();

	for (auto &child : children) {
		child->PassAltKeyEvent();
	}
}

void TuiAbstractWidget::PassCapsLockKeyEvent()
{
	OnInputEventCapsLock();

	for (auto &child : children) {
		child->PassCapsLockKeyEvent();
	}
}

void TuiAbstractWidget::PassNumLockKeyEvent()
{
	OnInputEventNumLock();

	for (auto &child : children) {
		child->PassNumLockKeyEvent();
	}
}

void TuiAbstractWidget::SetMinSizeXY(const TuiCoordinates new_size)
{
	if (min_size != new_size) {
		min_size = new_size;

		if (max_size.x != 0) {
			max_size.x = std::max(min_size.x, max_size.x);
		}
		if (max_size.y != 0) {
			max_size.y = std::max(min_size.y, max_size.y);
		}

		SetSizeXY(size);
	}
}

void TuiAbstractWidget::SetMinSizeX(const uint8_t new_size)
{
	if (min_size.x != new_size) {
		min_size.x = new_size;

		if (max_size.x != 0) {
			max_size.x = std::max(min_size.x, max_size.x);
		}

		SetSizeX(size.x);
	}
}

void TuiAbstractWidget::SetMinSizeY(const uint8_t new_size)
{
	if (min_size.y != new_size) {
		min_size.y = new_size;

		if (max_size.y != 0) {
			max_size.y = std::max(min_size.y, max_size.y);
		}

		SetSizeY(size.y);
	}
}

void TuiAbstractWidget::SetMaxSizeXY(const TuiCoordinates new_size)
{
	if (max_size != new_size) {
		max_size = new_size;

		if (max_size.x != 0) {
			min_size.x = std::min(min_size.x, max_size.x);
		}
		if (max_size.y != 0) {
			min_size.y = std::min(min_size.y, max_size.y);
		}

		SetSizeXY(size);
	}
}

void TuiAbstractWidget::SetMaxSizeX(const uint8_t new_size)
{
	if (max_size.x != new_size) {
		max_size.x = new_size;

		if (max_size.x != 0) {
			min_size.x = std::min(min_size.x, max_size.x);
		}

		SetSizeX(size.x);
	}
}

void TuiAbstractWidget::SetMaxSizeY(const uint8_t new_size)
{
	if (max_size.y != new_size) {
		max_size.y = new_size;

		if (max_size.y != 0) {
			min_size.y = std::min(min_size.y, max_size.y);
		}

		SetSizeY(size.y);
	}
}

TuiCoordinates TuiAbstractWidget::GetAdaptedSizeXY(const TuiCoordinates size) const
{
	return { GetAdaptedSizeX(size.x), GetAdaptedSizeY(size.y) };
}

uint8_t TuiAbstractWidget::GetAdaptedSizeX(const uint8_t size) const
{
	const uint8_t tmp = std::max(min_size.x, size);
	if (max_size.x == 0) {
		return tmp;
	}
	return std::min(tmp, max_size.x);
}

uint8_t TuiAbstractWidget::GetAdaptedSizeY(const uint8_t size) const
{
	const uint8_t tmp = std::max(min_size.y, size);
	if (max_size.y == 0) {
		return tmp;
	}
	return std::min(tmp, max_size.y);
}

TuiKeyboardStatus TuiAbstractWidget::KeyboardStatus() const
{
	return application.KeyboardStatus();
}

void TuiAbstractWidget::RequestBeep()
{
	// XXX
}

void TuiAbstractWidget::OnInit()
{
}

void TuiAbstractWidget::OnMove()
{
}

void TuiAbstractWidget::OnResize()
{
}

void TuiAbstractWidget::OnInputEvent([[maybe_unused]] const TuiScanCode scan_code)
{
}

void TuiAbstractWidget::OnInputEventShift()
{
}

void TuiAbstractWidget::OnInputEventControl()
{
}

void TuiAbstractWidget::OnInputEventAlt()
{
}

void TuiAbstractWidget::OnInputEventCapsLock()
{
}

void TuiAbstractWidget::OnInputEventNumLock()
{
}

void TuiAbstractWidget::Show()
{
	if (!is_widget_visible) {
		is_widget_visible = true;

		has_dirty_descendant    = !children.empty();
		has_outdated_descendant = !children.empty();

		SetSizePositionVisibilityCommon();
		SearchTreeForOutdatedWidgets();
	}
}

void TuiAbstractWidget::Hide()
{
	if (is_widget_visible) {
		is_widget_visible = false;
		is_surface_dirty  = false;

		has_dirty_descendant    = false;
		has_outdated_descendant = false;

		if (parent) {
			parent->MarkTreeDirty();
		}
	}
}

void TuiAbstractWidget::SetCell(const TuiCoordinates position, const TuiCell &content)
{
	if (position.x > size.x || position.y > size.y) {
		return;
	}

	surface[position.x][position.y] = content;
	MarkSurfaceDirty();
}

void TuiAbstractWidget::SetCells(const TuiCoordinates position,
                                 const TuiCoordinates width,
                                 const TuiCell &content)
{
	assert(width.x > 0);
	assert(width.y > 0);

	const auto max_x = std::min(static_cast<uint16_t>(position.x + width.x),
	                            static_cast<uint16_t>(size.x));
	const auto max_y = std::min(static_cast<uint16_t>(position.y + width.y),
	                            static_cast<uint16_t>(size.y));

	for (uint16_t idx_x = position.x; idx_x < max_x; ++idx_x) {
		for (uint16_t idx_y = position.y; idx_y < max_y; ++idx_y) {
			surface[idx_x][idx_y] = content;
		}
	}

	MarkSurfaceDirty();
}

std::optional<TuiCell> TuiAbstractWidget::CalculateCell(const TuiCoordinates position) const
{
	std::optional<TuiCell> result = {};
	if (position.x > size.x || position.y > size.y) {
		assert(false);
		return result;
	}

	for (const auto &child : children) {
		if (!child->is_widget_visible ||
		    position.x < child->position.x ||
		    position.y < child->position.y ||
		    position.x >= child->position.x + child->size.x ||
		    position.y >= child->position.y + child->size.y)  {
			continue;
		}

		return child->CalculateCell({ position.x - child->position.x,
		                              position.y - child->position.y });
	}

	if (is_surface_dirty) {
		result.emplace(surface[position.x][position.y]);
	}

	return result;
}

TuiCursor TuiAbstractWidget::CalculateCursorShape() const
{
	if (focus != nullptr) {
		return focus->CalculateCursorShape();
	}

	return cursor_shape;
}

TuiCoordinates TuiAbstractWidget::CalculateCursorPosition() const
{
	if (focus != nullptr) {
		const auto cursor_position = focus->CalculateCursorPosition();
		return cursor_position + focus->position;
	}

	return cursor_position;
}

void TuiAbstractWidget::SetPositionXY(const TuiCoordinates new_position)
{
	if (position != new_position) {
		position = new_position;
		MarkNeedsCallOnMove();
		SetSizePositionVisibilityCommon();
	}
}

void TuiAbstractWidget::SetPositionX(const uint8_t new_position)
{
	if (position.x != new_position) {
		position.x = new_position;
		MarkNeedsCallOnMove();
		SetSizePositionVisibilityCommon();
	}
}

void TuiAbstractWidget::SetPositionY(const uint8_t new_position)
{
	if (position.y != new_position) {
		position.y = new_position;
		MarkNeedsCallOnMove();
		SetSizePositionVisibilityCommon();
	}
}

void TuiAbstractWidget::SetPositionLeftFrom(const TuiAbstractWidget &other,
                                            const uint8_t margin)
{
	SetPositionX(clamp_to_uint8(other.GetPositionX() - GetSizeX() - margin));
}

void TuiAbstractWidget::SetPositionLeftFrom(const uint8_t position,
                                            const uint8_t margin)
{
	SetPositionX(clamp_to_uint8(position - GetSizeX() - margin));
}

void TuiAbstractWidget::SetPositionRightFrom(const TuiAbstractWidget &other,
                                             const uint8_t margin)
{
	SetPositionX(clamp_to_uint8(other.GetPositionX() + other.GetSizeX() + margin));
}

void TuiAbstractWidget::SetPositionRightFrom(const uint8_t position,
                                             const uint8_t margin)
{
	SetPositionX(clamp_to_uint8(position + margin));
}

void TuiAbstractWidget::SetPositionTopFrom(const TuiAbstractWidget &other,
                                           const uint8_t margin)
{
	SetPositionY(clamp_to_uint8(other.GetPositionY() - GetSizeY() - margin));
}

void TuiAbstractWidget::SetPositionTopFrom(const uint8_t position,
                                           const uint8_t margin)
{
	SetPositionY(clamp_to_uint8(position - GetSizeY() - margin));
}

void TuiAbstractWidget::SetPositionBottomFrom(const TuiAbstractWidget &other,
                                              const uint8_t margin)
{
	SetPositionY(clamp_to_uint8(other.GetPositionY() + other.GetSizeY() + margin));
}

void TuiAbstractWidget::SetPositionBottomFrom(const uint8_t position,
                                              const uint8_t margin)
{
	SetPositionY(clamp_to_uint8(position + margin));
}

void TuiAbstractWidget::SetSizeXY(TuiCoordinates new_size)
{
	const auto adapted = GetAdaptedSizeXY(new_size);

	if (size != adapted) {
		size = adapted;
		SetSizeCommon();
	}
}

void TuiAbstractWidget::SetSizeX(const uint8_t new_size)
{
	const auto adapted = GetAdaptedSizeX(new_size);
	if (size.x != adapted) {
		size.x = adapted;
		SetSizeCommon();
	}
}

void TuiAbstractWidget::SetSizeY(const uint8_t new_size)
{
	const auto adapted = GetAdaptedSizeY(new_size);
	if (size.y != adapted) {
		size.y = adapted;
		SetSizeCommon();
	}
}

void TuiAbstractWidget::SetCursorShape(const TuiCursor new_cursor_shape)
{
	cursor_shape = new_cursor_shape;
}

void TuiAbstractWidget::SetCursorPositionXY(const TuiCoordinates position)
{
	// XXX clip coordinates
	cursor_position = position;
}

void TuiAbstractWidget::SetCursorPositionX(const uint8_t position)
{
	// XXX clip coordinates
	cursor_position.x = position;
}

void TuiAbstractWidget::SetCursorPositionY(const uint8_t position)
{
	// XXX clip coordinates
	cursor_position.y = position;
}

void TuiAbstractWidget::SetSizeCommon()
{
	surface.resize(size.x);
	for (auto &entry : surface) {
		entry.resize(size.y);
	}

	// XXX clip cursor position

	MarkNeedsCallOnResize();
	MarkNeedsCallOnRedraw();

	SetSizePositionVisibilityCommon();
}

void TuiAbstractWidget::SetSizePositionVisibilityCommon()
{
	if (is_widget_visible) {
		if (parent) {
			parent->MarkTreeDirty();
		} else {
			MarkTreeDirty();
		}
	}
}

void TuiAbstractWidget::SearchTreeForOutdatedWidgets()
{
	if (needs_call_on_init || needs_call_on_move || needs_call_on_resize ||
	    needs_call_on_redraw || has_outdated_descendant) {
		if (parent) {
			parent->MarkDescendantOutdated();
		}
	}

	for (auto &child : children) {
		child->SearchTreeForOutdatedWidgets();
	}
}
