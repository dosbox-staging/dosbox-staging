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


static void FPU_FINIT(void) {
	FPU_SetCW(0x37);
	fpu.sw=0;
	fpu.tags[0]=TAG_Empty;
	fpu.tags[1]=TAG_Empty;
	fpu.tags[2]=TAG_Empty;
	fpu.tags[3]=TAG_Empty;
	fpu.tags[4]=TAG_Empty;
	fpu.tags[5]=TAG_Empty;
	fpu.tags[6]=TAG_Empty;
	fpu.tags[7]=TAG_Empty;
}
static void FPU_FCLEX(void){
	fpu.sw&=0x7f00;				//should clear exceptions
};

static void FPU_FNOP(void){
	return;
}