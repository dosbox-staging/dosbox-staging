// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later


#include "dosbox.h"
#include "mem.h"
#include "callback.h"
#include "regs.h"
#include "inout.h"
#include "int10.h"
#include "mouse.h"
#include "setup.h"

Int10Data int10;
static callback_number_t call_10 = 0;
static bool warned_ff=false;

static Bitu INT10_Handler(void) {
#if 0
	switch (reg_ah) {
	case 0x02:
	case 0x03:
	case 0x09:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x10:
	case 0x4f:
		break;
	default:
		LOG_DEBUG("INT10: Function AX: %02X %02X, BX: %02X %02X, CX: %02X %02X, DX: %02X %02X",
		          reg_ah,
		          reg_al,
		          reg_bh,
		          reg_bl,
		          reg_ch,
		          reg_cl,
		          reg_dh,
		          reg_dl);
		break;
	}
#endif
	INT10_SetCurMode();

	switch (reg_ah) {
	case 0x00:								/* Set VideoMode */
		MOUSEDOS_BeforeNewVideoMode();
		INT10_SetVideoMode(reg_al);
		MOUSEDOS_AfterNewVideoMode(true);
		break;
	case 0x01:								/* Set TextMode Cursor Shape */
		INT10_SetCursorShape(reg_ch,reg_cl);
		break;
	case 0x02:								/* Set Cursor Pos */
		INT10_SetCursorPos(reg_dh,reg_dl,reg_bh);
		break;
	case 0x03:								/* get Cursor Pos and Cursor Shape*/
//		reg_ah=0;
		reg_dl=CURSOR_POS_COL(reg_bh);
		reg_dh=CURSOR_POS_ROW(reg_bh);
		reg_cx=real_readw(BIOSMEM_SEG,BIOSMEM_CURSOR_TYPE);
		break;
	case 0x04:								/* read light pen pos YEAH RIGHT */
		/* Light pen is not supported */
		reg_ax=0;
		break;
	case 0x05:								/* Set Active Page */
		if ((reg_al & 0x80) && is_machine_pcjr_or_tandy()) {
			uint8_t crtcpu=real_readb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE);
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
			if (is_machine_pcjr()) {
				/* always return graphics mapping, even for invalid values of AL */
				reg_bh=crtcpu & 7;
				reg_bl=(crtcpu >> 3) & 0x7;
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
		if (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE)==0x11)
			INT10_WriteChar(reg_al,(reg_bl&0x80)|0x3f,reg_bh,reg_cx,true);
		else INT10_WriteChar(reg_al,reg_bl,reg_bh,reg_cx,true);
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
		default:
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
		reg_bh = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
		reg_al = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE);
		if (is_machine_ega_or_better()) {
			reg_al |= real_readb(BIOSMEM_SEG, BIOSMEM_VIDEO_CTL) & 0x80;
		}
		reg_ah = (uint8_t) real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
		break;
	case 0x10:								/* Palette functions */
		if (!is_machine_ega_or_better() && (reg_al > 0x02)) {
			break;
		} else if (!is_machine_vga_or_better() && (reg_al > 0x03)) {
			break;
		}
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
			INT10_SetSingleDACRegister(reg_bl,reg_dh,reg_ch,reg_cl);
			break;
		case 0x12:							/* SET BLOCK OF DAC REGISTERS */
			INT10_SetDACBlock(reg_bx,reg_cx,SegPhys(es)+reg_dx);
			break;
		case 0x13:							/* SELECT VIDEO DAC COLOR PAGE */
			INT10_SelectDACPage(reg_bl,reg_bh);
			break;
		case 0x15:							/* GET INDIVIDUAL DAC REGISTER */
			INT10_GetSingleDACRegister(reg_bl,&reg_dh,&reg_ch,&reg_cl);
			break;
		case 0x17:							/* GET BLOCK OF DAC REGISTER */
			INT10_GetDACBlock(reg_bx,reg_cx,SegPhys(es)+reg_dx);
			break;
		case 0x18:							/* undocumented - SET PEL MASK */
			INT10_SetPelMask(reg_bl);
			break;
		case 0x19:							/* undocumented - GET PEL MASK */
			INT10_GetPelMask(reg_bl);
			reg_bh=0;	// bx for get mask
			break;
		case 0x1A:							/* GET VIDEO DAC COLOR PAGE */
			INT10_GetDACPage(&reg_bl,&reg_bh);
			break;
		case 0x1B:							/* PERFORM GRAY-SCALE SUMMING */
			INT10_PerformGrayScaleSumming(reg_bx,reg_cx);
			break;
		case 0xF0:							/* ET4000: SET HiColor GRAPHICS MODE */
		case 0xF1:							/* ET4000: GET DAC TYPE */
		case 0xF2:							/* ET4000: CHECK/SET HiColor MODE */
		default:
			LOG(LOG_INT10,LOG_ERROR)("Function 10:Unhandled EGA/VGA Palette Function %2X",reg_al);
			break;
		}
		break;

	// Character generator functions
	case 0x11:
		if (!is_machine_ega_or_better()) {
			break;
		}

		if ((reg_al & 0xf0) == 0x10) {
			MOUSEDOS_BeforeNewVideoMode();
		}

		switch (reg_al) {
		// Text mode functions
		// ===================
		// Load user font
		case 0x00: [[fallthrough]];
		// Load and activate user font
		case 0x10: {
			const auto font_data   = SegPhys(es) + reg_bp;
			const auto reload      = (reg_al == 0x10);
			const auto num_chars   = reg_cx;
			const auto first_char  = reg_dx;
			const auto font_block  = reg_bl;
			const auto char_height = reg_bh;

			INT10_LoadFont(font_data,
			               reload,
			               num_chars,
			               first_char,
			               font_block,
			               char_height);
		} break;

		// Load ROM 8x14 font
		case 0x01: [[fallthrough]];
		// Load and activate ROM 8x14 font
		case 0x11: {
			const auto font_data = RealToPhysical(int10.rom.font_14);
			const auto reload         = (reg_al == 0x11);
			constexpr auto NumChars   = 256;
			constexpr auto FirstChar  = 0;
			const auto font_block     = reg_bl;
			constexpr auto CharHeight = 14;

			INT10_LoadFont(font_data,
			               reload,
			               NumChars,
			               FirstChar,
			               font_block,
			               CharHeight);
		} break;

		// Load ROM 8x8 font
		case 0x02: [[fallthrough]];
		// Load and activate ROM 8x8 font
		case 0x12: {
			const auto font_data = RealToPhysical(int10.rom.font_8_first);
			const auto reload         = (reg_al == 0x12);
			constexpr auto NumChars   = 256;
			constexpr auto FirstChar  = 0;
			const auto font_block     = reg_bl;
			constexpr auto CharHeight = 8;

			INT10_LoadFont(font_data,
			               reload,
			               NumChars,
			               FirstChar,
			               font_block,
			               CharHeight);
		} break;

		// Set Block Specifier
		case 0x03:
			IO_Write(0x3c4, 0x3);
			IO_Write(0x3c5, reg_bl);
			break;

		// Load ROM 8x16 font
		case 0x04:
		// Load and activate ROM 8x16 font
		case 0x14: {
			if (!is_machine_vga_or_better()) {
				break;
			}

			const auto font_data = RealToPhysical(int10.rom.font_16);
			const auto reload         = (reg_al == 0x14);
			constexpr auto NumChars   = 256;
			constexpr auto FirstChar  = 0;
			const auto font_block     = reg_bl;
			constexpr auto CharHeight = 16;

			INT10_LoadFont(font_data,
			               reload,
			               NumChars,
			               FirstChar,
			               font_block,
			               CharHeight);
		} break;

		// Graphics mode calls
		// ===================
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

		case 0x23:			/* Rom 8x8 double dot set */
			RealSetVec(0x43,int10.rom.font_8_first);
			real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,8);
			goto graphics_chars;

		case 0x24:			/* Rom 8x16 set */
			if (!is_machine_vga_or_better()) {
				break;
			}
			RealSetVec(0x43,int10.rom.font_16);
			real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,16);
			goto graphics_chars;

