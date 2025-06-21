// SPDX-FileCopyrightText:  2023-2023 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

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
