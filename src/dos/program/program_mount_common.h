// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MOUNT_COMMON_H
#define DOSBOX_PROGRAM_MOUNT_COMMON_H

#include "dosbox.h"

#include <string>

#if defined(WIN32)
#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&S_IFMT)==S_IFDIR)
#endif
#endif

extern Bitu ZDRIVE_NUM;

// The minimum length for columns where drives are listed
constexpr int minimum_column_length = 11;

std::string UnmountHelper(char umount);
void AddCommonMountMessages();
void AddMountTypeMessages();

#endif // DOSBOX_PROGRAM_MOUNT_COMMON_H
