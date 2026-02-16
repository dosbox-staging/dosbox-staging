// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

/* Character displaying moving functions */

#include "int10.h"

#include "ints/bios.h"
#include "cpu/callback.h"
#include "hardware/port.h"
#include "hardware/memory.h"
#include "hardware/pic.h"
#include "cpu/registers.h"

static void CGA2_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
	BIOS_CHEIGHT;
	PhysPt dest=base+((CurMode->twidth*rnew)*(cheight/2)+cleft);
	PhysPt src=base+((CurMode->twidth*rold)*(cheight/2)+cleft);
	Bitu copy=(cright-cleft);
	Bitu nextline=CurMode->twidth;
	for (Bitu i=0;i<cheight/2U;i++) {
		MEM_BlockCopy(dest,src,copy);
		MEM_BlockCopy(dest+8*1024,src+8*1024,copy);
		dest+=nextline;src+=nextline;
	}
}

static void CGA4_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
	BIOS_CHEIGHT;
	PhysPt dest=base+((CurMode->twidth*rnew)*(cheight/2)+cleft)*2;
	PhysPt src=base+((CurMode->twidth*rold)*(cheight/2)+cleft)*2;	
	Bitu copy=(cright-cleft)*2;Bitu nextline=CurMode->twidth*2;
	for (Bitu i=0;i<cheight/2U;i++) {
		MEM_BlockCopy(dest,src,copy);
		MEM_BlockCopy(dest+8*1024,src+8*1024,copy);
		dest+=nextline;src+=nextline;
	}
}

static void TANDY16_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
	BIOS_CHEIGHT;
	uint8_t banks=CurMode->twidth/10;
	PhysPt dest=base+((CurMode->twidth*rnew)*(cheight/banks)+cleft)*4;
	PhysPt src=base+((CurMode->twidth*rold)*(cheight/banks)+cleft)*4;
	Bitu copy=(cright-cleft)*4;Bitu nextline=CurMode->twidth*4;
	for (Bitu i=0;i<static_cast<Bitu>(cheight/banks);i++) {
		for (Bitu b=0;b<banks;b++) MEM_BlockCopy(dest+b*8*1024,src+b*8*1024,copy);
		dest+=nextline;src+=nextline;
	}
}

static void EGA16_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
	PhysPt src,dest;Bitu copy;
	BIOS_CHEIGHT;
	dest=base+(CurMode->twidth*rnew)*cheight+cleft;
	src=base+(CurMode->twidth*rold)*cheight+cleft;
	Bitu nextline=CurMode->twidth;
	/* Setup registers correctly */
	IO_Write(0x3ce,5);IO_Write(0x3cf,1);		/* Memory transfer mode */
	IO_Write(0x3c4,2);IO_Write(0x3c5,0xf);		/* Enable all Write planes */
	/* Do some copying */
	Bitu rowsize=(cright-cleft);
	copy=cheight;
	for (;copy>0;copy--) {
		for (Bitu x=0;x<rowsize;x++) mem_writeb(dest+x,mem_readb(src+x));
		dest+=nextline;src+=nextline;
	}
	/* Restore registers */
	IO_Write(0x3ce,5);IO_Write(0x3cf,0);		/* Normal transfer mode */
}

static void VGA_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
	PhysPt src,dest;Bitu copy;
	BIOS_CHEIGHT;
	dest=base+8*((CurMode->twidth*rnew)*cheight+cleft);
	src=base+8*((CurMode->twidth*rold)*cheight+cleft);
	Bitu nextline=8*CurMode->twidth;
	Bitu rowsize=8*(cright-cleft);
	copy=cheight;
	for (;copy>0;copy--) {
		for (Bitu x=0;x<rowsize;x++) mem_writeb(dest+x,mem_readb(src+x));
		dest+=nextline;src+=nextline;
	}
}

