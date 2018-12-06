/*
 *  Copyright (C) 2002-2018  The DOSBox Team
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


#include "dosbox.h"
//#include "setup.h"
#include "video.h"
#include "pic.h"
#include "vga.h"

#include "../save_state.h"
#include "mem.h"

#include <string.h>

VGA_Type vga;
SVGA_Driver svga;

Bit32u CGA_2_Table[16];
Bit32u CGA_4_Table[256];
Bit32u CGA_4_HiRes_Table[256];
Bit32u CGA_16_Table[256];
Bit32u TXT_Font_Table[16];
Bit32u TXT_FG_Table[16];
Bit32u TXT_BG_Table[16];
Bit32u ExpandTable[256];
Bit32u Expand16Table[4][16];
Bit32u FillTable[16];
Bit32u ColorTable[16];

void VGA_SetModeNow(VGAModes mode) {
	if (vga.mode == mode) return;
	vga.mode=mode;
	VGA_SetupHandlers();
	VGA_StartResize(0);
}


void VGA_SetMode(VGAModes mode) {
	if (vga.mode == mode) return;
	vga.mode=mode;
	VGA_SetupHandlers();
	VGA_StartResize();
}

void VGA_DetermineMode(void) {
	if (svga.determine_mode) {
		svga.determine_mode();
		return;
	}
	/* Test for VGA output active or direct color modes */
	switch (vga.s3.misc_control_2 >> 4) {
	case 0:
		if (vga.attr.mode_control & 1) { // graphics mode
			if (IS_VGA_ARCH && (vga.gfx.mode & 0x40)) {
				// access above 256k?
				if (vga.s3.reg_31 & 0x8) VGA_SetMode(M_LIN8);
				else VGA_SetMode(M_VGA);
			}
			else if (vga.gfx.mode & 0x20) VGA_SetMode(M_CGA4);
			else if ((vga.gfx.miscellaneous & 0x0c)==0x0c) VGA_SetMode(M_CGA2);
			else {
				// access above 256k?
				if (vga.s3.reg_31 & 0x8) VGA_SetMode(M_LIN4);
				else VGA_SetMode(M_EGA);
			}
		} else {
			VGA_SetMode(M_TEXT);
		}
		break;
	case 1:VGA_SetMode(M_LIN8);break;
	case 3:VGA_SetMode(M_LIN15);break;
	case 5:VGA_SetMode(M_LIN16);break;
	case 13:VGA_SetMode(M_LIN32);break;
	}
}

void VGA_StartResize(Bitu delay /*=50*/) {
	if (!vga.draw.resizing) {
		vga.draw.resizing=true;
		if (vga.mode==M_ERROR) delay = 5;
		/* Start a resize after delay (default 50 ms) */
		if (delay==0) VGA_SetupDrawing(0);
		else PIC_AddEvent(VGA_SetupDrawing,(float)delay);
	}
}

void VGA_SetClock(Bitu which,Bitu target) {
	if (svga.set_clock) {
		svga.set_clock(which, target);
		return;
	}
	struct{
		Bitu n,m;
		Bits err;
	} best;
	best.err=target;
	best.m=1;
	best.n=1;
	Bitu n,r;
	Bits m;

	for (r = 0; r <= 3; r++) {
		Bitu f_vco = target * (1 << r);
		if (MIN_VCO <= f_vco && f_vco < MAX_VCO) break;
    }
	for (n=1;n<=31;n++) {
		m=(target * (n + 2) * (1 << r) + (S3_CLOCK_REF/2)) / S3_CLOCK_REF - 2;
		if (0 <= m && m <= 127)	{
			Bitu temp_target = S3_CLOCK(m,n,r);
			Bits err = target - temp_target;
			if (err < 0) err = -err;
			if (err < best.err) {
				best.err = err;
				best.m = m;
				best.n = n;
			}
		}
    }
	/* Program the s3 clock chip */
	vga.s3.clk[which].m=best.m;
	vga.s3.clk[which].r=r;
	vga.s3.clk[which].n=best.n;
	VGA_StartResize();
}

