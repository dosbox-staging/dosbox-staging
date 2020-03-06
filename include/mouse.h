/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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



#ifndef DOSBOX_MOUSE_H
#define DOSBOX_MOUSE_H


void Mouse_ShowCursor(void);
void Mouse_HideCursor(void);

bool Mouse_SetPS2State(bool use);

void Mouse_ChangePS2Callback(Bit16u pseg, Bit16u pofs);


void Mouse_CursorMoved(float xrel,float yrel,float x,float y,bool emulate);
void Mouse_CursorSet(float x,float y);
void Mouse_ButtonPressed(Bit8u button);
void Mouse_ButtonReleased(Bit8u button);

void Mouse_AutoLock(bool enable);
void Mouse_BeforeNewVideoMode(bool setmode);
void Mouse_AfterNewVideoMode(bool setmode);

#endif
