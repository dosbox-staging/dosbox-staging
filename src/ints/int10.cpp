/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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

#include <string.h>
#include "dosbox.h"
#include "bios.h"
#include "mem.h"
#include "callback.h"
#include "regs.h"
#include "video.h"
#include "inout.h"
#include "int10.h"
#include "setup.h"
#include "support.h"
#include "vga.h"

Int10Data int10;
static Bitu call_10;
static bool warned_ff=false;

static Bitu INT10_Handler(void) {
#if 0
	switch (reg_ah) {
	case 0x02:
	case 0x03:
	case 0x09:
	case 0xc:
	case 0xd:
	case 0x0e:
	case 0x10:
	case 0x4f:

		break;
	default:
		LOG(LOG_INT10,LOG_NORMAL)("Function AX:%04X , BX %04X DX %04X",reg_ax,reg_bx,reg_dx);
		break;
	}
#endif
	switch (reg_ah) {
	case 0x00:								/* Set VideoMode */
		INT10_SetVideoMode(reg_al);
		break;
	case 0x01:								/* Set TextMode Cursor Shape */
		INT10_SetCursorShape(reg_ch,reg_cl);
		break;
	case 0x02:								/* Set Cursor Pos */
		INT10_SetCursorPos(reg_dh,reg_dl,reg_bh);
		break;
	case 0x03:								/* get Cursor Pos and Cursor Shape*/
		reg_ah=0;
		reg_dl=CURSOR_POS_COL(reg_bh);
		reg_dh=CURSOR_POS_ROW(reg_bh);
		reg_cx=real_readw(BIOSMEM_SEG,BIOSMEM_CURSOR_TYPE);
		break;
	case 0x04:								/* read light pen pos YEAH RIGHT */
		/* Light pen is not supported */
		reg_ah=0;
		break;
	case 0x05:								/* Set Active Page */
		if (reg_al & 0x80 && machine==MCH_TANDY) {
			Bit8u crtcpu=real_readb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE);		
			switch (reg_al) {
			case 0x80:
				reg_bh=crtcpu & 7;
				reg_bl=(crtcpu >> 3) & 0x7;
				break;
			case 0x81:
				crtcpu=(crtcpu & 0xc7) | ((reg_bl & 7) << 3);
				break;
			case 0x82:
				crtcpu=(crtcpu & 0xf8) | (reg_bh & 7);
				break;
			case 0x83:
				crtcpu=(crtcpu & 0xc0) | (reg_bh & 7) | ((reg_bl & 7) << 3);
				break;
			}
			IO_WriteB(0x3df,crtcpu);
			real_writeb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE,crtcpu);
		}
		else INT10_SetActivePage(reg_al);
		break;	
	case 0x06:								/* Scroll Up */
		INT10_ScrollWindow(reg_ch,reg_cl,reg_dh,reg_dl,-reg_al,reg_bh,0xFF);
		break;
	case 0x07:								/* Scroll Down */
		INT10_ScrollWindow(reg_ch,reg_cl,reg_dh,reg_dl,reg_al,reg_bh,0xFF);
		break;
	case 0x08:								/* Read character & attribute at cursor */
		INT10_ReadCharAttr(&reg_ax,reg_bh);
		break;						
	case 0x09:								/* Write Character & Attribute at cursor CX times */
		INT10_WriteChar(reg_al,reg_bl,reg_bh,reg_cx,true);
		break;
	case 0x0A:								/* Write Character at cursor CX times */
		INT10_WriteChar(reg_al,reg_bl,reg_bh,reg_cx,false);
		break;
	case 0x0B:								/* Set Background/Border Colour & Set Palette*/
		switch (reg_bh) {
		case 0x00:		//Background/Border color
			INT10_SetBackgroundBorder(reg_bl);
			break;
		case 0x01:		//Set color Select
			INT10_SetColorSelect(reg_bl);
			break;
		}
		break;
	case 0x0C:								/* Write Graphics Pixel */
		INT10_PutPixel(reg_cx,reg_dx,reg_bh,reg_al);
		break;
	case 0x0D:								/* Read Graphics Pixel */
		INT10_GetPixel(reg_cx,reg_dx,reg_bh,&reg_al);
		break;
	case 0x0E:								/* Teletype OutPut */
		INT10_TeletypeOutput(reg_al,reg_bl);
		break;
	case 0x0F:								/* Get videomode */
		reg_bh=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
		reg_al=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE);
		reg_ah=(Bit8u)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
		break;					
	case 0x10:								/* EGA/VGA Palette functions */
		switch (reg_al) {
		case 0x00:							/* SET SINGLE PALETTE REGISTER */
			INT10_SetSinglePaletteRegister(reg_bl,reg_bh);
			break;
		case 0x01:							/* SET BORDER (OVERSCAN) COLOR*/
			INT10_SetOverscanBorderColor(reg_bh);
			break;
		case 0x02:							/* SET ALL PALETTE REGISTERS */
			INT10_SetAllPaletteRegisters(SegPhys(es)+reg_dx);
			break;
		case 0x03:							/* TOGGLE INTENSITY/BLINKING BIT */
			INT10_ToggleBlinkingBit(reg_bl);
			break;
		case 0x07:							/* GET SINGLE PALETTE REGISTER */
			INT10_GetSinglePaletteRegister(reg_bl,&reg_bh);
			break;
		case 0x08:							/* READ OVERSCAN (BORDER COLOR) REGISTER */
			INT10_GetOverscanBorderColor(&reg_bh);
			break;
		case 0x09:							/* READ ALL PALETTE REGISTERS AND OVERSCAN REGISTER */
			INT10_GetAllPaletteRegisters(SegPhys(es)+reg_dx);
			break;
		case 0x10:							/* SET INDIVIDUAL DAC REGISTER */
			INT10_SetSingleDacRegister(reg_bl,reg_dh,reg_ch,reg_cl);
			break;
		case 0x12:							/* SET BLOCK OF DAC REGISTERS */
			INT10_SetDACBlock(reg_bx,reg_cx,SegPhys(es)+reg_dx);
			break;
		case 0x13:							/* SELECT VIDEO DAC COLOR PAGE */
			INT10_SelectDACPage(reg_bl,reg_bh);
			break;
		case 0x15:							/* GET INDIVIDUAL DAC REGISTER */
			INT10_GetSingleDacRegister(reg_bl,&reg_dh,&reg_ch,&reg_cl);
			break;
		case 0x17:							/* GET BLOCK OF DAC REGISTER */
			INT10_GetDACBlock(reg_bx,reg_cx,SegPhys(es)+reg_dx);
			break;
		case 0x18:							/* undocumented - SET PEL MASK */
			INT10_SetPelMask(reg_bl);
			break;
		case 0x19:							/* undocumented - GET PEL MASK */
			INT10_GetPelMask(reg_bl);
			break;
		default:
			LOG(LOG_INT10,LOG_ERROR)("Function 10:Unhandled EGA/VGA Palette Function %2X",reg_al);
		}
		break;
	case 0x11:								/* Character generator functions */
		switch (reg_al) {
/* Textmode calls */
		case 0x00:			/* Load user font */
		case 0x10:
			INT10_LoadFont(SegPhys(es)+reg_bp,reg_al==0x10,reg_cx,reg_dx,reg_bl,reg_bh);
			break;
		case 0x01:			/* Load 8x14 font */
		case 0x11:
			INT10_LoadFont(Real2Phys(int10.rom.font_14),true,256,0,0,14);
			break;
		case 0x02:			/* Load 8x8 font */
		case 0x12:
			INT10_LoadFont(Real2Phys(int10.rom.font_8_first),true,256,0,0,8);
			break;
		case 0x03:			/* Set Block Specifier */
			IO_Write(0x3c4,0x3);IO_Write(0x3c5,reg_bl);
			break;
/* Graphics mode calls */
		case 0x20:			/* Set User 8x8 Graphics characters */
			RealSetVec(0x1f,RealMake(SegValue(es),reg_bp));
			break;
		case 0x21:			/* Set user graphics characters */
			RealSetVec(0x43,RealMake(SegValue(es),reg_bp));
			real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,reg_cx);
			goto graphics_chars;
		case 0x22:			/* Rom 8x14 set */
			RealSetVec(0x43,int10.rom.font_14);
			real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,14);
			goto graphics_chars;
