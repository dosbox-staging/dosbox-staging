/*
 *  Copyright (C) 2002-2015  The DOSBox Team
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


#include "dosbox.h"
#include "mem.h"
#include "bios.h"
#include "regs.h"
#include "cpu.h"
#include "callback.h"
#include "inout.h"
#include "pic.h"
#include "hardware.h"
#include "pci_bus.h"
#include "joystick.h"
#include "mouse.h"
#include "setup.h"
#include "serialport.h"
#include <time.h>

#if defined(DB_HAVE_CLOCK_GETTIME) && ! defined(WIN32)
//time.h is already included
#else
#include <sys/timeb.h>
#endif

/* if mem_systems 0 then size_extended is reported as the real size else 
 * zero is reported. ems and xms can increase or decrease the other_memsystems
 * counter using the BIOS_ZeroExtendedSize call */
static Bit16u size_extended;
static Bits other_memsystems=0;
void CMOS_SetRegister(Bitu regNr, Bit8u val); //For setting equipment word

static Bitu INT70_Handler(void) {
	/* Acknowledge irq with cmos */
	IO_Write(0x70,0xc);
	IO_Read(0x71);
	if (mem_readb(BIOS_WAIT_FLAG_ACTIVE)) {
		Bit32u count=mem_readd(BIOS_WAIT_FLAG_COUNT);
		if (count>997) {
			mem_writed(BIOS_WAIT_FLAG_COUNT,count-997);
		} else {
			mem_writed(BIOS_WAIT_FLAG_COUNT,0);
			PhysPt where=Real2Phys(mem_readd(BIOS_WAIT_FLAG_POINTER));
			mem_writeb(where,mem_readb(where)|0x80);
			mem_writeb(BIOS_WAIT_FLAG_ACTIVE,0);
			mem_writed(BIOS_WAIT_FLAG_POINTER,RealMake(0,BIOS_WAIT_FLAG_TEMP));
			IO_Write(0x70,0xb);
			IO_Write(0x71,IO_Read(0x71)&~0x40);
		}
	} 
	/* Signal EOI to both pics */
	IO_Write(0xa0,0x20);
	IO_Write(0x20,0x20);
	return 0;
}

CALLBACK_HandlerObject* tandy_DAC_callback[2];
static struct {
	Bit16u port;
	Bit8u irq;
	Bit8u dma;
} tandy_sb;
static struct {
	Bit16u port;
	Bit8u irq;
	Bit8u dma;
} tandy_dac;

static bool Tandy_InitializeSB() {
	/* see if soundblaster module available and at what port/IRQ/DMA */
	Bitu sbport, sbirq, sbdma;
	if (SB_Get_Address(sbport, sbirq, sbdma)) {
		tandy_sb.port=(Bit16u)(sbport&0xffff);
		tandy_sb.irq =(Bit8u)(sbirq&0xff);
		tandy_sb.dma =(Bit8u)(sbdma&0xff);
		return true;
	} else {
		/* no soundblaster accessible, disable Tandy DAC */
		tandy_sb.port=0;
		return false;
	}
}

static bool Tandy_InitializeTS() {
	/* see if Tandy DAC module available and at what port/IRQ/DMA */
	Bitu tsport, tsirq, tsdma;
	if (TS_Get_Address(tsport, tsirq, tsdma)) {
		tandy_dac.port=(Bit16u)(tsport&0xffff);
		tandy_dac.irq =(Bit8u)(tsirq&0xff);
		tandy_dac.dma =(Bit8u)(tsdma&0xff);
		return true;
	} else {
		/* no Tandy DAC accessible */
		tandy_dac.port=0;
		return false;
	}
}

/* check if Tandy DAC is still playing */
static bool Tandy_TransferInProgress(void) {
	if (real_readw(0x40,0xd0)) return true;			/* not yet done */
	if (real_readb(0x40,0xd4)==0xff) return false;	/* still in init-state */

	Bit8u tandy_dma = 1;
	if (tandy_sb.port) tandy_dma = tandy_sb.dma;
	else if (tandy_dac.port) tandy_dma = tandy_dac.dma;

	IO_Write(0x0c,0x00);
	Bit16u datalen=(Bit8u)(IO_ReadB(tandy_dma*2+1)&0xff);
	datalen|=(IO_ReadB(tandy_dma*2+1)<<8);
	if (datalen==0xffff) return false;	/* no DMA transfer */
	else if ((datalen<0x10) && (real_readb(0x40,0xd4)==0x0f) && (real_readw(0x40,0xd2)==0x1c)) {
		/* stop already requested */
		return false;
	}
	return true;
}

static void Tandy_SetupTransfer(PhysPt bufpt,bool isplayback) {
	Bitu length=real_readw(0x40,0xd0);
	if (length==0) return;	/* nothing to do... */

	if ((tandy_sb.port==0) && (tandy_dac.port==0)) return;

	Bit8u tandy_irq = 7;
	if (tandy_sb.port) tandy_irq = tandy_sb.irq;
	else if (tandy_dac.port) tandy_irq = tandy_dac.irq;
	Bit8u tandy_irq_vector = tandy_irq;
	if (tandy_irq_vector<8) tandy_irq_vector += 8;
	else tandy_irq_vector += (0x70-8);

	/* revector IRQ-handler if necessary */
	RealPt current_irq=RealGetVec(tandy_irq_vector);
	if (current_irq!=tandy_DAC_callback[0]->Get_RealPointer()) {
		real_writed(0x40,0xd6,current_irq);
		RealSetVec(tandy_irq_vector,tandy_DAC_callback[0]->Get_RealPointer());
	}

	Bit8u tandy_dma = 1;
	if (tandy_sb.port) tandy_dma = tandy_sb.dma;
	else if (tandy_dac.port) tandy_dma = tandy_dac.dma;

	if (tandy_sb.port) {
		IO_Write(tandy_sb.port+0xc,0xd0);				/* stop DMA transfer */
		IO_Write(0x21,IO_Read(0x21)&(~(1<<tandy_irq)));	/* unmask IRQ */
		IO_Write(tandy_sb.port+0xc,0xd1);				/* turn speaker on */
	} else {
		IO_Write(tandy_dac.port,IO_Read(tandy_dac.port)&0x60);	/* disable DAC */
		IO_Write(0x21,IO_Read(0x21)&(~(1<<tandy_irq)));			/* unmask IRQ */
	}

	IO_Write(0x0a,0x04|tandy_dma);	/* mask DMA channel */
	IO_Write(0x0c,0x00);			/* clear DMA flipflop */
	if (isplayback) IO_Write(0x0b,0x48|tandy_dma);
	else IO_Write(0x0b,0x44|tandy_dma);
	/* set physical address of buffer */
	Bit8u bufpage=(Bit8u)((bufpt>>16)&0xff);
	IO_Write(tandy_dma*2,(Bit8u)(bufpt&0xff));
	IO_Write(tandy_dma*2,(Bit8u)((bufpt>>8)&0xff));
	switch (tandy_dma) {
		case 0: IO_Write(0x87,bufpage); break;
		case 1: IO_Write(0x83,bufpage); break;
		case 2: IO_Write(0x81,bufpage); break;
		case 3: IO_Write(0x82,bufpage); break;
	}
	real_writeb(0x40,0xd4,bufpage);

	/* calculate transfer size (respects segment boundaries) */
	Bit32u tlength=length;
	if (tlength+(bufpt&0xffff)>0x10000) tlength=0x10000-(bufpt&0xffff);
	real_writew(0x40,0xd0,(Bit16u)(length-tlength));	/* remaining buffer length */
	tlength--;

	/* set transfer size */
	IO_Write(tandy_dma*2+1,(Bit8u)(tlength&0xff));
	IO_Write(tandy_dma*2+1,(Bit8u)((tlength>>8)&0xff));

	Bit16u delay=(Bit16u)(real_readw(0x40,0xd2)&0xfff);
	Bit8u amplitude=(Bit8u)((real_readw(0x40,0xd2)>>13)&0x7);
	if (tandy_sb.port) {
		IO_Write(0x0a,tandy_dma);	/* enable DMA channel */
		/* set frequency */
		IO_Write(tandy_sb.port+0xc,0x40);
		IO_Write(tandy_sb.port+0xc,256-delay*100/358);
		/* set playback type to 8bit */
		if (isplayback) IO_Write(tandy_sb.port+0xc,0x14);
		else IO_Write(tandy_sb.port+0xc,0x24);
		/* set transfer size */
		IO_Write(tandy_sb.port+0xc,(Bit8u)(tlength&0xff));
		IO_Write(tandy_sb.port+0xc,(Bit8u)((tlength>>8)&0xff));
	} else {
		if (isplayback) IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x03);
		else IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x02);
		IO_Write(tandy_dac.port+2,(Bit8u)(delay&0xff));
		IO_Write(tandy_dac.port+3,(Bit8u)(((delay>>8)&0xf) | (amplitude<<5)));
		if (isplayback) IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x1f);
		else IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x1e);
		IO_Write(0x0a,tandy_dma);	/* enable DMA channel */
	}

	if (!isplayback) {
		/* mark transfer as recording operation */
		real_writew(0x40,0xd2,(Bit16u)(delay|0x1000));
	}
}