graphics_chars:
			switch (reg_bl) {
			case 0x00:
				real_writeb(BIOSMEM_SEG, BIOSMEM_NB_ROWS, reg_dl - 1);
				break;
			case 0x01:
				real_writeb(BIOSMEM_SEG, BIOSMEM_NB_ROWS, 13);
				break;
			case 0x03:
				real_writeb(BIOSMEM_SEG, BIOSMEM_NB_ROWS, 42);
				break;
			case 0x02:
			default:
				real_writeb(BIOSMEM_SEG, BIOSMEM_NB_ROWS, 24);
				break;
			}
			break;
			/* General */
		case 0x30:/* Get Font Information */
			switch (reg_bh) {
			case 0x00:	/* interupt 0x1f vector */
				{
					RealPt int_1f=RealGetVec(0x1f);
					SegSet16(es,RealSegment(int_1f));
					reg_bp=RealOffset(int_1f);
				}
				break;

			case 0x01:	/* interupt 0x43 vector */
				{
					RealPt int_43=RealGetVec(0x43);
					SegSet16(es,RealSegment(int_43));
					reg_bp=RealOffset(int_43);
				}
				break;

			case 0x02:	/* font 8x14 */
				SegSet16(es,RealSegment(int10.rom.font_14));
				reg_bp=RealOffset(int10.rom.font_14);
				break;

			case 0x03:	/* font 8x8 first 128 */
				SegSet16(es,RealSegment(int10.rom.font_8_first));
				reg_bp=RealOffset(int10.rom.font_8_first);
				break;

			case 0x04:	/* font 8x8 second 128 */
				SegSet16(es,RealSegment(int10.rom.font_8_second));
				reg_bp=RealOffset(int10.rom.font_8_second);
				break;

			case 0x05:	/* alpha alternate 9x14 */
				SegSet16(es,RealSegment(int10.rom.font_14_alternate));
				reg_bp=RealOffset(int10.rom.font_14_alternate);
				break;

			case 0x06:	/* font 8x16 */
				if (!is_machine_vga_or_better()) {
					break;
				}
				SegSet16(es,RealSegment(int10.rom.font_16));
				reg_bp=RealOffset(int10.rom.font_16);
				break;

			case 0x07:	/* alpha alternate 9x16 */
				if (!is_machine_vga_or_better()) {
					break;
				}
				SegSet16(es,RealSegment(int10.rom.font_16_alternate));
				reg_bp=RealOffset(int10.rom.font_16_alternate);
				break;

			default:
				LOG(LOG_INT10,LOG_ERROR)("Function 11:30 Request for font %2X",reg_bh);
				break;
			}

			if ((reg_bh <= 7) || (svga_type == SvgaType::TsengEt4k)) {
				reg_cx=real_readw(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
				reg_dl=real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS);
			}
			break;

		default:
			LOG(LOG_INT10,LOG_ERROR)("Function 11:Unsupported character generator call %2X",reg_al);
			break;

		}
		if ((reg_al & 0xf0) == 0x10) {
			MOUSEDOS_AfterNewVideoMode(false);
		}
		break;

	case 0x12:								/* alternate function select */
		if (!is_machine_ega_or_better()) {
			break;
		}
		switch (reg_bl) {
		case 0x10:							/* Get EGA Information */
			reg_bh=(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS)==0x3B4);
			reg_bl=3;	//256 kb
			reg_cl=real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES) & 0x0F;
			reg_ch=real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES) >> 4;
			break;

		case 0x20:							/* Set alternate printscreen */
			break;

		case 0x30:							/* Select vertical resolution */
			{
				if (!is_machine_vga_or_better()) {
					break;
				}

				LOG(LOG_INT10,LOG_WARN)("Function 12:Call %2X (select vertical resolution)",reg_bl);

				if (reg_al > 2) {
					reg_al = 0;	// invalid VGA subfunction
					break;
				}
				uint8_t modeset_ctl = real_readb(BIOSMEM_SEG, BiosDataArea::VgaFlagsRecOffset);
				uint8_t video_switches = real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES)&0xf0;
				switch(reg_al) {
				case 0: // 200
					modeset_ctl &= 0xef;
					modeset_ctl |= 0x80;
					video_switches |= 8;	// ega normal/cga emulation
					break;

				case 1: // 350
					modeset_ctl &= 0x6f;
					video_switches |= 9;	// ega enhanced
					break;

				case 2: // 400
					modeset_ctl &= 0x6f;
					modeset_ctl |= 0x10;	// use 400-line mode at next mode set
					video_switches |= 9;	// ega enhanced
					break;

				default:
					modeset_ctl &= 0xef;
					video_switches |= 8;	// ega normal/cga emulation
					break;

				}
				real_writeb(BIOSMEM_SEG, BiosDataArea::VgaFlagsRecOffset,modeset_ctl);
				real_writeb(BIOSMEM_SEG,BIOSMEM_SWITCHES,video_switches);
				reg_al=0x12;	// success
				break;
			}
		case 0x31:							/* Palette loading on modeset */
			{
				if (!is_machine_vga_or_better()) {
					break;
				}
				if (svga_type == SvgaType::TsengEt4k) {
					reg_al &= 1;
				}
				if (reg_al > 1) {
					// invalid subfunction
					reg_al = 0;
					break;
				}
				uint8_t temp = real_readb(BIOSMEM_SEG, BiosDataArea::VgaFlagsRecOffset) & 0xf7;
				if (reg_al&1) temp|=8;		// enable if al=0
				real_writeb(BIOSMEM_SEG, BiosDataArea::VgaFlagsRecOffset, temp);
				reg_al=0x12;
				break;
			}
		case 0x32:							/* Video addressing */
			if (!is_machine_vga_or_better()) {
				break;
			}
			LOG(LOG_INT10,LOG_ERROR)("Function 12:Call %2X not handled",reg_bl);
			if (svga_type == SvgaType::TsengEt4k) {
				reg_al &= 1;
			}
			if (reg_al > 1) {
				// invalid subfunction
				reg_al = 0;
			} else {
				// fake a success call
				reg_al = 0x12;
			}
			break;

		case 0x33: /* SWITCH GRAY-SCALE SUMMING */
			{
				if (!is_machine_vga_or_better()) {
					break;
				}
				if (svga_type == SvgaType::TsengEt4k) {
					reg_al &= 1;
				}
				if (reg_al > 1) {
					reg_al = 0;
					break;
				}
				uint8_t temp = real_readb(BIOSMEM_SEG, BiosDataArea::VgaFlagsRecOffset) & 0xfd;
				if (!(reg_al&1)) temp|=2;		// enable if al=0
				real_writeb(BIOSMEM_SEG, BiosDataArea::VgaFlagsRecOffset, temp);
				reg_al=0x12;
				break;
			}
		case 0x34: /* ALTERNATE FUNCTION SELECT (VGA) - CURSOR EMULATION */
			{
				// bit 0: 0=enable, 1=disable
				if (!is_machine_vga_or_better()) {
					break;
				}
				if (svga_type == SvgaType::TsengEt4k) {
					reg_al &= 1;
				}
				if (reg_al > 1) {
					reg_al = 0;
					break;
				}
				uint8_t temp = real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL) & 0xfe;
				real_writeb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL,temp|reg_al);
				reg_al=0x12;
				break;
			}
		case 0x35:
			if (!is_machine_vga_or_better()) {
				break;
			}
			LOG(LOG_INT10,LOG_ERROR)("Function 12:Call %2X not handled",reg_bl);
			reg_al=0x12;
			break;

		case 0x36: {						/* VGA Refresh control */
			if (!is_machine_vga_or_better()) {
				break;
			}
			if ((svga_type == SvgaType::S3) && (reg_al > 1)) {
				reg_al = 0;
				break;
			}
			IO_Write(0x3c4,0x1);
			uint8_t clocking = IO_Read(0x3c5);

			if (reg_al==0) clocking &= ~0x20;
			else clocking |= 0x20;

			IO_Write(0x3c4,0x1);
			IO_Write(0x3c5,clocking);

			reg_al=0x12; // success
			break;
		}
		default:
			LOG(LOG_INT10,LOG_ERROR)("Function 12:Call %2X not handled",reg_bl);
			if (!is_machine_ega()) {
				reg_al = 0;
			}
			break;
		}
		break;

	case 0x13:								/* Write String */
		INT10_WriteString(reg_dh,reg_dl,reg_al,reg_bl,SegPhys(es)+reg_bp,reg_cx,reg_bh);
		break;

	case 0x1A:								/* Display Combination */
		if (!is_machine_vga_or_better()) {
			break;
		}
		if (reg_al<2) {
			INT10_DisplayCombinationCode(&reg_bx,reg_al==1);
			reg_ax=0x1A;	// high part destroyed or zeroed depending on BIOS
		}
		break;

	case 0x1B:								/* functionality State Information */
		if (!is_machine_vga_or_better()) {
			break;
		}
		switch (reg_bx) {
		case 0x0000:
			INT10_GetFuncStateInformation(SegPhys(es)+reg_di);
			reg_al=0x1B;
			break;

		default:
			LOG(LOG_INT10,LOG_ERROR)("1B:Unhandled call BX %2X",reg_bx);
			reg_al=0;
			break;

		}
		break;

	case 0x1C:	/* Video Save Area */
		if (!is_machine_vga_or_better()) {
			break;
		}
		switch (reg_al) {
			case 0: {
				Bitu ret=INT10_VideoState_GetSize(reg_cx);
				if (ret) {
					reg_al=0x1c;
					reg_bx=(uint16_t)ret;
				} else reg_al=0;
				}
				break;
			case 1:
				if (INT10_VideoState_Save(reg_cx,RealMake(SegValue(es),reg_bx))) reg_al=0x1c;
				else reg_al=0;
				break;
			case 2:
				if (INT10_VideoState_Restore(reg_cx,RealMake(SegValue(es),reg_bx))) reg_al=0x1c;
				else reg_al=0;
				break;
			default:
				if (svga_type == SvgaType::TsengEt4k) {
					reg_ax = 0;
				} else {
					reg_al = 0;
				}
				break;
		}
		break;

	case 0x4f:								/* VESA Calls */
		if (svga_type != SvgaType::S3) {
			break;
		}
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
			MOUSEDOS_BeforeNewVideoMode();
			reg_al=0x4f;
			reg_ah=VESA_SetSVGAMode(reg_bx);
			MOUSEDOS_AfterNewVideoMode(true);
			break;

		case 0x03:							/* Get videomode */
			reg_al=0x4f;
			reg_ah=VESA_GetSVGAMode(reg_bx);
			break;

		case 0x04:							/* Save/restore state */
			reg_al=0x4f;
			switch (reg_dl) {
				case 0: {
					Bitu ret=INT10_VideoState_GetSize(reg_cx);
					if (ret) {
						reg_ah=0;
						reg_bx=(uint16_t)ret;
					} else reg_ah=1;
					}
					break;

				case 1:
					if (INT10_VideoState_Save(reg_cx,RealMake(SegValue(es),reg_bx))) reg_ah=0;
					else reg_ah=1;
					break;

				case 2:
					if (INT10_VideoState_Restore(reg_cx,RealMake(SegValue(es),reg_bx))) reg_ah=0;
					else reg_ah=1;
					break;

				default:
					reg_ah=1;
					break;
			}
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
			reg_ah=VESA_ScanLineLength(reg_bl,reg_cx,reg_bx,reg_cx,reg_dx);
			break;

		case 0x07:
			switch (reg_bl) {
			case 0x80:						/* Set Display Start during retrace */
			case 0x00:						/* Set display Start */
				reg_al=0x4f;
				reg_ah=VESA_SetDisplayStart(reg_cx,reg_dx,reg_bl==0x80);
				break;

			case 0x01:
				reg_al=0x4f;
				reg_bh=0x00;				//reserved
				reg_ah=VESA_GetDisplayStart(reg_cx,reg_dx);
				break;

			default:
				LOG(LOG_INT10,LOG_ERROR)("Unhandled VESA Function %X Subfunction %X",reg_al,reg_bl);
				reg_ah=0x1;
				break;
			}
			break;

		case 0x09:
			switch (reg_bl) {
			case 0x80:						/* Set Palette during retrace */
			case 0x00:						/* Set Palette */
				reg_ah=VESA_SetPalette(SegPhys(es)+reg_di,reg_dx,reg_cx,reg_bl==0x80);
				reg_al=0x4f;
				break;

			case 0x01:						/* Get Palette */
				reg_ah=VESA_GetPalette(SegPhys(es)+reg_di,reg_dx,reg_cx);
				reg_al=0x4f;
				break;

			default:
				LOG(LOG_INT10,LOG_ERROR)("Unhandled VESA Function %X Subfunction %X",reg_al,reg_bl);
				reg_ah=0x01;
				break;
			}
			break;

		case 0x0a:							/* Get Pmode Interface */
			if (int10.vesa_oldvbe) {
				reg_ax=0x014f;
				break;
			}
			switch (reg_bl) {
			case 0x00:
				SegSet16(es,RealSegment(int10.rom.pmode_interface));
				reg_di=RealOffset(int10.rom.pmode_interface);
				reg_cx=int10.rom.pmode_interface_size;
				reg_ax=0x004f;
				break;

			case 0x01:						/* Get code for "set window" */
				SegSet16(es,RealSegment(int10.rom.pmode_interface));
				reg_di=RealOffset(int10.rom.pmode_interface)+int10.rom.pmode_interface_window;
				reg_cx=int10.rom.pmode_interface_start-int10.rom.pmode_interface_window;
				reg_ax=0x004f;
				break;

			case 0x02:						/* Get code for "set display start" */
				SegSet16(es,RealSegment(int10.rom.pmode_interface));
				reg_di=RealOffset(int10.rom.pmode_interface)+int10.rom.pmode_interface_start;
				reg_cx=int10.rom.pmode_interface_palette-int10.rom.pmode_interface_start;
				reg_ax=0x004f;
				break;

			case 0x03:						/* Get code for "set palette" */
				SegSet16(es,RealSegment(int10.rom.pmode_interface));
				reg_di=RealOffset(int10.rom.pmode_interface)+int10.rom.pmode_interface_palette;
				reg_cx=int10.rom.pmode_interface_size-int10.rom.pmode_interface_palette;
				reg_ax=0x004f;
				break;

			default:
				reg_ax=0x014f;
				break;
			}
			break;

		default:
			LOG(LOG_INT10,LOG_ERROR)("Unhandled VESA Function %X",reg_al);
			reg_al=0x0;
			break;
		}
		break;

	case 0xf0:
		INT10_EGA_RIL_ReadRegister(reg_bl, reg_dx);
		break;

	case 0xf1:
		INT10_EGA_RIL_WriteRegister(reg_bl, reg_bh, reg_dx);
		break;

	case 0xf2:
		INT10_EGA_RIL_ReadRegisterRange(reg_ch, reg_cl, reg_dx, SegPhys(es)+reg_bx);
		break;

	case 0xf3:
		INT10_EGA_RIL_WriteRegisterRange(reg_ch, reg_cl, reg_dx, SegPhys(es)+reg_bx);
		break;

	case 0xf4:
		INT10_EGA_RIL_ReadRegisterSet(reg_cx, SegPhys(es)+reg_bx);
		break;

	case 0xf5:
		INT10_EGA_RIL_WriteRegisterSet(reg_cx, SegPhys(es)+reg_bx);
		break;

	case 0xfa: {
		RealPt pt=INT10_EGA_RIL_GetVersionPt();
		SegSet16(es,RealSegment(pt));
		reg_bx=RealOffset(pt);
		}
		break;

	case 0xff:
		if (!warned_ff) {
			LOG(LOG_INT10, LOG_NORMAL)("INT10: FF:Weird NC call");
		}
		warned_ff = true;
		break;

	default:
		LOG(LOG_INT10,LOG_ERROR)("Function %4X not supported",reg_ax);
