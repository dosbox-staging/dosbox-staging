// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "bit_types_compat.h"
#include "misc/types.h"

typedef struct {
	Bitu codepage;
	const Bit16u* map;
} CHARMAP;

extern const CHARMAP charmap[];
extern const Bit16u codepages[15];
extern const Bit16u intCharSets[15][12];
