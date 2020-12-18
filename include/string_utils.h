/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2019-2020  The dosbox-staging team
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

#ifndef DOSBOX_STRING_UTILS_H
#define DOSBOX_STRING_UTILS_H

#include <cstdarg>
#include <cstring>
#include <string>

#include "compiler.h"


template <size_t N>
int safe_sprintf(char (&dst)[N], const char *fmt, ...)
        GCC_ATTRIBUTE(format(printf, 2, 3));

template <size_t N>
int safe_sprintf(char (&dst)[N], const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const int ret = vsnprintf(dst, N, fmt, args);
	va_end(args);
	return ret;
}

template <size_t N>
bool starts_with(const char (&pfx)[N], const char *str) noexcept
{
	return (strncmp(pfx, str, N - 1) == 0);
}

template <size_t N>
bool starts_with(const char (&pfx)[N], const std::string &str) noexcept
{
	return (strncmp(pfx, str.c_str(), N - 1) == 0);
}

#endif
