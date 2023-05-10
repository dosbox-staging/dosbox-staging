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

#include "tui_scrollbar.h"

#include <cassert>

#include "checks.h"
#include "logging.h"

CHECK_NARROWING();

constexpr uint8_t min_scrollbar_size = 5;


TuiAbstractScrollBar::TuiAbstractScrollBar(TuiApplication &application) :
	TuiAbstractWidget(application)
{
}

TuiAbstractScrollBar::~TuiAbstractScrollBar()
{
}

TuiScrollBarH::TuiScrollBarH(TuiApplication &application) :
	TuiAbstractScrollBar(application)
{
	SetMinSizeXY({min_scrollbar_size, 1});
	SetMaxSizeY(1);
}

TuiScrollBarH::~TuiScrollBarH()
{
}

TuiScrollBarV::TuiScrollBarV(TuiApplication &application) :
	TuiAbstractScrollBar(application)
{
	SetMinSizeXY({1, min_scrollbar_size});
	SetMaxSizeX(1);
}

TuiScrollBarV::~TuiScrollBarV()
{
}

void TuiScrollBarH::SetScrollBarSize(const uint8_t size)
{
	SetSizeXY({size, 1});
}

void TuiScrollBarV::SetScrollBarSize(const uint8_t size)
{
	SetSizeXY({1, size});
}

void TuiAbstractScrollBar::OnInit()
{
	scrollbar_color = TuiColor::Black + TuiBgColor::White;

	filling    = { 0x20, scrollbar_color };
	background = { 0xb1, scrollbar_color }; // XXX char code check availability
}

void TuiScrollBarH::OnInit()
{
	TuiAbstractScrollBar::OnInit();

	arrow_1 = { 0x11, scrollbar_color }; // arrow left
	arrow_2 = { 0x10, scrollbar_color }; // arrow right
}

void TuiScrollBarV::OnInit()
{
	TuiAbstractScrollBar::OnInit();

	arrow_1 = { 0x1e, scrollbar_color }; // arrow up
	arrow_2 = { 0x1f, scrollbar_color }; // arrow down
}

void TuiAbstractScrollBar::OnRedraw()
{
	SetScrollBarCell(0, arrow_1);
	SetScrollBarCell(GetScrollBarSize() - 1, arrow_2);

	RedrawScrollBar();
}

uint8_t TuiScrollBarH::GetScrollBarSize() const
{
	return GetSizeX();
}

uint8_t TuiScrollBarV::GetScrollBarSize() const
{
	return GetSizeY();
}

void TuiScrollBarH::SetScrollBarCell(const uint8_t position,
                                     const TuiCell cell)
{
	SetCell({position, 0}, cell);
}

void TuiScrollBarV::SetScrollBarCell(const uint8_t position,
                                     const TuiCell cell)
{
	SetCell({0, position}, cell);
}

void TuiScrollBarH::SetScrollBarCells(const uint8_t position,
                                      const uint8_t width,
                                      const TuiCell cell)
{
	SetCells({position, 0}, {width, 1}, cell);
}

void TuiScrollBarV::SetScrollBarCells(const uint8_t position,
                                      const uint8_t width,
	                              const TuiCell cell)
{
	SetCells({0, position}, {1, width}, cell);
}

void TuiAbstractScrollBar::SetScrollBarParams(const size_t new_total_size,
	                                      const size_t new_view_size,
	                                      const size_t new_view_offset)
{
	// Check if there is anything to do
	if (new_total_size == total_size && new_view_offset == view_offset &&
	    new_view_size == view_size) {
		return;
	}

	// Store new parameters
    	view_offset = new_view_offset;
    	total_size  = new_total_size;
        view_size   = new_view_size;

        // Store old calculation results
	uint8_t old_cells_filled = cells_filled;
	uint8_t old_bar_offset   = bar_offset;

        // Retrieve maximum size of the bar
	const uint8_t widget_size = GetScrollBarSize();
	assert(widget_size >= 5);
	const uint8_t max_bar_size = widget_size - 2;

	// Calculate bar size as percent of total length
	const auto tmp1 = std::max(std::min(view_size, total_size),
	                           static_cast<size_t>(1));
	const auto tmp2 = std::max(total_size, static_cast<size_t>(1));
	const float proportion_filled = static_cast<float>(tmp1) /
                                        static_cast<float>(tmp2);

	// Calculate bar size in cells
	const auto tmp3 = std::lround(proportion_filled * max_bar_size);
	cells_filled = std::min(max_bar_size, static_cast<uint8_t>(tmp3));
	if (view_offset + view_size > total_size) {
		// Make sure there is some buffer for scrolling
		cells_filled = std::min(cells_filled,
			static_cast<uint8_t>(max_bar_size - 1));
	}
	cells_filled = std::max(cells_filled, static_cast<uint8_t>(1));
	const uint8_t bar_cells_empty = max_bar_size - cells_filled;

	// Calculate bar offset
	bar_offset = 0;
	if (view_offset + view_size >= total_size) {
		bar_offset = bar_cells_empty;
	} else if (view_offset != 0) {
		const auto tmp1 = static_cast<float>(view_offset) /
		                  static_cast<float>(total_size - view_size);
		const auto proportion = std::clamp(tmp1, 0.0f, 1.0f);
		const auto tmp2 = std::lround(proportion * bar_cells_empty);
		bar_offset = static_cast<uint8_t>(tmp2);
	}

	// Redraw scroll bar if needed
	if (old_cells_filled != cells_filled ||
	    old_bar_offset != bar_offset) {
		RedrawScrollBar();
	}
}

void TuiAbstractScrollBar::RedrawScrollBar()
{
	SetScrollBarCells(1, GetScrollBarSize() - 2, background);
	SetScrollBarCells(1 + bar_offset, cells_filled, filling);
}