static Bitu IRQ_TandyDAC(void) {
	if (tandy_dac.port) {
		IO_Read(tandy_dac.port);
	}
	if (real_readw(0x40,0xd0)) {	/* play/record next buffer */
		/* acknowledge IRQ */
		IO_Write(0x20,0x20);
		if (tandy_sb.port) {
			IO_Read(tandy_sb.port+0xe);
		}

		/* buffer starts at the next page */
		Bit8u npage=real_readb(0x40,0xd4)+1;
		real_writeb(0x40,0xd4,npage);

		Bitu rb=real_readb(0x40,0xd3);
		if (rb&0x10) {
			/* start recording */
			real_writeb(0x40,0xd3,rb&0xef);
			Tandy_SetupTransfer(npage<<16,false);
		} else {
			/* start playback */
			Tandy_SetupTransfer(npage<<16,true);
		}
	} else {	/* playing/recording is finished */
		Bit8u tandy_irq = 7;
		if (tandy_sb.port) tandy_irq = tandy_sb.irq;
		else if (tandy_dac.port) tandy_irq = tandy_dac.irq;
		Bit8u tandy_irq_vector = tandy_irq;
		if (tandy_irq_vector<8) tandy_irq_vector += 8;
		else tandy_irq_vector += (0x70-8);

		RealSetVec(tandy_irq_vector,real_readd(0x40,0xd6));

		/* turn off speaker and acknowledge soundblaster IRQ */
		if (tandy_sb.port) {
			IO_Write(tandy_sb.port+0xc,0xd3);
			IO_Read(tandy_sb.port+0xe);
		}

		/* issue BIOS tandy sound device busy callout */
		SegSet16(cs, RealSeg(tandy_DAC_callback[1]->Get_RealPointer()));
		reg_ip = RealOff(tandy_DAC_callback[1]->Get_RealPointer());
	}
	return CBRET_NONE;
}

static void TandyDAC_Handler(Bit8u tfunction) {
	if ((!tandy_sb.port) && (!tandy_dac.port)) return;
	switch (tfunction) {
	case 0x81:	/* Tandy sound system check */
		if (tandy_dac.port) {
			reg_ax=tandy_dac.port;
		} else {
			reg_ax=0xc4;
		}
		CALLBACK_SCF(Tandy_TransferInProgress());
		break;
	case 0x82:	/* Tandy sound system start recording */
	case 0x83:	/* Tandy sound system start playback */
		if (Tandy_TransferInProgress()) {
			/* cannot play yet as the last transfer isn't finished yet */
			reg_ah=0x00;
			CALLBACK_SCF(true);
			break;
		}
		/* store buffer length */
		real_writew(0x40,0xd0,reg_cx);
		/* store delay and volume */
		real_writew(0x40,0xd2,(reg_dx&0xfff)|((reg_al&7)<<13));
		Tandy_SetupTransfer(PhysMake(SegValue(es),reg_bx),reg_ah==0x83);
		reg_ah=0x00;
		CALLBACK_SCF(false);
		break;
	case 0x84:	/* Tandy sound system stop playing */
		reg_ah=0x00;

		/* setup for a small buffer with silence */
		real_writew(0x40,0xd0,0x0a);
		real_writew(0x40,0xd2,0x1c);
		Tandy_SetupTransfer(PhysMake(0xf000,0xa084),true);
		CALLBACK_SCF(false);
		break;
	case 0x85:	/* Tandy sound system reset */
		if (tandy_dac.port) {
			IO_Write(tandy_dac.port,(Bit8u)(IO_Read(tandy_dac.port)&0xe0));
		}
		reg_ah=0x00;
		CALLBACK_SCF(false);
		break;
	}
}

