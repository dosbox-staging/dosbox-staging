/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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

#ifndef _RENDER_SCALERS_H
#define _RENDER_SCALERS_H

#include "render.h"
#include "video.h"

#define SCALER_MAXWIDTH 1280
#define SCALER_MAXHEIGHT 1024

extern Bitu Scaler_Line;
extern Bitu Scaler_SrcWidth;
extern Bitu Scaler_SrcHeight;
extern Bitu Scaler_DstPitch;
extern Bit8u * Scaler_DstWrite;
extern Bit8u Scaler_Data[];
extern Bit8u * Scaler_Index;

union PaletteLut {
	Bit16u b16[256];
	Bit32u b32[256];
};

extern PaletteLut Scaler_PaletteLut;

enum RENDER_Operation {
	OP_Normal,
	OP_Normal2x,
	OP_AdvMame2x,
	OP_AdvMame3x,
	OP_AdvInterp2x,
	OP_Interp2x,
	OP_TV2x,
};

struct ScalerBlock {
	Bitu flags;
	Bitu xscale,yscale,miny;
	RENDER_Line_Handler	handlers[4];
};

extern ScalerBlock Normal_8;
extern ScalerBlock NormalDbl_8;
extern ScalerBlock Normal2x_8;
extern ScalerBlock AdvMame2x_8;
extern ScalerBlock AdvMame3x_8;
extern ScalerBlock AdvInterp2x_8;
extern ScalerBlock Interp2x_8;
extern ScalerBlock TV2x_8;


#endif
