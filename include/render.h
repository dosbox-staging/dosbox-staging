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



enum RENDER_Operation {
	OP_None,
	OP_Shot,
	OP_Normal2x,
	OP_AdvMame2x,
};

typedef void (* RENDER_Part_Handler)(Bit8u * src,Bitu x,Bitu y,Bitu dx,Bitu dy);
typedef void (* RENDER_Draw_Handler)(RENDER_Part_Handler part_handler);

void RENDER_DoUpdate(void);

void RENDER_SetSize(Bitu width,Bitu height,Bitu bpp,Bitu pitch,double ratio,Bitu scalew,Bitu scaleh,RENDER_Draw_Handler draw_handler);

void RENDER_SetPal(Bit8u entry,Bit8u red,Bit8u green,Bit8u blue);