static void TEXT_CopyRow(uint8_t cleft,uint8_t cright,uint8_t rold,uint8_t rnew,PhysPt base) {
	PhysPt src,dest;
	src=base+(rold*CurMode->twidth+cleft)*2;
	dest=base+(rnew*CurMode->twidth+cleft)*2;
	MEM_BlockCopy(dest,src,(cright-cleft)*2);
}

static void CGA2_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
	BIOS_CHEIGHT;
	PhysPt dest=base+((CurMode->twidth*row)*(cheight/2)+cleft);
	Bitu copy=(cright-cleft);
	Bitu nextline=CurMode->twidth;
	attr=(attr & 0x3) | ((attr & 0x3) << 2) | ((attr & 0x3) << 4) | ((attr & 0x3) << 6);
	for (Bitu i=0;i<cheight/2U;i++) {
		for (Bitu x=0;x<copy;x++) {
			mem_writeb(dest+x,attr);
			mem_writeb(dest+8*1024+x,attr);
		}
		dest+=nextline;
	}
}

static void CGA4_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
	BIOS_CHEIGHT;
	PhysPt dest=base+((CurMode->twidth*row)*(cheight/2)+cleft)*2;
	Bitu copy=(cright-cleft)*2;Bitu nextline=CurMode->twidth*2;
	attr=(attr & 0x3) | ((attr & 0x3) << 2) | ((attr & 0x3) << 4) | ((attr & 0x3) << 6);
	for (Bitu i=0;i<cheight/2U;i++) {
		for (Bitu x=0;x<copy;x++) {
			mem_writeb(dest+x,attr);
			mem_writeb(dest+8*1024+x,attr);
		}
		dest+=nextline;
	}
}

static void TANDY16_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
	BIOS_CHEIGHT;
	uint8_t banks=CurMode->twidth/10;
	PhysPt dest=base+((CurMode->twidth*row)*(cheight/banks)+cleft)*4;
	Bitu copy=(cright-cleft)*4;Bitu nextline=CurMode->twidth*4;
	attr=(attr & 0xf) | (attr & 0xf) << 4;
	for (Bitu i=0;i<static_cast<Bitu>(cheight/banks);i++) {
		for (Bitu x=0;x<copy;x++) {
			for (Bitu b=0;b<banks;b++) mem_writeb(dest+b*8*1024+x,attr);
		}
		dest+=nextline;
	}
}

static void EGA16_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
	/* Set Bitmask / Color / Full Set Reset */
	IO_Write(0x3ce,0x8);IO_Write(0x3cf,0xff);
	IO_Write(0x3ce,0x0);IO_Write(0x3cf,attr);
	IO_Write(0x3ce,0x1);IO_Write(0x3cf,0xf);
	/* Enable all Write planes */
	IO_Write(0x3c4,2);IO_Write(0x3c5,0xf);
	/* Write some bytes */
	BIOS_CHEIGHT;
	PhysPt dest=base+(CurMode->twidth*row)*cheight+cleft;	
	Bitu nextline=CurMode->twidth;
	Bitu copy = cheight;Bitu rowsize=(cright-cleft);
	for (;copy>0;copy--) {
		for (Bitu x=0;x<rowsize;x++) mem_writeb(dest+x,0xff);
		dest+=nextline;
	}
	IO_Write(0x3cf,0);
}

static void VGA_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
	/* Write some bytes */
	BIOS_CHEIGHT;
	PhysPt dest=base+8*((CurMode->twidth*row)*cheight+cleft);
	Bitu nextline=8*CurMode->twidth;
	Bitu copy = cheight;Bitu rowsize=8*(cright-cleft);
	for (;copy>0;copy--) {
		for (Bitu x=0;x<rowsize;x++) mem_writeb(dest+x,attr);
		dest+=nextline;
	}
}

static void TEXT_FillRow(uint8_t cleft,uint8_t cright,uint8_t row,PhysPt base,uint8_t attr) {
	/* Do some filing */
	PhysPt dest;
	dest=base+(row*CurMode->twidth+cleft)*2;
	uint16_t fill=(attr<<8)+' ';
	for (uint8_t x=0;x<(cright-cleft);x++) {
		mem_writew(dest,fill);
		dest+=2;
	}
}

