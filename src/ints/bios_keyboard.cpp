// SPDX-FileCopyrightText:  2019-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ints/bios.h"

#include "cpu/callback.h"
#include "hardware/memory.h"
#include "keyboard.h"
#include "cpu/registers.h"
#include "hardware/inout.h"
#include "dos_inc.h"

static callback_number_t call_int16 = 0;
static callback_number_t call_irq1  = 0;
static callback_number_t call_irq6  = 0;

/* Nice table from BOCHS i should feel bad for ripping this */
#define none 0
struct KeyCodes {
	uint16_t normal  = none;
	uint16_t shift   = none;
	uint16_t control = none;
	uint16_t alt     = none;
};

static const KeyCodes& get_key_codes_for(const uint8_t scan_code)
{
	// LOG_MSG("BIOS: Scan code %u (0x%xh) requested", scan_code, scan_code);

	// clang-format off
	// Ref: http://www.quadibloc.com/comp/scan.htm, Set 1 layout
	static constexpr KeyCodes keyboard[] = {
		// index    normal   shift control     alt
		/*   0 */ {   none,   none,   none,   none },
		/*   1 */ { 0x011b, 0x011b, 0x011b, 0x01f0 }, /* escape */
		/*   2 */ { 0x0231, 0x0221,   none, 0x7800 }, /* 1! */
		/*   3 */ { 0x0332, 0x0340, 0x0300, 0x7900 }, /* 2@ */
		/*   4 */ { 0x0433, 0x0423,   none, 0x7a00 }, /* 3# */
		/*   5 */ { 0x0534, 0x0524,   none, 0x7b00 }, /* 4$ */
		/*   6 */ { 0x0635, 0x0625,   none, 0x7c00 }, /* 5% */
		/*   7 */ { 0x0736, 0x075e, 0x071e, 0x7d00 }, /* 6^ */
		/*   8 */ { 0x0837, 0x0826,   none, 0x7e00 }, /* 7& */
		/*   9 */ { 0x0938, 0x092a,   none, 0x7f00 }, /* 8* */
		/*  10 */ { 0x0a39, 0x0a28,   none, 0x8000 }, /* 9( */
		/*  11 */ { 0x0b30, 0x0b29,   none, 0x8100 }, /* 0) */
		/*  12 */ { 0x0c2d, 0x0c5f, 0x0c1f, 0x8200 }, /* -_ */
		/*  13 */ { 0x0d3d, 0x0d2b,   none, 0x8300 }, /* =+ */
		/*  14 */ { 0x0e08, 0x0e08, 0x0e7f, 0x0ef0 }, /* backspace */
		/*  15 */ { 0x0f09, 0x0f00, 0x9400,   none }, /* tab */
		/*  16 */ { 0x1071, 0x1051, 0x1011, 0x1000 }, /* Q */
		/*  17 */ { 0x1177, 0x1157, 0x1117, 0x1100 }, /* W */
		/*  18 */ { 0x1265, 0x1245, 0x1205, 0x1200 }, /* E */
		/*  19 */ { 0x1372, 0x1352, 0x1312, 0x1300 }, /* R */
		/*  20 */ { 0x1474, 0x1454, 0x1414, 0x1400 }, /* T */
		/*  21 */ { 0x1579, 0x1559, 0x1519, 0x1500 }, /* Y */
		/*  22 */ { 0x1675, 0x1655, 0x1615, 0x1600 }, /* U */
		/*  23 */ { 0x1769, 0x1749, 0x1709, 0x1700 }, /* I */
		/*  24 */ { 0x186f, 0x184f, 0x180f, 0x1800 }, /* O */
		/*  25 */ { 0x1970, 0x1950, 0x1910, 0x1900 }, /* P */
		/*  26 */ { 0x1a5b, 0x1a7b, 0x1a1b, 0x1af0 }, /* [{ */
		/*  27 */ { 0x1b5d, 0x1b7d, 0x1b1d, 0x1bf0 }, /* ]} */
		/*  28 */ { 0x1c0d, 0x1c0d, 0x1c0a,   none }, /* Enter */
		/*  29 */ {   none,   none,   none,   none }, /* L Ctrl */
		/*  30 */ { 0x1e61, 0x1e41, 0x1e01, 0x1e00 }, /* A */
		/*  31 */ { 0x1f73, 0x1f53, 0x1f13, 0x1f00 }, /* S */
		/*  32 */ { 0x2064, 0x2044, 0x2004, 0x2000 }, /* D */
		/*  33 */ { 0x2166, 0x2146, 0x2106, 0x2100 }, /* F */
		/*  34 */ { 0x2267, 0x2247, 0x2207, 0x2200 }, /* G */
		/*  35 */ { 0x2368, 0x2348, 0x2308, 0x2300 }, /* H */
		/*  36 */ { 0x246a, 0x244a, 0x240a, 0x2400 }, /* J */
		/*  37 */ { 0x256b, 0x254b, 0x250b, 0x2500 }, /* K */
		/*  38 */ { 0x266c, 0x264c, 0x260c, 0x2600 }, /* L */
		/*  39 */ { 0x273b, 0x273a,   none, 0x27f0 }, /* ;: */
		/*  40 */ { 0x2827, 0x2822,   none, 0x28f0 }, /* '" */
		/*  41 */ { 0x2960, 0x297e,   none, 0x29f0 }, /* `~ */
		/*  42 */ {   none,   none,   none,   none }, /* L shift */
		/*  43 */ { 0x2b5c, 0x2b7c, 0x2b1c, 0x2bf0 }, /* |\ */
		/*  44 */ { 0x2c7a, 0x2c5a, 0x2c1a, 0x2c00 }, /* Z */
		/*  45 */ { 0x2d78, 0x2d58, 0x2d18, 0x2d00 }, /* X */
		/*  46 */ { 0x2e63, 0x2e43, 0x2e03, 0x2e00 }, /* C */
		/*  47 */ { 0x2f76, 0x2f56, 0x2f16, 0x2f00 }, /* V */
		/*  48 */ { 0x3062, 0x3042, 0x3002, 0x3000 }, /* B */
		/*  49 */ { 0x316e, 0x314e, 0x310e, 0x3100 }, /* N */
		/*  50 */ { 0x326d, 0x324d, 0x320d, 0x3200 }, /* M */
		/*  51 */ { 0x332c, 0x333c,   none, 0x33f0 }, /* ,< */
		/*  52 */ { 0x342e, 0x343e,   none, 0x34f0 }, /* .> */
		/*  53 */ { 0x352f, 0x353f,   none, 0x35f0 }, /* /? */
		/*  54 */ {   none,   none,   none,   none }, /* R Shift */
		/*  55 */ { 0x372a, 0x372a, 0x9600, 0x37f0 }, /* * */
		/*  56 */ {   none,   none,   none,   none }, /* L Alt */
		/*  57 */ { 0x3920, 0x3920, 0x3920, 0x3920 }, /* space */
		/*  58 */ {   none,   none,   none,   none }, /* caps lock */
		/*  59 */ { 0x3b00, 0x5400, 0x5e00, 0x6800 }, /* F1 */
		/*  60 */ { 0x3c00, 0x5500, 0x5f00, 0x6900 }, /* F2 */
		/*  61 */ { 0x3d00, 0x5600, 0x6000, 0x6a00 }, /* F3 */
		/*  62 */ { 0x3e00, 0x5700, 0x6100, 0x6b00 }, /* F4 */
		/*  63 */ { 0x3f00, 0x5800, 0x6200, 0x6c00 }, /* F5 */
		/*  64 */ { 0x4000, 0x5900, 0x6300, 0x6d00 }, /* F6 */
		/*  65 */ { 0x4100, 0x5a00, 0x6400, 0x6e00 }, /* F7 */
		/*  66 */ { 0x4200, 0x5b00, 0x6500, 0x6f00 }, /* F8 */
		/*  67 */ { 0x4300, 0x5c00, 0x6600, 0x7000 }, /* F9 */
		/*  68 */ { 0x4400, 0x5d00, 0x6700, 0x7100 }, /* F10 */
		/*  69 */ {   none,   none,   none,   none }, /* Num Lock */
		/*  70 */ {   none,   none,   none,   none }, /* Scroll Lock */
		/*  71 */ { 0x4700, 0x4737, 0x7700, 0x0007 }, /* 7 Home */
		/*  72 */ { 0x4800, 0x4838, 0x8d00, 0x0008 }, /* 8 UP */
		/*  73 */ { 0x4900, 0x4939, 0x8400, 0x0009 }, /* 9 PgUp */
		/*  74 */ { 0x4a2d, 0x4a2d, 0x8e00, 0x4af0 }, /* - */
		/*  75 */ { 0x4b00, 0x4b34, 0x7300, 0x0004 }, /* 4 Left */
		/*  76 */ { 0x4cf0, 0x4c35, 0x8f00, 0x0005 }, /* 5 */
		/*  77 */ { 0x4d00, 0x4d36, 0x7400, 0x0006 }, /* 6 Right */
		/*  78 */ { 0x4e2b, 0x4e2b, 0x9000, 0x4ef0 }, /* + */
		/*  79 */ { 0x4f00, 0x4f31, 0x7500, 0x0001 }, /* 1 End */
		/*  80 */ { 0x5000, 0x5032, 0x9100, 0x0002 }, /* 2 Down */
		/*  81 */ { 0x5100, 0x5133, 0x7600, 0x0003 }, /* 3 PgDn */
		/*  82 */ { 0x5200, 0x5230, 0x9200, 0x0000 }, /* 0 Ins */
		/*  83 */ { 0x5300, 0x532e, 0x9300,   none }, /* Del */
		/*  84 */ {   none,   none,   none,   none }, /* SysRq */
		/*  85 */ {   none,   none,   none,   none },
		/*  86 */ { 0x565c, 0x567c,   none,   none }, /* OEM102 */
		/*  87 */ { 0x8500, 0x8700, 0x8900, 0x8b00 }, /* F11 */
		/*  88 */ { 0x8600, 0x8800, 0x8a00, 0x8c00 }, /* F12 */
		/*  89 */ { /* placeholder */ },
		/*  90 */ { /* placeholder */ },
		/*  91 */ {   none,   none,   none,   none }, /* Win Left */
		/*  92 */ {   none,   none,   none,   none }, /* Win Right */
		/*  93 */ { /* placeholder */ }, /* Win Menu */
		/*  94 */ { /* placeholder */ },
		/*  95 */ { /* placeholder */ },
		/*  96 */ { /* placeholder */ },
		/*  97 */ { /* placeholder */ },
		/*  98 */ { /* placeholder */ },
		/*  99 */ { /* placeholder */ }, /* F16 */
		/* 100 */ { /* placeholder */ }, /* F17 */
		/* 101 */ { /* placeholder */ }, /* F18 */
		/* 102 */ { /* placeholder */ }, /* F19 */
		/* 103 */ { /* placeholder */ }, /* F20 */
		/* 104 */ { /* placeholder */ }, /* F21 */
		/* 105 */ { /* placeholder */ }, /* F22 */
		/* 106 */ { /* placeholder */ }, /* F23 */
		/* 107 */ { /* placeholder */ }, /* F24 */
		/* 108 */ { /* placeholder */ },
		/* 109 */ { /* placeholder */ },
		/* 110 */ { /* placeholder */ },
		/* 111 */ { /* placeholder */ },
		/* 112 */ { /* placeholder */ },
		/* 113 */ { /* placeholder */ }, /* Attn  */
		/* 114 */ { /* placeholder */ }, /* CrSel */

		// Key directly left of Right Shift on ABNT layouts
		/* 115 */ {   0x7330, 0x7340, none,   0x73f0 } /* /? ABNT1 or ABNT_C1 */
		//            0x__^^  0x__^^          0x__^^
		// Note: These lower-byte values are guesses based based on prior
		//       patterns. Update them if you find an actual source or spec.
	};
	// clang-format on

	constexpr auto num_scan_codes = std::end(keyboard) - std::begin(keyboard);
	static_assert(num_scan_codes == MAX_SCAN_CODE + 1);

	assert(scan_code < num_scan_codes);
	return keyboard[scan_code];
}

