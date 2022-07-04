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

#ifndef DOSBOX_ENUM_H
#define DOSBOX_ENUM_H

// A reflective enum gap-filler until it's standardized in the langauge.
//
// Usage: https://github.com/Neargye/magic_enum/blob/master/README.md
//
// Why prefer 'Magic Enum C++' over others, like 'Better Enum'?
//
// Magic Enum C++ provides a C++17-and-newer augmentation of enum
// specification, which means its primarily easy to read for maintainers
// and secondarily (should be) easy to remove/upgrade when the language
// supports similar calls.
//
// See more here: https://github.com/aantron/better-enums/issues/78

#include "../src/libs/magic_enum/include/magic_enum.hpp"
#include "../src/libs/magic_enum/include/magic_enum_fuse.hpp"

// We're interested in native-feeling functionality and don't want
// "magic_enum::" prefixes poluting each point-of-use.
//
using namespace magic_enum;

#endif
