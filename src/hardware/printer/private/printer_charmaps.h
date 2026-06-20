// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRIVATE_PRINTER_CHARMAPS_H
#define DOSBOX_PRIVATE_PRINTER_CHARMAPS_H

#include <optional>

#include "misc/types.h"

struct Charmap {
	uint16_t codepage   = 0;
	const uint16_t* map = nullptr;
};

extern const Charmap charmap[];

// Looks up the codepage ID assigned to the (d2, d3) character table identifier
// from ESC ( t. Returns std::nullopt for tables we don't register. See
// escp2ref.pdf C-73 / C-74 for the full spec table.
std::optional<uint16_t> LookupCharTableCodepage(uint8_t d2, uint8_t d3);

extern const uint16_t int_char_sets[15][12];

#endif // DOSBOX_PRIVATE_PRINTER_CHARMAPS_H