bool BIOS_AddKeyToBuffer(uint16_t code) {
	if (mem_readb(BIOS_KEYBOARD_FLAGS2)&8) return true;
	uint16_t start,end,head,tail,ttail;
	if (is_machine_pcjr()) {
		/* should be done for cga and others as well, to be tested */
		start=0x1e;
		end=0x3e;
	} else {
		start=mem_readw(BIOS_KEYBOARD_BUFFER_START);
		end	 =mem_readw(BIOS_KEYBOARD_BUFFER_END);
	}
	head =mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
	tail =mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);
	ttail=tail+2;
	if (ttail>=end) {
		ttail=start;
	}
	/* Check for buffer Full */
	//TODO Maybe beeeeeeep or something although that should happend when internal buffer is full
	if (ttail==head) return false;
	real_writew(0x40,tail,code);
	mem_writew(BIOS_KEYBOARD_BUFFER_TAIL,ttail);
	return true;
}

static void add_key(uint16_t code) {
	if (code!=0) BIOS_AddKeyToBuffer(code);
}

static bool get_key(uint16_t &code) {
	uint16_t start,end,head,tail,thead;
	if (is_machine_pcjr()) {
		/* should be done for cga and others as well, to be tested */
		start=0x1e;
		end=0x3e;
	} else {
		start=mem_readw(BIOS_KEYBOARD_BUFFER_START);
		end	 =mem_readw(BIOS_KEYBOARD_BUFFER_END);
	}
	head =mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
	tail =mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);

	if (head==tail) return false;
	thead=head+2;
	if (thead>=end) thead=start;
	mem_writew(BIOS_KEYBOARD_BUFFER_HEAD,thead);
	code = real_readw(0x40,head);
	return true;
}