uint16_t INT10_GetTextColumns()
{
	return real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
}

uint16_t INT10_GetTextRows()
{
	if (is_machine_ega_or_better()) {
		return real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS) + 1;
	} else {
		return 25;
	}
}

void INT10_ScrollWindow(uint8_t rul,uint8_t cul,uint8_t rlr,uint8_t clr,int8_t nlines,uint8_t attr,uint8_t page) {
/* Do some range checking */
	if (CurMode->type!=M_TEXT) page=0xff;
	BIOS_NCOLS;BIOS_NROWS;
	if(rul>rlr) return;
	if(cul>clr) return;
	if(rlr>=nrows) rlr=(uint8_t)nrows-1;
	if(clr>=ncols) clr=(uint8_t)ncols-1;
	clr++;

	/* Get the correct page: current start address for current page (0xFF),
	   otherwise calculate from page number and page size */
	PhysPt base=CurMode->pstart;
	if (page==0xff) base+=real_readw(BIOSMEM_SEG,BIOSMEM_CURRENT_START);
	else base+=page*real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);

	if (is_machine_pcjr()) {
		if (real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE) >= 9) {
			// PCJr cannot handle these modes at 0xb800
			// See INT10_PutPixel M_TANDY16
			Bitu cpupage =
				(real_readb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE) >> 3) & 0x7;

			base = cpupage << 14;
			if (page!=0xff)
				base += page*real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
		}
	}

	/* See how much lines need to be copied */
	uint8_t start,end;Bits next;
	/* Copy some lines */
	if (nlines>0) {
		start=rlr-nlines+1;
		end=rul;
		next=-1;
	} else if (nlines<0) {
		start=rul-nlines-1;
		end=rlr;
		next=1;
	} else {
		nlines=rlr-rul+1;
		goto filling;
	}
	while (start!=end) {
		start+=next;
		switch (CurMode->type) {
		case M_TEXT:
			TEXT_CopyRow(cul,clr,start,start+nlines,base);break;
		case M_CGA2:
			CGA2_CopyRow(cul,clr,start,start+nlines,base);break;
		case M_CGA4:
			CGA4_CopyRow(cul,clr,start,start+nlines,base);break;
		case M_TANDY16:
			TANDY16_CopyRow(cul,clr,start,start+nlines,base);break;
		case M_EGA:		
			EGA16_CopyRow(cul,clr,start,start+nlines,base);break;
		case M_VGA:		
			VGA_CopyRow(cul,clr,start,start+nlines,base);break;
		case M_LIN4:
			if ((svga_type == SvgaType::TsengEt4k) &&
			    (CurMode->swidth <= 800)) {
				// the ET4000 BIOS supports text output in 800x600 SVGA
				EGA16_CopyRow(cul,clr,start,start+nlines,base);break;
			}
			[[fallthrough]];
		default:
			LOG(LOG_INT10,LOG_ERROR)("Unhandled mode %d for scroll",CurMode->type);
		}	
	} 
	/* Fill some lines */
filling:
	if (nlines>0) {
		start=rul;
	} else {
		nlines=-nlines;
		start=rlr-nlines+1;
	}
	for (;nlines>0;nlines--) {
		switch (CurMode->type) {
		case M_TEXT:
			TEXT_FillRow(cul,clr,start,base,attr);break;
		case M_CGA2:
			CGA2_FillRow(cul,clr,start,base,attr);break;
		case M_CGA4:
			CGA4_FillRow(cul,clr,start,base,attr);break;
		case M_TANDY16:		
			TANDY16_FillRow(cul,clr,start,base,attr);break;
		case M_EGA:		
			EGA16_FillRow(cul,clr,start,base,attr);break;
		case M_VGA:		
			VGA_FillRow(cul,clr,start,base,attr);break;
		case M_LIN4:
			if ((svga_type == SvgaType::TsengEt4k) &&
			    (CurMode->swidth <= 800)) {
				EGA16_FillRow(cul,clr,start,base,attr);break;
			}
			[[fallthrough]];
		default:
			LOG(LOG_INT10,LOG_ERROR)("Unhandled mode %d for scroll",CurMode->type);
		}	
		start++;
	} 
}

