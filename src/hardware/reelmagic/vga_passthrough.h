/*
 *  Copyright (C) 2022 Jon Dennis
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

#ifndef DOSBOX_VGA_REELMAGIC_OVERRIDE_H
#define DOSBOX_VGA_REELMAGIC_OVERRIDE_H


#include "reelmagic.h"


#define RENDER_SetPal        ReelMagic_RENDER_SetPal
#define RENDER_SetSize       ReelMagic_RENDER_SetSize
#define RENDER_StartUpdate   ReelMagic_RENDER_StartUpdate
#define RENDER_DrawLine      ReelMagic_RENDER_DrawLine
//#define RENDER_EndUpdate     ReelMagic_RENDER_EndUpdate


#endif /* #ifndef DOSBOX_VGA_REELMAGIC_OVERRIDE_H */
