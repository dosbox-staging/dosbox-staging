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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _HARDWARE_H_
#define _HARDWARE_H_

class Section;
enum OPL_Mode {
	OPL_none,OPL_cms,OPL_opl2,OPL_dualopl2,OPL_opl3
};

void OPL_Init(Section* sec,Bitu base,OPL_Mode mode,Bitu rate);
void CMS_Init(Section* sec,Bitu base,Bitu rate);

#endif


