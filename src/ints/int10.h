/*
 *  Copyright (C) 2002  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define BIOSMEM_SEG 0x40

#define BIOSMEM_INITIAL_MODE  0x10
#define BIOSMEM_CURRENT_MODE  0x49
#define BIOSMEM_NB_COLS       0x4A
#define BIOSMEM_PAGE_SIZE     0x4C
#define BIOSMEM_CURRENT_START 0x4E
#define BIOSMEM_CURSOR_POS    0x50
#define BIOSMEM_CURSOR_TYPE   0x60
#define BIOSMEM_CURRENT_PAGE  0x62
#define BIOSMEM_CRTC_ADDRESS  0x63
#define BIOSMEM_CURRENT_MSR   0x65
#define BIOSMEM_CURRENT_PAL   0x66
#define BIOSMEM_NB_ROWS       0x84
#define BIOSMEM_CHAR_HEIGHT   0x85
#define BIOSMEM_VIDEO_CTL     0x87
#define BIOSMEM_SWITCHES      0x88
#define BIOSMEM_MODESET_CTL   0x89
#define BIOSMEM_DCC_INDEX     0x8A
#define BIOSMEM_VS_POINTER    0xA8


/*
 *
 * VGA registers
 *
 */
#define VGAREG_ACTL_ADDRESS            0x3c0
#define VGAREG_ACTL_WRITE_DATA         0x3c0
#define VGAREG_ACTL_READ_DATA          0x3c1

#define VGAREG_INPUT_STATUS            0x3c2
#define VGAREG_WRITE_MISC_OUTPUT       0x3c2
#define VGAREG_VIDEO_ENABLE            0x3c3
#define VGAREG_SEQU_ADDRESS            0x3c4
#define VGAREG_SEQU_DATA               0x3c5

#define VGAREG_PEL_MASK                0x3c6
#define VGAREG_DAC_STATE               0x3c7
#define VGAREG_DAC_READ_ADDRESS        0x3c7
#define VGAREG_DAC_WRITE_ADDRESS       0x3c8
#define VGAREG_DAC_DATA                0x3c9

#define VGAREG_READ_FEATURE_CTL        0x3ca
#define VGAREG_READ_MISC_OUTPUT        0x3cc

#define VGAREG_GRDC_ADDRESS            0x3ce
#define VGAREG_GRDC_DATA               0x3cf

#define VGAREG_MDA_CRTC_ADDRESS        0x3b4
#define VGAREG_MDA_CRTC_DATA           0x3b5
#define VGAREG_VGA_CRTC_ADDRESS        0x3d4
#define VGAREG_VGA_CRTC_DATA           0x3d5

#define VGAREG_MDA_WRITE_FEATURE_CTL   0x3ba
#define VGAREG_VGA_WRITE_FEATURE_CTL   0x3da
#define VGAREG_ACTL_RESET              0x3da

#define VGAREG_MDA_MODECTL             0x3b8
#define VGAREG_CGA_MODECTL             0x3d8
#define VGAREG_CGA_PALETTE             0x3d9

/* Video memory */
#define VGAMEM_GRAPH 0xA000
#define VGAMEM_CTEXT 0xB800
#define VGAMEM_MTEXT 0xB000



/*
 *
 * Tables of default values for each mode
 *
 */
#define MODE_MAX   0x13
#define TEXT       0x00
#define GRAPH      0x01

#define CTEXT      0x00
#define MTEXT      0x01
#define CGA        0x02
#define PLANAR1    0x03
#define PLANAR2    0x04
#define PLANAR4    0x05
#define LINEAR8    0x06
#define CGA2	   0x07
// for Tandy

#define TANDY16    0x0A
	

#define LINEAR15   0x10
#define LINEAR16   0x11
#define LINEAR24   0x12
#define LINEAR32   0x13


#define SCREEN_SIZE(x,y) (((x*y*2)|0x00ff)+1)
#define SCREEN_MEM_START(x,y,p) ((((x*y*2)|0x00ff)+1)*p)
#define SCREEN_IO_START(x,y,p) ((((x*y)|0x00ff)+1)*p)


