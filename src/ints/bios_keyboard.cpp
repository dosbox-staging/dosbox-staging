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

#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "bios.h"
#include "keyboard.h"
#include "regs.h"
#include "inout.h"


static Bitu call_int16,call_irq1;


/* Nice table from BOCHS i should feel bad for ripping this */
#define none 0
#define MAX_SCAN_CODE 0x53
static struct {
  Bit16u normal;
  Bit16u shift;
  Bit16u control;
  Bit16u alt;
  } scan_to_scanascii[MAX_SCAN_CODE + 1] = {
      {   none,   none,   none,   none },
      { 0x011b, 0x011b, 0x011b, 0x0100 }, /* escape */
      { 0x0231, 0x0221,   none, 0x7800 }, /* 1! */
      { 0x0332, 0x0340, 0x0300, 0x7900 }, /* 2@ */
      { 0x0433, 0x0423,   none, 0x7a00 }, /* 3# */
      { 0x0534, 0x0524,   none, 0x7b00 }, /* 4$ */
      { 0x0635, 0x0625,   none, 0x7c00 }, /* 5% */
      { 0x0736, 0x075e, 0x071e, 0x7d00 }, /* 6^ */
      { 0x0837, 0x0826,   none, 0x7e00 }, /* 7& */
      { 0x0938, 0x092a,   none, 0x7f00 }, /* 8* */
      { 0x0a39, 0x0a28,   none, 0x8000 }, /* 9( */
      { 0x0b30, 0x0b29,   none, 0x8100 }, /* 0) */
      { 0x0c2d, 0x0c5f, 0x0c1f, 0x8200 }, /* -_ */
      { 0x0d3d, 0x0d2b,   none, 0x8300 }, /* =+ */
      { 0x0e08, 0x0e08, 0x0e7f,   none }, /* backspace */
      { 0x0f09, 0x0f00,   none,   none }, /* tab */
      { 0x1071, 0x1051, 0x1011, 0x1000 }, /* Q */
      { 0x1177, 0x1157, 0x1117, 0x1100 }, /* W */
      { 0x1265, 0x1245, 0x1205, 0x1200 }, /* E */
      { 0x1372, 0x1352, 0x1312, 0x1300 }, /* R */
      { 0x1474, 0x1454, 0x1414, 0x1400 }, /* T */
      { 0x1579, 0x1559, 0x1519, 0x1500 }, /* Y */
      { 0x1675, 0x1655, 0x1615, 0x1600 }, /* U */
      { 0x1769, 0x1749, 0x1709, 0x1700 }, /* I */
      { 0x186f, 0x184f, 0x180f, 0x1800 }, /* O */
      { 0x1970, 0x1950, 0x1910, 0x1900 }, /* P */
      { 0x1a5b, 0x1a7b, 0x1a1b,   none }, /* [{ */
      { 0x1b5d, 0x1b7d, 0x1b1d,   none }, /* ]} */
      { 0x1c0d, 0x1c0d, 0x1c0a,   none }, /* Enter */
      {   none,   none,   none,   none }, /* L Ctrl */
      { 0x1e61, 0x1e41, 0x1e01, 0x1e00 }, /* A */
      { 0x1f73, 0x1f53, 0x1f13, 0x1f00 }, /* S */
      { 0x2064, 0x2044, 0x2004, 0x2000 }, /* D */
      { 0x2166, 0x2146, 0x2106, 0x2100 }, /* F */
      { 0x2267, 0x2247, 0x2207, 0x2200 }, /* G */
      { 0x2368, 0x2348, 0x2308, 0x2300 }, /* H */
      { 0x246a, 0x244a, 0x240a, 0x2400 }, /* J */
      { 0x256b, 0x254b, 0x250b, 0x2500 }, /* K */
      { 0x266c, 0x264c, 0x260c, 0x2600 }, /* L */
      { 0x273b, 0x273a,   none,   none }, /* ;: */
      { 0x2827, 0x2822,   none,   none }, /* '" */
      { 0x2960, 0x297e,   none,   none }, /* `~ */
      {   none,   none,   none,   none }, /* L shift */
      { 0x2b5c, 0x2b7c, 0x2b1c,   none }, /* |\ */
      { 0x2c7a, 0x2c5a, 0x2c1a, 0x2c00 }, /* Z */
      { 0x2d78, 0x2d58, 0x2d18, 0x2d00 }, /* X */
      { 0x2e63, 0x2e43, 0x2e03, 0x2e00 }, /* C */
      { 0x2f76, 0x2f56, 0x2f16, 0x2f00 }, /* V */
      { 0x3062, 0x3042, 0x3002, 0x3000 }, /* B */
      { 0x316e, 0x314e, 0x310e, 0x3100 }, /* N */
      { 0x326d, 0x324d, 0x320d, 0x3200 }, /* M */
      { 0x332c, 0x333c,   none,   none }, /* ,< */
      { 0x342e, 0x343e,   none,   none }, /* .> */
      { 0x352f, 0x353f,   none,   none }, /* /? */
      {   none,   none,   none,   none }, /* R Shift */
      { 0x372a, 0x372a,   none,   none }, /* * */
      {   none,   none,   none,   none }, /* L Alt */
      { 0x3920, 0x3920, 0x3920, 0x3920 }, /* space */
      {   none,   none,   none,   none }, /* caps lock */
      { 0x3b00, 0x5400, 0x5e00, 0x6800 }, /* F1 */
      { 0x3c00, 0x5500, 0x5f00, 0x6900 }, /* F2 */
      { 0x3d00, 0x5600, 0x6000, 0x6a00 }, /* F3 */
      { 0x3e00, 0x5700, 0x6100, 0x6b00 }, /* F4 */
      { 0x3f00, 0x5800, 0x6200, 0x6c00 }, /* F5 */
      { 0x4000, 0x5900, 0x6300, 0x6d00 }, /* F6 */
      { 0x4100, 0x5a00, 0x6400, 0x6e00 }, /* F7 */
      { 0x4200, 0x5b00, 0x6500, 0x6f00 }, /* F8 */
      { 0x4300, 0x5c00, 0x6600, 0x7000 }, /* F9 */
      { 0x4400, 0x5d00, 0x6700, 0x7100 }, /* F10 */
      {   none,   none,   none,   none }, /* Num Lock */
      {   none,   none,   none,   none }, /* Scroll Lock */
      { 0x4700, 0x4737, 0x7700,   none }, /* 7 Home */
      { 0x4800, 0x4838,   none,   none }, /* 8 UP */
      { 0x4900, 0x4939, 0x8400,   none }, /* 9 PgUp */
      { 0x4a2d, 0x4a2d,   none,   none }, /* - */
      { 0x4b00, 0x4b34, 0x7300,   none }, /* 4 Left */
      { 0x4c00, 0x4c35,   none,   none }, /* 5 */
      { 0x4d00, 0x4d36, 0x7400,   none }, /* 6 Right */
      { 0x4e2b, 0x4e2b,   none,   none }, /* + */
      { 0x4f00, 0x4f31, 0x7500,   none }, /* 1 End */
      { 0x5000, 0x5032,   none,   none }, /* 2 Down */
      { 0x5100, 0x5133, 0x7600,   none }, /* 3 PgDn */
      { 0x5200, 0x5230,   none,   none }, /* 0 Ins */
      { 0x5300, 0x532e,   none,   none }  /* Del */
      };

