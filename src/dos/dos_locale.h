/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_DOS_LOCALE_H
#define DOSBOX_DOS_LOCALE_H

#include "setup.h"

#include <cstdint>
#include <string>

constexpr uint16_t DefaultCodePage437 = 437;

std::string DOS_GenerateListCountriesMessage();

bool DOS_SetCountry(const uint16_t country_id);
uint16_t DOS_GetCountry();
void DOS_RefreshCountryInfo(const bool reason_keyboard_layout = false);

std::string DOS_GetBundledCodePageFileName(const uint16_t code_page);

uint16_t DOS_GetCodePageFromCountry(const uint16_t country);

std::string DOS_CheckLanguageToLayoutException(const std::string& language_code);

uint16_t DOS_GetDefaultCountry();

bool DOS_GetCountryFromLayout(const std::string& layout, uint16_t& country);

std::string DOS_GetLayoutFromHost();

// Lifecycle

void DOS_Locale_Init(Section* sec);
void DOS_Locale_ShutDown(Section* sec);

// We need a separate function to support '--list-countries' command line switch
// (and possibly others in the future) - it needs translated strings, but does
// not initialize DOSBox fully.
void DOS_Locale_AddMessages();

#endif
