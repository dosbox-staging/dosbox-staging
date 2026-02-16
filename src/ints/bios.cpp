// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ints/bios.h"

#include <ctime>
#include <memory>

#include "config/config.h"
#include "config/setup.h"
#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "dos/dos_memory.h"
#include "dosbox.h"
#include "hardware/audio/ps1audio.h"
#include "hardware/audio/soundblaster.h"
#include "hardware/audio/tandy_sound.h"
#include "hardware/input/joystick.h"
#include "hardware/input/mouse.h"
#include "hardware/memory.h"
#include "hardware/pic.h"
#include "hardware/port.h"
#include "hardware/serialport/serialport.h"
#include "int10.h"
#include "utils/bitops.h"
#include "utils/math_utils.h"

// Constants
constexpr uint32_t BiosMachineSignatureAddress = 0xfffff;

#if defined(HAVE_CLOCK_GETTIME) && !defined(WIN32)
// time.h is already included
#else
#include <sys/timeb.h>
#endif

// Reference:
// - Ralf Brown's Interrupt List
// - https://www.stanislavs.org/helppc/idx_interrupt.html
// - http://www2.ift.ulaval.ca/~marchand/ift17583/dosints.pdf

void INT1AB1_Handler(); // PCI BIOS calls

/* if mem_systems 0 then size_extended is reported as the real size else
 * zero is reported. ems and xms can increase or decrease the other_memsystems
 * counter using the BIOS_ZeroExtendedSize call */
static uint16_t size_extended;
static Bits other_memsystems=0;
void CMOS_SetRegister(Bitu regNr, uint8_t val); //For setting equipment word

