/*
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_INT10_H
#define DOSBOX_INT10_H

#include "dosbox.h"

#include <vector>

#include "mem.h"
#include "vga.h"

// forward declarations
class Rgb666;

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
#define BIOSMEM_CRTCPU_PAGE   0x8A
#define BIOSMEM_VS_POINTER    0xA8

constexpr uint16_t MinVesaBiosModeNumber = 0x100;
constexpr uint16_t MaxVesaBiosModeNumber = 0x7ff;

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
#define VGAREG_TDY_RESET               0x3da
#define VGAREG_TDY_ADDRESS             0x3da
#define VGAREG_TDY_DATA                0x3de
#define VGAREG_PCJR_DATA               0x3da

#define VGAREG_MDA_MODECTL             0x3b8
#define VGAREG_CGA_MODECTL             0x3d8
#define VGAREG_CGA_PALETTE             0x3d9

/* Video memory */
#define VGAMEM_GRAPH 0xA000
#define VGAMEM_CTEXT 0xB800
#define VGAMEM_MTEXT 0xB000

#define BIOS_NCOLS uint16_t ncols=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
#define BIOS_NROWS uint16_t nrows=IS_EGAVGA_ARCH?((uint16_t)real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1):25;
#define BIOS_CHEIGHT uint8_t cheight=IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT):8;

uint16_t INT10_GetTextColumns();
uint16_t INT10_GetTextRows();

extern uint8_t int10_font_08[256 * 8];
extern uint8_t int10_font_14[256 * 14];
extern uint8_t int10_font_16[256 * 16];
extern uint8_t int10_font_14_alternate[20 * 15 + 1];
extern uint8_t int10_font_16_alternate[19 * 17 + 1];

struct palette_t {
	// 64 entries
	std::vector<Rgb666> mono_text = {};

	// 64 entries
	std::vector<Rgb666> mono_text_s3 = {};

	// 16 entries. This is default canonical 16-colour CGA palette as emulated
	// by VGA cards.
	std::vector<Rgb666> cga16 = {};

	// 64 entries. This is the default 64-colour 6-bit RGB EGA palette as
	// emulated by VGA cards. The BIOS sets up these colours in the first 64
	// of the 256 VGA colour registers in EGA modes.
	std::vector<Rgb666> cga64 = {};

	// 64 entries
	std::vector<Rgb666> ega = {};

	// 256 entries. This is the default 256-colour VGA palette.
	std::vector<Rgb666> vga = {};
};

extern palette_t palette;

struct VideoModeBlock {
	// BIOS video mode number
	uint16_t mode;

	VGAModes type;

	uint16_t swidth, sheight;
	uint8_t twidth, theight;
	uint8_t cwidth, cheight;
	uint8_t ptotal;
	uint32_t pstart;
	uint32_t plength;

	uint16_t htotal, vtotal;
	uint16_t hdispend, vdispend;
	uint16_t special;
};

extern std::vector<VideoModeBlock> ModeList_VGA;
extern std::vector<VideoModeBlock> ModeList_VGA_Paradise;
extern std::vector<VideoModeBlock> ModeList_VGA_Tseng;

using video_mode_block_iterator_t = std::vector<VideoModeBlock>::const_iterator;

// Holds the last set "coarse" video mode via an INT 10H BIOS call.
// A "refined", more accurate version of the current mode is stored in the
// `vga.mode` global (e.g. M_TEXT may get refined into M_CGA_TEXT_COMPOSITE,
// M_CGA4 into M_TANDY4 or M_CGA4_COMPOSITE, etc.)
extern video_mode_block_iterator_t CurMode;

enum class VesaModePref {
	Compatible,  // Prunes the available S3 modes to maximize DOS game compatibility
	Halfline, // Replaces mode 120h with the halfline mode used by Extreme Assault
	All, // Enables all S3 864 and Trio VESA modes (but some games might not handle them properly)
};

struct Int10Data {
	struct Int10DataRom{
		RealPt font_8_first;
		RealPt font_8_second;
		RealPt font_14;
		RealPt font_16;
		RealPt font_14_alternate;
		RealPt font_16_alternate;
		RealPt static_state;
		RealPt video_save_pointers;
		RealPt video_parameter_table;
		RealPt video_save_pointer_table;
		RealPt video_dcc_table;
		RealPt oemstring;
		RealPt vesa_modes;
		RealPt wait_retrace;
		RealPt set_window;
		RealPt pmode_interface;
		uint16_t pmode_interface_size;
		uint16_t pmode_interface_start;
		uint16_t pmode_interface_window;
		uint16_t pmode_interface_palette;
		uint16_t used;
	} rom = {};
	uint16_t vesa_setmode = 0;

	VesaModePref vesa_mode_preference = VesaModePref::Compatible;
	bool vesa_nolfb = false;
	bool vesa_oldvbe = false;
};

extern Int10Data int10;

inline uint8_t CURSOR_POS_COL(const uint8_t page)
{
	const auto cursor_offset = static_cast<uint16_t>(page * 2);
	return real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_POS + cursor_offset);
}

inline uint8_t CURSOR_POS_ROW(const uint8_t page)
{
	const auto cursor_offset = static_cast<uint16_t>(page * 2 + 1);
	return real_readb(BIOSMEM_SEG, BIOSMEM_CURSOR_POS + cursor_offset);
}

void INT10_SetupPalette();

bool INT10_SetVideoMode(uint16_t mode);
void INT10_SetCurMode(void);

bool INT10_IsTextMode(const VideoModeBlock& mode_block);

void INT10_ScrollWindow(uint8_t rul,uint8_t cul,uint8_t rlr,uint8_t clr,int8_t nlines,uint8_t attr,uint8_t page);