/*
Old Stuff not needed after i ripped Bochs :)
static Bit8u KeyNoShift[128]={
	27,'1','2','3',		'4','5','6','7',	'8','9','0','-',	'=',8,9,'q',
	'w','e','r','t',	'y','u','i','o',	'p','[',']',13,		255,'a','s','d',
	'f','g','h','j',	'k','l',';','\'',	'`',255,'\\','z',	'x','c','v','b',
	'n','m',',','.',	'/',255,'*',255,	' ',255,0,0,		0,0,0,0,
	0,0,0,0,			255,255,0,0,		0,'-',0,255,		0,'+',0,0,
	0,0,0,0,			0
};

static Bit8u KeyShift[128]={
	27,'!','@','#',		'$','%','^','&',	'*','(',')','_',	'+',8,9,'Q',
	'W','E','R','T',	'Y','U','I','O',	'P','{','}',13,		255,'A','S','D',
	'F','G','H','J',	'K','L',':','"',	'~',255,'|','Z',	'X','C','V','B',
	'N','M','<','>',	'?',255,'*',255,	' ',255,0,0,		0,0,0,0,
	0,0,0,0,			255,255,0,0,		0,'-',0,255,		0,'+',0,0,
	0,0,0,0,			0
};
*/

static void add_key(Bit16u code) {
	Bit16u start,end,head,tail,ttail;
	start=mem_readw(BIOS_KEYBOARD_BUFFER_START);
	end	 =mem_readw(BIOS_KEYBOARD_BUFFER_END);
	head =mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
	tail =mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);
	ttail=tail+2;
	if (ttail>=end) ttail=start;
	/* Check for buffer Full */
	//TODO Maybe beeeeeeep or something although that should happend when internal buffer is full
	if (ttail==head) return;			
	real_writew(0x40,tail,code);
	mem_writew(BIOS_KEYBOARD_BUFFER_TAIL,ttail);
}

