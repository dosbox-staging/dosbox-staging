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

#include "tui_label.h"

#include "checks.h"
#include "logging.h"
#include "math_utils.h"

#include <cassert>

CHECK_NARROWING();


TuiLabel::TuiLabel(TuiApplication &application) :
	TuiAbstractWidget(application)
{
	SetMinSizeXY({0, 1});
	SetMaxSizeXY({0, 1});
}

TuiLabel::~TuiLabel()
{
}

void TuiLabel::OnRedraw()
{
	const uint16_t limit_x = GetSizeX();
	const auto text_size = text.size();

	for (uint16_t idx = 0; idx < limit_x; idx++) {
		if (idx < margin_left || idx >= margin_left + text_size) {
			SetCell({idx, 0}, { ' ', attributes });
		} else if (idx < margin_left + text_size) {
			SetCell({idx, 0}, { text[idx - margin_left], attributes });
		}
	}
}

void TuiLabel::SetText(const std::string &new_text)
{
	if (text != new_text) {
		text = new_text;
		SetLabelCommon();
	}
}

void TuiLabel::SetAttributes(const uint8_t new_attributes)
{
	if (attributes != new_attributes) {
		attributes = new_attributes;
		SetLabelCommon();
	}
}

void TuiLabel::SetMarginLeft(const uint8_t new_margin)
{
	if (margin_left != new_margin) {
		margin_left = new_margin;
		SetLabelCommon();
	}
}

void TuiLabel::SetMarginRight(const uint8_t new_margin)
{
	if (margin_right != new_margin) {
		margin_right = new_margin;
		SetLabelCommon();
	}
}

void TuiLabel::SetMarginLeftRight(const uint8_t new_margin)
{
	if ((margin_left != new_margin) || (margin_right != new_margin)) {
		margin_left  = new_margin;
		margin_right = new_margin;
		SetLabelCommon();
	}
}

void TuiLabel::SetLabelCommon()
{
	const auto size = clamp_to_uint8(margin_left + text.size() + margin_right);

	SetMinSizeX(size);
	SetMaxSizeX(size);

	MarkNeedsCallOnRedraw();
}
