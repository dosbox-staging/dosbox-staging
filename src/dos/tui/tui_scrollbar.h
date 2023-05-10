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

#ifndef DOSBOX_TUI_SCROLLBAR_H
#define DOSBOX_TUI_SCROLLBAR_H

#include "tui_abstractwidget.h"

class TuiAbstractScrollBar : public TuiAbstractWidget
{
public:
	TuiAbstractScrollBar(TuiApplication &application);
	~TuiAbstractScrollBar() override;

	void OnInit() override;
	void OnRedraw() override;

	virtual void SetScrollBarSize(const uint8_t size) = 0;
	void SetScrollBarParams(const size_t total_size,
	                        const size_t view_size,
	                        const size_t view_offset);

protected:

	virtual uint8_t GetScrollBarSize() const = 0;

	virtual void SetScrollBarCell(const uint8_t position,
	                              const TuiCell cell) = 0;
	virtual void SetScrollBarCells(const uint8_t position,
	                               const uint8_t width,
                                       const TuiCell cell) = 0;
	uint8_t scrollbar_color = 0;
	TuiCell arrow_1 = {};
	TuiCell arrow_2 = {};
	TuiCell filling    = {};
	TuiCell background = {};

private:
	size_t total_size  = 0;
	size_t view_size   = 0;
	size_t view_offset = 0;

	uint8_t cells_filled = 0;
	uint8_t bar_offset   = 0;

	void RedrawScrollBar();
};

class TuiScrollBarH : public TuiAbstractScrollBar
{
public:
	TuiScrollBarH(TuiApplication &application);
	~TuiScrollBarH() override;	

	void OnInit() override;

	void SetScrollBarSize(const uint8_t size) override;

protected:

	uint8_t GetScrollBarSize() const override;

	void SetScrollBarCell(const uint8_t position,
	                      const TuiCell cell) override;
	void SetScrollBarCells(const uint8_t position,
	                       const uint8_t width,
                               const TuiCell cell) override;
};

class TuiScrollBarV : public TuiAbstractScrollBar
{
public:
	TuiScrollBarV(TuiApplication &application);
	~TuiScrollBarV() override;	

	void OnInit() override;

	void SetScrollBarSize(const uint8_t size) override;

protected:

	uint8_t GetScrollBarSize() const override;

	void SetScrollBarCell(const uint8_t position,
	                      const TuiCell cell) override;
	void SetScrollBarCells(const uint8_t position,
	                       const uint8_t width,
                               const TuiCell cell) override;
};

#endif // DOSBOX_TUI_SCROLLBAR_H