void VGA_SetCGA2Table(Bit8u val0,Bit8u val1) {
	Bit8u total[2]={ val0,val1};
	for (Bitu i=0;i<16;i++) {
		CGA_2_Table[i]=
#ifdef WORDS_BIGENDIAN
			(total[(i >> 0) & 1] << 0  ) | (total[(i >> 1) & 1] << 8  ) |
			(total[(i >> 2) & 1] << 16 ) | (total[(i >> 3) & 1] << 24 );
#else 
			(total[(i >> 3) & 1] << 0  ) | (total[(i >> 2) & 1] << 8  ) |
			(total[(i >> 1) & 1] << 16 ) | (total[(i >> 0) & 1] << 24 );
#endif
	}
}

void VGA_SetCGA4Table(Bit8u val0,Bit8u val1,Bit8u val2,Bit8u val3) {
	Bit8u total[4]={ val0,val1,val2,val3};
	for (Bitu i=0;i<256;i++) {
		CGA_4_Table[i]=
#ifdef WORDS_BIGENDIAN
			(total[(i >> 0) & 3] << 0  ) | (total[(i >> 2) & 3] << 8  ) |
			(total[(i >> 4) & 3] << 16 ) | (total[(i >> 6) & 3] << 24 );
#else
			(total[(i >> 6) & 3] << 0  ) | (total[(i >> 4) & 3] << 8  ) |
			(total[(i >> 2) & 3] << 16 ) | (total[(i >> 0) & 3] << 24 );
#endif
		CGA_4_HiRes_Table[i]=
#ifdef WORDS_BIGENDIAN
			(total[((i >> 0) & 1) | ((i >> 3) & 2)] << 0  ) | (total[((i >> 1) & 1) | ((i >> 4) & 2)] << 8  ) |
			(total[((i >> 2) & 1) | ((i >> 5) & 2)] << 16 ) | (total[((i >> 3) & 1) | ((i >> 6) & 2)] << 24 );
#else
			(total[((i >> 3) & 1) | ((i >> 6) & 2)] << 0  ) | (total[((i >> 2) & 1) | ((i >> 5) & 2)] << 8  ) |
			(total[((i >> 1) & 1) | ((i >> 4) & 2)] << 16 ) | (total[((i >> 0) & 1) | ((i >> 3) & 2)] << 24 );
#endif
	}	
}