static Bit16u get_key(void) {
	Bit16u start,end,head,tail,thead;
	start=mem_readw(BIOS_KEYBOARD_BUFFER_START);
	end	 =mem_readw(BIOS_KEYBOARD_BUFFER_END);
	head =mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
	tail =mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);

	if (head==tail) return 0;
	thead=head+2;
	if (thead>=end) thead=start;
	mem_writew(BIOS_KEYBOARD_BUFFER_HEAD,thead);
	return real_readw(0x40,head);
}

static Bit16u check_key(void) {
	Bit16u head,tail;
	head =mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
	tail =mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);
	if (head==tail) return 0;
	return real_readw(0x40,head);
}


	/*	Flag Byte 1 
		bit 7 =1 INSert active
		bit 6 =1 Caps Lock active
		bit 5 =1 Num Lock active
		bit 4 =1 Scroll Lock active
		bit 3 =1 either Alt pressed
		bit 2 =1 either Ctrl pressed
		bit 1 =1 Left Shift pressed
		bit 0 =1 Right Shift pressed
	*/
	/*	Flag Byte 2
		bit 7 =1 INSert pressed
		bit 6 =1 Caps Lock pressed
		bit 5 =1 Num Lock pressed
		bit 4 =1 Scroll Lock pressed
		bit 3 =1 Pause state active
		bit 2 =1 Sys Req pressed
		bit 1 =1 Left Alt pressed
		bit 0 =1 Left Ctrl pressed
	*/
	/* 
		Keyboard status byte 3
		bit 7 =1 read-ID in progress
		bit 6 =1 last code read was first of two ID codes
		bit 5 =1 force Num Lock if read-ID and enhanced keyboard
		bit 4 =1 enhanced keyboard installed
		bit 3 =1 Right Alt pressed
		bit 2 =1 Right Ctrl pressed
		bit 1 =1 last code read was E0h
		bit 0 =1 last code read was E1h
	*/


static Bitu IRQ1_Handler(void) {
    //TODO CAPSLOCK NUMLOCK SCROLLLOCK maybe :)
	Bit8u code=IO_Read(0x60);
	//TODO maybe implement the int 0x15 ah=4f scancode lookup hook
/* Changed it so the flag handling takes place in here too */
	Bit8u flags1=mem_readb(BIOS_KEYBOARD_FLAGS1);
	Bit8u flags2=mem_readb(BIOS_KEYBOARD_FLAGS2);
	Bit8u flags3=mem_readb(BIOS_KEYBOARD_FLAGS3);
	switch (code) {
	/* First the hard ones  */
	case 0xe0:
		//TODO Think of something else maybe
		flags3|=2;
		break;
	case 29:													/* Ctrl Pressed */
		flags1|=4;
		if (flags3 & 2) flags3|=4;
		else flags2|=1;
		break;
	case 157:													/* Ctrl Released */
		flags1&=~4;
		if (flags3 & 2) flags3&=~4;
		else flags2&=~1;
		break;
	case 42:													/* Left Shift Pressed */
		flags1|=2;
		break;
	case 170:													/* Left Shift Released */
		flags1&=~2;
		break;
	case 54:													/* Right Shift Pressed */
		flags1|=1;
		break;
	case 182:													/* Right Shift Released */
		flags1&=~1;
		break;
	case 56:													/* Alt Pressed */
		flags1|=8;
		if (flags3 & 2) flags3|=8;
		else flags2|=2;
		break;
	case 184:													/* Alt Released */
		flags1&=~8;
		if (flags3 & 2) flags3&=~8;
		else flags2&=~2;
		break;

#if 0
	case 58:p_capslock=true;break;					/* Caps Lock */
	case 186:p_capslock=false;break;
	case 69:p_numlock=true;break;					/* Num Lock */
	case 197:p_numlock=false;break;
	case 70:p_scrolllock=true;break;				/* Scroll Lock */
	case 198:p_scrolllock=false;break;
	case 82:p_insert=true;break;					/* Insert */
	case 210:p_insert=false;a_insert=!a_insert;break;
#endif		
	default:										/* Normal Key */
		Bit16u asciiscan;
		/* Now Handle the releasing of keys and see if they match up for a code */
		flags3&=~2;									//Reset 0xE0 Flag
		/* Handle the actual scancode */
		if (code & 0x80) goto irq1_end;
		if (code > MAX_SCAN_CODE) goto irq1_end;
		if (flags1 & 8) {							/* Alt is being pressed */
			asciiscan=scan_to_scanascii[code].alt;
		} else if (flags1 & 4) {					/* Ctrl is being pressed */
			asciiscan=scan_to_scanascii[code].control;
		} else if (flags1 & 3) {					/* Either shift is being pressed */
//TODO Maybe check for Capslock sometime in some bored way
			asciiscan=scan_to_scanascii[code].shift;
		} else {
			asciiscan=scan_to_scanascii[code].normal;
		}
		add_key(asciiscan);
	};
irq1_end:
	mem_writeb(BIOS_KEYBOARD_FLAGS1,flags1);
	mem_writeb(BIOS_KEYBOARD_FLAGS2,flags2);
	mem_writeb(BIOS_KEYBOARD_FLAGS3,flags3);
	IO_Write(0x20,0x20);
	/* Signal the keyboard for next code */
	Bit8u old61=IO_Read(0x61);
	IO_Write(0x61,old61 | 128);
	IO_Write(0x61,old61 & 127);
	return CBRET_NONE;
}

