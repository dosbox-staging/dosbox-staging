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

#ifndef VGA_H_
#define VGA_H_

#include <mem.h>

enum { TEXT, GRAPH };
enum { GFX_256C,GFX_256U,GFX_16,GFX_4,GFX_2, TEXT_16 };

typedef struct {

	bool attrindex;
} VGA_Internal;

typedef struct {

/* Video drawing */
	Bit16u display_start;
	Bit16u real_start;
	bool retrace;					/* A retrace has started */
	Bitu scan_len;

/* Screen resolution and memory mode */	
	Bitu vdisplayend;
	Bitu hdisplayend;

	bool chained;					/* Enable or Disabled Chain 4 Mode */
	bool gfxmode;					/* Yes or No Easy no */
	bool blinking;					/* Attribute bit 7 is blinking */

	bool vga_enabled;
	bool cga_enabled;

	bool vline_double;
	Bit8u vline_height;

	bool pixel_double;
	/* Pixel Scrolling */
	Bit8u pel_panning;				/* Amount of pixels to skip when starting horizontal line */
	Bit8u hlines_skip;
	Bit8u bytes_skip;

/* Specific stuff memory write/read handling */
	Bit8u read_mode;
	Bit8u write_mode;
	Bit8u read_map_select;
	Bit8u color_dont_care;
	Bit8u color_compare;
	Bit8u data_rotate;
	Bit8u raster_op;

	Bit32u full_bit_mask;
	Bit32u full_map_mask;
	Bit32u full_not_map_mask;
	Bit32u full_set_reset;
	Bit32u full_not_enable_set_reset;
	Bit32u full_enable_set_reset;
	Bit32u full_enable_and_set_reset;

} VGA_Config;

typedef struct {
	bool resizing;
	Bitu width;
	Bitu height;
	Bit8u font_height;
	Bit8u cursor_enable;
	Bit8u cursor_row;
	Bit8u cursor_col;
	Bit8u cursor_count;
} VGA_Draw;


typedef struct {
	Bit8u index;

	Bit8u reset;
	Bit8u clocking_mode;
	Bit8u map_mask;
	Bit8u character_map_select;
	Bit8u memory_mode;
} VGA_Seq;


typedef struct {
	Bit8u palette[16];
	Bit8u mode_control;
	Bit8u horizontal_pel_panning;
	Bit8u overscan_color;
	Bit8u color_plane_enable;
	Bit8u color_select;
	Bit8u index;
} VGA_Attr;


typedef struct {
	Bit8u horizontal_total;
	Bit8u horizontal_display_end;
	Bit8u start_horizontal_blanking;
	Bit8u end_horizontal_blanking;
	Bit8u start_horizontal_retrace;
	Bit8u end_horizontal_retrace;
	Bit8u vertical_total;
	Bit8u overflow;
	Bit8u preset_row_scan;
	Bit8u maximum_scan_line;
	Bit8u cursor_start;
	Bit8u cursor_end;
	Bit8u start_address_high;
	Bit8u start_address_low;
	Bit8u cursor_location_high;
	Bit8u cursor_location_low;
	Bit8u vertical_retrace_start;
	Bit8u vertical_retrace_end;
	Bit8u vertical_display_end;
	Bit8u offset;
	Bit8u underline_location;
	Bit8u start_vertical_blank;
	Bit8u end_vertical_blank;
	Bit8u mode_control;
	Bit8u line_compare;

	Bit8u index;
} VGA_Crtc;

typedef struct {
	Bit8u index;
	Bit8u set_reset;
	Bit8u enable_set_reset;
	Bit8u color_compare;
	Bit8u data_rotate;
	Bit8u read_map_select;
	Bit8u mode;
	Bit8u miscellaneous;
	Bit8u color_dont_care;
	Bit8u bit_mask;
} VGA_Gfx;

struct RGBEntry {
	Bit8u red;
	Bit8u green;
	Bit8u blue;
	Bit8u attr_entry;
};

typedef struct {
	Bit8u bits;						/* DAC bits, usually 6 or 8 */
	Bit8u pel_mask;
	Bit8u pel_index;	
	Bit8u state;
	Bit8u index;
	Bitu first_changed;
	RGBEntry rgb[0x100];
} VGA_Dac;

union VGA_Latch {
	Bit32u d;
	Bit8u b[4];
};

union VGA_Memory {
	Bit8u linear[64*1024*4];
	Bit8u paged[64*1024][4];
	VGA_Latch latched[64*1024];
};	



typedef struct {
	Bitu mode;								/* The mode the vga system is in */
	VGA_Draw draw;
	VGA_Config config;
	VGA_Internal internal;
/* Internal module groups */
	VGA_Seq seq;
	VGA_Attr attr;
	VGA_Crtc crtc;
	VGA_Gfx gfx;
	VGA_Dac dac;
	VGA_Latch latch;
	VGA_Memory mem;
//Special little hack to let the memory run over into the buffer
	Bit8u buffer[1024*1024];
} VGA_Type;





/* Functions for different resolutions */
//void VGA_FindSize(void);
void VGA_FindSettings(void);
void VGA_StartResize(void);

/* The Different Drawing functions */
void VGA_DrawTEXT(Bit8u * bitdata,Bitu next_line);
void VGA_DrawGFX256_Fast(Bit8u * bitdata,Bitu next_line);
void VGA_DrawGFX256_Full(Bit8u * bitdata,Bitu next_line);
void VGA_DrawGFX16_Full(Bit8u * bitdata,Bitu next_line);
void VGA_DrawGFX4_Full(Bit8u * bitdata,Bitu next_line);
void VGA_DrawGFX2_Full(Bit8u * bitdata,Bitu next_line);
/* The Different Memory Read/Write Handlers */
Bit8u VGA_NormalReadHandler(Bit32u start);

void VGA_GFX_256U_WriteHandler(Bit32u start,Bit8u val);
void VGA_GFX_16_WriteHandler(Bit32u start,Bit8u val);
void VGA_GFX_4_WriteHandler(Bit32u start,Bit8u val);

Bit8u VGA_GFX_4_ReadHandler(Bit32u start);



/* Some support functions */
void VGA_DAC_CombineColor(Bit8u attr,Bit8u pal);


/* The VGA Subfunction startups */
void VGA_SetupAttr(void);
void VGA_SetupMemory(void);
void VGA_SetupDAC(void);
void VGA_SetupCRTC(void);
void VGA_SetupMisc(void);
void VGA_SetupGFX(void);
void VGA_SetupSEQ(void);

/* Some Support Functions */
void VGA_DACSetEntirePalette(void);

extern VGA_Type vga;
extern Bit8u vga_rom_8[256 * 8];
extern Bit8u vga_rom_14[256 * 14];
extern Bit8u vga_rom_16[256 * 16];

extern Bit32u ExpandTable[256];
extern Bit32u FillTable[16];
extern Bit32u CGAWriteTable[256];
extern Bit32u Expand16Table[4][16];
extern Bit32u Expand16BigTable[0x10000];

#if DEBUG_VGA
#define LOG_VGA LOG_DEBUG
#else
#define LOG_VGA
#endif


#endif