void VGA_Init(Section* sec) {
//	Section_prop * section=static_cast<Section_prop *>(sec);
	vga.draw.resizing=false;
	vga.mode=M_ERROR;			//For first init
	SVGA_Setup_Driver();
	VGA_SetupMemory(sec);
	VGA_SetupMisc();
	VGA_SetupDAC();
	VGA_SetupGFX();
	VGA_SetupSEQ();
	VGA_SetupAttr();
	VGA_SetupOther();
	VGA_SetupXGA();
	VGA_SetClock(0,CLK_25);
	VGA_SetClock(1,CLK_28);
/* Generate tables */
	VGA_SetCGA2Table(0,1);
	VGA_SetCGA4Table(0,1,2,3);
	Bitu i,j;
	for (i=0;i<256;i++) {
		ExpandTable[i]=i | (i << 8)| (i <<16) | (i << 24);
	}
	for (i=0;i<16;i++) {
		TXT_FG_Table[i]=i | (i << 8)| (i <<16) | (i << 24);
		TXT_BG_Table[i]=i | (i << 8)| (i <<16) | (i << 24);
#ifdef WORDS_BIGENDIAN
		FillTable[i]=
			((i & 1) ? 0xff000000 : 0) |
			((i & 2) ? 0x00ff0000 : 0) |
			((i & 4) ? 0x0000ff00 : 0) |
			((i & 8) ? 0x000000ff : 0) ;
		TXT_Font_Table[i]=
			((i & 1) ? 0x000000ff : 0) |
			((i & 2) ? 0x0000ff00 : 0) |
			((i & 4) ? 0x00ff0000 : 0) |
			((i & 8) ? 0xff000000 : 0) ;
#else 
		FillTable[i]=
			((i & 1) ? 0x000000ff : 0) |
			((i & 2) ? 0x0000ff00 : 0) |
			((i & 4) ? 0x00ff0000 : 0) |
			((i & 8) ? 0xff000000 : 0) ;
		TXT_Font_Table[i]=	
			((i & 1) ? 0xff000000 : 0) |
			((i & 2) ? 0x00ff0000 : 0) |
			((i & 4) ? 0x0000ff00 : 0) |
			((i & 8) ? 0x000000ff : 0) ;
#endif
	}
	for (j=0;j<4;j++) {
		for (i=0;i<16;i++) {
#ifdef WORDS_BIGENDIAN
			Expand16Table[j][i] =
				((i & 1) ? 1 << j : 0) |
				((i & 2) ? 1 << (8 + j) : 0) |
				((i & 4) ? 1 << (16 + j) : 0) |
				((i & 8) ? 1 << (24 + j) : 0);
#else
			Expand16Table[j][i] =
				((i & 1) ? 1 << (24 + j) : 0) |
				((i & 2) ? 1 << (16 + j) : 0) |
				((i & 4) ? 1 << (8 + j) : 0) |
				((i & 8) ? 1 << j : 0);
#endif
		}
	}
}

void SVGA_Setup_Driver(void) {
	memset(&svga, 0, sizeof(SVGA_Driver));

	switch(svgaCard) {
	case SVGA_S3Trio:
		SVGA_Setup_S3Trio();
		break;
	case SVGA_TsengET4K:
		SVGA_Setup_TsengET4K();
		break;
	case SVGA_TsengET3K:
		SVGA_Setup_TsengET3K();
		break;
	case SVGA_ParadisePVGA1A:
		SVGA_Setup_ParadisePVGA1A();
		break;
	default:
		vga.vmemsize = vga.vmemwrap = 256*1024;
		break;
	}
}

extern void POD_Save_VGA_Draw( std::ostream & );
extern void POD_Save_VGA_Seq( std::ostream & );
extern void POD_Save_VGA_Attr( std::ostream & );
extern void POD_Save_VGA_Crtc( std::ostream & );
extern void POD_Save_VGA_Gfx( std::ostream & );
extern void POD_Save_VGA_Dac( std::ostream & );
extern void POD_Save_VGA_S3( std::ostream & );
extern void POD_Save_VGA_Other( std::ostream & );
extern void POD_Save_VGA_Memory( std::ostream & );
extern void POD_Save_VGA_Paradise( std::ostream & );
extern void POD_Save_VGA_Tseng( std::ostream & );
extern void POD_Save_VGA_XGA( std::ostream & );
extern void POD_Load_VGA_Draw( std::istream & );
extern void POD_Load_VGA_Seq( std::istream & );
extern void POD_Load_VGA_Attr( std::istream & );
extern void POD_Load_VGA_Crtc( std::istream & );
extern void POD_Load_VGA_Gfx( std::istream & );
extern void POD_Load_VGA_Dac( std::istream & );
extern void POD_Load_VGA_S3( std::istream & );
extern void POD_Load_VGA_Other( std::istream & );
extern void POD_Load_VGA_Memory( std::istream & );
extern void POD_Load_VGA_Paradise( std::istream & );
extern void POD_Load_VGA_Tseng( std::istream & );
extern void POD_Load_VGA_XGA( std::istream & );

//save state support
void *VGA_SetupDrawing_PIC_Event = (void*)VGA_SetupDrawing;