static Bitu INT1A_Handler(void) {
	switch (reg_ah) {
	case 0x00:	/* Get System time */
		{
			Bit32u ticks=mem_readd(BIOS_TIMER);
			reg_al=mem_readb(BIOS_24_HOURS_FLAG);
			mem_writeb(BIOS_24_HOURS_FLAG,0); // reset the "flag"
			reg_cx=(Bit16u)(ticks >> 16);
			reg_dx=(Bit16u)(ticks & 0xffff);
			break;
		}
	case 0x01:	/* Set System time */
		mem_writed(BIOS_TIMER,(reg_cx<<16)|reg_dx);
		break;
	case 0x02:	/* GET REAL-TIME CLOCK TIME (AT,XT286,PS) */
		IO_Write(0x70,0x04);		//Hours
		reg_ch=IO_Read(0x71);
		IO_Write(0x70,0x02);		//Minutes
		reg_cl=IO_Read(0x71);
		IO_Write(0x70,0x00);		//Seconds
		reg_dh=IO_Read(0x71);
		reg_dl=0;			//Daylight saving disabled
		CALLBACK_SCF(false);
		break;
	case 0x04:	/* GET REAL-TIME ClOCK DATE  (AT,XT286,PS) */
		IO_Write(0x70,0x32);		//Centuries
		reg_ch=IO_Read(0x71);
		IO_Write(0x70,0x09);		//Years
		reg_cl=IO_Read(0x71);
		IO_Write(0x70,0x08);		//Months
		reg_dh=IO_Read(0x71);
		IO_Write(0x70,0x07);		//Days
		reg_dl=IO_Read(0x71);
		CALLBACK_SCF(false);
		break;
	case 0x80:	/* Pcjr Setup Sound Multiplexer */
		LOG(LOG_BIOS,LOG_ERROR)("INT1A:80:Setup tandy sound multiplexer to %d",reg_al);
		break;
	case 0x81:	/* Tandy sound system check */
	case 0x82:	/* Tandy sound system start recording */
	case 0x83:	/* Tandy sound system start playback */
	case 0x84:	/* Tandy sound system stop playing */
	case 0x85:	/* Tandy sound system reset */
		TandyDAC_Handler(reg_ah);
		break;
	case 0xb1:		/* PCI Bios Calls */
		LOG(LOG_BIOS,LOG_WARN)("INT1A:PCI bios call %2X",reg_al);
#if defined(PCI_FUNCTIONALITY_ENABLED)
		switch (reg_al) {
			case 0x01:	// installation check
				if (PCI_IsInitialized()) {
					reg_ah=0x00;
					reg_al=0x01;	// cfg space mechanism 1 supported
					reg_bx=0x0210;	// ver 2.10
					reg_cx=0x0000;	// only one PCI bus
					reg_edx=0x20494350;
					reg_edi=PCI_GetPModeInterface();
					CALLBACK_SCF(false);
				} else {
					CALLBACK_SCF(true);
				}
				break;
			case 0x02: {	// find device
				Bitu devnr=0;
				Bitu count=0x100;
				Bit32u devicetag=(reg_cx<<16)|reg_dx;
				Bits found=-1;
				for (Bitu i=0; i<=count; i++) {
					IO_WriteD(0xcf8,0x80000000|(i<<8));	// query unique device/subdevice entries
					if (IO_ReadD(0xcfc)==devicetag) {
						if (devnr==reg_si) {
							found=i;
							break;
						} else {
							// device found, but not the SIth device
							devnr++;
						}
					}
				}
				if (found>=0) {
					reg_ah=0x00;
					reg_bh=0x00;	// bus 0
					reg_bl=(Bit8u)(found&0xff);
					CALLBACK_SCF(false);
				} else {
					reg_ah=0x86;	// device not found
					CALLBACK_SCF(true);
				}
				}
				break;
			case 0x03: {	// find device by class code
				Bitu devnr=0;
				Bitu count=0x100;
				Bit32u classtag=reg_ecx&0xffffff;
				Bits found=-1;
				for (Bitu i=0; i<=count; i++) {
					IO_WriteD(0xcf8,0x80000000|(i<<8));	// query unique device/subdevice entries
					if (IO_ReadD(0xcfc)!=0xffffffff) {
						IO_WriteD(0xcf8,0x80000000|(i<<8)|0x08);
						if ((IO_ReadD(0xcfc)>>8)==classtag) {
							if (devnr==reg_si) {
								found=i;
								break;
							} else {
								// device found, but not the SIth device
								devnr++;
							}
						}
					}
				}
				if (found>=0) {
					reg_ah=0x00;
					reg_bh=0x00;	// bus 0
					reg_bl=(Bit8u)(found&0xff);
					CALLBACK_SCF(false);
				} else {
					reg_ah=0x86;	// device not found
					CALLBACK_SCF(true);
				}
				}
				break;
			case 0x08:	// read configuration byte
				IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
				reg_cl=IO_ReadB(0xcfc+(reg_di&3));
				CALLBACK_SCF(false);
				break;
			case 0x09:	// read configuration word
				IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
				reg_cx=IO_ReadW(0xcfc+(reg_di&2));
				CALLBACK_SCF(false);
				break;
			case 0x0a:	// read configuration dword
				IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
				reg_ecx=IO_ReadD(0xcfc+(reg_di&3));
				CALLBACK_SCF(false);
				break;
			case 0x0b:	// write configuration byte
				IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
				IO_WriteB(0xcfc+(reg_di&3),reg_cl);
				CALLBACK_SCF(false);
				break;
			case 0x0c:	// write configuration word
				IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
				IO_WriteW(0xcfc+(reg_di&2),reg_cx);
				CALLBACK_SCF(false);
				break;
			case 0x0d:	// write configuration dword
				IO_WriteD(0xcf8,0x80000000|(reg_bx<<8)|(reg_di&0xfc));
				IO_WriteD(0xcfc+(reg_di&3),reg_ecx);
				CALLBACK_SCF(false);
				break;
			default:
				LOG(LOG_BIOS,LOG_ERROR)("INT1A:PCI BIOS: unknown function %x (%x %x %x)",
					reg_ax,reg_bx,reg_cx,reg_dx);
				CALLBACK_SCF(true);
				break;
		}
#else
		CALLBACK_SCF(true);
#endif
		break;
	default:
		LOG(LOG_BIOS,LOG_ERROR)("INT1A:Undefined call %2X",reg_ah);
	}
	return CBRET_NONE;
}	

static Bitu INT11_Handler(void) {
	reg_ax=mem_readw(BIOS_CONFIGURATION);
	return CBRET_NONE;
}
/* 
 * Define the following define to 1 if you want dosbox to check 
 * the system time every 5 seconds and adjust 1/2 a second to sync them.
 */
#ifndef DOSBOX_CLOCKSYNC
#define DOSBOX_CLOCKSYNC 0
#endif

static void BIOS_HostTimeSync() {
	Bit32u milli = 0;
#if defined(DB_HAVE_CLOCK_GETTIME) && ! defined(WIN32)
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME,&tp);
	
	struct tm *loctime;
	loctime = localtime(&tp.tv_sec);
	milli = (Bit32u) (tp.tv_nsec / 1000000);
#else
	/* Setup time and date */
	struct timeb timebuffer;
	ftime(&timebuffer);
	
	struct tm *loctime;
	loctime = localtime (&timebuffer.time);
	milli = (Bit32u) timebuffer.millitm;
#endif
	/*
	loctime->tm_hour = 23;
	loctime->tm_min = 59;
	loctime->tm_sec = 45;
	loctime->tm_mday = 28;
	loctime->tm_mon = 2-1;
	loctime->tm_year = 2007 - 1900;
	*/

	dos.date.day=(Bit8u)loctime->tm_mday;
	dos.date.month=(Bit8u)loctime->tm_mon+1;
	dos.date.year=(Bit16u)loctime->tm_year+1900;

	Bit32u ticks=(Bit32u)(((double)(
		loctime->tm_hour*3600*1000+
		loctime->tm_min*60*1000+
		loctime->tm_sec*1000+
		milli))*(((double)PIT_TICK_RATE/65536.0)/1000.0));
	mem_writed(BIOS_TIMER,ticks);
}