//		case 0x23:			/* Rom 8x8 double dot set */
			//TODO
		case 0x24:			/* Rom 8x16 set */
			RealSetVec(0x43,int10.rom.font_16);
			real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,16);
			goto graphics_chars;
graphics_chars:
			switch (reg_bl) {
			case 0x00:real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,reg_dl-1);break;
			case 0x01:real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,13);break;
			case 0x03:real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,42);break;
			case 0x02:
			default:real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,24);break;
			}
			break;
/* General */
		case 0x30:/* Get Font Information */
			switch (reg_bh) {
			case 0x00:	/* interupt 0x1f vector */
				{
					RealPt int_1f=RealGetVec(0x1f);
					SegSet16(es,RealSeg(int_1f));
					reg_bp=RealOff(int_1f);
					reg_cx=8;
				}
				break;
			case 0x01:	/* interupt 0x43 vector */
				{
					RealPt int_43=RealGetVec(0x43);
					SegSet16(es,RealSeg(int_43));
					reg_bp=RealOff(int_43);
					reg_cx=8;
				}
				break;
			case 0x02:	/* font 8x14 */
				SegSet16(es,RealSeg(int10.rom.font_14));
				reg_bp=RealOff(int10.rom.font_14);
				reg_cx=14;
				break;
			case 0x03:	/* font 8x8 first 128 */
				SegSet16(es,RealSeg(int10.rom.font_8_first));
				reg_bp=RealOff(int10.rom.font_8_first);
				reg_cx=8;
				break;
			case 0x04:	/* font 8x8 second 128 */
				SegSet16(es,RealSeg(int10.rom.font_8_second));
				reg_bp=RealOff(int10.rom.font_8_second);
				reg_cx=8;
				break;
			case 0x06:	/* font 8x16 */
				SegSet16(es,RealSeg(int10.rom.font_16));
				reg_bp=RealOff(int10.rom.font_16);
				reg_cx=16;
				break;
			default:
				reg_cx=16;
				LOG(LOG_INT10,LOG_ERROR)("Fucntion 11:30 Request for font %2X",reg_bh);	
			}
			reg_dl=real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS);
			break;
		default:
			LOG(LOG_INT10,LOG_ERROR)("Function 11:Unsupported character generator call %2X",reg_al);
		}
		break;
	case 0x12:								/* alternate function select */
		if (machine<MCH_VGA) break;
		switch (reg_bl) {
		case 0x10:							/* Get EGA Information */
			{
				reg_bh=(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS)==0x3B4);	
				reg_bl=3;	//256 kb
				reg_cx=real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES) & 0x0F;
				break;
			}
		case 0x36:							/* VGA Refresh control */
			/* 
				Call disables/enables the vga from outputting video,
				don't support it, but fake a success return 
			*/
			reg_al=0x12;
			break;
		default:
			LOG(LOG_INT10,LOG_ERROR)("Function 12:Call %2X not handled",reg_bl);
			reg_al=0x12;			//Always fake a success call
		}
		break;
	case 0x13:								/* Write String */
		INT10_WriteString(reg_dh,reg_dl,reg_al,reg_bl,SegPhys(es)+reg_bp,reg_cx,reg_bh);
		break;
	case 0x1A:								/* Display Combination */
		if (machine<MCH_VGA) break;
		if (reg_al==0) {
			reg_bx=real_readb(BIOSMEM_SEG,BIOSMEM_DCC_INDEX);
			reg_al=0x1A;
			break;
		}
		if (reg_al==1) {
			real_writeb(BIOSMEM_SEG,BIOSMEM_DCC_INDEX,reg_bl);
			reg_al=0x1A;
			break;
		}
		break;
	case 0x1B:								/* functionality State Information */
		if (machine<MCH_VGA) break;
		switch (reg_bx) {
		case 0x0000:
			INT10_GetFuncStateInformation(SegPhys(es)+reg_di);
			reg_al=0x1B;
			break;
		default:
			LOG(LOG_INT10,LOG_ERROR)("1B:Unhandled call BX %2X",reg_bx);
		}
		break;
	case 0x1C:	/* Video Save Area */
		if (machine<MCH_VGA) break;
		if (reg_al==0) reg_bx = 0;
		reg_al = 0x1C;	
		break;
	case 0x4f:								/* VESA Calls */
		switch (reg_al) {
		case 0x00:							/* Get SVGA Information */
			reg_al=0x4f;
			reg_ah=VESA_GetSVGAInformation(SegValue(es),reg_di);
			break;
		case 0x01:							/* Get SVGA Mode Information */
			reg_al=0x4f;
			reg_ah=VESA_GetSVGAModeInformation(reg_cx,SegValue(es),reg_di);
			break;
		case 0x02:							/* Set videomode */
			reg_al=0x4f;
			reg_ah=VESA_SetSVGAMode(reg_bx);
			break;
		case 0x03:							/* Get videomode */
			reg_al=0x4f;
			reg_ah=VESA_GetSVGAMode(reg_bx);
			break;
		case 0x05:							
			if (reg_bh==0) {				/* Set CPU Window */
				reg_ah=VESA_SetCPUWindow(reg_bl,reg_dl);
				reg_al=0x4f;
			} else if (reg_bh == 1) {		/* Get CPU Window */
				reg_ah=VESA_GetCPUWindow(reg_bl,reg_dx);
				reg_al=0x4f;
			} else {
				LOG(LOG_INT10,LOG_ERROR)("Unhandled VESA Function %X Subfunction %X",reg_al,reg_bh);
				reg_ah=0x01;
			}
			break;
		case 0x06:
			reg_al=0x4f;
			reg_ah=VESA_ScanLineLength(reg_bl,reg_bx,reg_cx,reg_dx);
			break;
		case 0x07:
			switch (reg_bl) {
			case 0x80:						/* Set Display Start during retrace ?? */
			case 0x00:						/* Set display Start */
				reg_al=0x4f;
				reg_ah=VESA_SetDisplayStart(reg_cx,reg_dx);
				break;
			case 0x01:
				reg_al=0x4f;
				reg_bh=0x00;				//Weird?
				reg_ah=VESA_GetDisplayStart(reg_cx,reg_dx);
				break;
			default:
				LOG(LOG_INT10,LOG_ERROR)("Unhandled VESA Function %X Subfunction %X",reg_al,reg_bl);
				reg_ah=0x1;
			}
			break;
		case 0x09:
			switch (reg_bl) {
			case 0x80:						/* Set Palette during retrace */
				//TODO
			case 0x00:						/* Set Palette */
				reg_ah=VESA_SetPalette(SegPhys(es)+reg_di,reg_dx,reg_cx);
				reg_al=0x4f;
				break;
			case 0x01:						/* Get Palette */
				reg_ah=VESA_GetPalette(SegPhys(es)+reg_di,reg_dx,reg_cx);
				reg_al=0x4f;
				break;
			default:
				LOG(LOG_INT10,LOG_ERROR)("Unhandled VESA Function %X Subfunction %X",reg_al,reg_bl);
				reg_ah=0x01;
			}
			break;
		case 0x0a:							/* Get Pmode Inteface */
			reg_edi=RealOff(int10.rom.pmode_interface);
			SegSet16(es,RealSeg(int10.rom.pmode_interface));
			reg_cx=int10.rom.pmode_interface_size;
			reg_ax=0x004f;
			break;

		default:
			LOG(LOG_INT10,LOG_ERROR)("Unhandled VESA Function %X",reg_al);
			reg_al=0x0;
		}
		break;
	case 0xff:
		if (!warned_ff) LOG(LOG_INT10,LOG_NORMAL)("INT10:FF:Weird NC call");
		warned_ff=true;
		break;
	default:
		LOG(LOG_INT10,LOG_ERROR)("Function %2X not supported",reg_ah);
		reg_al=0x00;		//Successfull
	};
	return CBRET_NONE;
}

