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

#ifndef _HARDWARE_H_
#define _HARDWARE_H_

#include <programs.h>
#include <support.h>
#include <stdio.h>

typedef void (* HW_OutputHandler)(char * towrite);
typedef void (* HW_InputHandler)(char * line);

struct HWBlock {
	char * dev_name;				/* 8 characters max dev name */
	char * full_name;				/* 60 characters full name */
	char * help;
	HW_InputHandler get_input;
	HW_OutputHandler show_status;	/* Supplied with a string to display 50 chars of status info in */
	HWBlock * next;
};


void HW_Register(HWBlock * block);


#endif


