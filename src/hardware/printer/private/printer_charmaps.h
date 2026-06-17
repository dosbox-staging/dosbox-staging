// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRIVATE_PRINTER_CHARMAPS_H
#define DOSBOX_PRIVATE_PRINTER_CHARMAPS_H

#include "misc/types.h"

typedef struct {
	uint64_t codepage;
	const uint16_t* map;
} Charmap;

extern const Charmap charmap[];
extern const uint16_t codepages[15];
extern const uint16_t int_char_sets[15][12];

#endif // DOSBOX_PRIVATE_PRINTER_CHARMAPS_H