void INT10_SetActivePage(uint8_t page) {
	uint16_t mem_address;
	if (page>7) LOG(LOG_INT10,LOG_ERROR)("INT10_SetActivePage page %d",page);

	if (svga_type == SvgaType::S3) {
		page &= 7;
	}

	mem_address=page*real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
	/* Write the new page start */
	real_writew(BIOSMEM_SEG,BIOSMEM_CURRENT_START,mem_address);
	if (is_machine_ega_or_better()) {
		if (CurMode->mode<8) mem_address>>=1;
		// rare alternative: if (CurMode->type==M_TEXT)  mem_address>>=1;
	} else {
		mem_address>>=1;
	}
	/* Write the new start address in vgahardware */
	uint16_t base=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
	IO_Write(base,0x0c);
	IO_Write(base+1,(uint8_t)(mem_address>>8));
	IO_Write(base,0x0d);
	IO_Write(base+1,(uint8_t)mem_address);

	// And change the BIOS page
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,page);
	uint8_t cur_row=CURSOR_POS_ROW(page);
	uint8_t cur_col=CURSOR_POS_COL(page);
	// Display the cursor, now the page is active
	INT10_SetCursorPos(cur_row,cur_col,page);
}

void INT10_SetCursorShape(uint8_t first, uint8_t last)
{
	real_writew(BIOSMEM_SEG,BIOSMEM_CURSOR_TYPE,last|(first<<8));
	if (is_machine_cga() || is_machine_pcjr_or_tandy()) goto dowrite;
	/* Skip CGA cursor emulation if EGA/VGA system is active */
	if (is_machine_hercules() ||
	    !(real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL) & 0x8)) {
		/* Check for CGA type 01, invisible */
		if ((first & 0x60) == 0x20) {
			first=0x1e;
			last=0x00;
			goto dowrite;
		}
		/* Check if we need to convert CGA Bios cursor values */
		if (is_machine_hercules() ||
		    !(real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL) & 0x1)) { // set by int10 fun12 sub34
//			if (CurMode->mode>0x3) goto dowrite;	//Only mode 0-3 are text modes on cga
			if ((first & 0xe0) || (last & 0xe0)) goto dowrite;
			uint8_t cheight = (is_machine_hercules() ? 14 : real_readb(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT)) - 1;
			/* Creative routine i based of the original ibmvga bios */

			if (last<first) {
				if (!last) goto dowrite;
				first=last;
				last=cheight;
			/* Test if this might be a cga style cursor set, if not don't do anything */
			} else if (((first | last)>=cheight) || !(last==(cheight-1)) || !(first==cheight) ) {
				if (last<=3) goto dowrite;
				if (first+2<last) {
					if (first>2) {
						first=(cheight+1)/2;
						last=cheight;
					} else {
						last=cheight;
					}
				} else {
					first=(first-last)+cheight;
					last=cheight;

					if (cheight>0xc) { // vgatest sets 15 15 2x where only one should be decremented to 14 14
						first--;     // implementing int10 fun12 sub34 fixed this.
						last--;
					}
				}
			}

		}
	}
dowrite:
	uint16_t base=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
	IO_Write(base,0xa);IO_Write(base+1,first);
	IO_Write(base,0xb);IO_Write(base+1,last);
}