static bool check_key(uint16_t &code) {
	uint16_t head,tail;
	head =mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
	tail =mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);
	code = real_readw(0x40,head);
	// cpu flags from instruction comparing head and tail pointers
	CALLBACK_SZF(head==tail);
	CALLBACK_SCF(head<tail);
	return (head!=tail);
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


/* the scancode is in reg_al */
static Bitu IRQ1_Handler(void) {
/* handling of the locks key is difficult as sdl only gives
 * states for numlock capslock. 
 */
	const auto scancode = reg_al; // Read the code

	uint8_t flags1,flags2,flags3,leds;
	flags1=mem_readb(BIOS_KEYBOARD_FLAGS1);
	flags2=mem_readb(BIOS_KEYBOARD_FLAGS2);
	flags3=mem_readb(BIOS_KEYBOARD_FLAGS3);
	leds  =mem_readb(BIOS_KEYBOARD_LEDS); 
	if (DOS_LayoutKey(scancode,flags1,flags2,flags3)) return CBRET_NONE;
//LOG_MSG("key input %d %d %d %d",scancode,flags1,flags2,flags3);
	switch (scancode) {
	/* First the hard ones  */
	case 0xfa:	/* Acknowledge */
		leds |=0x10;
		break;
	case 0xe1:	/* Extended key special. Only pause uses this */
		flags3 |=0x01;
		break;
	case 0xe0:						/* Extended key */
		flags3 |=0x02;
		break;
	case 0x1d:						/* Ctrl Pressed */
		if (!(flags3 &0x01)) {
			flags1 |=0x04;
			if (flags3 &0x02) flags3 |=0x04;
			else flags2 |=0x01;
		}	/* else it's part of the pause scancodes */
		break;
	case 0x9d:						/* Ctrl Released */
		if (!(flags3 &0x01)) {
			if (flags3 &0x02) flags3 &=~0x04;
			else flags2 &=~0x01;
			if( !( (flags3 &0x04) || (flags2 &0x01) ) ) flags1 &=~0x04;
		}
		break;
	case 0x2a:						/* Left Shift Pressed */
		flags1 |=0x02;
		break;
	case 0xaa:						/* Left Shift Released */
		flags1 &=~0x02;
		break;
	case 0x36:						/* Right Shift Pressed */
		flags1 |=0x01;
		break;
	case 0xb6:						/* Right Shift Released */
		flags1 &=~0x01;
		break;
	case 0x37:						/* Keypad * or PrtSc Pressed */
		if (!(flags3 &0x02)) goto normal_key;
		reg_ip+=7; // call int 5
		break;
	case 0xb7:						/* Keypad * or PrtSc Released */
		if (!(flags3 &0x02)) goto normal_key;
		break;
	case 0x38:						/* Alt Pressed */
		flags1 |=0x08;
		if (flags3 &0x02) flags3 |=0x08;
		else flags2 |=0x02;
		break;
	case 0xb8:						/* Alt Released */
		if (flags3 &0x02) flags3 &= ~0x08;
		else flags2 &= ~0x02;
		if( !( (flags3 &0x08) || (flags2 &0x02) ) ) { /* Both alt released */
			flags1 &= ~0x08;
			uint16_t token =mem_readb(BIOS_KEYBOARD_TOKEN);
			if(token != 0){
				add_key(token);
				mem_writeb(BIOS_KEYBOARD_TOKEN,0);
			}
		}
		break;
	case 0x3a:flags2 |=0x40;break;//CAPSLOCK
	case 0xba:flags1 ^=0x40;flags2 &=~0x40;leds ^=0x04;break;
	case 0x45:
		if (flags3 &0x01) {
			/* last scancode of pause received; first remove 0xe1-prefix */
			flags3 &=~0x01;
			mem_writeb(BIOS_KEYBOARD_FLAGS3,flags3);
			if (flags2&1) {
				/* Ctrl+Pause (Break), special handling needed:
				   add zero to the keyboard buffer, call int 0x1b which
				   sets Ctrl+C flag which calls int 0x23 in certain dos
				   input/output functions;    not handled */
			} else if ((flags2&8)==0) {
				/* normal pause key, enter loop */
				mem_writeb(BIOS_KEYBOARD_FLAGS2,flags2|8);
				IO_Write(0x20,0x20);
				// Interrupts screen output by BIOS until another key is pressed
				// This is seemingly accurate behavior as tested in 86box
				// https://en.wikipedia.org/wiki/Break_key
				while (!shutdown_requested && (mem_readb(BIOS_KEYBOARD_FLAGS2) & 8)) {
					CALLBACK_Idle();
				}
				reg_ip+=5;	// skip out 20,20
				return CBRET_NONE;
			}
		} else {
			/* Num Lock */
			flags2 |=0x20;
		}
		break;
	case 0xc5:
		if (flags3 &0x01) {
			/* pause released */
			flags3 &=~0x01;
		} else {
			flags1^=0x20;
			leds^=0x02;
			flags2&=~0x20;
		}
		break;
	case 0x46:flags2 |=0x10;break;				/* Scroll Lock SDL Seems to do this one fine (so break and make codes) */
	case 0xc6:flags1 ^=0x10;flags2 &=~0x10;leds ^=0x01;break;
//	case 0x52:flags2|=128;break;//See numpad					/* Insert */
	case 0xd2:	
		if(flags3&0x02) { /* Maybe honour the insert on keypad as well */
			flags1^=0x80;
			flags2&=~0x80;
			break; 
		} else {
			goto irq1_end;/*Normal release*/ 
		}
	case 0x47:		/* Numpad */
	case 0x48:
	case 0x49:
	case 0x4b:
	case 0x4c:
	case 0x4d:
	case 0x4f:
	case 0x50:
	case 0x51:
	case 0x52:
	case 0x53: /* del . Not entirely correct, but works fine */
		if(flags3 &0x02) {	/*extend key. e.g key above arrows or arrows*/
			if(scancode == 0x52) flags2 |=0x80; /* press insert */		   
			if(flags1 &0x08) {
				add_key(get_key_codes_for(scancode).normal + 0x5000);
			} else if (flags1 &0x04) {
				add_key((get_key_codes_for(scancode).control & 0xff00) | 0xe0);
			} else if (((flags1 & 0x3) != 0) || ((flags1 & 0x20) != 0)) {
				// Due to |0xe0 results are identical
				add_key((get_key_codes_for(scancode).shift & 0xff00) | 0xe0);
			} else {
				add_key((get_key_codes_for(scancode).normal & 0xff00) | 0xe0);
			}
			break;
		}
		if(flags1 &0x08) {
			const auto token    = mem_readb(BIOS_KEYBOARD_TOKEN);
			const auto alt      = get_key_codes_for(scancode).alt;
			const auto combined = (token * 10 + alt) &
			                      std::numeric_limits<uint8_t>::max();
			mem_writeb(BIOS_KEYBOARD_TOKEN,
			           static_cast<uint8_t>(combined));
		} else if (flags1 & 0x04) {
			add_key(get_key_codes_for(scancode).control);
		} else if (((flags1 & 0x3) != 0) ^ ((flags1 & 0x20) != 0)) {
			// Xor shift and numlock (both means off)
			add_key(get_key_codes_for(scancode).shift);
		} else {
			add_key(get_key_codes_for(scancode).normal);
		}
		break;

	default: /* Normal Key */
normal_key:
		uint16_t asciiscan;
		/* Now Handle the releasing of keys and see if they match up for a code */
		/* Handle the actual scancode */
		if (scancode & 0x80) goto irq1_end;
		if (scancode > MAX_SCAN_CODE) goto irq1_end;
		if (flags1 & 0x08) { 					/* Alt is being pressed */
			asciiscan = get_key_codes_for(scancode).alt;
#if 0 /* old unicode support disabled*/
		} else if (ascii) {
			asciiscan=(scancode << 8) | ascii;
#endif
		} else if (flags1 & 0x04) {					/* Ctrl is being pressed */
			asciiscan = get_key_codes_for(scancode).control;
		} else if (flags1 & 0x03) {					/* Either shift is being pressed */
			asciiscan = get_key_codes_for(scancode).shift;
		} else {
			asciiscan = get_key_codes_for(scancode).normal;
		}
		/* cancel shift is letter and capslock active */
		if(flags1&64) {
			if(flags1&3) {
				/*cancel shift */
				if (((asciiscan & 0x00ff) > 0x40) && ((asciiscan & 0x00ff) < 0x5b)) {
					asciiscan = get_key_codes_for(scancode).normal;
				}
			} else {
				/* add shift */
				if (((asciiscan & 0x00ff) > 0x60) && ((asciiscan & 0x00ff) < 0x7b)) {
					asciiscan = get_key_codes_for(scancode).shift;
				}
			}
		}
		if (flags3 &0x02) {
			/* extended key (numblock), return and slash need special handling */
			if (scancode==0x1c) {	/* return */
				if (flags1 &0x08) asciiscan=0xa600;
				else asciiscan=(asciiscan&0xff)|0xe000;
			} else if (scancode==0x35) {	/* slash */
				if (flags1 &0x08) asciiscan=0xa400;
				else if (flags1 &0x04) asciiscan=0x9500;
				else asciiscan=0xe02f;
			}
		}
		add_key(asciiscan);
		break;
	};
irq1_end:
	if(scancode !=0xe0) flags3 &=~0x02;									//Reset 0xE0 Flag
	mem_writeb(BIOS_KEYBOARD_FLAGS1,flags1);
	if ((scancode&0x80)==0) flags2&=0xf7;
	mem_writeb(BIOS_KEYBOARD_FLAGS2,flags2);
	mem_writeb(BIOS_KEYBOARD_FLAGS3,flags3);
	mem_writeb(BIOS_KEYBOARD_LEDS,leds);
/*	IO_Write(0x20,0x20); moved out of handler to be virtualizable */
#if 0
/* Signal the keyboard for next code */
/* In dosbox port 60 reads do this as well */
	uint8_t old61=IO_Read(0x61);
	IO_Write(0x61,old61 | 128);
	IO_Write(0x64,0xae);
#endif
	return CBRET_NONE;
}