extern Bit8u int10_font_08[256 * 8];
extern Bit8u int10_font_14[256 * 14];
extern Bit8u int10_font_16[256 * 16];



typedef struct
{Bit8u  svgamode;
 Bit16u vesamode;
 Bit8u  type;		/* TEXT, GRAPH */
 Bit8u  memmodel;	/* CTEXT,MTEXT,CGA,PL1,PL2,PL4,P8,P15,P16,P24,P32 */
 Bit8u  nbpages; 
 Bit8u  pixbits;
 Bit16u swidth, sheight;
 Bit16u twidth, theight;
 Bit16u cwidth, cheight;
 Bit16u sstart;
 Bit16u slength;
 Bit8u  miscreg;
 Bit8u  pelmask;
 Bit8u  crtcmodel;
 Bit8u  actlmodel;
 Bit8u  grdcmodel;
 Bit8u  sequmodel;
 Bit8u  dacmodel;	/* 0 1 2 3 */
} VGAMODES;

extern VGAMODES vga_modes[MODE_MAX+1];


typedef struct {
	RealPt font_8_first;
	RealPt font_8_second;
	RealPt font_14;
	RealPt font_16;
	RealPt static_state;
} VGAROMAREA;
extern VGAROMAREA int10_romarea;

#define BIOS_NCOLS Bit16u ncols=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
#define BIOS_NROWS Bit16u nrows=real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;

inline Bit8u CURSOR_POS_COL(Bit8u page) {
	return real_readb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2);
}

inline Bit8u CURSOR_POS_ROW(Bit8u page) {
	return real_readb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2+1);
}


void INT10_SetVideoMode(Bit8u mode);

void INT10_ScrollWindow(Bit8u rul,Bit8u cul,Bit8u rlr,Bit8u clr,Bit8s nlines,Bit8u attr,Bit8u page);

void INT10_SetActivePage(Bit8u page);
void INT10_GetFuncStateInformation(PhysPt save);

void INT10_SetCursorPos(Bit8u row,Bit8u col,Bit8u page);
void INT10_TeletypeOutput(Bit8u chr,Bit8u attr,bool showattr, Bit8u page);
void INT10_ReadCharAttr(Bit16u * result,Bit8u page);
void INT10_WriteChar(Bit8u chr,Bit8u attr,Bit8u page,Bit16u count,bool showattr);
void INT10_WriteString(Bit8u row,Bit8u col,Bit8u flag,Bit8u attr,PhysPt string,Bit16u count,Bit8u page);

/* Graphics Stuff */
void INT10_PutPixel(Bit16u x,Bit16u y,Bit8u page,Bit8u color);
void INT10_GetPixel(Bit16u x,Bit16u y,Bit8u page,Bit8u * color);
VGAMODES * GetCurrentMode(void);

/* Palette Group */
void INT10_SetSinglePaletteRegister(Bit8u reg,Bit8u val);
void INT10_SetOverscanBorderColor(Bit8u val);
void INT10_SetAllPaletteRegisters(PhysPt data);
void INT10_ToggleBlinkingBit(Bit8u state);
void INT10_GetSinglePaletteRegister(Bit8u reg,Bit8u * val);
void INT10_GetOverscanBorderColor(Bit8u * val);
void INT10_GetAllPaletteRegisters(PhysPt data);
void INT10_SetSingleDacRegister(Bit8u index,Bit8u red,Bit8u green,Bit8u blue);
void INT10_GetSingleDacRegister(Bit8u index,Bit8u * red,Bit8u * green,Bit8u * blue);
void INT10_SetDACBlock(Bit16u index,Bit16u count,PhysPt data);
void INT10_GetDACBlock(Bit16u index,Bit16u count,PhysPt data);

/* Mouse pointer */
void INT10_SetGfxControllerToDefault(void);

/* Sup Groups */
void INT10_SetupRomMemory(void);

struct Int10Data {
	Bit8u mode;
	VGAMODES * entry;
};

extern Int10Data int10;

