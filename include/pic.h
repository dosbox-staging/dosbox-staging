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

#ifndef __PIC_H
#define __PIC_H


/* CPU Cycle Timing */
extern Bits CPU_Cycles;
extern Bits CPU_CycleLeft;
extern Bits CPU_CycleMax;

typedef void (PIC_EOIHandler) (void);
typedef void (* PIC_EventHandler)(void);


#define PIC_MAXIRQ 15
#define PIC_NOIRQ 0xFF


extern Bitu PIC_IRQCheck;
extern Bitu PIC_IRQActive;
extern Bitu PIC_Ticks;

INLINE Bitu PIC_Index(void) {
	return ((CPU_CycleMax-CPU_CycleLeft-CPU_Cycles)*1000)/CPU_CycleMax;
}

INLINE Bits PIC_MakeCycles(Bitu amount) {
	return (CPU_CycleMax*amount)/1000;
}

INLINE Bit64u PIC_MicroCount(void) {
	return PIC_Ticks*1000+PIC_Index();
}

void PIC_ActivateIRQ(Bitu irq);

void PIC_DeActivateIRQ(Bitu irq);

void PIC_runIRQs(void);

void PIC_RegisterIRQ(Bitu irq,PIC_EOIHandler handler,char * name);
void PIC_FreeIRQ(Bitu irq);
bool PIC_RunQueue(void);

void PIC_AddEvent(PIC_EventHandler handler,Bitu delay);

void PIC_RemoveEvents(PIC_EventHandler handler);

void PIC_SetIRQMask(Bitu irq, bool masked);
#endif