void INT10_SetActivePage(uint8_t page);
void INT10_DisplayCombinationCode(uint16_t * dcc,bool set);
void INT10_GetFuncStateInformation(PhysPt save);

void INT10_SetCursorShape(uint8_t first,uint8_t last);

void INT10_SetCursorPos(uint8_t row,uint8_t col,uint8_t page);
void INT10_SetCursorPosViaInterrupt(const uint8_t row, const uint8_t col,
                                    const uint8_t page);

void INT10_TeletypeOutput(const uint8_t char_value, const uint8_t attribute);
void INT10_TeletypeOutputViaInterrupt(const uint8_t char_value,
                                      const uint8_t attribute);

void INT10_TeletypeOutputAttr(const uint8_t char_value, const uint8_t attribute,
                              const bool use_attribute);
void INT10_TeletypeOutputAttrViaInterrupt(const uint8_t char_value,
                                          const uint8_t attribute,
                                          const bool use_attribute);

void INT10_ReadCharAttr(uint16_t* result, uint8_t page);

void INT10_WriteChar(const uint8_t char_value, const uint8_t attribute,
                     uint8_t page, uint16_t count, bool use_attribute);
void INT10_WriteCharViaInterrupt(const uint8_t char_value, const uint8_t attribute,
                                 uint8_t page, uint16_t count, bool use_attribute);

void INT10_WriteString(uint8_t row, uint8_t col, uint8_t flag, uint8_t attr,
                       PhysPt string, uint16_t count, uint8_t page);

/* Graphics Stuff */
void INT10_PutPixel(uint16_t x,uint16_t y,uint8_t page,uint8_t color);
void INT10_GetPixel(uint16_t x,uint16_t y,uint8_t page,uint8_t * color);

/* Font Stuff */
void INT10_LoadFont(PhysPt font,bool reload,Bitu count,Bitu offset,Bitu map,Bitu height);
void INT10_ReloadFont(void);

/* Palette Group */
void INT10_SetBackgroundBorder(uint8_t val);
void INT10_SetColorSelect(uint8_t val);
void INT10_SetSinglePaletteRegister(uint8_t reg, uint8_t val);
void INT10_SetOverscanBorderColor(uint8_t val);
void INT10_SetAllPaletteRegisters(PhysPt data);
void INT10_ToggleBlinkingBit(uint8_t state);
void INT10_GetSinglePaletteRegister(uint8_t reg,uint8_t * val);
void INT10_GetOverscanBorderColor(uint8_t * val);
void INT10_GetAllPaletteRegisters(PhysPt data);
void INT10_SetSingleDACRegister(uint8_t index,uint8_t red,uint8_t green,uint8_t blue);
void INT10_GetSingleDACRegister(uint8_t index,uint8_t * red,uint8_t * green,uint8_t * blue);
void INT10_SetDACBlock(uint16_t index,uint16_t count,PhysPt data);
void INT10_GetDACBlock(uint16_t index,uint16_t count,PhysPt data);
void INT10_SelectDACPage(uint8_t function,uint8_t mode);
void INT10_GetDACPage(uint8_t* mode,uint8_t* page);
void INT10_SetPelMask(uint8_t mask);
void INT10_GetPelMask(uint8_t & mask);
void INT10_PerformGrayScaleSumming(uint16_t start_reg,uint16_t count);


/* Vesa Group */
uint8_t VESA_GetSVGAInformation(const uint16_t segment, const uint16_t offset);
bool VESA_IsVesaMode(const uint16_t bios_mode_number);
uint8_t VESA_GetSVGAModeInformation(uint16_t mode,uint16_t seg,uint16_t off);
uint8_t VESA_SetSVGAMode(uint16_t mode);
uint8_t VESA_GetSVGAMode(uint16_t & mode);
uint8_t VESA_SetCPUWindow(uint8_t window,uint8_t address);
uint8_t VESA_GetCPUWindow(uint8_t window,uint16_t & address);
uint8_t VESA_ScanLineLength(uint8_t subcall, uint16_t val, uint16_t & bytes,uint16_t & pixels,uint16_t & lines);
uint8_t VESA_SetDisplayStart(uint16_t x,uint16_t y,bool wait);
uint8_t VESA_GetDisplayStart(uint16_t & x,uint16_t & y);
uint8_t VESA_SetPalette(PhysPt data,Bitu index,Bitu count,bool wait);
uint8_t VESA_GetPalette(PhysPt data,Bitu index,Bitu count);

/* Sub Groups */
void INT10_SetupRomMemory(void);
void INT10_SetupRomMemoryChecksum(void);
void INT10_SetupVESA(void);

/* EGA RIL */
RealPt INT10_EGA_RIL_GetVersionPt(void);
void INT10_EGA_RIL_ReadRegister(uint8_t & bl, uint16_t dx);
void INT10_EGA_RIL_WriteRegister(uint8_t & bl, uint8_t bh, uint16_t dx);
void INT10_EGA_RIL_ReadRegisterRange(uint8_t ch, uint8_t cl, uint16_t dx, PhysPt dst);
void INT10_EGA_RIL_WriteRegisterRange(uint8_t ch, uint8_t cl, uint16_t dx, PhysPt dst);
void INT10_EGA_RIL_ReadRegisterSet(uint16_t cx, PhysPt tbl);
void INT10_EGA_RIL_WriteRegisterSet(uint16_t cx, PhysPt tbl);

/* Video State */
Bitu INT10_VideoState_GetSize(Bitu state);
bool INT10_VideoState_Save(Bitu state,RealPt buffer);
bool INT10_VideoState_Restore(Bitu state,RealPt buffer);

/* Video Parameter Tables */
uint16_t INT10_SetupVideoParameterTable(PhysPt basepos);
void INT10_SetupBasicVideoParameterTable(void);

#endif