static Bitu INT8_Handler(void) {
	/* Increase the bios tick counter */
	Bit32u value = mem_readd(BIOS_TIMER) + 1;
	if(value >= 0x1800B0) {
		// time wrap at midnight
		mem_writeb(BIOS_24_HOURS_FLAG,mem_readb(BIOS_24_HOURS_FLAG)+1);
		value=0;
	}

#if DOSBOX_CLOCKSYNC
	static bool check = false;
	if((value %50)==0) {
		if(((value %100)==0) && check) {
			check = false;
			time_t curtime;struct tm *loctime;
			curtime = time (NULL);loctime = localtime (&curtime);
			Bit32u ticksnu = (Bit32u)((loctime->tm_hour*3600+loctime->tm_min*60+loctime->tm_sec)*(float)PIT_TICK_RATE/65536.0);
			Bit32s bios = value;Bit32s tn = ticksnu;
			Bit32s diff = tn - bios;
			if(diff>0) {
				if(diff < 18) { diff  = 0; } else diff = 9;
			} else {
				if(diff > -18) { diff = 0; } else diff = -9;
			}
	     
			value += diff;
		} else if((value%100)==50) check = true;
	}
#endif
	mem_writed(BIOS_TIMER,value);

	/* decrease floppy motor timer */
	Bit8u val = mem_readb(BIOS_DISK_MOTOR_TIMEOUT);
	if (val) mem_writeb(BIOS_DISK_MOTOR_TIMEOUT,val-1);
	/* and running drive */
	mem_writeb(BIOS_DRIVE_RUNNING,mem_readb(BIOS_DRIVE_RUNNING) & 0xF0);
	return CBRET_NONE;
}
#undef DOSBOX_CLOCKSYNC

static Bitu INT1C_Handler(void) {
	return CBRET_NONE;
}

static Bitu INT12_Handler(void) {
	reg_ax=mem_readw(BIOS_MEMORY_SIZE);
	return CBRET_NONE;
}

static Bitu INT17_Handler(void) {
	LOG(LOG_BIOS,LOG_NORMAL)("INT17:Function %X",reg_ah);
	switch(reg_ah) {
	case 0x00:		/* PRINTER: Write Character */
		reg_ah=1;	/* Report a timeout */
		break;
	case 0x01:		/* PRINTER: Initialize port */
		break;
	case 0x02:		/* PRINTER: Get Status */
		reg_ah=0;	
		break;
	case 0x20:		/* Some sort of printerdriver install check*/
		break;
	default:
		E_Exit("Unhandled INT 17 call %2X",reg_ah);
	};
	return CBRET_NONE;
}

static bool INT14_Wait(Bit16u port, Bit8u mask, Bit8u timeout, Bit8u* retval) {
	double starttime = PIC_FullIndex();
	double timeout_f = timeout * 1000.0;
	while (((*retval = IO_ReadB(port)) & mask) != mask) {
		if (starttime < (PIC_FullIndex() - timeout_f)) {
			return false;
		}
		CALLBACK_Idle();
	}
	return true;
}

static Bitu INT14_Handler(void) {
	if (reg_ah > 0x3 || reg_dx > 0x3) {	// 0-3 serial port functions
										// and no more than 4 serial ports
		LOG_MSG("BIOS INT14: Unhandled call AH=%2X DX=%4x",reg_ah,reg_dx);
		return CBRET_NONE;
	}
	
	Bit16u port = real_readw(0x40,reg_dx*2); // DX is always port number
	Bit8u timeout = mem_readb(BIOS_COM1_TIMEOUT + reg_dx);
	if (port==0)	{
		LOG(LOG_BIOS,LOG_NORMAL)("BIOS INT14: port %d does not exist.",reg_dx);
		return CBRET_NONE;
	}
	switch (reg_ah)	{
	case 0x00:	{
		// Initialize port
		// Parameters:				Return:
		// AL: port parameters		AL: modem status
		//							AH: line status

		// set baud rate
		Bitu baudrate = 9600;
		Bit16u baudresult;
		Bitu rawbaud=reg_al>>5;
		
		if (rawbaud==0){ baudrate=110;}
		else if (rawbaud==1){ baudrate=150;}
		else if (rawbaud==2){ baudrate=300;}
		else if (rawbaud==3){ baudrate=600;}
		else if (rawbaud==4){ baudrate=1200;}
		else if (rawbaud==5){ baudrate=2400;}
		else if (rawbaud==6){ baudrate=4800;}
		else if (rawbaud==7){ baudrate=9600;}

		baudresult = (Bit16u)(115200 / baudrate);

		IO_WriteB(port+3, 0x80);	// enable divider access
		IO_WriteB(port, (Bit8u)baudresult&0xff);
		IO_WriteB(port+1, (Bit8u)(baudresult>>8));

		// set line parameters, disable divider access
		IO_WriteB(port+3, reg_al&0x1F); // LCR
		
		// disable interrupts
		IO_WriteB(port+1, 0); // IER

		// get result
		reg_ah=(Bit8u)(IO_ReadB(port+5)&0xff);
		reg_al=(Bit8u)(IO_ReadB(port+6)&0xff);
		CALLBACK_SCF(false);
		break;
	}
	case 0x01: // Transmit character
		// Parameters:				Return:
		// AL: character			AL: unchanged
		// AH: 0x01					AH: line status from just before the char was sent
		//								(0x80 | unpredicted) in case of timeout
		//						[undoc]	(0x80 | line status) in case of tx timeout
		//						[undoc]	(0x80 | modem status) in case of dsr/cts timeout

		// set DTR & RTS on
		IO_WriteB(port+4,0x3);
		// wait for DSR & CTS
		if (INT14_Wait(port+6, 0x30, timeout, &reg_ah)) {
			// wait for TX buffer empty
			if (INT14_Wait(port+5, 0x20, timeout, &reg_ah)) {
				// fianlly send the character
				IO_WriteB(port,reg_al);
			} else
				reg_ah |= 0x80;
		} else // timed out
			reg_ah |= 0x80;

		CALLBACK_SCF(false);
		break;
	case 0x02: // Read character
		// Parameters:				Return:
		// AH: 0x02					AL: received character
		//						[undoc]	will be trashed in case of timeout
		//							AH: (line status & 0x1E) in case of success
		//								(0x80 | unpredicted) in case of timeout
		//						[undoc]	(0x80 | line status) in case of rx timeout
		//						[undoc]	(0x80 | modem status) in case of dsr timeout

		// set DTR on
		IO_WriteB(port+4,0x1);

		// wait for DSR
		if (INT14_Wait(port+6, 0x20, timeout, &reg_ah)) {
			// wait for character to arrive
			if (INT14_Wait(port+5, 0x01, timeout, &reg_ah)) {
				reg_ah &= 0x1E;
				reg_al = IO_ReadB(port);
			} else
				reg_ah |= 0x80;
		} else
			reg_ah |= 0x80;

		CALLBACK_SCF(false);
		break;
	case 0x03: // get status
		reg_ah=(Bit8u)(IO_ReadB(port+5)&0xff);
		reg_al=(Bit8u)(IO_ReadB(port+6)&0xff);
		CALLBACK_SCF(false);
		break;

	}
	return CBRET_NONE;
}