namespace {
class SerializeVga : public SerializeGlobalPOD {
public:
	SerializeVga() : SerializeGlobalPOD("Vga")
	{}

private:
	virtual void getBytes(std::ostream& stream)
	{
		Bit32u tandy_drawbase_idx, tandy_membase_idx;




		if( vga.tandy.draw_base == vga.mem.linear ) tandy_drawbase_idx=0xffffffff;
		else tandy_drawbase_idx = vga.tandy.draw_base - MemBase;

		if( vga.tandy.mem_base == vga.mem.linear ) tandy_membase_idx=0xffffffff;
		else tandy_membase_idx = vga.tandy.mem_base - MemBase;

		//********************************
		//********************************

		SerializeGlobalPOD::getBytes(stream);


		// - pure data
		WRITE_POD( &vga.mode, vga.mode );
		WRITE_POD( &vga.misc_output, vga.misc_output );

		
		// VGA_Draw.cpp
		POD_Save_VGA_Draw(stream);


		// - pure struct data
		WRITE_POD( &vga.config, vga.config );
		WRITE_POD( &vga.internal, vga.internal );


		// VGA_Seq.cpp / VGA_Attr.cpp / (..)
		POD_Save_VGA_Seq(stream);
		POD_Save_VGA_Attr(stream);
		POD_Save_VGA_Crtc(stream);
		POD_Save_VGA_Gfx(stream);
		POD_Save_VGA_Dac(stream);


		// - pure data
		WRITE_POD( &vga.latch, vga.latch );


		// VGA_S3.cpp
		POD_Save_VGA_S3(stream);


		// - pure struct data
		WRITE_POD( &vga.svga, vga.svga );
		WRITE_POD( &vga.herc, vga.herc );


		// - near-pure struct data
		WRITE_POD( &vga.tandy, vga.tandy );

		// - reloc data
		WRITE_POD( &tandy_drawbase_idx, tandy_drawbase_idx );
		WRITE_POD( &tandy_membase_idx, tandy_membase_idx );


		// vga_other.cpp / vga_memory.cpp
		POD_Save_VGA_Other(stream);
		POD_Save_VGA_Memory(stream);


		// - pure data
		WRITE_POD( &vga.vmemwrap, vga.vmemwrap );


		// - static ptrs + 'new' data
		//Bit8u* fastmem;
		//Bit8u* fastmem_orgptr;

		// - 'new' data
		WRITE_POD_SIZE( vga.fastmem_orgptr, sizeof(Bit8u) * ((vga.vmemsize << 1) + 4096 + 16) );


		// - pure data (variable on S3 card)
		WRITE_POD( &vga.vmemsize, vga.vmemsize );


#ifdef VGA_KEEP_CHANGES
		// - static ptr
		//Bit8u* map;

		// - 'new' data
		WRITE_POD_SIZE( vga.changes.map, sizeof(Bit8u) * (VGA_MEMORY >> VGA_CHANGE_SHIFT) + 32 );


		// - pure data
		WRITE_POD( &vga.changes.checkMask, vga.changes.checkMask );
		WRITE_POD( &vga.changes.frame, vga.changes.frame );
		WRITE_POD( &vga.changes.writeMask, vga.changes.writeMask );
		WRITE_POD( &vga.changes.active, vga.changes.active );
		WRITE_POD( &vga.changes.clearMask, vga.changes.clearMask );
		WRITE_POD( &vga.changes.start, vga.changes.start );
		WRITE_POD( &vga.changes.last, vga.changes.last );
		WRITE_POD( &vga.changes.lastAddress, vga.changes.lastAddress );
#endif


		// - pure data
		WRITE_POD( &vga.lfb.page, vga.lfb.page );
		WRITE_POD( &vga.lfb.addr, vga.lfb.addr );
		WRITE_POD( &vga.lfb.mask, vga.lfb.mask );

		// - static ptr
		//PageHandler *handler;


		// VGA_paradise.cpp / VGA_tseng.cpp / VGA_xga.cpp
		POD_Save_VGA_Paradise(stream);
		POD_Save_VGA_Tseng(stream);
		POD_Save_VGA_XGA(stream);
	}

