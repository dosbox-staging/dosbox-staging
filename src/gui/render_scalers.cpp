#include "dosbox.h"
#include "render_scalers.h"

Bitu Scaler_Line;
Bitu Scaler_SrcWidth;
Bitu Scaler_SrcHeight;
Bitu Scaler_DstPitch;
Bit8u * Scaler_DstWrite;
Bit8u * Scaler_Index;
Bit8u Scaler_Data[SCALER_MAXHEIGHT*5];		//5cmds/line
PaletteLut Scaler_PaletteLut;

static union {
	Bit32u b32 [4][SCALER_MAXWIDTH];
	Bit16u b16 [4][SCALER_MAXWIDTH];
	Bit8u b8 [4][SCALER_MAXWIDTH];
} line_cache;
static union {
	Bit32u b32 [4][SCALER_MAXWIDTH*3];
	Bit16u b16 [4][SCALER_MAXWIDTH*3];
	Bit8u b8 [4][SCALER_MAXWIDTH*3];
} write_cache;

static Bit8u * ln[3];

#define _conc2(A,B) A ## B
#define _conc3(A,B,C) A ## B ## C
#define _conc4(A,B,C,D) A ## B ## C ## D
#define _conc5(A,B,C,D,E) A ## B ## C ## D ## E

#define conc2(A,B) _conc2(A,B)
#define conc3(A,B,C) _conc3(A,B,C)
#define conc4(A,B,C,D) _conc4(A,B,C,D)
#define conc2d(A,B) _conc3(A,_,B)
#define conc3d(A,B,C) _conc5(A,_,B,_,C)

#define BituMove(_DST,_SRC,_SIZE)			\
{											\
	Bitu bsize=(_SIZE)/sizeof(Bitu);		\
	Bitu * bdst=(Bitu *)(_DST);				\
	Bitu * bsrc=(Bitu *)(_SRC);				\
	while (bsize--) *bdst++=*bsrc++;		\
}

#define interp_w2(P0,P1,W0,W1)															\
	((((P0&redblueMask)*W0+(P1&redblueMask)*W1)/(W0+W1)) & redblueMask) |	\
	((((P0&  greenMask)*W0+(P1&  greenMask)*W1)/(W0+W1)) & greenMask)
#define interp_w3(P0,P1,P2,W0,W1,W2)														\
	((((P0&redblueMask)*W0+(P1&redblueMask)*W1+(P2&redblueMask)*W2)/(W0+W1+W2)) & redblueMask) |	\
	((((P0&  greenMask)*W0+(P1&  greenMask)*W1+(P2&  greenMask)*W2)/(W0+W1+W2)) & greenMask)
#define interp_w4(P0,P1,P2,P3,W0,W1,W2,W3)														\
	((((P0&redblueMask)*W0+(P1&redblueMask)*W1+(P2&redblueMask)*W2+(P3&redblueMask)*W3)/(W0+W1+W2+W3)) & redblueMask) |	\
	((((P0&  greenMask)*W0+(P1&  greenMask)*W1+(P2&  greenMask)*W2+(P3&  greenMask)*W3)/(W0+W1+W2+W3)) & greenMask)

/* Include the different rendering routines */
#define SBPP 8
#define DBPP 8
#include "render_templates.h"
#undef DBPP
#define DBPP 15
#include "render_templates.h"
#undef DBPP
#define DBPP 16
#include "render_templates.h"
#undef DBPP
#define DBPP 32
#include "render_templates.h"
#undef SBPP
#undef DBPP



ScalerBlock Normal_8={
	CAN_8|CAN_16|CAN_32|LOVE_8,
	1,1,1,
	Normal_8_8,Normal_8_16,Normal_8_16,Normal_8_32
};

ScalerBlock NormalDbl_8= {
	CAN_8|CAN_16|CAN_32|LOVE_8,
	2,1,1,
	Normal2x_8_8,Normal2x_8_16,Normal2x_8_16,Normal2x_8_32
};

ScalerBlock Normal2x_8={
	CAN_8|CAN_16|CAN_32|LOVE_8,
	2,2,1,
	Normal2x_8_8,Normal2x_8_16,Normal2x_8_16,Normal2x_8_32
};

ScalerBlock AdvMame2x_8={
	CAN_8|CAN_16|CAN_32|LOVE_8,
	2,2,1,
	AdvMame2x_8_8,AdvMame2x_8_16,AdvMame2x_8_16,AdvMame2x_8_32
};

ScalerBlock AdvMame3x_8={
	CAN_8|CAN_16|CAN_32|LOVE_8,
	3,3,2,
	AdvMame3x_8_8,AdvMame3x_8_16,AdvMame3x_8_16,AdvMame3x_8_32
};

ScalerBlock Interp2x_8={
	CAN_16|CAN_32|LOVE_32|NEED_RGB,
	2,2,1,
	0,Interp2x_8_16,Interp2x_8_16,Interp2x_8_32
};

ScalerBlock AdvInterp2x_8={
	CAN_16|CAN_32|LOVE_32|NEED_RGB,
	2,2,1,
	0,AdvInterp2x_8_16,AdvInterp2x_8_16,AdvInterp2x_8_32
};

ScalerBlock TV2x_8={
	CAN_16|CAN_32|LOVE_32|NEED_RGB,
	2,2,1,
	0,TV2x_8_16,TV2x_8_16,TV2x_8_32
};


