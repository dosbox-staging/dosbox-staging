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

#ifndef _SETUP_H_
#define _SETUP_H_

#include <cross.h>
enum { S_STRING,S_HEX,S_INT,S_BOOL};

typedef char *(String_Handler)(char * input);
typedef char *(Hex_Handler)(Bitu * input);
typedef char *(Int_Handler)(Bits * input);
typedef char *(Bool_Handler)(bool input);
	
class Setup {

	
private:
	int argc;
	char * * argv;

};



extern char dosbox_basedir[CROSS_LEN];	

#endif
