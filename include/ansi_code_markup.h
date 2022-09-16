/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
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

#ifndef DOSBOX_ANSI_CODE_MARKUP_H
#define DOSBOX_ANSI_CODE_MARKUP_H

#include <string>

/*!
 * \brief Convert marked up strings to strings with ANSI codes
 * 
 * \param str 
 * \return std::string 
 */
std::string convert_ansi_markup(const char* str);

/*!
 * \brief Convert marked up strings to strings with ANSI codes
 * 
 * \param str 
 * \return std::string 
 */
std::string convert_ansi_markup(std::string &str);

#endif // DOSBOX_ANSI_CODE_MARKUP_H
