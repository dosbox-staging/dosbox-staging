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

// Get the memory page size on POSIX systems
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Ref: https://pubs.opengroup.org/onlinepubs/9699919799/
//
// Usage: ./posix_pagesize
//
// On success, prints the page size in bytes and return 0 to the shell.
// On failure, prints an error messages and return 1 to the shell.
//
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
	// Try two queries
	long pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize == -1) {
		pagesize = sysconf(_SC_PAGE_SIZE);
	}

	// Print the results
	if (pagesize > 0) {
		printf("%ld\n", pagesize);
	} else {
		printf("Error getting page size: %s\n", strerror(errno));
	}

	return (pagesize > 0) ? 0 : 1;
}
