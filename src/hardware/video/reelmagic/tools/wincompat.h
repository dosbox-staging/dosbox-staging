// SPDX-FileCopyrightText:  2022-2022 Jon Dennis
// SPDX-License-Identifier: GPL-2.0-or-later

#define __USE_MINGW_ANSI_STDIO 1

#define bzero(PTR, SIZE) memset(PTR, 0, SIZE)

#include <stddef.h>
#include <string.h>

/* from stackoverflow: https://stackoverflow.com/questions/52988769/writing-own-memmem-for-windows */
static inline void *
memmem(void *haystack, size_t haystack_len,
    const void * const needle, const size_t needle_len)
{
    if (haystack == NULL) return NULL; // or assert(haystack != NULL);
    if (haystack_len == 0) return NULL;
    if (needle == NULL) return NULL; // or assert(needle != NULL);
    if (needle_len == 0) return NULL;

    for (char *h = haystack;
            haystack_len >= needle_len;
            ++h, --haystack_len) {
        if (!memcmp(h, needle, needle_len)) {
            return h;
        }
    }
    return NULL;
}