/* check whether key combination is enhanced or not,
   translate key if necessary */
static bool IsEnhancedKey(uint16_t &key) {
	/* test for special keys (return and slash on numblock) */
	if ((key>>8)==0xe0) {
		if (((key&0xff)==0x0a) || ((key&0xff)==0x0d)) {
			/* key is return on the numblock */
			key=(key&0xff)|0x1c00;
		} else {
			/* key is slash on the numblock */
			key=(key&0xff)|0x3500;
		}
		/* both keys are not considered enhanced keys */
		return false;
	} else if (((key>>8)>0x84) || (((key&0xff)==0xf0) && (key>>8))) {
		/* key is enhanced key (either scancode part>0x84 or
		   specially-marked keyboard combination, low part==0xf0) */
		return true;
	}
	/* convert key if necessary (extended keys) */
	if ((key>>8) && ((key&0xff)==0xe0))  {
		key&=0xff00;
	}
	return false;
}

static Bitu INT16_Handler(void) {
	uint16_t temp=0;
	switch (reg_ah) {
	case 0x00: /* GET KEYSTROKE */
		if ((get_key(temp)) && (!IsEnhancedKey(temp))) {
			/* normal key found, return translated key in ax */
			reg_ax=temp;
		} else {
			/* enter small idle loop to allow for irqs to happen */
			reg_ip+=1;
		}
		break;
	case 0x10: /* GET KEYSTROKE (enhanced keyboards only) */
		if (get_key(temp)) {
			if (((temp&0xff)==0xf0) && (temp>>8)) {
				/* special enhanced key, clear low part before returning key */
				temp&=0xff00;
			}
			reg_ax=temp;
		} else {
			/* enter small idle loop to allow for irqs to happen */
			reg_ip+=1;
		}
		break;
	case 0x01: /* CHECK FOR KEYSTROKE */
		// enable interrupt-flag after IRET of this int16
		CALLBACK_SIF(true);
		for (;;) {
			if (check_key(temp)) { //  check_key changes ZF and CF as required
				if (!IsEnhancedKey(temp)) {
					/* normal key, return translated key in ax */
					break;
				} else {
					/* remove enhanced key from buffer and ignore it */
					get_key(temp);
				}
			} else {
				/* no key available, return key at buffer head anyway */
				break;
			}
//			CALLBACK_Idle();
		}
		reg_ax=temp;
		break;
	case 0x11: /* CHECK FOR KEYSTROKE (enhanced keyboards only) */
		// enable interrupt-flag after IRET of this int16
		CALLBACK_SIF(true);
		if (check_key(temp)) { // check_key changes ZF and CF as required
			if (((temp&0xff)==0xf0) && (temp>>8)) {
				/* special enhanced key, clear low part before returning key */
				temp&=0xff00;
			}
		}
		reg_ax=temp;
		break;
	case 0x02:	/* GET SHIFT FLAGS */
		reg_al=mem_readb(BIOS_KEYBOARD_FLAGS1);
		break;
	case 0x03:	/* SET TYPEMATIC RATE AND DELAY */
		if (reg_al == 0x00) { // set default delay and rate
			IO_Write(0x60,0xf3);
			IO_Write(0x60,0x20); // 500 msec delay, 30 cps
		} else if (reg_al == 0x05) { // set repeat rate and delay
			IO_Write(0x60,0xf3);
			const auto rate_and_delay = (reg_bh & 3) << 5 | (reg_bl & 0x1f);
			IO_Write(0x60, check_cast<uint8_t>(rate_and_delay));
		} else {
			LOG(LOG_BIOS,LOG_ERROR)("INT16:Unhandled Typematic Rate Call %2X BX=%X",reg_al,reg_bx);
		}
		break;
	case 0x05:	/* STORE KEYSTROKE IN KEYBOARD BUFFER */
		if (BIOS_AddKeyToBuffer(reg_cx)) reg_al=0;
		else reg_al=1;
		break;
	case 0x12: /* GET EXTENDED SHIFT STATES */
		reg_al = mem_readb(BIOS_KEYBOARD_FLAGS1);
		reg_ah = check_cast<uint8_t>((mem_readb(BIOS_KEYBOARD_FLAGS2) & 0x73) |
		                             // SysReq pressed, bit 7
		                             ((mem_readb(BIOS_KEYBOARD_FLAGS2) & 4) << 5) |
		                             // Right Ctrl/Alt pressed, bits 2,3
		                             (mem_readb(BIOS_KEYBOARD_FLAGS3) & 0x0c));
		break;
	case 0x55:
		/* Weird call used by some dos apps */
		LOG(LOG_BIOS,LOG_NORMAL)("INT16:55:Word TSR compatible call");
		break;
	default:
		LOG(LOG_BIOS,LOG_ERROR)("INT16:Unhandled call %02X",reg_ah);
		break;

	};

	return CBRET_NONE;
}

