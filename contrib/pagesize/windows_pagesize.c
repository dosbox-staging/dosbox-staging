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

// Get the memory page size on Windows systems
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// What are the page sizes used by Windows on various processors?
// For many processors, the page size is dictated by the processor,
// but some processors give you a choice.
//  -- Raymond Chen, May 10th, 2021
//
//                         Page size
// Processor               Normal  Large    Reasonable choices
// x86-32 without PAE      4KB     4MB      4KB only
// x86-32 with PAE         4KB     2MB      4KB only
// x86-64                  4KB     2MB      4KB only
// SH-4                    4KB      —       1KB, 4KB
// MIPS                    4KB      —       1KB, 4KB
// PowerPC                 4KB      —       4KB only
// Alpha AXP               8KB      —       8KB, 16KB, 32KB
// Alpha AXP 64            8KB      —       8KB, 16KB, 32KB
// Itanium                 8KB      —       4KB, 8KB
// ARM (AArch32)           4KB     N/A      1KB, 4KB
// ARM64 (AArch64)         4KB     2MB      4KB only
//
// Ref: https://devblogs.microsoft.com/oldnewthing/20210510-00/?p=105200
//
// Usage: ./windows_pagesize
//
// Prints the normal page size in bytes.
// Returns 0 to the shell.
//
#include <windows.h>
#include <sysinfoapi.h>
#include <stdio.h>

int main(void) {
    SYSTEM_INFO system_info = {};

    GetSystemInfo(&system_info);

    printf("%u\n", system_info.dwPageSize);

    return 0;
}