void INT10_SetCursorPos(uint8_t row,uint8_t col,uint8_t page) {
	uint16_t address;

	if (page>7) LOG(LOG_INT10,LOG_ERROR)("INT10_SetCursorPos page %d",page);
	// Bios cursor pos
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2,col);
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2+1,row);
	// Set the hardware cursor
	uint8_t current=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	if(page==current) {
		// Get the dimensions
		BIOS_NCOLS;
		// Calculate the address knowing nbcols nbrows and page num
		// NOTE: BIOSMEM_CURRENT_START counts in colour/flag pairs
		address=(ncols*row)+col+real_readw(BIOSMEM_SEG,BIOSMEM_CURRENT_START)/2;
		// CRTC regs 0x0e and 0x0f
		uint16_t base=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
		IO_Write(base,0x0e);
		IO_Write(base+1,(uint8_t)(address>>8));
		IO_Write(base,0x0f);
		IO_Write(base+1,(uint8_t)address);
	}
}

void INT10_SetCursorPosViaInterrupt(const uint8_t row, const uint8_t col,
                                    const uint8_t page)
{
	constexpr uint8_t position_cmd = 0x2;

	// Save regs
	const auto old_ax = reg_ax;
	const auto old_bx = reg_bx;
	const auto old_dx = reg_dx;

	// Set the cursor position
	reg_ah = position_cmd;
	reg_bh = page;
	reg_dh = row;
	reg_dl = col;
	CALLBACK_RunRealInt(0x10);

	// Restore regs
	reg_ax = old_ax;
	reg_bx = old_bx;
	reg_dx = old_dx;
}