static void INT10_Seg40Init(void) {
	// the default char height
	real_writeb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,16);
	// Clear the screen 
	real_writeb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL,0x60);
	// Set the basic screen we have
	real_writeb(BIOSMEM_SEG,BIOSMEM_SWITCHES,0xF9);
	// Set the basic modeset options
	real_writeb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL,0x51);
	// Set the  default MSR
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x09);
}


static void INT10_InitVGA(void) {
/* switch to color mode and enable CPU access 480 lines */
	IO_Write(0x3c2,0xc3);
	/* More than 64k */
	IO_Write(0x3c4,0x04);
	IO_Write(0x3c5,0x02);
};

static void SetupTandyBios(void) {
	static Bit8u TandyConfig[130]= {
		0x21, 0x42, 0x49, 0x4f, 0x53, 0x20, 0x52, 0x4f, 0x4d, 0x20, 0x76, 0x65, 0x72,
		0x73, 0x69, 0x6f, 0x6e, 0x20, 0x30, 0x32, 0x2e, 0x30, 0x30, 0x2e, 0x30, 0x30,
		0x0d, 0x0a, 0x43, 0x6f, 0x6d, 0x70, 0x61, 0x74, 0x69, 0x62, 0x69, 0x6c, 0x69,
		0x74, 0x79, 0x20, 0x53, 0x6f, 0x66, 0x74, 0x77, 0x61, 0x72, 0x65, 0x0d, 0x0a,
		0x43, 0x6f, 0x70, 0x79, 0x72, 0x69, 0x67, 0x68, 0x74, 0x20, 0x28, 0x43, 0x29,
		0x20, 0x31, 0x39, 0x38, 0x34, 0x2c, 0x31, 0x39, 0x38, 0x35, 0x2c, 0x31, 0x39,
		0x38, 0x36, 0x2c, 0x31, 0x39, 0x38, 0x37, 0x0d, 0x0a, 0x50, 0x68, 0x6f, 0x65,
		0x6e, 0x69, 0x78, 0x20, 0x53, 0x6f, 0x66, 0x74, 0x77, 0x61, 0x72, 0x65, 0x20,
		0x41, 0x73, 0x73, 0x6f, 0x63, 0x69, 0x61, 0x74, 0x65, 0x73, 0x20, 0x4c, 0x74,
		0x64, 0x2e, 0x0d, 0x0a, 0x61, 0x6e, 0x64, 0x20, 0x54, 0x61, 0x6e, 0x64, 0x79
	};
	Bitu i;
	phys_writeb(0xffffe,0xff);
	for(i=0;i<130;i++) {
		phys_writeb(0xf0000+i+0xc000, TandyConfig[i]);
	}
}

void INT10_Init(Section* sec) {
	INT10_InitVGA();
	if (machine==MCH_TANDY) SetupTandyBios();
	/* Setup the INT 10 vector */
	call_10=CALLBACK_Allocate();	
	CALLBACK_Setup(call_10,&INT10_Handler,CB_IRET,"Int 10 video");
	RealSetVec(0x10,CALLBACK_RealPointer(call_10));
	//Init the 0x40 segment and init the datastructures in the the video rom area
	INT10_SetupRomMemory();
	INT10_Seg40Init();
	INT10_SetupVESA();
	INT10_SetVideoMode(machine==MCH_HERC ? 0x7 : 0x3);
};


