// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MMX_H
#define DOSBOX_MMX_H

typedef union {
	uint64_t q;

#ifndef WORDS_BIGENDIAN
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
#else
	struct {
		uint32_t d1, d0;
	} ud;

	struct {
		int32_t d1, d0;
	} sd;

	struct {
		uint16_t w3, w2, w1, w0;
	} uw;

	struct {
		uint16_t w3, w2, w1, w0;
	} sw;

	struct {
		uint8_t b7, b6, b5, b4, b3, b2, b1, b0;
	} ub;

	struct {
		uint8_t b7, b6, b5, b4, b3, b2, b1, b0;
	} sb;
#endif

} MMX_reg;

extern MMX_reg* reg_mmx[8];
extern MMX_reg* lookupRMregMM[256];

void setFPUTagEmpty();

#endif