void ReadCharAttr(uint16_t col,uint16_t row,uint8_t page,uint16_t * result) {
	/* Externally used by the mouse routine */
	RealPt fontdata;
	uint16_t cols = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
	BIOS_CHEIGHT;
	bool split_chr = false;
	switch (CurMode->type) {
	case M_TEXT:
		{	
			// Compute the address  
			uint16_t address=page*real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
			address+=(row*cols+col)*2;
			// read the char 
			PhysPt where = CurMode->pstart+address;
			*result=mem_readw(where);
		}
		return;
	case M_CGA4:
	case M_CGA2:
	case M_TANDY16:
		split_chr = true;
		switch (machine) {
		case MachineType::Hercules:
		case MachineType::CgaMono:
		case MachineType::CgaColor:
			fontdata = RealMake(0xf000, 0xfa6e);
			break;
		case MachineType::Pcjr:
		case MachineType::Tandy:
			fontdata = RealGetVec(0x44);
			break;
		default:
			fontdata = RealGetVec(0x43);
			break;
		}
		break;
	default:
		fontdata=RealGetVec(0x43);
		break;
	}
	const auto x = col * 8;
	assert(x >= 0 && x <= UINT16_MAX);

	const auto y = row * cheight * (cols / CurMode->twidth);
	assert(y >= 0 && y <= UINT16_MAX);

	for (uint16_t chr=0;chr<256;chr++) {

		if (chr==128 && split_chr) fontdata=RealGetVec(0x1f);

		bool error=false;
		auto ty = static_cast<uint16_t>(y);
		for (uint8_t h=0;h<cheight;h++) {
			uint8_t bitsel=128;
			uint8_t bitline=mem_readb(RealToPhysical(fontdata));
			fontdata=RealMake(RealSegment(fontdata),RealOffset(fontdata)+1);
			uint8_t res=0;
			uint8_t vidline=0;
			auto tx = static_cast<uint16_t>(x);
			while (bitsel) {
				//Construct bitline in memory
				INT10_GetPixel(tx,ty,page,&res);
				if(res) vidline|=bitsel;
				tx++;
				bitsel>>=1;
			}
			ty++;
			if(bitline != vidline){
				/* It's not character 'chr', move on to the next */
				fontdata=RealMake(RealSegment(fontdata),RealOffset(fontdata)+cheight-h-1);
				error = true;
				break;
			}
		}
		if(!error){
			/* We found it */
			*result = chr;
			return;
		}
	}
	LOG(LOG_INT10,LOG_ERROR)("ReadChar didn't find character");
	*result = 0;
}
void INT10_ReadCharAttr(uint16_t * result,uint8_t page) {
	if(CurMode->ptotal==1) page=0;
	if(page==0xFF) page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	uint8_t cur_row=CURSOR_POS_ROW(page);
	uint8_t cur_col=CURSOR_POS_COL(page);
	ReadCharAttr(cur_col,cur_row,page,result);
}
void WriteChar(uint16_t col,uint16_t row,uint8_t page,uint8_t chr,uint8_t attr,bool useattr) {
	/* Externally used by the mouse routine */
	RealPt fontdata;
	uint16_t cols = real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
	uint8_t back;
	BIOS_CHEIGHT;
	switch (CurMode->type) {
	case M_TEXT:
		{	
			// Compute the address  
			uint16_t address=page*real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
			address+=(row*cols+col)*2;
			// Write the char 
			PhysPt where = CurMode->pstart+address;
			mem_writeb(where,chr);
			if (useattr) mem_writeb(where+1,attr);
		}
		return;
	case M_CGA4:
	case M_CGA2:
	case M_TANDY16:
		if (chr >= 128) {
			chr -= 128;
			fontdata = RealGetVec(0x1f);
			break;
		}
		switch (machine) {
		case MachineType::Hercules:
		case MachineType::CgaMono:
		case MachineType::CgaColor:
			fontdata = RealMake(0xf000, 0xfa6e);
			break;
		case MachineType::Pcjr:
		case MachineType::Tandy:
			fontdata = RealGetVec(0x44);
			break;
		default:
			fontdata = RealGetVec(0x43);
			break;
		}
		break;
	default:
		fontdata=RealGetVec(0x43);
		break;
	}
	fontdata=RealMake(RealSegment(fontdata),RealOffset(fontdata)+chr*cheight);

	if (!useattr) { // Set attribute(color) to a sensible value
		static bool warned_use = false;
		if (!warned_use) {
			LOG(LOG_INT10,LOG_ERROR)("writechar used without attribute in non-textmode %c %X",chr,chr);
			warned_use = true;
		}
		switch(CurMode->type) {
		case M_CGA4:
			attr = 0x3;
			break;
		case M_CGA2:
			attr = 0x1;
			break;
		case M_TANDY16:
		case M_EGA:
		default:
			attr = 0xf;
			break;
		}
	}

	//Attribute behavior of mode 6; mode 11 does something similar but
	//it is in INT 10h handler because it only applies to function 09h
	if (CurMode->mode==0x06) attr=(attr&0x80)|1;

	switch (CurMode->type) {
	case M_VGA:
	case M_LIN8:
		// 256-color modes have background color instead of page
		back=page;
		page=0;
		break;
	case M_EGA:
		/* enable all planes for EGA modes (Ultima 1 colour bug) */
		/* might be put into INT10_PutPixel but different vga bios
		   implementations have different opinions about this */
		IO_Write(0x3c4,0x2);IO_Write(0x3c5,0xf);
		[[fallthrough]];
	default:
		back=attr&0x80;
		break;
	}

	const auto x = col * 8;
	assert(x >= 0 && x <= UINT16_MAX);

	const auto y = row * cheight * (cols / CurMode->twidth);
	assert(y >= 0 && y <= UINT16_MAX);

	auto ty = static_cast<uint16_t>(y);
	for (uint8_t h = 0; h < cheight; h++) {
		uint8_t bitsel=128;
		uint8_t bitline=mem_readb(RealToPhysical(fontdata));
		fontdata=RealMake(RealSegment(fontdata),RealOffset(fontdata)+1);
		auto tx = static_cast<uint16_t>(x);
		while (bitsel) {
			INT10_PutPixel(tx,ty,page,(bitline&bitsel)?attr:back);
			tx++;
			bitsel>>=1;
		}
		ty++;
	}
}