static Bitu INT15_Handler(void) {
	static Bit16u biosConfigSeg=0;
	switch (reg_ah) {
	case 0x06:
		LOG(LOG_BIOS,LOG_NORMAL)("INT15 Unkown Function 6");
		break;
	case 0xC0:	/* Get Configuration*/
		{
			if (biosConfigSeg==0) biosConfigSeg = DOS_GetMemory(1); //We have 16 bytes
			PhysPt data	= PhysMake(biosConfigSeg,0);
			mem_writew(data,8);						// 8 Bytes following
			if (IS_TANDY_ARCH) {
				if (machine==MCH_TANDY) {
					// Model ID (Tandy)
					mem_writeb(data+2,0xFF);
				} else {
					// Model ID (PCJR)
					mem_writeb(data+2,0xFD);
				}
				mem_writeb(data+3,0x0A);					// Submodel ID
				mem_writeb(data+4,0x10);					// Bios Revision
				/* Tandy doesn't have a 2nd PIC, left as is for now */
				mem_writeb(data+5,(1<<6)|(1<<5)|(1<<4));	// Feature Byte 1
			} else {
				mem_writeb(data+2,0xFC);					// Model ID (PC)
				mem_writeb(data+3,0x00);					// Submodel ID
				mem_writeb(data+4,0x01);					// Bios Revision
				mem_writeb(data+5,(1<<6)|(1<<5)|(1<<4));	// Feature Byte 1
			}
			mem_writeb(data+6,(1<<6));				// Feature Byte 2
			mem_writeb(data+7,0);					// Feature Byte 3
			mem_writeb(data+8,0);					// Feature Byte 4
			mem_writeb(data+9,0);					// Feature Byte 5
			CPU_SetSegGeneral(es,biosConfigSeg);
			reg_bx = 0;
			reg_ah = 0;
			CALLBACK_SCF(false);
		}; break;
	case 0x4f:	/* BIOS - Keyboard intercept */
		/* Carry should be set but let's just set it just in case */
		CALLBACK_SCF(true);
		break;
	case 0x83:	/* BIOS - SET EVENT WAIT INTERVAL */
		{
			if(reg_al == 0x01) { /* Cancel it */
				mem_writeb(BIOS_WAIT_FLAG_ACTIVE,0);
				IO_Write(0x70,0xb);
				IO_Write(0x71,IO_Read(0x71)&~0x40);
				CALLBACK_SCF(false);
				break;
			}
			if (mem_readb(BIOS_WAIT_FLAG_ACTIVE)) {
				reg_ah=0x80;
				CALLBACK_SCF(true);
				break;
			}
			Bit32u count=(reg_cx<<16)|reg_dx;
			mem_writed(BIOS_WAIT_FLAG_POINTER,RealMake(SegValue(es),reg_bx));
			mem_writed(BIOS_WAIT_FLAG_COUNT,count);
			mem_writeb(BIOS_WAIT_FLAG_ACTIVE,1);
			/* Reprogram RTC to start */
			IO_Write(0x70,0xb);
			IO_Write(0x71,IO_Read(0x71)|0x40);
			CALLBACK_SCF(false);
		}
		break;
	case 0x84:	/* BIOS - JOYSTICK SUPPORT (XT after 11/8/82,AT,XT286,PS) */
		if (reg_dx == 0x0000) {
			// Get Joystick button status
			if (JOYSTICK_IsEnabled(0) || JOYSTICK_IsEnabled(1)) {
				reg_al = IO_ReadB(0x201)&0xf0;
				CALLBACK_SCF(false);
			} else {
				// dos values
				reg_ax = 0x00f0; reg_dx = 0x0201;
				CALLBACK_SCF(true);
			}
		} else if (reg_dx == 0x0001) {
			if (JOYSTICK_IsEnabled(0)) {
				reg_ax = (Bit16u)(JOYSTICK_GetMove_X(0)*127+128);
				reg_bx = (Bit16u)(JOYSTICK_GetMove_Y(0)*127+128);
				if(JOYSTICK_IsEnabled(1)) {
					reg_cx = (Bit16u)(JOYSTICK_GetMove_X(1)*127+128);
					reg_dx = (Bit16u)(JOYSTICK_GetMove_Y(1)*127+128);
				}
				else {
					reg_cx = reg_dx = 0;
				}
				CALLBACK_SCF(false);
			} else if (JOYSTICK_IsEnabled(1)) {
				reg_ax = reg_bx = 0;
				reg_cx = (Bit16u)(JOYSTICK_GetMove_X(1)*127+128);
				reg_dx = (Bit16u)(JOYSTICK_GetMove_Y(1)*127+128);
				CALLBACK_SCF(false);
			} else {			
				reg_ax = reg_bx = reg_cx = reg_dx = 0;
				CALLBACK_SCF(true);
			}
		} else {
			LOG(LOG_BIOS,LOG_ERROR)("INT15:84:Unknown Bios Joystick functionality.");
		}
		break;
	case 0x86:	/* BIOS - WAIT (AT,PS) */
		{
			if (mem_readb(BIOS_WAIT_FLAG_ACTIVE)) {
				reg_ah=0x83;
				CALLBACK_SCF(true);
				break;
			}
			Bit32u count=(reg_cx<<16)|reg_dx;
			mem_writed(BIOS_WAIT_FLAG_POINTER,RealMake(0,BIOS_WAIT_FLAG_TEMP));
			mem_writed(BIOS_WAIT_FLAG_COUNT,count);
			mem_writeb(BIOS_WAIT_FLAG_ACTIVE,1);
			/* Reprogram RTC to start */
			IO_Write(0x70,0xb);
			IO_Write(0x71,IO_Read(0x71)|0x40);
			while (mem_readd(BIOS_WAIT_FLAG_COUNT)) {
				CALLBACK_Idle();
			}
			CALLBACK_SCF(false);
			break;
		}
	case 0x87:	/* Copy extended memory */
		{
			bool enabled = MEM_A20_Enabled();
			MEM_A20_Enable(true);
			Bitu   bytes	= reg_cx * 2;
			PhysPt data		= SegPhys(es)+reg_si;
			PhysPt source	= (mem_readd(data+0x12) & 0x00FFFFFF) + (mem_readb(data+0x16)<<24);
			PhysPt dest		= (mem_readd(data+0x1A) & 0x00FFFFFF) + (mem_readb(data+0x1E)<<24);
			MEM_BlockCopy(dest,source,bytes);
			reg_ax = 0x00;
			MEM_A20_Enable(enabled);
			CALLBACK_SCF(false);
			break;
		}	
	case 0x88:	/* SYSTEM - GET EXTENDED MEMORY SIZE (286+) */
		reg_ax=other_memsystems?0:size_extended;
		LOG(LOG_BIOS,LOG_NORMAL)("INT15:Function 0x88 Remaining %04X kb",reg_ax);
		CALLBACK_SCF(false);
		break;
	case 0x89:	/* SYSTEM - SWITCH TO PROTECTED MODE */
		{
			IO_Write(0x20,0x10);IO_Write(0x21,reg_bh);IO_Write(0x21,0);
			IO_Write(0xA0,0x10);IO_Write(0xA1,reg_bl);IO_Write(0xA1,0);
			MEM_A20_Enable(true);
			PhysPt table=SegPhys(es)+reg_si;
			CPU_LGDT(mem_readw(table+0x8),mem_readd(table+0x8+0x2) & 0xFFFFFF);
			CPU_LIDT(mem_readw(table+0x10),mem_readd(table+0x10+0x2) & 0xFFFFFF);
			CPU_SET_CRX(0,CPU_GET_CRX(0)|1);
			CPU_SetSegGeneral(ds,0x18);
			CPU_SetSegGeneral(es,0x20);
			CPU_SetSegGeneral(ss,0x28);
			reg_sp+=6;			//Clear stack of interrupt frame
			CPU_SetFlags(0,FMASK_ALL);
			reg_ax=0;
			CPU_JMP(false,0x30,reg_cx,0);
		}
		break;
	case 0x90:	/* OS HOOK - DEVICE BUSY */
		CALLBACK_SCF(false);
		reg_ah=0;
		break;
	case 0x91:	/* OS HOOK - DEVICE POST */
		CALLBACK_SCF(false);
		reg_ah=0;
		break;
	case 0xc2:	/* BIOS PS2 Pointing Device Support */
		switch (reg_al) {
		case 0x00:		// enable/disable
			if (reg_bh==0) {	// disable
				Mouse_SetPS2State(false);
				reg_ah=0;
				CALLBACK_SCF(false);
			} else if (reg_bh==0x01) {	//enable
				if (!Mouse_SetPS2State(true)) {
					reg_ah=5;
					CALLBACK_SCF(true);
					break;
				}
				reg_ah=0;
				CALLBACK_SCF(false);
			} else {
				CALLBACK_SCF(true);
				reg_ah=1;
			}
			break;
		case 0x01:		// reset
			reg_bx=0x00aa;	// mouse
			// fall through
		case 0x05:		// initialize
			Mouse_SetPS2State(false);
			CALLBACK_SCF(false);
			reg_ah=0;
			break;
		case 0x02:		// set sampling rate
		case 0x03:		// set resolution
			CALLBACK_SCF(false);
			reg_ah=0;
			break;
		case 0x04:		// get type
			reg_bh=0;	// ID
			CALLBACK_SCF(false);
			reg_ah=0;
			break;
		case 0x06:		// extended commands
			if ((reg_bh==0x01) || (reg_bh==0x02)) {
				CALLBACK_SCF(false); 
				reg_ah=0;
			} else {
				CALLBACK_SCF(true);
				reg_ah=1;
			}
			break;
		case 0x07:		// set callback
			Mouse_ChangePS2Callback(SegValue(es),reg_bx);
			CALLBACK_SCF(false);
			reg_ah=0;
			break;
		default:
			CALLBACK_SCF(true);
			reg_ah=1;
			break;
		}
		break;
	case 0xc3:      /* set carry flag so BorlandRTM doesn't assume a VECTRA/PS2 */
		reg_ah=0x86;
		CALLBACK_SCF(true);
		break;
	case 0xc4:	/* BIOS POS Programm option Select */
		LOG(LOG_BIOS,LOG_NORMAL)("INT15:Function %X called, bios mouse not supported",reg_ah);
		CALLBACK_SCF(true);
		break;
	default:
		LOG(LOG_BIOS,LOG_ERROR)("INT15:Unknown call %4X",reg_ax);
		reg_ah=0x86;
		CALLBACK_SCF(true);
		if ((IS_EGAVGA_ARCH) || (machine==MCH_CGA)) {
			/* relict from comparisons, as int15 exits with a retf2 instead of an iret */
			CALLBACK_SZF(false);
		}
	}
	return CBRET_NONE;
}