//		reg_al=0x00;		//Successfull, breaks marriage
		break;
	};
	return CBRET_NONE;
}

static void INT10_Seg40Init(void) {
	// Set the default MSR
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x09);
	if (is_machine_ega_or_better()) {
		// Set the default char height
		real_writeb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,16);
		// Clear the screen
		real_writeb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL,0x60);
		// Set the basic screen we have
		real_writeb(BIOSMEM_SEG,BIOSMEM_SWITCHES,0xF9);
		// Set the basic modeset options
		real_writeb(BIOSMEM_SEG, BiosDataArea::VgaFlagsRecOffset,0x51);
		// Set the pointer to video save pointer table
		real_writed(BIOSMEM_SEG,BIOSMEM_VS_POINTER,int10.rom.video_save_pointers);
	}
}

static void INT10_InitVGA()
{
	if (is_machine_ega_or_better()) {
		// Switch to color mode and enable CPU access 480 lines
		IO_Write(0x3c2, 0xc3);

		// More than 64k
		IO_Write(0x3c4, 0x04);
		IO_Write(0x3c5, 0x02);

		if (is_machine_vga_or_better()) {
			// Initialize 256-colour VGA DAC palette to black
			IO_Write(0x3c8, 0);

			for (auto i = 0; i < NumVgaColors; ++i) {
				constexpr Rgb666 black = {0, 0, 0};

				IO_Write(0x3c9, black.red);
				IO_Write(0x3c9, black.green);
				IO_Write(0x3c9, black.blue);
			}
		}
	}
}

static void SetupTandyBios()
{
	static uint8_t TandyConfig[130]= {
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

	if (is_machine_tandy()) {
		Bitu i;
		for(i=0;i<130;i++) {
			phys_writeb(0xf0000+i+0xc000, TandyConfig[i]);
		}
	}
}

void INT10_Init(Section* /*sec*/)
{
	INT10_SetupPalette();
	INT10_InitVGA();
	if (is_machine_pcjr_or_tandy()) {
		SetupTandyBios();
	}
	/* Setup the INT 10 vector */
	call_10=CALLBACK_Allocate();
	CALLBACK_Setup(call_10,&INT10_Handler,CB_IRET,"Int 10 video");
	RealSetVec(0x10,CALLBACK_RealPointer(call_10));
	//Init the 0x40 segment and init the datastructures in the video rom area
	INT10_SetupRomMemory();
	INT10_Seg40Init();
	INT10_SetVideoMode(0x3);
}
