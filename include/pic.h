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

#ifndef __PIC_H
#define __PIC_H

typedef void (PIC_EOIHandler) (void);
typedef void (PIC_Function)(void);

extern Bit32u PIC_IRQCheck;

void PIC_ActivateIRQ(Bit32u irq);

void PIC_DeActivateIRQ(Bit32u irq);

void PIC_runIRQs(void);

void PIC_RegisterIRQ(Bit32u irq,PIC_EOIHandler handler,char * name);
void PIC_FreeIRQ(Bit32u irq);

bool PIC_IRQActive(Bit32u irq);

/* A Queued function should never queue itself again this will go horribly wrong */
void PIC_QueueFunction(PIC_Function * function);



#endif

