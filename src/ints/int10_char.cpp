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


/* Character displaying moving functions */

#include "dosbox.h"
#include "bios.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"


void INT10_ScrollDownWindow(Bit8u rul,Bit8u cul,Bit8u rlr,Bit8u clr,Bit8u nlines,Bit8u attr,Bit8u page) {

//	Bit8u mode;
	Bit16u fill=( (attr << 8) | ' ');
	BIOS_NCOLS;BIOS_NROWS;

	if(rul>rlr) return;
	if(cul>clr) return;
	if(rlr>=nrows) rlr=(Bit8u)nrows-1;
	if(clr>=ncols) clr=(Bit8u)ncols-1;

	// Get the current page
	if(page==0xFF) page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

	/* Get this from active video mode */
	Bit16u textseg=0xb800;
	PhysPt start=PhysMake(textseg,ncols*nrows*2*page);

	Bit32u dcol=clr-cul+1;
	Bit32u drow=rlr-rul+1;
	
	Bit32u tocopy;
	PhysPt dest=start+((rlr*ncols)+cul)*2;
	PhysPt src = 0;/* for gcc */
	if (nlines==0) {
		nlines=(Bit8u)nrows;
	}
	if (nlines>=drow) { 
		tocopy=0;
	} else { 
		tocopy=drow-nlines;
		src=start+(((rul+tocopy-1)*ncols)+cul)*2;
	}

	for (Bit32u y=0;y<drow;y++) {
		if (tocopy) {
			for(Bit32u x=0;x<dcol;x++) {
				mem_writew(dest,mem_readw(src));
				src+=2;
				dest+=2;
			}
			src-=(ncols+dcol)*2;
			dest-=(ncols+dcol)*2;
			tocopy--;
		} else {
			for(Bit32u x=0;x<dcol;x++) {
				mem_writew(dest,fill);
				dest+=2;
			}
			dest-=(ncols+dcol)*2;
		}
	}
}

void INT10_ScrollUpWindow(Bit8u rul,Bit8u cul,Bit8u rlr,Bit8u clr,Bit8u nlines,Bit8u attr,Bit8u page) {

//	Bit8u mode;
	Bit16u fill=( (attr << 8) | ' ');
	BIOS_NCOLS;BIOS_NROWS;

	if(rlr>nrows) rlr=(Bit8u)nrows;
	if(clr>ncols) clr=(Bit8u)ncols;

	if(rul>rlr) return;
	if(cul>clr) return;
	
	VGAMODES * curmode=GetCurrentMode();
	switch (curmode->memmodel) {
	case CGA:
		{
			if (nlines==0) {
				/* Clear Screen that we can */
				PhysPt dest=PhysMake(0xb800,0);
				for (Bit32u tel=0;tel<0x4000;tel++) {
					mem_writew(dest,0x0000);
					dest+=2;
				}
				return;
			}
			LOG_WARN("INT10:Scroll in CGA Mode");
		}
	case MTEXT:
	case CTEXT:
		break;
	default:
		LOG_ERROR("INT10:Scroll on non supported graphics mode");
	}

	// Get the current page
	if(page==0xFF) page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);

	/* Get this from active video mode */
	Bit16u textseg=0xb800;
	PhysPt start=PhysMake(textseg,ncols*nrows*2*page);

	Bit32u dcol=clr-cul+1;
	Bit32u drow=rlr-rul+1;
	
	Bit32u tocopy;
	PhysPt dest=start+((rul*ncols)+cul)*2;
	PhysPt src;
	if (nlines==0) {
		nlines=(Bit8u)nrows;
	}

	if (nlines>=drow) { 
		tocopy=0;
	} else { 
		tocopy=drow-nlines;
		src=start+(((rul+nlines)*ncols)+cul)*2;
	}
	for (Bit32u y=0;y<drow;y++) {
		if (tocopy) {
			for (Bit32u x=0;x<dcol;x++) {
				mem_writew(dest,mem_readw(src));
				src+=2;
				dest+=2;
			}
			src+=(ncols-dcol)*2;
			dest+=(ncols-dcol)*2;
			tocopy--;
		} else {
			for(Bit32u x=0;x<dcol;x++) {
				mem_writew(dest,fill);
				dest+=2;
			}
			dest+=(ncols-dcol)*2;
		}
	}
}