static void write_char_via_interrupt(const uint8_t cur_col, const uint8_t cur_row,
                                     const uint8_t page, const uint8_t char_value,
                                     const uint8_t attribute, const bool use_attribute)
{
	constexpr uint8_t write_char_cmd        = 0x1;
	constexpr uint8_t with_attribute_cmd    = 0x9;
	constexpr uint8_t without_attribute_cmd = 0x0A;

	// Position the cursor
	INT10_SetCursorPosViaInterrupt(cur_row, cur_col, page);

	// Save regs
	const auto old_ax = reg_ax;
	const auto old_bx = reg_bx;
	const auto old_cx = reg_cx;

	// Write the character
	reg_ah = use_attribute ? with_attribute_cmd : without_attribute_cmd;
	reg_al = char_value;
	reg_bl = attribute;
	reg_bh = page;
	reg_cx = write_char_cmd;
	CALLBACK_RunRealInt(0x10);

	// Restore regs
	reg_ax = old_ax;
	reg_bx = old_bx;
	reg_cx = old_cx;
}

// A template parameter to indicate if a function's actions should be
// called immediately (the default) or if they should be made via the interrupt
// handler and run just like any other DOS program.
//
enum class CallPlacement {
	Immediate,
	Interrupt,
};

template <CallPlacement call_placement = CallPlacement::Immediate>
void write_char(const uint8_t chr, const uint8_t attr, uint8_t page,
                uint16_t count, bool showattr)
{
	uint8_t pospage=page;
	if (CurMode->type!=M_TEXT) {
		showattr=true; //Use attr in graphics mode always
		switch (machine) {
			case MachineType::Ega:
			case MachineType::Vga:
				switch (CurMode->type) {
				case M_VGA:
				case M_LIN8:
					pospage=0;
					break;
				default:
					page%=CurMode->ptotal;
					pospage=page;
					break;
				}
				break;

		        case MachineType::CgaMono:
		        case MachineType::CgaColor:
			case MachineType::Pcjr:
				page    = 0;
				pospage = 0;
				break;

			case MachineType::Hercules:
			case MachineType::Tandy:
				break;

			default: assertm(false, "Invalid MachineType value");
		}
	}

	uint8_t cur_row=CURSOR_POS_ROW(pospage);
	uint8_t cur_col=CURSOR_POS_COL(pospage);
	BIOS_NCOLS;
	while (count>0) {
		if constexpr (call_placement == CallPlacement::Immediate) {
			WriteChar(cur_col, cur_row, page, chr, attr, showattr);
		} else {
			write_char_via_interrupt(cur_col, cur_row, page, chr, attr, showattr);
		}
		count--;
		cur_col++;
		if(cur_col==ncols) {
			cur_col=0;
			cur_row++;
		}
	}

	if (CurMode->type==M_EGA) {
		// Reset write ops for EGA graphics modes
		IO_Write(0x3ce,0x3);IO_Write(0x3cf,0x0);
	}
}

void INT10_WriteChar(const uint8_t char_value, const uint8_t attribute,
                     uint8_t page, uint16_t count, bool use_attribute)
{
	write_char<CallPlacement::Immediate>(char_value, attribute, page, count, use_attribute);
}

void INT10_WriteCharViaInterrupt(const uint8_t char_value, const uint8_t attribute,
                                 uint8_t page, uint16_t count, bool use_attribute)
{
	write_char<CallPlacement::Interrupt>(char_value, attribute, page, count, use_attribute);
}

