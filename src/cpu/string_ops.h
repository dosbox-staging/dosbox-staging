// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_STRING_OPS_H
#define DOSBOX_STRING_OPS_H

// string instructions
enum STRING_OP {
	R_OUTSB = 0,
	R_OUTSW,
	R_OUTSD,
	R_INSB = 4,
	R_INSW,
	R_INSD,
	R_MOVSB = 8,
	R_MOVSW,
	R_MOVSD,
	R_LODSB = 12,
	R_LODSW,
	R_LODSD,
	R_STOSB = 16,
	R_STOSW,
	R_STOSD,
	R_SCASB = 20,
	R_SCASW,
	R_SCASD,
	R_CMPSB = 24,
	R_CMPSW,
	R_CMPSD,
};

#endif