static Bitu Reboot_Handler(void) {
	// switch to text mode, notify user (let's hope INT10 still works)
	const char* const text = "\n\n   Reboot requested, quitting now.";
	reg_ax = 0;
	CALLBACK_RunRealInt(0x10);
	reg_ah = 0xe;
	reg_bx = 0;
	for(Bitu i = 0; i < strlen(text);i++) {
		reg_al = text[i];
		CALLBACK_RunRealInt(0x10);
	}
	LOG_MSG(text);
	double start = PIC_FullIndex();
	while((PIC_FullIndex()-start)<3000) CALLBACK_Idle();
	throw 1;
	return CBRET_NONE;
}

void BIOS_ZeroExtendedSize(bool in) {
	if(in) other_memsystems++; 
	else other_memsystems--;
	if(other_memsystems < 0) other_memsystems=0;
}

void BIOS_SetupKeyboard(void);
void BIOS_SetupDisks(void);

class BIOS:public Module_base{
private:
	CALLBACK_HandlerObject callback[11];
public:
	BIOS(Section* configuration):Module_base(configuration){
		/* tandy DAC can be requested in tandy_sound.cpp by initializing this field */
		bool use_tandyDAC=(real_readb(0x40,0xd4)==0xff);

		/* Clear the Bios Data Area (0x400-0x5ff, 0x600- is accounted to DOS) */
		for (Bit16u i=0;i<0x200;i++) real_writeb(0x40,i,0);

		/* Setup all the interrupt handlers the bios controls */

		/* INT 8 Clock IRQ Handler */
		Bitu call_irq0=CALLBACK_Allocate();	
		CALLBACK_Setup(call_irq0,INT8_Handler,CB_IRQ0,Real2Phys(BIOS_DEFAULT_IRQ0_LOCATION),"IRQ 0 Clock");
		RealSetVec(0x08,BIOS_DEFAULT_IRQ0_LOCATION);
		// pseudocode for CB_IRQ0:
		//	sti
		//	callback INT8_Handler
		//	push ds,ax,dx
		//	int 0x1c
		//	cli
		//	mov al, 0x20
		//	out 0x20, al
		//	pop dx,ax,ds
		//	iret

		mem_writed(BIOS_TIMER,0);			//Calculate the correct time

		/* INT 11 Get equipment list */
		callback[1].Install(&INT11_Handler,CB_IRET,"Int 11 Equipment");
		callback[1].Set_RealVec(0x11);

		/* INT 12 Memory Size default at 640 kb */
		callback[2].Install(&INT12_Handler,CB_IRET,"Int 12 Memory");
		callback[2].Set_RealVec(0x12);
		if (IS_TANDY_ARCH) {
			/* reduce reported memory size for the Tandy (32k graphics memory
			   at the end of the conventional 640k) */
			if (machine==MCH_TANDY) mem_writew(BIOS_MEMORY_SIZE,624);
			else mem_writew(BIOS_MEMORY_SIZE,640);
			mem_writew(BIOS_TRUE_MEMORY_SIZE,640);
		} else mem_writew(BIOS_MEMORY_SIZE,640);
		
		/* INT 13 Bios Disk Support */
		BIOS_SetupDisks();

		/* INT 14 Serial Ports */
		callback[3].Install(&INT14_Handler,CB_IRET_STI,"Int 14 COM-port");
		callback[3].Set_RealVec(0x14);

		/* INT 15 Misc Calls */
		callback[4].Install(&INT15_Handler,CB_IRET,"Int 15 Bios");
		callback[4].Set_RealVec(0x15);

		/* INT 16 Keyboard handled in another file */
		BIOS_SetupKeyboard();

		/* INT 17 Printer Routines */
		callback[5].Install(&INT17_Handler,CB_IRET_STI,"Int 17 Printer");
		callback[5].Set_RealVec(0x17);

		/* INT 1A TIME and some other functions */
		callback[6].Install(&INT1A_Handler,CB_IRET_STI,"Int 1a Time");
		callback[6].Set_RealVec(0x1A);

		/* INT 1C System Timer tick called from INT 8 */
		callback[7].Install(&INT1C_Handler,CB_IRET,"Int 1c Timer");
		callback[7].Set_RealVec(0x1C);
		
		/* IRQ 8 RTC Handler */
		callback[8].Install(&INT70_Handler,CB_IRET,"Int 70 RTC");
		callback[8].Set_RealVec(0x70);

		/* Irq 9 rerouted to irq 2 */
		callback[9].Install(NULL,CB_IRQ9,"irq 9 bios");
		callback[9].Set_RealVec(0x71);

		/* Reboot */
		// This handler is an exit for more than only reboots, since we
		// don't handle these cases
		callback[10].Install(&Reboot_Handler,CB_IRET,"reboot");
		
		// INT 18h: Enter BASIC
		// Non-IBM BIOS would display "NO ROM BASIC" here
		callback[10].Set_RealVec(0x18);
		RealPt rptr = callback[10].Get_RealPointer();

		// INT 19h: Boot function
		// This is not a complete reboot as it happens after the POST
		// We don't handle it, so use the reboot function as exit.
		RealSetVec(0x19,rptr);

		// The farjump at the processor reset entry point (jumps to POST routine)
		phys_writeb(0xFFFF0,0xEA);		// FARJMP
		phys_writew(0xFFFF1,RealOff(BIOS_DEFAULT_RESET_LOCATION));	// offset
		phys_writew(0xFFFF3,RealSeg(BIOS_DEFAULT_RESET_LOCATION));	// segment

		// Compatible POST routine location: jump to the callback
		phys_writeb(Real2Phys(BIOS_DEFAULT_RESET_LOCATION)+0,0xEA);				// FARJMP
		phys_writew(Real2Phys(BIOS_DEFAULT_RESET_LOCATION)+1,RealOff(rptr));	// offset
		phys_writew(Real2Phys(BIOS_DEFAULT_RESET_LOCATION)+3,RealSeg(rptr));	// segment

		/* Irq 2 */
		Bitu call_irq2=CALLBACK_Allocate();	
		CALLBACK_Setup(call_irq2,NULL,CB_IRET_EOI_PIC1,Real2Phys(BIOS_DEFAULT_IRQ2_LOCATION),"irq 2 bios");
		RealSetVec(0x0a,BIOS_DEFAULT_IRQ2_LOCATION);

		// INT 05h: Print Screen
		// IRQ1 handler calls it when PrtSc key is pressed; does nothing unless hooked
		phys_writeb(Real2Phys(BIOS_DEFAULT_INT5_LOCATION),0xcf);
		RealSetVec(0x05,BIOS_DEFAULT_INT5_LOCATION);

		/* Some hardcoded vectors */
		phys_writeb(Real2Phys(BIOS_DEFAULT_HANDLER_LOCATION),0xcf);	/* bios default interrupt vector location -> IRET */
		phys_writew(Real2Phys(RealGetVec(0x12))+0x12,0x20); //Hack for Jurresic

		if (machine==MCH_TANDY) phys_writeb(0xffffe,0xff)	;	/* Tandy model */
		else if (machine==MCH_PCJR) phys_writeb(0xffffe,0xfd);	/* PCJr model */
		else phys_writeb(0xffffe,0xfc);	/* PC */

		// System BIOS identification
		const char* const b_type =
			"IBM COMPATIBLE 486 BIOS COPYRIGHT The DOSBox Team.";
		for(Bitu i = 0; i < strlen(b_type); i++) phys_writeb(0xfe00e + i,b_type[i]);
		
		// System BIOS version
		const char* const b_vers =
			"DOSBox FakeBIOS v1.0";
		for(Bitu i = 0; i < strlen(b_vers); i++) phys_writeb(0xfe061+i,b_vers[i]);

		// write system BIOS date
		const char* const b_date = "01/01/92";
		for(Bitu i = 0; i < strlen(b_date); i++) phys_writeb(0xffff5+i,b_date[i]);
		phys_writeb(0xfffff,0x55); // signature

		tandy_sb.port=0;
		tandy_dac.port=0;
		if (use_tandyDAC) {
			/* tandy DAC sound requested, see if soundblaster device is available */
			Bitu tandy_dac_type = 0;
			if (Tandy_InitializeSB()) {
				tandy_dac_type = 1;
			} else if (Tandy_InitializeTS()) {
				tandy_dac_type = 2;
			}
			if (tandy_dac_type) {
				real_writew(0x40,0xd0,0x0000);
				real_writew(0x40,0xd2,0x0000);
				real_writeb(0x40,0xd4,0xff);	/* tandy DAC init value */
				real_writed(0x40,0xd6,0x00000000);
				/* install the DAC callback handler */
				tandy_DAC_callback[0]=new CALLBACK_HandlerObject();
				tandy_DAC_callback[1]=new CALLBACK_HandlerObject();
				tandy_DAC_callback[0]->Install(&IRQ_TandyDAC,CB_IRET,"Tandy DAC IRQ");
				tandy_DAC_callback[1]->Install(NULL,CB_TDE_IRET,"Tandy DAC end transfer");
				// pseudocode for CB_TDE_IRET:
				//	push ax
				//	mov ax, 0x91fb
				//	int 15
				//	cli
				//	mov al, 0x20
				//	out 0x20, al
				//	pop ax
				//	iret

				Bit8u tandy_irq = 7;
				if (tandy_dac_type==1) tandy_irq = tandy_sb.irq;
				else if (tandy_dac_type==2) tandy_irq = tandy_dac.irq;
				Bit8u tandy_irq_vector = tandy_irq;
				if (tandy_irq_vector<8) tandy_irq_vector += 8;
				else tandy_irq_vector += (0x70-8);

				RealPt current_irq=RealGetVec(tandy_irq_vector);
				real_writed(0x40,0xd6,current_irq);
				for (Bit16u i=0; i<0x10; i++) phys_writeb(PhysMake(0xf000,0xa084+i),0x80);
			} else real_writeb(0x40,0xd4,0x00);
		}
	
		/* Setup some stuff in 0x40 bios segment */
		
		// port timeouts
		// always 1 second even if the port does not exist
		mem_writeb(BIOS_LPT1_TIMEOUT,1);
		mem_writeb(BIOS_LPT2_TIMEOUT,1);
		mem_writeb(BIOS_LPT3_TIMEOUT,1);
		mem_writeb(BIOS_COM1_TIMEOUT,1);
		mem_writeb(BIOS_COM2_TIMEOUT,1);
		mem_writeb(BIOS_COM3_TIMEOUT,1);
		mem_writeb(BIOS_COM4_TIMEOUT,1);
		
		/* detect parallel ports */
		Bitu ppindex=0; // number of lpt ports
		if ((IO_Read(0x378)!=0xff)|(IO_Read(0x379)!=0xff)) {
			// this is our LPT1
			mem_writew(BIOS_ADDRESS_LPT1,0x378);
			ppindex++;
			if((IO_Read(0x278)!=0xff)|(IO_Read(0x279)!=0xff)) {
				// this is our LPT2
				mem_writew(BIOS_ADDRESS_LPT2,0x278);
				ppindex++;
				if((IO_Read(0x3bc)!=0xff)|(IO_Read(0x3be)!=0xff)) {
					// this is our LPT3
					mem_writew(BIOS_ADDRESS_LPT3,0x3bc);
					ppindex++;
				}
			} else if((IO_Read(0x3bc)!=0xff)|(IO_Read(0x3be)!=0xff)) {
				// this is our LPT2
				mem_writew(BIOS_ADDRESS_LPT2,0x3bc);
				ppindex++;
			}
		} else if((IO_Read(0x3bc)!=0xff)|(IO_Read(0x3be)!=0xff)) {
			// this is our LPT1
			mem_writew(BIOS_ADDRESS_LPT1,0x3bc);
			ppindex++;
			if((IO_Read(0x278)!=0xff)|(IO_Read(0x279)!=0xff)) {
				// this is our LPT2
				mem_writew(BIOS_ADDRESS_LPT2,0x278);
				ppindex++;
			}
		} else if((IO_Read(0x278)!=0xff)|(IO_Read(0x279)!=0xff)) {
			// this is our LPT1
			mem_writew(BIOS_ADDRESS_LPT1,0x278);
			ppindex++;
		}

		/* Setup equipment list */
		// look http://www.bioscentral.com/misc/bda.htm
		
		//Bit16u config=0x4400;	//1 Floppy, 2 serial and 1 parallel 
		Bit16u config = 0x0;
		
		// set number of parallel ports
		// if(ppindex == 0) config |= 0x8000; // looks like 0 ports are not specified
		//else if(ppindex == 1) config |= 0x0000;
		if(ppindex == 2) config |= 0x4000;
		else config |= 0xc000;	// 3 ports
#if (C_FPU)
		//FPU
		config|=0x2;
#endif
		switch (machine) {
		case MCH_HERC:
			//Startup monochrome
			config|=0x30;
			break;
		case EGAVGA_ARCH_CASE:
		case MCH_CGA:
		case TANDY_ARCH_CASE:
			//Startup 80x25 color
			config|=0x20;
			break;
		default:
			//EGA VGA
			config|=0;
			break;
		}
		// PS2 mouse
		config |= 0x04;
		// DMA *not* supported - Ancient Art of War CGA uses this to identify PCjr
		if (machine==MCH_PCJR) config |= 0x100;
		// Gameport
		config |= 0x1000;
		mem_writew(BIOS_CONFIGURATION,config);
		CMOS_SetRegister(0x14,(Bit8u)(config&0xff)); //Should be updated on changes
		/* Setup extended memory size */
		IO_Write(0x70,0x30);
		size_extended=IO_Read(0x71);
		IO_Write(0x70,0x31);
		size_extended|=(IO_Read(0x71) << 8);
		BIOS_HostTimeSync();
	}
	~BIOS(){
		/* abort DAC playing */
		if (tandy_sb.port) {
			IO_Write(tandy_sb.port+0xc,0xd3);
			IO_Write(tandy_sb.port+0xc,0xd0);
		}
		real_writeb(0x40,0xd4,0x00);
		if (tandy_DAC_callback[0]) {
			Bit32u orig_vector=real_readd(0x40,0xd6);
			if (orig_vector==tandy_DAC_callback[0]->Get_RealPointer()) {
				/* set IRQ vector to old value */
				Bit8u tandy_irq = 7;
				if (tandy_sb.port) tandy_irq = tandy_sb.irq;
				else if (tandy_dac.port) tandy_irq = tandy_dac.irq;
				Bit8u tandy_irq_vector = tandy_irq;
				if (tandy_irq_vector<8) tandy_irq_vector += 8;
				else tandy_irq_vector += (0x70-8);

				RealSetVec(tandy_irq_vector,real_readd(0x40,0xd6));
				real_writed(0x40,0xd6,0x00000000);
			}
			delete tandy_DAC_callback[0];
			delete tandy_DAC_callback[1];
			tandy_DAC_callback[0]=NULL;
			tandy_DAC_callback[1]=NULL;
		}
	}
};

