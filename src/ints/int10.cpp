/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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

#include <string.h>
#include "dosbox.h"
#include "bios.h"
#include "mem.h"
#include "callback.h"
#include "regs.h"
#include "video.h"
#include "inout.h"
#include "int10.h"
#include "../hardware/vga.h"                /* Maybe move this thing */ 

#define TEXT_SEG 0xb800

static Bitu call_10;
static bool warned_ff=false;
static bool warned_int10_0b=false;
Int10Data int10;


static Bitu INT10_Handler(void) {
	switch (reg_ah) {
	case 0x00:								/* Set VideoMode */
		INT10_SetVideoMode(reg_al);
		break;
	case 0x01:								/* Set TextMode Cursor Shape */
        vga.internal.cursor=reg_cx;         // maybe write some memory somewhere
		LOG(LOG_INT10,"INT10:01:Set textmode cursor shape partially supported: %X",reg_cx);
		break;
	case 0x02:								/* Set Cursor Pos */
		//TODO Check some shit but not really usefull
		INT10_SetCursorPos(reg_dh,reg_dl,reg_bh);
		break;
	case 0x03:								/* get Cursor Pos and Cursor Shape*/
		//TODO the Cursor Shape Stuff
		reg_dl=CURSOR_POS_COL(reg_bh);
		reg_dh=CURSOR_POS_ROW(reg_bh);
		break;
	case 0x04:								/* read light pen pos YEAH RIGHT */
		/* Light pen is not supported */
		reg_ah=0;
		break;
	case 0x05:								/* Set Active Page */
		if (reg_al & 0x80) LOG(LOG_INT10,"Func %x",reg_al);
		else INT10_SetActivePage(reg_al);
		break;	
	case 0x06:								/* Scroll Up */
//TODO Graphics mode scroll
		INT10_ScrollWindow(reg_ch,reg_cl,reg_dh,reg_dl,-reg_al,reg_bh,0xFF);
		break;
	case 0x07:
						/* Scroll Down */
		INT10_ScrollWindow(reg_ch,reg_cl,reg_dh,reg_dl,reg_al,reg_bh,0xFF);
		break;
	case 0x08:								/* Read character & attribute at cursor */
//TODO Check for GRAPH and then just return 
		INT10_ReadCharAttr(&reg_ax,reg_bh);
		break;						
	case 0x09:								/* Write Character & Attribute at cursor CX times */
		INT10_WriteChar(reg_al,reg_bl,reg_bh,reg_cx,true);
		break;
	case 0x0A:								/* Write Character at cursor CX times */
		INT10_WriteChar(reg_al,reg_bl,reg_bh,reg_cx,false);
		break;
	case 0x0B:								/* Set Background/Border Colour & Set Palette*/
        if(!warned_int10_0b) {
            LOG(LOG_ERROR|LOG_INT10,"Function 0B Unsupported: Set Background/border colour & Set Pallete");
            warned_int10_0b=true;
        }
		break;
	case 0x0C:								/* Write Graphics Pixel */
		INT10_PutPixel(reg_cx,reg_dx,reg_bh,reg_al);
		break;
	case 0x0D:								/* Read Graphics Pixel */
		INT10_GetPixel(reg_cx,reg_dx,reg_bh,&reg_al);
		break;
	case 0x0E:								/* Teletype OutPut */
		INT10_TeletypeOutput(reg_al,reg_bl,false,reg_bh);
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
		case 0x15:							/* GET INDIVIDUAL DAC REGISTER */
			INT10_GetSingleDacRegister(reg_bl,&reg_dh,&reg_ch,&reg_cl);
			break;
		case 0x17:							/* GET BLOCK OF DAC REGISTER */
			INT10_GetDACBlock(reg_bx,reg_cx,SegPhys(es)+reg_dx);
			break;
		default:
			LOG(LOG_ERROR|LOG_INT10,"Function 10:Unhandled EGA/VGA Palette Function %2X",reg_al);
		}
		break;
	case 0x11:								/* Character generator functions */
		switch (reg_al) {
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
				SegSet16(es,RealSeg(int10_romarea.font_14));
				reg_bp=RealOff(int10_romarea.font_14);
				reg_cx=14;
				break;
			case 0x03:	/* font 8x8 first 128 */
				SegSet16(es,RealSeg(int10_romarea.font_8_first));
				reg_bp=RealOff(int10_romarea.font_8_first);
				reg_cx=8;
				break;
			case 0x04:	/* font 8x8 second 128 */
				SegSet16(es,RealSeg(int10_romarea.font_8_second));
				reg_bp=RealOff(int10_romarea.font_8_second);
				reg_cx=8;
				break;
			case 0x06:	/* font 8x16 */
				SegSet16(es,RealSeg(int10_romarea.font_16));
				reg_bp=RealOff(int10_romarea.font_16);
				reg_cx=16;
				break;
			default:
				reg_cx=16;
				LOG(LOG_ERROR|LOG_INT10,"Fucntion 11:30 Request for font %2X",reg_bh);	
			}
			reg_dl=real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS);
			break;
		default:
			LOG(LOG_ERROR|LOG_INT10,"Function 11:Unsupported character generator call %2X",reg_al);
		}
		break;
	case 0x12:								/* alternate function select */
		switch (reg_bl) {
		case 0x10:							/* Get EGA Information */
			{
				reg_bh=(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS)==0x3B4);	
				reg_bl=3;	//256 kb
				reg_cx=real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES) & 0x0F;
				break;
			}
		default:
			LOG(LOG_ERROR|LOG_INT10,"Function 12:Call %2X not handled",reg_bl);
		}
		break;
	case 0x13:								/* Write String */
		INT10_WriteString(reg_dh,reg_dl,reg_al,reg_bl,SegPhys(es)+reg_bp,reg_cx,reg_bh);
		break;
	case 0x1A:								/* Display Combination */
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
		switch (reg_bx) {
		case 0x0000:
			INT10_GetFuncStateInformation(SegPhys(es)+reg_di);
			reg_al=0x1B;
			break;
		default:
			LOG(LOG_ERROR|LOG_INT10,"Function 1B:Unhandled call BX %2X",reg_bx);
		}
		break;
	case 0xff:
		if (!warned_ff) LOG(LOG_INT10,"INT10:FF:Weird NC call");
		warned_ff=true;
		break;
	default:
		LOG(LOG_ERROR|LOG_INT10,"Function %2X not supported",reg_ah);
	};
	return CBRET_NONE;
}

static void INT10_Seg40Init(void) {
	//This should fill the ega vga structures
	// init detected hardware BIOS Area 
	real_writew(BIOSMEM_SEG,BIOSMEM_INITIAL_MODE,real_readw(BIOSMEM_SEG,BIOSMEM_INITIAL_MODE)&0xFFCF);
	// Just for the first int10 find its children
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

	INT10_SetVideoMode(3);
}


static void INT10_InitVGA(void) {
/* switch to color mode and enable CPU access 480 lines */

	IO_Write(0x3c2,0xc3);

	/* More than 64k */
	IO_Write(0x3c4,0x04);
	IO_Write(0x3c5,0x02);
};

void INT10_Init(Section* sec) {
	INT10_InitVGA();
	/* Setup the INT 10 vector */
	call_10=CALLBACK_Allocate();	
	CALLBACK_Setup(call_10,&INT10_Handler,CB_IRET);
	RealSetVec(0x10,CALLBACK_RealPointer(call_10));
	//Init the 0x40 segment and init the datastructures in the the video rom area
	INT10_SetupRomMemory();
	INT10_Seg40Init();
};


