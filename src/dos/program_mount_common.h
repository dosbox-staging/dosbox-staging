/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
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

#ifndef DOSBOX_PROGRAM_MOUNT_COMMON_H
#define DOSBOX_PROGRAM_MOUNT_COMMON_H

#include "dosbox.h"

#if defined(WIN32)
#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&S_IFMT)==S_IFDIR)
#endif
#endif

extern Bitu ZDRIVE_NUM;

// The minimum length for columns where drives are listed
constexpr int minimum_column_length = 11;

const char *UnmountHelper(char umount);
void AddCommonMountMessages();
void AddMountTypeMessages();

#endif // DOSBOX_PROGRAM_MOUNT_COMMON_H