static Bitu INT16_Handler(void) {
	Bit16u temp;	
	switch (reg_ah) {
	case 0x00: /* GET KEYSTROKE */
	case 0x10:
		{
//TODO find a more elegant way to do this
			do {
				temp=get_key();
				if (temp==0) { flags.intf=true;CALLBACK_Idle();};
			} while (temp==0);
			reg_ax=temp;
			break;
		}
	case 0x01: /* CHECK FOR KEYSTROKE */
	case 0x11:
		temp=check_key();
		if (temp==0) {
			CALLBACK_SZF(true);
		} else {
			CALLBACK_SZF(false);
			reg_ax=temp;
		}
		break;
	case 0x02:	/* GET SHIFT FlAGS */
		reg_al=mem_readb(BIOS_KEYBOARD_FLAGS1);
		break;
	case 0x03:	/* SET TYPEMATIC RATE AND DELAY */
//Have to implement this trhough SDL
		LOG_DEBUG("INT16:Unhandled Typematic Rate Call %2X",reg_al);
		break;
	case 0x05:	/* STORE KEYSTROKE IN KEYBOARD BUFFER */
//TODO make add_key bool :)
		add_key(reg_ax);
		reg_al=0;
		break;
	case 0x12: /* GET EXTENDED SHIFT STATES */
		reg_al=mem_readb(BIOS_KEYBOARD_FLAGS1);
		reg_ah=mem_readb(BIOS_KEYBOARD_FLAGS2);		
		break;
	case 0x55:
		/* Weird call used by some dos apps */
		LOG_DEBUG("INT16:55:Word TSR compatible call");
		break;
	default:
		E_Exit("INT16:Unhandled call %02X",reg_ah);
		break;

	};

	return CBRET_NONE;
}

static void InitBiosSegment(void) {
	/* Setup the variables for keyboard in the bios data segment */
	mem_writew(BIOS_KEYBOARD_BUFFER_START,0x1e);
	mem_writew(BIOS_KEYBOARD_BUFFER_END,0x3e);
	mem_writew(BIOS_KEYBOARD_BUFFER_HEAD,0x1e);
	mem_writew(BIOS_KEYBOARD_BUFFER_TAIL,0x1e);
	mem_writeb(BIOS_KEYBOARD_FLAGS1,0);
	mem_writeb(BIOS_KEYBOARD_FLAGS2,0);
	mem_writeb(BIOS_KEYBOARD_FLAGS3,16);	/* Enhanced keyboard installed */
}

void BIOS_SetupKeyboard(void) {
	/* Init the variables */
	InitBiosSegment();
	/* Allocate a callback for int 0x16 and for standard IRQ 1 handler */
	call_int16=CALLBACK_Allocate();	
	call_irq1=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int16,&INT16_Handler,CB_IRET);
	RealSetVec(0x16,CALLBACK_RealPointer(call_int16));
	CALLBACK_Setup(call_irq1,&IRQ1_Handler,CB_IRET);
	RealSetVec(0x9,CALLBACK_RealPointer(call_irq1));
}

