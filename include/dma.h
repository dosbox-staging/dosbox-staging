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


Bit16u DMA_8_Read(Bit32u channel,Bit8u * buffer,Bit16u count);
Bit16u DMA_8_Write(Bit32u dmachan,Bit8u * buffer,Bit16u count);

Bit16u DMA_16_Read(Bit32u channel,Bit8u * buffer,Bit16u count);
Bit16u DMA_16_Write(Bit32u dmachan,Bit8u * buffer,Bit16u count);