//Keyboard initialisation. src/gui/sdlmain.cpp
extern bool startup_state_numlock;
extern bool startup_state_capslock;

static void InitBiosSegment(void) {
	/* Setup the variables for keyboard in the bios data segment */
	mem_writew(BIOS_KEYBOARD_BUFFER_START,0x1e);
	mem_writew(BIOS_KEYBOARD_BUFFER_END,0x3e);
	mem_writew(BIOS_KEYBOARD_BUFFER_HEAD,0x1e);
	mem_writew(BIOS_KEYBOARD_BUFFER_TAIL,0x1e);
	uint8_t flag1 = 0;
	uint8_t leds = 16; /* Ack received */
	mem_writeb(BIOS_KEYBOARD_FLAGS1,flag1);
	mem_writeb(BIOS_KEYBOARD_FLAGS2,0);
	mem_writeb(BIOS_KEYBOARD_FLAGS3,16); /* Enhanced keyboard installed */	
	mem_writeb(BIOS_KEYBOARD_TOKEN,0);
	mem_writeb(BIOS_KEYBOARD_LEDS,leds);
}

void BIOS_SetupKeyboard(void) {
	/* Init the variables */
	InitBiosSegment();

	/* Allocate/setup a callback for int 0x16 and for standard IRQ 1 handler */
	call_int16=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int16,&INT16_Handler,CB_INT16,"Keyboard");
	RealSetVec(0x16,CALLBACK_RealPointer(call_int16));

	call_irq1=CALLBACK_Allocate();	
	CALLBACK_Setup(call_irq1,&IRQ1_Handler,CB_IRQ1,RealToPhysical(BIOS_DEFAULT_IRQ1_LOCATION),"IRQ 1 Keyboard");
	RealSetVec(0x09,BIOS_DEFAULT_IRQ1_LOCATION);

	if (is_machine_pcjr()) {
		call_irq6=CALLBACK_Allocate();
		CALLBACK_Setup(call_irq6,nullptr,CB_IRQ6_PCJR,"PCJr kb irq");
		RealSetVec(0x0e,CALLBACK_RealPointer(call_irq6));
	}
}