// set com port data in bios data area
// parameter: array of 4 com port base addresses, 0 = none
void BIOS_SetComPorts(Bit16u baseaddr[]) {
	Bit16u portcount=0;
	Bit16u equipmentword;
	for(Bitu i = 0; i < 4; i++) {
		if(baseaddr[i]!=0) portcount++;
		if(i==0)		mem_writew(BIOS_BASE_ADDRESS_COM1,baseaddr[i]);
		else if(i==1)	mem_writew(BIOS_BASE_ADDRESS_COM2,baseaddr[i]);
		else if(i==2)	mem_writew(BIOS_BASE_ADDRESS_COM3,baseaddr[i]);
		else			mem_writew(BIOS_BASE_ADDRESS_COM4,baseaddr[i]);
	}
	// set equipment word
	equipmentword = mem_readw(BIOS_CONFIGURATION);
	equipmentword &= (~0x0E00);
	equipmentword |= (portcount << 9);
	mem_writew(BIOS_CONFIGURATION,equipmentword);
	CMOS_SetRegister(0x14,(Bit8u)(equipmentword&0xff)); //Should be updated on changes
}


static BIOS* test;

void BIOS_Destroy(Section* /*sec*/){
	delete test;
}

void BIOS_Init(Section* sec) {
	test = new BIOS(sec);
	sec->AddDestroyFunction(&BIOS_Destroy,false);
}