	virtual void setBytes(std::istream& stream)
	{
		Bit32u tandy_drawbase_idx, tandy_membase_idx;



		//********************************
		//********************************

		SerializeGlobalPOD::setBytes(stream);


		// - pure data
		READ_POD( &vga.mode, vga.mode );
		READ_POD( &vga.misc_output, vga.misc_output );

		
		// VGA_Draw.cpp
		POD_Load_VGA_Draw(stream);


		// - pure struct data
		READ_POD( &vga.config, vga.config );
		READ_POD( &vga.internal, vga.internal );


		// VGA_Seq.cpp / VGA_Attr.cpp / (..)
		POD_Load_VGA_Seq(stream);
		POD_Load_VGA_Attr(stream);
		POD_Load_VGA_Crtc(stream);
		POD_Load_VGA_Gfx(stream);
		POD_Load_VGA_Dac(stream);


		// - pure data
		READ_POD( &vga.latch, vga.latch );


		// VGA_S3.cpp
		POD_Load_VGA_S3(stream);


		// - pure struct data
		READ_POD( &vga.svga, vga.svga );
		READ_POD( &vga.herc, vga.herc );


		// - near-pure struct data
		READ_POD( &vga.tandy, vga.tandy );

		// - reloc data
		READ_POD( &tandy_drawbase_idx, tandy_drawbase_idx );
		READ_POD( &tandy_membase_idx, tandy_membase_idx );


		// vga_other.cpp / vga_memory.cpp
		POD_Load_VGA_Other(stream);
		POD_Load_VGA_Memory(stream);


		// - pure data
		READ_POD( &vga.vmemwrap, vga.vmemwrap );


		// - static ptrs + 'new' data
		//Bit8u* fastmem;
		//Bit8u* fastmem_orgptr;

		// - 'new' data
		READ_POD_SIZE( vga.fastmem_orgptr, sizeof(Bit8u) * ((vga.vmemsize << 1) + 4096 + 16) );


		// - pure data (variable on S3 card)
		READ_POD( &vga.vmemsize, vga.vmemsize );


#ifdef VGA_KEEP_CHANGES
		// - static ptr
		//Bit8u* map;

		// - 'new' data
		READ_POD_SIZE( vga.changes.map, sizeof(Bit8u) * (VGA_MEMORY >> VGA_CHANGE_SHIFT) + 32 );


		// - pure data
		READ_POD( &vga.changes.checkMask, vga.changes.checkMask );
		READ_POD( &vga.changes.frame, vga.changes.frame );
		READ_POD( &vga.changes.writeMask, vga.changes.writeMask );
		READ_POD( &vga.changes.active, vga.changes.active );
		READ_POD( &vga.changes.clearMask, vga.changes.clearMask );
		READ_POD( &vga.changes.start, vga.changes.start );
		READ_POD( &vga.changes.last, vga.changes.last );
		READ_POD( &vga.changes.lastAddress, vga.changes.lastAddress );
#endif


		// - pure data
		READ_POD( &vga.lfb.page, vga.lfb.page );
		READ_POD( &vga.lfb.addr, vga.lfb.addr );
		READ_POD( &vga.lfb.mask, vga.lfb.mask );

		// - static ptr
		//PageHandler *handler;


		// VGA_paradise.cpp / VGA_tseng.cpp / VGA_xga.cpp
		POD_Load_VGA_Paradise(stream);
		POD_Load_VGA_Tseng(stream);
		POD_Load_VGA_XGA(stream);

		//********************************
		//********************************

		if( tandy_drawbase_idx == 0xffffffff ) vga.tandy.draw_base = vga.mem.linear;
		else vga.tandy.draw_base = MemBase + tandy_drawbase_idx;

		if( tandy_membase_idx == 0xffffffff ) vga.tandy.mem_base = vga.mem.linear;
		else vga.tandy.mem_base = MemBase + tandy_membase_idx;
	}
} dummy;
}