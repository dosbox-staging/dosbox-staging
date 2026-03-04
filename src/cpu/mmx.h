// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MMX_H
#define DOSBOX_MMX_H

typedef union {
	uint64_t q;

	struct {
		uint32_t d0, d1;
	} ud;

	struct {
		int32_t d0, d1;
	} sd;

	struct {
		uint16_t w0, w1, w2, w3;
	} uw;

	struct {
		int16_t w0, w1, w2, w3;
	} sw;

	struct {
		uint8_t b0, b1, b2, b3, b4, b5, b6, b7;
	} ub;

	struct {
		int8_t b0, b1, b2, b3, b4, b5, b6, b7;
	} sb;

} MMX_reg;

extern MMX_reg* reg_mmx[8];
extern MMX_reg* lookupRMregMM[256];

void setFPUTagEmpty();

#endif
