/*
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

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