template <CallPlacement call_placement = CallPlacement::Immediate>
static void teletype_output_attr(const uint8_t chr, const uint8_t attr,
                                 const bool useattr, const uint8_t page)
{
	BIOS_NCOLS;
	BIOS_NROWS;
	uint8_t cur_row = CURSOR_POS_ROW(page);
	uint8_t cur_col = CURSOR_POS_COL(page);
	switch (chr) {
	case 7: /* Beep */
		// Prepare PIT counter 2 for ~900 Hz square wave
		IO_Write(0x43,0xb6);
		IO_Write(0x42,0x28);
		IO_Write(0x42,0x05);
		// Speaker on
		IO_Write(0x61,IO_Read(0x61)|3);
		// Idle for 1/3rd of a second
		double start;
		start=PIC_FullIndex();

		while ((PIC_FullIndex() - start) < 333.0) {
			if (CALLBACK_Idle()) {
				break;
			}
		}

		// Speaker off
		IO_Write(0x61,IO_Read(0x61)&~3);
		// No change in position
		return;
	case 8:
		if(cur_col>0) cur_col--;
		break;
	case '\r':
		cur_col=0;
		break;
	case '\n':
//		cur_col=0; //Seems to break an old chess game
		cur_row++;
		break;
	default:
		/* Draw the actual Character */
		if constexpr (call_placement == CallPlacement::Immediate) {
			WriteChar(cur_col, cur_row, page, chr, attr, useattr);
		} else {
			write_char_via_interrupt(cur_col, cur_row, page, chr, attr, useattr);
		}
		cur_col++;
	}
	if(cur_col==ncols) {
		cur_col=0;
		cur_row++;
	}
	// Do we need to scroll ?
	if(cur_row==nrows) {
		//Fill with black on non-text modes and with attribute at cursor on textmode
		uint8_t fill=0;
		if (CurMode->type==M_TEXT) {
			uint16_t chat;
			INT10_ReadCharAttr(&chat,page);
			fill=(uint8_t)(chat>>8);
		}
		INT10_ScrollWindow(0,0,(uint8_t)(nrows-1),(uint8_t)(ncols-1),-1,fill,page);
		cur_row--;
	}
	// Set the cursor for the page
	if constexpr (call_placement == CallPlacement::Immediate) {
		INT10_SetCursorPos(cur_row, cur_col, page);
	} else {
		INT10_SetCursorPosViaInterrupt(cur_row, cur_col, page);
	}
}

void INT10_TeletypeOutputAttr(const uint8_t char_value, const uint8_t attribute,
                              const bool use_attribute)
{
	const auto page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);

	teletype_output_attr<CallPlacement::Immediate>(char_value,
	                                               attribute,
	                                               use_attribute,
	                                               page);
}

void INT10_TeletypeOutputAttrViaInterrupt(const uint8_t char_value,
                                          const uint8_t attribute,
                                          const bool use_attribute)
{
	const auto page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);

	teletype_output_attr<CallPlacement::Interrupt>(char_value,
	                                               attribute,
	                                               use_attribute,
	                                               page);
}

void INT10_TeletypeOutput(uint8_t chr,uint8_t attr) {
	INT10_TeletypeOutputAttr(chr,attr,CurMode->type!=M_TEXT);
}

void INT10_TeletypeOutputViaInterrupt(const uint8_t char_value, const uint8_t attribute)
{
	constexpr uint8_t teletype_cmd = 0xE;

	// Save regs
	const auto old_ax = reg_ax;
	const auto old_bx = reg_bx;

	// Teletype the output
	reg_ah = teletype_cmd;
	reg_al = char_value;
	reg_bl = attribute;
	CALLBACK_RunRealInt(0x10);

	// Restore regs
	reg_ax = old_ax;
	reg_bx = old_bx;
}

void INT10_WriteString(uint8_t row,uint8_t col,uint8_t flag,uint8_t attr,PhysPt string,uint16_t count,uint8_t page) {
	uint8_t cur_row=CURSOR_POS_ROW(page);
	uint8_t cur_col=CURSOR_POS_COL(page);
	
	// if row=0xff special case : use current cursor position
	if (row==0xff) {
		row=cur_row;
		col=cur_col;
	}
	INT10_SetCursorPos(row,col,page);
	while (count>0) {
		uint8_t chr=mem_readb(string);
		string++;
		if (flag&2) {
			attr=mem_readb(string);
			string++;
		};
		constexpr auto use_attribute = true;
		teletype_output_attr(chr, attr, use_attribute, page);
		count--;
	}
	if (!(flag&1)) {
		INT10_SetCursorPos(cur_row,cur_col,page);
	}
}
