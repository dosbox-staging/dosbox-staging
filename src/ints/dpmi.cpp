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

#include <stdlib.h>
#include <string.h>
#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_system.h"
#include "setup.h"
#include "inout.h"
#include "xms.h"
#include "cpu.h"


static bool DPMI_Multiplex(void) {
	switch (reg_ax) {
	case 0x1686:	/* Get CPU Mode */
		reg_ax=1;	/* Not protected */
		return true;
		break;
	case 0x1687:	/* Get Mode switch entry point */
		reg_ax=0;	/* supported */
		reg_bx=1;	/* Support 32-bit */
		reg_cl=4;	/* 486 */
		reg_dh=0;	/* dpmi 0.9 */
		reg_dl=9;
		reg_si=0;	/* No need for private data */



		break;
	}
	return false;

}

void DPMI_Init(Section* sec) {
	return;
	DOS_AddMultiplexHandler(DPMI_Multiplex);


}