void INT10_SetActivePage(Bit8u page) {

	Bit16u mem_address;
	Bit16u vid_address;
	Bit8u cur_col=0 ,cur_row=0 ;
	
	VGAMODES * curmode=GetCurrentMode();
	if (curmode==0) return;
	if (page>7) return;
	switch (curmode->memmodel) {
	case MTEXT:
	case CTEXT:{
					BIOS_NCOLS;BIOS_NROWS;
					cur_col=CURSOR_POS_COL(page);
					cur_row=CURSOR_POS_ROW(page);
					vid_address=SCREEN_IO_START(ncols,nrows,page);
					mem_address=SCREEN_MEM_START(ncols,nrows,page);
					break;	
			   }
	case CGA:{
				vid_address=0;
				mem_address=0;
				break;
			 }
	case PLANAR4:{
				mem_address=0;
				vid_address=((((curmode->sheight*curmode->swidth)>>3)|0xff)+1)*page;	

				break;
			 }
	default:
		vid_address=0;
		mem_address=0;
		break;
	}
	// Calculate the address knowing nbcols nbrows and page num
	real_writew(BIOSMEM_SEG,BIOSMEM_CURRENT_START,mem_address);

	// CRTC regs 0x0c and 0x0d
	IO_Write(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS),0x0c);
	IO_Write(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS)+1,(vid_address&0xff00)>>8);
	IO_Write(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS),0x0d);
	IO_Write(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS)+1,vid_address&0x00ff);

	// And change the BIOS page
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,page);

	// Display the cursor, now the page is active
	INT10_SetCursorPos(cur_row,cur_col,page);
}


void INT10_SetCursorPos(Bit8u row,Bit8u col,Bit8u page) {
	Bit16u address;

	if(page>7) return;
	// Bios cursor pos
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2,col);
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2+1,row);
	// Set the hardware cursor
	Bit8u current=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	if(page==current) {
		// Get the dimensions
		BIOS_NCOLS;BIOS_NROWS;
		// Calculate the address knowing nbcols nbrows and page num
		address=SCREEN_IO_START(ncols,nrows,page)+col+row*ncols;
		// CRTC regs 0x0e and 0x0f
		IO_Write(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS),0x0e);
		IO_Write(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS)+1,(address&0xff00)>>8);
		IO_Write(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS),0x0f);
		IO_Write(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS)+1,address&0x00ff);
  }
}


void INT10_ReadCharAttr(Bit16u * result,Bit8u page) {

	if(page==0xFF) page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	BIOS_NCOLS;BIOS_NROWS;
	Bit8u cur_row=CURSOR_POS_ROW(page);
	Bit8u cur_col=CURSOR_POS_COL(page);
 
	Bit16u address=SCREEN_MEM_START(ncols,nrows,page)+(cur_col+cur_row*ncols)*2;
	*result=real_readw(0xb800,address);
}