static Bitu INT70_Handler(void) {
	/* Acknowledge irq with cmos */
	IO_Write(0x70,0xc);
	IO_Read(0x71);
	if (mem_readb(BIOS_WAIT_FLAG_ACTIVE)) {
		uint32_t count=mem_readd(BIOS_WAIT_FLAG_COUNT);
		if (count>997) {
			mem_writed(BIOS_WAIT_FLAG_COUNT,count-997);
		} else {
			mem_writed(BIOS_WAIT_FLAG_COUNT,0);
			PhysPt where=RealToPhysical(mem_readd(BIOS_WAIT_FLAG_POINTER));
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

std::unique_ptr<CALLBACK_HandlerObject> tandy_dac_callback[2] = {};

static struct {
	uint16_t port = 0;
	uint8_t irq   = 0;
	uint8_t dma   = 0;
} tandy_sb = {};

static struct {
	uint16_t port = 0;
	uint8_t irq   = 0;
	uint8_t dma   = 0;
} tandy_dac = {};

static bool Tandy_InitializeSB() {
	/* see if soundblaster module available and at what port/IRQ/DMA */
	uint16_t sbport;
	uint8_t sbirq;
	uint8_t sbdma;
	if (SBLASTER_GetAddress(sbport, sbirq, sbdma)) {
		tandy_sb.port = sbport;
		tandy_sb.irq = sbirq;
		tandy_sb.dma = sbdma;
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
	if (TANDYSOUND_GetAddress(tsport, tsirq, tsdma)) {
		tandy_dac.port=(uint16_t)(tsport&0xffff);
		tandy_dac.irq =(uint8_t)(tsirq&0xff);
		tandy_dac.dma =(uint8_t)(tsdma&0xff);
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

	uint8_t tandy_dma = 1;
	if (tandy_sb.port) tandy_dma = tandy_sb.dma;
	else if (tandy_dac.port) tandy_dma = tandy_dac.dma;

	IO_Write(0x0c,0x00);
	uint16_t datalen=(uint8_t)(IO_ReadB(tandy_dma*2+1)&0xff);
	datalen|=(IO_ReadB(tandy_dma*2+1)<<8);
	if (datalen==0xffff) return false;	/* no DMA transfer */
	else if ((datalen<0x10) && (real_readb(0x40,0xd4)==0x0f) && (real_readw(0x40,0xd2)==0x1c)) {
		/* stop already requested */
		return false;
	}
	return true;
}

static void Tandy_SetupTransfer(PhysPt bufpt,bool isplayback) {
	const auto length=real_readw(0x40,0xd0);
	if (length==0) return;	/* nothing to do... */

	if ((tandy_sb.port==0) && (tandy_dac.port==0)) return;

	uint8_t tandy_irq = 7;
	if (tandy_sb.port) tandy_irq = tandy_sb.irq;
	else if (tandy_dac.port) tandy_irq = tandy_dac.irq;
	uint8_t tandy_irq_vector = tandy_irq;
	if (tandy_irq_vector<8) tandy_irq_vector += 8;
	else tandy_irq_vector += (0x70-8);

	/* revector IRQ-handler if necessary */
	RealPt current_irq=RealGetVec(tandy_irq_vector);
	if (current_irq != tandy_dac_callback[0]->Get_RealPointer()) {
		real_writed(0x40,0xd6,current_irq);
		RealSetVec(tandy_irq_vector,
		           tandy_dac_callback[0]->Get_RealPointer());
	}

	uint8_t tandy_dma = 1;
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
	auto bufpage=(uint8_t)((bufpt>>16)&0xff);
	IO_Write(tandy_dma*2,(uint8_t)(bufpt&0xff));
	IO_Write(tandy_dma*2,(uint8_t)((bufpt>>8)&0xff));
	switch (tandy_dma) {
	case 0: IO_Write(0x87, bufpage); break;
	case 1: IO_Write(0x83, bufpage); break;
	case 2: IO_Write(0x81, bufpage); break;
	case 3: IO_Write(0x82, bufpage); break;
	}
	real_writeb(0x40,0xd4,bufpage);

	/* calculate transfer size (respects segment boundaries) */
	uint32_t tlength=length;
	if (tlength+(bufpt&0xffff)>0x10000) tlength=0x10000-(bufpt&0xffff);
	real_writew(0x40,0xd0,(uint16_t)(length-tlength));	/* remaining buffer length */
	tlength--;

	/* set transfer size */
	IO_Write(tandy_dma*2+1,(uint8_t)(tlength&0xff));
	IO_Write(tandy_dma*2+1,(uint8_t)((tlength>>8)&0xff));

	auto delay=(uint16_t)(real_readw(0x40,0xd2)&0xfff);
	auto amplitude=(uint8_t)((real_readw(0x40,0xd2)>>13)&0x7);
	if (tandy_sb.port) {
		IO_Write(0x0a,tandy_dma);	/* enable DMA channel */
		/* set frequency */
		IO_Write(tandy_sb.port+0xc,0x40);
		IO_Write(tandy_sb.port+0xc,256-delay*100/358);
		/* set playback type to 8bit */
		if (isplayback) IO_Write(tandy_sb.port+0xc,0x14);
		else IO_Write(tandy_sb.port+0xc,0x24);
		/* set transfer size */
		IO_Write(tandy_sb.port+0xc,(uint8_t)(tlength&0xff));
		IO_Write(tandy_sb.port+0xc,(uint8_t)((tlength>>8)&0xff));
	} else {
		if (isplayback) IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x03);
		else IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x02);
		IO_Write(tandy_dac.port+2,(uint8_t)(delay&0xff));
		IO_Write(tandy_dac.port+3,(uint8_t)(((delay>>8)&0xf) | (amplitude<<5)));
		if (isplayback) IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x1f);
		else IO_Write(tandy_dac.port,(IO_Read(tandy_dac.port)&0x7c) | 0x1e);
		IO_Write(0x0a,tandy_dma);	/* enable DMA channel */
	}

	if (!isplayback) {
		/* mark transfer as recording operation */
		real_writew(0x40,0xd2,(uint16_t)(delay|0x1000));
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
		uint8_t npage=real_readb(0x40,0xd4)+1;
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
		uint8_t tandy_irq = 7;
		if (tandy_sb.port) tandy_irq = tandy_sb.irq;
		else if (tandy_dac.port) tandy_irq = tandy_dac.irq;
		uint8_t tandy_irq_vector = tandy_irq;
		if (tandy_irq_vector<8) tandy_irq_vector += 8;
		else tandy_irq_vector += (0x70-8);

		RealSetVec(tandy_irq_vector,real_readd(0x40,0xd6));

		/* turn off speaker and acknowledge soundblaster IRQ */
		if (tandy_sb.port) {
			IO_Write(tandy_sb.port+0xc,0xd3);
			IO_Read(tandy_sb.port+0xe);
		}

		/* issue BIOS tandy sound device busy callout */
		SegSet16(cs, RealSegment(tandy_dac_callback[1]->Get_RealPointer()));
		reg_ip = RealOffset(tandy_dac_callback[1]->Get_RealPointer());
	}
	return CBRET_NONE;
}

static void TandyDAC_Handler(uint8_t tfunction) {
	if ((!tandy_sb.port) && (!tandy_dac.port)) return;
	switch (tfunction) {
	case 0x81: /* Tandy sound system check */
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
		Tandy_SetupTransfer(PhysicalMake(SegValue(es),reg_bx),reg_ah==0x83);
		reg_ah=0x00;
		CALLBACK_SCF(false);
		break;
	case 0x84:	/* Tandy sound system stop playing */
		reg_ah=0x00;

		/* setup for a small buffer with silence */
		real_writew(0x40,0xd0,0x0a);
		real_writew(0x40,0xd2,0x1c);
		Tandy_SetupTransfer(PhysicalMake(0xf000,0xa084),true);
		CALLBACK_SCF(false);
		break;
	case 0x85:	/* Tandy sound system reset */
		if (tandy_dac.port) {
			IO_Write(tandy_dac.port,(uint8_t)(IO_Read(tandy_dac.port)&0xe0));
		}
		reg_ah=0x00;
		CALLBACK_SCF(false);
		break;
	}
}

static Bitu INT1A_Handler(void) {
	switch (reg_ah) {
	case 0x00: /* Get System time */
	{
		uint32_t ticks = mem_readd(BIOS_TIMER);
		reg_al = mem_readb(BIOS_24_HOURS_FLAG);
		mem_writeb(BIOS_24_HOURS_FLAG, 0); // reset the "flag"
		reg_cx = (uint16_t)(ticks >> 16);
		reg_dx = (uint16_t)(ticks & 0xffff);
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
	case 0xb1: // PCI_FUNCTION_ID - PCI BIOS calls
		INT1AB1_Handler();
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
	uint32_t milli = 0;
	// TODO investigate if clock_gettime and ftime can be replaced
	// by using C++11 chrono
#if defined(HAVE_CLOCK_GETTIME) && !defined(WIN32)
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME,&tp);

	struct tm *loctime;
	loctime = localtime(&tp.tv_sec);
	milli = (uint32_t) (tp.tv_nsec / 1000000);
#else
	/* Setup time and date */
	struct timeb timebuffer;
	ftime(&timebuffer);

	struct tm *loctime;
	loctime = localtime (&timebuffer.time);
	milli = (uint32_t) timebuffer.millitm;
#endif
	/*
	loctime->tm_hour = 23;
	loctime->tm_min = 59;
	loctime->tm_sec = 45;
	loctime->tm_mday = 28;
	loctime->tm_mon = 2-1;
	loctime->tm_year = 2007 - 1900;
	*/

	dos.date.day=(uint8_t)loctime->tm_mday;
	dos.date.month=(uint8_t)loctime->tm_mon+1;
	dos.date.year=(uint16_t)loctime->tm_year+1900;

	auto ticks=(uint32_t)(((double)(
		loctime->tm_hour*3600*1000+
		loctime->tm_min*60*1000+
		loctime->tm_sec*1000+
		milli))*(((double)PIT_TICK_RATE/65536.0)/1000.0));
	mem_writed(BIOS_TIMER,ticks);
}

static Bitu INT8_Handler(void) {
	/* Increase the bios tick counter */
	uint32_t value = mem_readd(BIOS_TIMER) + 1;
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
			uint32_t ticksnu = (uint32_t)((loctime->tm_hour * 3600 +
			                           loctime->tm_min * 60 + loctime->tm_sec) *
			                          (double)PIT_TICK_RATE / 65536.0);
			int32_t bios = value;int32_t tn = ticksnu;
			int32_t diff = tn - bios;
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

	/* decrement FDD motor timeout counter; roll over on earlier PC, stop at zero on later PC */
	uint8_t val = mem_readb(BIOS_DISK_MOTOR_TIMEOUT);
	if (val || !is_machine_ega_or_better()) {
		mem_writeb(BIOS_DISK_MOTOR_TIMEOUT, val - 1);
	}
	/* clear FDD motor bits when counter reaches zero */
	if (val == 1) {
		mem_writeb(BIOS_DRIVE_RUNNING, mem_readb(BIOS_DRIVE_RUNNING) & 0xF0);
	}
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
	switch (reg_ah) {
	case 0x00:              /* PRINTER: Write Character */
		reg_ah=1;	/* Report a timeout */
		break;
	case 0x01:		/* PRINTER: Initialize port */
		break;
	case 0x02:		/* PRINTER: Get Status */
		reg_ah=0;	
		break;
	};
	return CBRET_NONE;
}

static bool INT14_Wait(uint16_t port, uint8_t mask, uint8_t timeout, uint8_t* retval) {
	const auto starttime = PIC_FullIndex();
	const auto timeout_f = timeout * 1000.0;

	while (((*retval = IO_ReadB(port)) & mask) != mask) {
		if (starttime < (PIC_FullIndex() - timeout_f)) {
			return false;
		}
		if (CALLBACK_Idle()) {
			break;
		}
	}
	return true;
}

static Bitu INT14_Handler(void) {
	if (reg_ah > 0x3 || reg_dx > 0x3) {	// 0-3 serial port functions
										// and no more than 4 serial ports
		LOG_MSG("BIOS INT14: Unhandled call AH=%2X DX=%4x",reg_ah,reg_dx);
		return CBRET_NONE;
	}
	
	uint16_t port = real_readw(0x40,reg_dx*2); // DX is always port number
	uint8_t timeout = mem_readb(BIOS_COM1_TIMEOUT + reg_dx);
	if (port==0)	{
		LOG(LOG_BIOS,LOG_NORMAL)("BIOS INT14: port %d does not exist.",reg_dx);
		return CBRET_NONE;
	}
	switch (reg_ah) {
	case 0x00: {
		// Initialize port
		// Parameters:				Return:
		// AL: port parameters		AL: modem status
		//							AH: line status

		// set baud rate
		Bitu baudrate = {};
		switch (reg_al >> 5) {
		case 0: baudrate = 110; break;
		case 1: baudrate = 150; break;
		case 2: baudrate = 300; break;
		case 3: baudrate = 600; break;
		case 4: baudrate = 1200; break;
		case 5: baudrate = 2400; break;
		case 6: baudrate = 4800; break;
		case 7: baudrate = 9600; break;
		default: assert(false); return CBRET_NONE;
		}

		const auto baudresult = static_cast<uint16_t>(
		        SerialMaxBaudRate / baudrate);

		IO_WriteB(port+3, 0x80);	// enable divider access
		IO_WriteB(port, (uint8_t)baudresult&0xff);
		IO_WriteB(port+1, (uint8_t)(baudresult>>8));

		// set line parameters, disable divider access
		IO_WriteB(port+3, reg_al&0x1F); // LCR
		
		// disable interrupts
		IO_WriteB(port+1, 0); // IER

		// get result
		reg_ah=(uint8_t)(IO_ReadB(port+5)&0xff);
		reg_al=(uint8_t)(IO_ReadB(port+6)&0xff);
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
		reg_ah=(uint8_t)(IO_ReadB(port+5)&0xff);
		reg_al=(uint8_t)(IO_ReadB(port+6)&0xff);
		CALLBACK_SCF(false);
		break;
	}
	return CBRET_NONE;
}

static Bitu INT15_Handler(void) {
	static uint16_t biosConfigSeg=0;
	switch (reg_ah) {
	case 0x24: // A20 stuff
		switch (reg_al) {
		case 0:	//Disable a20
			MEM_A20_Enable(false);
			reg_ah = 0;                   //call successful
			CALLBACK_SCF(false);             //clear on success
			break;
		case 1:	//Enable a20
			MEM_A20_Enable( true );
			reg_ah = 0;                   //call successful
			CALLBACK_SCF(false);             //clear on success
			break;
		case 2:	//Query a20
			reg_al = MEM_A20_Enabled() ? 0x1 : 0x0;
			reg_ah = 0;                   //call successful
			CALLBACK_SCF(false);
			break;
		case 3:	//Get a20 support
			reg_bx = 0x3;		//Bitmask, keyboard and 0x92
			reg_ah = 0;         //call successful
			CALLBACK_SCF(false);
			break;
		default:
			goto unhandled;
		}
		break;
	case 0xC0: /* Get Configuration*/ {
		if (biosConfigSeg == 0)
			biosConfigSeg = DOS_GetMemory(1); // We have 16 bytes

		PhysPt data = PhysicalMake(biosConfigSeg, 0);
		mem_writew(data, 8); // 8 Bytes following

		// Tandy and IBM PCjr
		if (is_machine_pcjr_or_tandy()) {
			if (is_machine_tandy()) {
				mem_writeb(data + 2, 0xFF); // Model ID (Tandy)
			} else {
				mem_writeb(data + 2, 0xFD); // Model ID (PCjr)
			}
			mem_writeb(data + 3, 0x0A); // Submodel ID
			mem_writeb(data + 4, 0x10); // Bios Revision
			                            // Feature Bytes 1 and 2
			// Tandy doesn't have a 2nd PIC, left as-is
			mem_writeb(data + 5, (1 << 6) | (1 << 5) | (1 << 4));
			mem_writeb(data + 6, (1 << 6));
		}
		// IBM PS/1 2011
		else if (PS1AUDIO_IsEnabled()) {
			mem_writeb(data + 2, 0xFC); // Model ID
			mem_writeb(data + 3, 0x0B); // Submodel ID
			mem_writeb(data + 4, 0x00); // Bios Revision
			mem_writeb(data + 5, 0b1111'0100); // Feature Byte 1 (0xF4)
			mem_writeb(data + 6, 0b0100'0000); // Feature Byte 2 (0x40)
		}
		// General PCs
		else {
			mem_writeb(data + 2, 0xFC); // Model ID
			mem_writeb(data + 3, 0x00); // Submodel ID
			mem_writeb(data + 4, 0x01); // Bios Revision
			                            // Feature Bytes 1 and 2
			mem_writeb(data + 5, (1 << 6) | (1 << 5) | (1 << 4));
			mem_writeb(data + 6, (1 << 6));
		}
		mem_writeb(data + 7, 0); // Feature Byte 3
		mem_writeb(data + 8, 0); // Feature Byte 4
		mem_writeb(data + 9, 0); // Feature Byte 5
		CPU_SetSegGeneral(es, biosConfigSeg);
		reg_bx = 0;
		reg_ah = 0;
		CALLBACK_SCF(false);
	}; break;
	case 0x4f: /* BIOS - Keyboard intercept */
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
			uint32_t count=(reg_cx<<16)|reg_dx;
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
			if (JOYSTICK_IsAccessible(0) || JOYSTICK_IsAccessible(1)) {
				reg_al = IO_ReadB(0x201)&0xf0;
				CALLBACK_SCF(false);
			} else {
				// dos values
				reg_ax = 0x00f0; reg_dx = 0x0201;
				CALLBACK_SCF(true);
			}
		} else if (reg_dx == 0x0001) {
			if (JOYSTICK_IsAccessible(0)) {
				reg_ax = (uint16_t)(JOYSTICK_GetMove_X(0)*127+128);
				reg_bx = (uint16_t)(JOYSTICK_GetMove_Y(0)*127+128);
				if(JOYSTICK_IsAccessible(1)) {
					reg_cx = (uint16_t)(JOYSTICK_GetMove_X(1)*127+128);
					reg_dx = (uint16_t)(JOYSTICK_GetMove_Y(1)*127+128);
				}
				else {
					reg_cx = reg_dx = 0;
				}
				CALLBACK_SCF(false);
			} else if (JOYSTICK_IsAccessible(1)) {
				reg_ax = reg_bx = 0;
				reg_cx = (uint16_t)(JOYSTICK_GetMove_X(1)*127+128);
				reg_dx = (uint16_t)(JOYSTICK_GetMove_Y(1)*127+128);
				CALLBACK_SCF(false);
			} else {			
				reg_ax = reg_bx = reg_cx = reg_dx = 0;
				CALLBACK_SCF(true);
			}
		} else {
			LOG(LOG_BIOS,LOG_ERROR)("INT15:84:Unknown Bios Joystick functionality.");
		}
		break;

	case 0x86: /* BIOS - WAIT (AT,PS) */
	{
		if (mem_readb(BIOS_WAIT_FLAG_ACTIVE)) {
			reg_ah = 0x83;
			CALLBACK_SCF(true);
			break;
		}
		uint32_t count = (reg_cx << 16) | reg_dx;

		const auto timeout = PIC_FullIndex() +
		                     static_cast<double>(count) / 1000.0 + 1.0;

		mem_writed(BIOS_WAIT_FLAG_POINTER, RealMake(0, BIOS_WAIT_FLAG_TEMP));
		mem_writed(BIOS_WAIT_FLAG_COUNT, count);
		mem_writeb(BIOS_WAIT_FLAG_ACTIVE, 1);

		// Unmask IRQ 8 if masked
		uint8_t mask = IO_Read(0xa1);
		if (mask & 1) {
			IO_Write(0xa1, mask & ~1);
		}

		// Reprogram RTC to start
		IO_Write(0x70, 0xb);
		IO_Write(0x71, IO_Read(0x71) | 0x40);

		while (mem_readd(BIOS_WAIT_FLAG_COUNT)) {
			if (PIC_FullIndex() > timeout) {
				// RTC timer not working for some reason
				mem_writeb(BIOS_WAIT_FLAG_ACTIVE, 0);
				IO_Write(0x70, 0xb);
				IO_Write(0x71, IO_Read(0x71) & ~0x40);
				break;
			}
			if (CALLBACK_Idle()) {
				break;
			}
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
	case 0xc2: /* BIOS PS2 Pointing Device Support */
		MOUSEBIOS_Subfunction_C2();
		break;
	case 0xc3:      /* set carry flag so BorlandRTM doesn't assume a VECTRA/PS2 */
		reg_ah=0x86;
		CALLBACK_SCF(true);
		break;
	case 0xc4:	/* BIOS POS Programm option Select */
		LOG(LOG_BIOS, LOG_WARN)("INT15:Function %X called, programmable options not supported", reg_ah);
		CALLBACK_SCF(true);
		break;
	case 0xe8:
		switch (reg_al) {
		case 0x01:
			reg_ax = 0; // extended memory between 1MB and 16MB, in 1KB blocks
			reg_bx = 0; // extended memory above 16MB, in 64KB blocks
			{
				const auto mem_in_kb = MEM_TotalPages() * 4;
				if (mem_in_kb > 1024) {
					reg_ax = std::min(static_cast<uint16_t>(mem_in_kb - 1024),
					                  static_cast<uint16_t>(15 * 1024));
				}
				if (mem_in_kb > 16 * 1024) {
					reg_bx = clamp_to_uint16((mem_in_kb - 16 * 1024) / 64);
				}
			}
			reg_cx = reg_ax; // configured memory between 1MB and 16MB, in 1KB blocks
			reg_dx = reg_bx; // configured memory above 16MB, in 64KB blocks
			CALLBACK_SCF(false);
			break;
		case 0x20:
			if (reg_edx == 0x534d4150 && reg_ecx >= 20 &&
			    (MEM_TotalPages() * 4) >= 24000) {
				// return a minimalist list:
				// - 0x000000-0x09EFFF    free
				// - 0x0C0000-0x0FFFFF    reserved
				// - 0x100000-...         free (no ACPI tables)
			    	reg_eax = reg_edx;
				if (reg_ebx < 3) {
					uint32_t base = 0;
					uint32_t len  = 0;
					uint32_t type = 0;

					// type 1 - memory, available to OS
					// type 2 - reserved, not available
					//          (e.g. system ROM, memory-mapped device)
					// type 3 - ACPI reclaim memory
					//          (usable by OS after reading ACPI tables)
					// type 4 - ACPI NVS Memory
					//          (OS is required to save this memory between NVS)

					switch (reg_ebx) {
					case 0:
						base = 0x000000;
						len  = 0x09f000;
						type = 1;
						break;
					case 1:
						base = 0x0c0000;
						len  = 0x040000;
						type = 2;
						break;
					case 2:
						base = 0x100000;
						len  = (MEM_TotalPages() * 4096) - 0x100000;
						type = 1;
						break;
					default:
						E_Exit("Despite checks EBX is wrong value"); // BUG!
					}

					// Write map to ES:DI
					const auto seg = SegValue(es);
					real_writed(seg, reg_di + 0x00, base);
					real_writed(seg, reg_di + 0x04, 0);
					real_writed(seg, reg_di + 0x08, len);
					real_writed(seg, reg_di + 0x0c, 0);
					real_writed(seg, reg_di + 0x10, type);
					reg_ecx = 20;

					// Return EBX pointing to next entry.
					// Wrap around, as most BIOSes do.
					// The program is supposed to stop on
					// CF == 1 or when we return EBX == 0
					if (++reg_ebx >= 3) {
						reg_ebx = 0;
					}

					CALLBACK_SCF(false);
				} else {
					CALLBACK_SCF(true);
				}
			} else {
				reg_eax = 0x8600;
				CALLBACK_SCF(true);
			}
			break;
		case 0x81:
			reg_eax = 0; // extended memory between 1MB and 16MB, in 1KB blocks
			reg_ebx = 0; // extended memory above 16MB, in 64KB blocks
			{
				const auto mem_in_kb = MEM_TotalPages() * 4;
				if (mem_in_kb > 1024) {
					reg_eax = std::min(static_cast<uint32_t>(mem_in_kb - 1024),
					                   static_cast<uint32_t>(15 * 1024));
				}
				if (mem_in_kb > 16 * 1024) {
					reg_ebx = check_cast<uint32_t>((mem_in_kb - 16 * 1024) / 64);
				}
			}
			reg_ecx = reg_eax; // configured memory between 1MB and 16MB, in 1KB blocks
			reg_edx = reg_ebx; // configured memory above 16MB, in 64KB blocks
			CALLBACK_SCF(false);
			break;
		default:
			goto unhandled;
		}
		break;
	default:
	unhandled:
		LOG(LOG_BIOS,LOG_ERROR)("INT15:Unknown call %4X",reg_ax);
		reg_ah=0x86;
		CALLBACK_SCF(true);
		if (is_machine_ega_or_better() || is_machine_cga()) {
			/* relict from comparisons, as int15 exits with a retf2 instead of an iret */
			CALLBACK_SZF(false);
		}
	}
	return CBRET_NONE;
}

//  BIOS ISRs
// ~~~~~~~~~~
// Similar to identifying the PICs controllers as primary and secondary (see
// discussion in pic.cpp), the two BIOS Interrupt Service Routines (ISRs) are
// given similar identifiers for the same reasons.

static Bitu Default_IRQ_Handler()
{
	IO_WriteB(0x20,0x0b);
	auto primary_isr = static_cast<uint8_t>(IO_ReadB(0x20));
	if (primary_isr) {
		IO_WriteB(0xa0, 0x0b);
		const auto secondary_isr = static_cast<uint8_t>(IO_ReadB(0xa0));
		if (secondary_isr) {
			IO_WriteB(0xa1, IO_ReadB(0xa1) | secondary_isr);
			IO_WriteB(0xa0, 0x20);
		} else {
			IO_WriteB(0x21, IO_ReadB(0x21) | (primary_isr & ~4));
		}
		IO_WriteB(0x20, 0x20);
#if C_DEBUGGER
		uint16_t irq = 0;
		uint16_t isr = secondary_isr ? secondary_isr << 8 : primary_isr;
		while (isr >>= 1)
			irq++;
		LOG(LOG_BIOS, LOG_WARN)("Unexpected IRQ %u", irq);
#endif
	} else {
		primary_isr = 0xff;
	}
	mem_writeb(BIOS_LAST_UNEXPECTED_IRQ, primary_isr);
	return CBRET_NONE;
}

static Bitu reboot_handler()
{
	LOG_MSG("BIOS: Reboot requested");

	// Line number for text display
	constexpr uint8_t text_row = 2;

	// Switch to text mode
	reg_ah = 0x00;
	reg_al = 0x02; // 80x25
	CALLBACK_RunRealInt(0x10);
	const auto screen_width = INT10_GetTextColumns();

	// Disable the blinking cursor
	reg_ah = 0x01;
	reg_ch = bit::literals::b5; // bit 5 = disable cursor
	reg_cl = 0;
	CALLBACK_RunRealInt(0x10);

	// Prepare the text to display
	std::vector<std::string> conunter_text = {};

	conunter_text.emplace_back(MSG_Get("BIOS_REBOOTING_1"));
	conunter_text.emplace_back(MSG_Get("BIOS_REBOOTING_2"));
	conunter_text.emplace_back(MSG_Get("BIOS_REBOOTING_3"));

	size_t max_length = 0;

	for (auto& entry : conunter_text) {
		max_length = std::max(max_length, entry.length());
	}
	for (auto& entry : conunter_text) {
		entry.resize(max_length, ' ');
	}
	// We want the text mostly centered, but we don't want to change the
	// start column if the plural grammar form is longer/shorter than
	// the singular one
	const uint8_t start_column = (screen_width - max_length) / 2;

	// Display the text/counter
	while (!conunter_text.empty()) {

		// Set cursor position to center the text output
		reg_ah = 0x02;
		reg_dh = text_row;
		reg_dl = start_column;
		reg_bh = 0; // page
		CALLBACK_RunRealInt(0x10);

		// Display the counter text, remove it from the list
		reg_ah = 0x0e;
		reg_bl = 0x00; // page
		for (const auto c : conunter_text.back()) {
			reg_al = static_cast<uint8_t>(c);
			CALLBACK_RunRealInt(0x10);
		}
		conunter_text.pop_back();

		// Wait one second
		constexpr auto delay_ms = 1000.0;
		const auto start = PIC_FullIndex();
		while ((PIC_FullIndex() - start) < delay_ms) {
			// Bail out if the user closes the window.
			// Otherwise we get stuck in an infinite loop.
			if (CALLBACK_Idle()) {
				return CBRET_NONE;
			}
		}
	}

	// Restart
	DOSBOX_Restart();
	return CBRET_NONE;
}

void BIOS_SetEquipment(uint16_t equipment)
{
	mem_writew(BIOS_CONFIGURATION, equipment);
	if (is_machine_ega_or_better()) {
		// EGA/VGA startup display mode differs in CMOS
		equipment &= ~0x30;
	}
	CMOS_SetRegister(0x14, (uint8_t) (equipment & 0xff)); //Should be updated on changes
}

void BIOS_ZeroExtendedSize(bool in) {
	if(in) other_memsystems++; 
	else other_memsystems--;
	if(other_memsystems < 0) other_memsystems=0;
}

static void shutdown_tandy_sb_dac_callbacks()
{
	// Abort DAC playing when via the Sound Blaster's DAC
	if (tandy_sb.port) {
		IO_Write(tandy_sb.port + 0xc, 0xd3);
		IO_Write(tandy_sb.port + 0xc, 0xd0);
	}
	real_writeb(0x40, 0xd4, 0x00);
	if (tandy_dac_callback[0]) {
		LOG_MSG("BIOS: Shutting down Tandy DAC interrupt callbacks");
		uint32_t orig_vector = real_readd(0x40, 0xd6);
		if (orig_vector == tandy_dac_callback[0]->Get_RealPointer()) {
			// Set IRQ vector to old value
			uint8_t tandy_irq = 7;
			if (tandy_sb.port) {
				tandy_irq = tandy_sb.irq;
			} else if (tandy_dac.port) {
				tandy_irq = tandy_dac.irq;
			}
			uint8_t tandy_irq_vector = tandy_irq;
			if (tandy_irq_vector < 8) {
				tandy_irq_vector += 8;
			} else {
				tandy_irq_vector += (0x70 - 8);
			}

			RealSetVec(tandy_irq_vector, real_readd(0x40, 0xd6));
			real_writed(0x40, 0xd6, 0x00000000);
		}
		tandy_dac_callback[0] = {};
		tandy_dac_callback[1] = {};
	}
	tandy_sb.port  = 0;
	tandy_dac.port = 0;
}

// The Tandy Sound card requests DAC support which the following configures via
// BIOS-based interrupt callbacks using either a Sound Blaster or the 'actual'
// Tandy DAC module, respectively. If neither are present then the BIOS
// callbacks aren't setup.
//
// The BIOS callbacks are shutdown when the backing device is shutdown to avoid
// advertizing the DAC's presence when none exists.
//
// Returns true if a DAC was actually setup.
//
bool BIOS_ConfigureTandyDacCallbacks(const std::optional<bool> maybe_request_dac)
{
	// Holds the Tandy Sound card's request based on the presence of the
	// optional 'maybe_request_dac' argument. This allows other modules
	// (like the Sound Blaster) to run this function without any arguments
	// to re-assess if BIOS callback can potentially be setup.
	//
	static bool dac_requested = false;

	if (maybe_request_dac) {
		dac_requested = *maybe_request_dac;
	}

	// The BIOS DAC handling depends on the BIOS IRQ vectors being setup, so
	// we only proceed once those are in place. We use the presence of the
	// machine signature (either Tandy or PC) to indicate this.
	//
	if (mem_readb(BiosMachineSignatureAddress) == 0) {
		return false;
	}

	shutdown_tandy_sb_dac_callbacks();

	if (dac_requested) {
		// Tandy DAC sound requested, see if soundblaster device is available
		Bitu tandy_dac_type = 0;
		if (Tandy_InitializeSB()) {
			tandy_dac_type = 1;
		} else if (Tandy_InitializeTS()) {
			tandy_dac_type = 2;
		}
		if (tandy_dac_type) {
			real_writew(0x40, 0xd0, 0x0000);
			real_writew(0x40, 0xd2, 0x0000);
			real_writeb(0x40, 0xd4, 0xff); // Tandy DAC init value
			real_writed(0x40, 0xd6, 0x00000000);
			// Install the DAC callback handler
			tandy_dac_callback[0] = std::make_unique<CALLBACK_HandlerObject>(
			        CALLBACK_HandlerObject());
			tandy_dac_callback[1] = std::make_unique<CALLBACK_HandlerObject>(
			        CALLBACK_HandlerObject());
			tandy_dac_callback[0]->Install(&IRQ_TandyDAC,
			                               CB_IRET,
			                               "Tandy DAC IRQ");
			tandy_dac_callback[1]->Install(nullptr,
			                               CB_TDE_IRET,
			                               "Tandy DAC end transfer");
			// pseudocode for CB_TDE_IRET:
			//	push ax
			//	mov ax, 0x91fb
			//	int 15
			//	cli
			//	mov al, 0x20
			//	out 0x20, al
			//	pop ax
			//	iret

			uint8_t tandy_irq = 7;
			if (tandy_dac_type == 1) {
				LOG_MSG("BIOS: Tandy DAC interrupt linked to Sound Blaster on IRQ %u",
				        tandy_sb.irq);
				tandy_irq = tandy_sb.irq;
			} else if (tandy_dac_type == 2) {
				LOG_MSG("BIOS: Tandy DAC interrupt linked to Tandy Sound on IRQ %u",
				        tandy_dac.irq);
				tandy_irq = tandy_dac.irq;
			}
			uint8_t tandy_irq_vector = tandy_irq;
			if (tandy_irq_vector < 8) {
				tandy_irq_vector += 8;
			} else {
				tandy_irq_vector += (0x70 - 8);
			}

			RealPt current_irq = RealGetVec(tandy_irq_vector);
			real_writed(0x40, 0xd6, current_irq);
			for (auto i = 0; i < 0x10; i++) {
				phys_writeb(PhysicalMake(0xf000, 0xa084 + i), 0x80);
			}
			return true;
		}
	}
	// Indicate that the Tandy DAC callbacks are unavailable
	real_writeb(0x40, 0xd4, 0x00);
	return false;
}

void BIOS_SetupKeyboard(void);
void BIOS_SetupDisks(void);

class BIOS final {
private:
	CALLBACK_HandlerObject callback[11];
	void AddMessages();
public:
	BIOS()
	{
		AddMessages();

		/* Clear the Bios Data Area (0x400-0x5ff, 0x600- is accounted to DOS) */
		for (uint16_t i=0;i<0x200;i++) real_writeb(0x40,i,0);

		/* Setup all the interrupt handlers the bios controls */

		/* INT 8 Clock IRQ Handler */
		auto call_irq0 = CALLBACK_Allocate();
		CALLBACK_Setup(call_irq0,INT8_Handler,CB_IRQ0,RealToPhysical(BIOS_DEFAULT_IRQ0_LOCATION),"IRQ 0 Clock");
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
		if (is_machine_tandy()) {
			/* reduce reported memory size for the Tandy (32k graphics memory
			   at the end of the conventional 640k) */
			mem_writew(BIOS_MEMORY_SIZE, 624);
			mem_writew(BIOS_TRUE_MEMORY_SIZE, ConventionalMemorySizeKb);

		} else if (is_machine_pcjr()) {
			const auto section = get_section("dos");
			assert(section);

			const std::string pcjr_memory_config = section->GetString("pcjr_memory_config");
			if (pcjr_memory_config == "expanded") {
				mem_writew(BIOS_MEMORY_SIZE, ConventionalMemorySizeKb);
				mem_writew(BIOS_TRUE_MEMORY_SIZE, ConventionalMemorySizeKb);
			} else {
				assert(pcjr_memory_config == "standard");
				mem_writew(BIOS_MEMORY_SIZE, PcjrStandardMemorySizeKb - PcjrVideoMemorySizeKb);
				mem_writew(BIOS_TRUE_MEMORY_SIZE, PcjrStandardMemorySizeKb);
			}
		} else {
			mem_writew(BIOS_MEMORY_SIZE, ConventionalMemorySizeKb);
		}

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
		callback[9].Install(nullptr,CB_IRQ9,"irq 9 bios");
		callback[9].Set_RealVec(0x71);

		/* Reboot */
		// This handler is an exit for more than only reboots, since we
		// don't handle these cases
		callback[10].Install(&reboot_handler,CB_IRET,"reboot");
		
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
		phys_writew(0xFFFF1,RealOffset(BIOS_DEFAULT_RESET_LOCATION));	// offset
		phys_writew(0xFFFF3,RealSegment(BIOS_DEFAULT_RESET_LOCATION));	// segment

		// Compatible POST routine location: jump to the callback
		phys_writeb(RealToPhysical(BIOS_DEFAULT_RESET_LOCATION)+0,0xEA);				// FARJMP
		phys_writew(RealToPhysical(BIOS_DEFAULT_RESET_LOCATION)+1,RealOffset(rptr));	// offset
		phys_writew(RealToPhysical(BIOS_DEFAULT_RESET_LOCATION)+3,RealSegment(rptr)); // segment

		/* Irq 2 */
		auto call_irq2 = CALLBACK_Allocate();
		CALLBACK_Setup(call_irq2,nullptr,CB_IRET_EOI_PIC1,RealToPhysical(BIOS_DEFAULT_IRQ2_LOCATION),"irq 2 bios");
		RealSetVec(0x0a,BIOS_DEFAULT_IRQ2_LOCATION);

		/* Default IRQ handler */
		auto call_irq_default = CALLBACK_Allocate();
		CALLBACK_Setup(call_irq_default,&Default_IRQ_Handler,CB_IRET,"irq default");
		RealSetVec(0x0b,CALLBACK_RealPointer(call_irq_default)); // IRQ 3
		RealSetVec(0x0c,CALLBACK_RealPointer(call_irq_default)); // IRQ 4
		RealSetVec(0x0d,CALLBACK_RealPointer(call_irq_default)); // IRQ 5
		RealSetVec(0x0f,CALLBACK_RealPointer(call_irq_default)); // IRQ 7
		RealSetVec(0x72,CALLBACK_RealPointer(call_irq_default)); // IRQ 10
		RealSetVec(0x73,CALLBACK_RealPointer(call_irq_default)); // IRQ 11

		// INT 05h: Print Screen
		// IRQ1 handler calls it when PrtSc key is pressed; does nothing unless hooked
		phys_writeb(RealToPhysical(BIOS_DEFAULT_INT5_LOCATION),0xcf);
		RealSetVec(0x05,BIOS_DEFAULT_INT5_LOCATION);

		/* Some hardcoded vectors */
		phys_writeb(RealToPhysical(BIOS_DEFAULT_HANDLER_LOCATION),0xcf);	/* bios default interrupt vector location -> IRET */
		phys_writew(RealToPhysical(RealGetVec(0x12))+0x12,0x20); //Hack for Jurresic

		if (is_machine_tandy()) {
			// Tandy model
			phys_writeb(0xffffe, 0xff);
		} else if (is_machine_pcjr()) {
			// PCJr model
			phys_writeb(0xffffe, 0xfd);
		} else {
			phys_writeb(0xffffe, 0xfc);
		}

		// System BIOS identification
		uint16_t i = 0;
		for (const auto c : "IBM COMPATIBLE 486 BIOS COPYRIGHT The DOSBox Team.")
			phys_writeb(0xfe00e + i++, static_cast<uint8_t>(c));

		// System BIOS version
		i = 0;
		for (const auto c : "DOSBox FakeBIOS v1.0")
			phys_writeb(0xfe061 + i++, static_cast<uint8_t>(c));

		// write system BIOS date
		i = 0;
		for (const auto c : "01/01/92")
			phys_writeb(0xffff5 + i++, static_cast<uint8_t>(c));

		// write machine signature
		const uint8_t machine_signature = is_machine_tandy() ? 0xff : 0x55;
		phys_writeb(BiosMachineSignatureAddress, machine_signature);

		// Note: The BIOS 0x40 segment (Tandy DAC) callbacks can also be
		// re-configured when the Tandy Sound card is initialized
		// followed by state changes in either backing DAC modules
		// (pre-SB16 Sound Blaster or the Tandy DAC).
		//
		BIOS_ConfigureTandyDacCallbacks();

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
		if ((IO_Read(0x378) != 0xff) || (IO_Read(0x379) != 0xff)) {
			// this is our LPT1
			mem_writew(BIOS_ADDRESS_LPT1,0x378);
			ppindex++;
			if ((IO_Read(0x278) != 0xff) || (IO_Read(0x279) != 0xff)) {
				// this is our LPT2
				mem_writew(BIOS_ADDRESS_LPT2,0x278);
				ppindex++;
				if ((IO_Read(0x3bc) != 0xff) ||
				    (IO_Read(0x3be) != 0xff)) {
					// this is our LPT3
					mem_writew(BIOS_ADDRESS_LPT3,0x3bc);
					ppindex++;
				}
			} else if ((IO_Read(0x3bc) != 0xff) ||
			           (IO_Read(0x3be) != 0xff)) {
				// this is our LPT2
				mem_writew(BIOS_ADDRESS_LPT2,0x3bc);
				ppindex++;
			}
		} else if ((IO_Read(0x3bc) != 0xff) || (IO_Read(0x3be) != 0xff)) {
			// this is our LPT1
			mem_writew(BIOS_ADDRESS_LPT1,0x3bc);
			ppindex++;
			if ((IO_Read(0x278) != 0xff) || (IO_Read(0x279) != 0xff)) {
				// this is our LPT2
				mem_writew(BIOS_ADDRESS_LPT2,0x278);
				ppindex++;
			}
		} else if ((IO_Read(0x278) != 0xff) || (IO_Read(0x279) != 0xff)) {
			// this is our LPT1
			mem_writew(BIOS_ADDRESS_LPT1,0x278);
			ppindex++;
		}

		/* Setup equipment list */
		// look http://www.bioscentral.com/misc/bda.htm
		
		//uint16_t config=0x4400;	//1 Floppy, 2 serial and 1 parallel 
		uint16_t config = 0x0;
		
		// set number of parallel ports
		// if(ppindex == 0) config |= 0x8000; // looks like 0 ports are not specified
		//else if(ppindex == 1) config |= 0x0000;
		if(ppindex == 2) config |= 0x4000;
		else config |= 0xc000;	// 3 ports

		//FPU
		config|=0x2;

		switch (machine) {
		case MachineType::Hercules:
			//Startup monochrome
			config|=0x30;
			break;
		case MachineType::CgaMono:
		case MachineType::CgaColor:
		case MachineType::Pcjr:
		case MachineType::Tandy:
		case MachineType::Ega:
		case MachineType::Vga:
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
		if (is_machine_pcjr()) {
			config |= 0x100;
		}
		// Gameport
		config |= 0x1000;
		BIOS_SetEquipment(config);
		/* Setup extended memory size */
		IO_Write(0x70,0x30);
		size_extended=IO_Read(0x71);
		IO_Write(0x70,0x31);
		size_extended|=(IO_Read(0x71) << 8);
		BIOS_HostTimeSync();
	}
	~BIOS(){
		shutdown_tandy_sb_dac_callbacks();
	}
};

void BIOS::AddMessages()
{
	// Some languages have more than one plural form, see:
	// https://en.wikipedia.org/wiki/Grammatical_number
	MSG_Add("BIOS_REBOOTING_3", "Rebooting in 3 seconds...");
	MSG_Add("BIOS_REBOOTING_2", "Rebooting in 2 seconds...");
	MSG_Add("BIOS_REBOOTING_1", "Rebooting in 1 second...");
}

// set com port data in bios data area
// parameter: array of 4 com port base addresses, 0 = none
void BIOS_SetComPorts(uint16_t baseaddr[]) {
	uint16_t portcount=0;
	uint16_t equipmentword;
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
	BIOS_SetEquipment(equipmentword);
}

static std::unique_ptr<BIOS> bios = {};

void BIOS_Init()
{
	bios = std::make_unique<BIOS>();
}

void BIOS_Destroy()
{
	bios = {};
}

