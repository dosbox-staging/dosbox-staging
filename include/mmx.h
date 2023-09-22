/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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

	Bit64u q;

#ifndef WORDS_BIGENDIAN
	struct {
		Bit32u d0,d1;
	} ud;

	struct {
		Bit32s d0,d1;
	} sd;

	struct {
		Bit16u w0,w1,w2,w3;
	} uw;

	struct {
		Bit16s w0,w1,w2,w3;
	} sw;

	struct {
		Bit8u b0,b1,b2,b3,b4,b5,b6,b7;
	} ub;

	struct {
		Bit8s b0,b1,b2,b3,b4,b5,b6,b7;
	} sb;
#else
	struct {
		Bit32u d1,d0;
	} ud;

	struct {
		Bit32s d1,d0;
	} sd;

	struct {
		Bit16u w3,w2,w1,w0;
	} uw;

	struct {
		Bit16u w3,w2,w1,w0;
	} sw;

	struct {
		Bit8u b7,b6,b5,b4,b3,b2,b1,b0;
	} ub;

	struct {
		Bit8u b7,b6,b5,b4,b3,b2,b1,b0;
	} sb;
#endif

} MMX_reg;

extern MMX_reg reg_mmx[8];
extern MMX_reg * lookupRMregMM[256];


Bit8s  SaturateWordSToByteS(Bit16s value);
Bit16s SaturateDwordSToWordS(Bit32s value);
Bit8u  SaturateWordSToByteU(Bit16s value);
Bit16u SaturateDwordSToWordU(Bit32s value);

void   setFPU(Bit16u tag);

#endif