INLINE static void WriteChar(Bit16u col,Bit16u row,Bit8u page,Bit8u chr,Bit8u attr,bool useattr) {
	VGAMODES * curmode=GetCurrentMode();
	switch (curmode->type) {
	case TEXT:
		{	
			// Compute the address  
			Bit16u address=SCREEN_MEM_START(curmode->twidth,curmode->theight,page)+(col+row*curmode->twidth)*2;
			// Write the char 
			real_writeb(0xb800,address,chr);
			if (useattr) {
				real_writeb(0xb800,address+1,attr);
			}
		}
		break;
	case GRAPH:
		{
			/* Amount of lines */
			Bit8u * fontdata;
			Bit16u x,y;
			switch (curmode->cheight) {
			case 8:
//				fontdata=&int10_font_08[chr*8];
				if (chr<128) fontdata=Real2Host(RealGetVec(0x43))+chr*8;
				else fontdata=Real2Host(RealGetVec(0x1F))+(chr-128)*8;
				break;
			case 14:
				fontdata=&int10_font_14[chr*14];
				break;
			case 16:
				fontdata=&int10_font_16[chr*16];
				break;
			default:
				LOG_ERROR("INT10:Teletype Illegal Font Height");
				return;
			}
			x=8*col;
			y=curmode->cheight*row;
			//TODO Check for out of bounds
			for (Bit8u h=0;h<curmode->cheight;h++) {
				Bit8u bitsel=128;
				Bit8u bitline=*fontdata++;
				Bit16u tx=x;
				while (bitsel) {
					if (bitline&bitsel) INT10_PutPixel(tx,y,page,attr);
					else INT10_PutPixel(tx,y,page,0);
					tx++;
					bitsel>>=1;
				}
				y++;
			}
		}
		break;
	}
}




void INT10_WriteChar(Bit8u chr,Bit8u attr,Bit8u page,Bit16u count,bool showattr) {
	if(page==0xFF) page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	Bit8u cur_row=CURSOR_POS_ROW(page);
	Bit8u cur_col=CURSOR_POS_COL(page);
	BIOS_NCOLS;BIOS_NROWS;
	while (count>0) {
		WriteChar(cur_col,cur_row,page,chr,attr,showattr);
		count--;
		cur_col++;
		if(cur_col==ncols) {
			cur_col=0;
			cur_row++;
		}
	}
}


void INT10_TeletypeOutput(Bit8u chr,Bit8u attr,bool showattr, Bit8u page) {
	if(page==0xFF) page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	BIOS_NCOLS;BIOS_NROWS;
	Bit8u cur_row=CURSOR_POS_ROW(page);
	Bit8u cur_col=CURSOR_POS_COL(page);

	switch (chr) {
	case 7:
	//TODO BEEP
	break;
	case 8:
		if(cur_col>0) cur_col--;
		break;
	case '\r':
		cur_col=0;
		break;
	case '\n':
		cur_col=0;
		cur_row++;
		break;
	case '\t':
		do {
			INT10_TeletypeOutput(' ',attr,showattr,page);
			cur_row=CURSOR_POS_ROW(page);
			cur_col=CURSOR_POS_COL(page);
		} while(cur_col%8==0);
		break;
	default:
		/* Draw the actual Character */
		WriteChar(cur_col,cur_row,page,chr,attr,showattr);
		cur_col++;
	}
	if(cur_col==ncols) {
		cur_col=0;
		cur_row++;
	}
	// Do we need to scroll ?
	if(cur_row==nrows) {
		INT10_ScrollUpWindow(0,0,nrows-1,ncols-1,1,0x07,page);
		cur_row--;
	}
 	// Set the cursor for the page
	INT10_SetCursorPos(cur_row,cur_col,page);
}

void INT10_WriteString(Bit8u row,Bit8u col,Bit8u flag,Bit8u attr,PhysPt string,Bit16u count,Bit8u page) {

	if(page==0xFF) page=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	BIOS_NCOLS;BIOS_NROWS;
	Bit8u cur_row=CURSOR_POS_ROW(page);
	Bit8u cur_col=CURSOR_POS_COL(page);
	
	// if row=0xff special case : use current cursor position
	if (row==0xff) {
		row=cur_row;
		col=cur_col;
	}

	INT10_SetCursorPos(row,col,page);
	while (count>0) {
		Bit8u chr=mem_readb(string);
		string++;
		if (flag&2) {
			attr=mem_readb(string);
			string++;
		}
		INT10_TeletypeOutput(chr,attr,true,page);
		count--;
	}
	if (flag & 1) {
		INT10_SetCursorPos(cur_row,cur_col,page);
	}
}

