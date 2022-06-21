/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "dosbox.h"
#include "keyboard.h"

#include "bitops.h"
#include "inout.h"
#include "pic.h"
#include "mem.h"
#include "mixer.h"
#include "timer.h"
#include "support.h"

#define KEYBUFSIZE 32
#define KEYDELAY   0.300 // Considering 20-30 khz serial clock and 11 bits/char

using namespace bit::literals;

enum KeyCommands {
	CMD_NONE,
	CMD_SETLEDS,
	CMD_SETTYPERATE,
	CMD_SETOUTPORT
};

static struct {
	uint8_t buffer[KEYBUFSIZE];
	Bitu used;
	Bitu pos;
	struct {
		KBD_KEYS key;
		Bitu wait;
		Bitu pause,rate;
	} repeat;
	KeyCommands command;
	uint8_t p60data;
	bool p60changed;
	bool active;
	bool scanning;
	bool scheduled;
} keyb;

static void KEYBOARD_SetPort60(uint8_t val) {
	keyb.p60changed=true;
	keyb.p60data=val;
	if (machine==MCH_PCJR) PIC_ActivateIRQ(6);
	else PIC_ActivateIRQ(1);
}

static void KEYBOARD_TransferBuffer(uint32_t /*val*/)
{
	keyb.scheduled = false;
	if (!keyb.used) {
		LOG(LOG_KEYBOARD,LOG_NORMAL)("Transfer started with empty buffer");
		return;
	}
	KEYBOARD_SetPort60(keyb.buffer[keyb.pos]);
	if (++keyb.pos >= KEYBUFSIZE) keyb.pos -= KEYBUFSIZE;
	keyb.used--;
}

void KEYBOARD_ClrBuffer(void) {
	keyb.used=0;
	keyb.pos=0;
	PIC_RemoveEvents(KEYBOARD_TransferBuffer);
	keyb.scheduled=false;
}

static void KEYBOARD_AddBuffer(uint8_t data) {
	if (keyb.used>=KEYBUFSIZE) {
		LOG(LOG_KEYBOARD,LOG_NORMAL)("Buffer full, dropping code");
		return;
	}
	Bitu start=keyb.pos+keyb.used;
	if (start>=KEYBUFSIZE) start-=KEYBUFSIZE;
	keyb.buffer[start]=data;
	keyb.used++;
	/* Start up an event to start the first IRQ */
	if (!keyb.scheduled && !keyb.p60changed) {
		keyb.scheduled=true;
		PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
	}
}

static uint8_t read_p60(io_port_t, io_width_t)
{
	keyb.p60changed = false;
	if (!keyb.scheduled && keyb.used) {
		keyb.scheduled = true;
		PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
	}
	return keyb.p60data;
}

static void write_p60(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	switch (keyb.command) {
	case CMD_NONE:	/* None */
		/* No active command this would normally get sent to the keyboard then */
		KEYBOARD_ClrBuffer();
		switch (val) {
		case 0xed:	/* Set Leds */
			keyb.command=CMD_SETLEDS;
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xee:	/* Echo */
			KEYBOARD_AddBuffer(0xee);	/* Echo */
			break;
		case 0xf2:	/* Identify keyboard */
			/* AT's just send acknowledge */
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xf3: /* Typematic rate programming */
			keyb.command=CMD_SETTYPERATE;
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xf4:	/* Enable keyboard,clear buffer, start scanning */
			LOG(LOG_KEYBOARD, LOG_NORMAL)("Clear buffer,enable Scanning");
			KEYBOARD_AddBuffer(0xfa); /* Acknowledge */
			keyb.scanning=true;
			break;
		case 0xf5:	 /* Reset keyboard and disable scanning */
			LOG(LOG_KEYBOARD,LOG_NORMAL)("Reset, disable scanning");
			keyb.scanning=false;
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			break;
		case 0xf6:	/* Reset keyboard and enable scanning */
			LOG(LOG_KEYBOARD,LOG_NORMAL)("Reset, enable scanning");
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
			keyb.scanning=false;
			break;
		default:
			/* Just always acknowledge strange commands */
			LOG(LOG_KEYBOARD, LOG_ERROR)("60:Unhandled command %x", val);
			KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
		}
		return;
	case CMD_SETOUTPORT:
		MEM_A20_Enable((val & 2)>0);
		keyb.command = CMD_NONE;
		break;
	case CMD_SETTYPERATE:
		{
			static const int delay[] = { 250, 500, 750, 1000 };
			static const int repeat[] =
				{ 33,37,42,46,50,54,58,63,67,75,83,92,100,
				  109,118,125,133,149,167,182,200,217,233,
				  250,270,303,333,370,400,435,476,500 };
			keyb.repeat.pause = delay[(val>>5)&3];
			keyb.repeat.rate = repeat[val&0x1f];
			keyb.command=CMD_NONE;
		}
		[[fallthrough]]; // CMD_SETLEDS does what we want
	case CMD_SETLEDS:
		keyb.command=CMD_NONE;
		KEYBOARD_ClrBuffer();
		KEYBOARD_AddBuffer(0xfa);	/* Acknowledge */
		break;
	}
}

/* Bochs: 8255 Programmable Peripheral Interface

0061	w	KB controller port B (ISA, EISA)   (PS/2 port A is at 0092)
system control port for compatibility with 8255
bit 7      (1= IRQ 0 reset )
bit 6-4    reserved
bit 3 = 1  channel check enable
bit 2 = 1  parity check enable
bit 1 = 1  speaker data enable
bit 0 = 1  timer 2 gate to speaker enable

0061	w	PPI  Programmable Peripheral Interface 8255 (XT only)
system control port
bit 7 = 1  clear keyboard
bit 6 = 0  hold keyboard clock low
bit 5 = 0  I/O check enable
bit 4 = 0  RAM parity check enable
bit 3 = 0  read low switches
bit 2      reserved, often used as turbo switch
bit 1 = 1  speaker data enable
bit 0 = 1  timer 2 gate to speaker enable
*/
static PpiPortB port_b = {0};
extern void TIMER_SetGate2(bool);
static void write_p61(io_port_t, io_val_t value, io_width_t)
{
	const PpiPortB new_port_b = {check_cast<uint8_t>(value)};

	// Determine how the state changed
	const auto output_changed = new_port_b.timer2_gating_and_speaker_out !=
	                            port_b.timer2_gating_and_speaker_out;
	const auto timer_changed = new_port_b.timer2_gating != port_b.timer2_gating;

	// Update the state
	port_b.data = new_port_b.data;

	if (machine < MCH_EGA && port_b.xt_clear_keyboard)
		KEYBOARD_ClrBuffer();

	if (!output_changed)
		return;

	if (timer_changed)
		TIMER_SetGate2(port_b.timer2_gating);

	PCSPEAKER_SetType(port_b);
}

/* Bochs: 8255 Programmable Peripheral Interface

0061	r	KB controller port B control register (ISA, EISA)
system control port for compatibility with 8255
bit 7    parity check occurred
bit 6    channel check occurred
bit 5    mirrors timer 2 output condition
bit 4    toggles with each refresh request
bit 3    channel check status
bit 2    parity check status
bit 1    speaker data status
bit 0    timer 2 gate to speaker status
*/
extern bool TIMER_GetOutput2(void);
static uint8_t read_p61(io_port_t, io_width_t)
{
	// Bit 4 must be toggled each request
	port_b.read_toggle.flip();

	// On PC/AT systems, bit 5 sets the timer 2 output status
	if (is_machine(MCH_EGA | MCH_VGA))
		port_b.timer2_gating_alias = TIMER_GetOutput2();
	else
		// On XT systems always toggle bit 5 (Spellicopter CGA)
		port_b.xt_read_toggle.flip();

	return port_b.data;
}

/* Bochs: 8255 Programmable Peripheral Interface
0062	r/w	PPI (XT only)
bit 7 = 1  RAM parity check
bit 6 = 1  I/O channel check
bit 5 = 1  timer 2 channel out
bit 4      reserved
bit 3 = 1  system board RAM size type 1
bit 2 = 1  system board RAM size type 2
bit 1 = 1  coprocessor installed
bit 0 = 1  loop in POST
*/
static uint8_t read_p62(io_port_t, io_width_t)
{
	auto ret = bit::all<uint8_t>();

	if(!TIMER_GetOutput2())
		bit::clear(ret, b5);

	return ret;
}

static void write_p64(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	switch (val) {
	case 0xae:		/* Activate keyboard */
		keyb.active=true;
		if (keyb.used && !keyb.scheduled && !keyb.p60changed) {
			keyb.scheduled=true;
			PIC_AddEvent(KEYBOARD_TransferBuffer,KEYDELAY);
		}
		LOG(LOG_KEYBOARD,LOG_NORMAL)("Activated");
		break;
	case 0xad:		/* Deactivate keyboard */
		keyb.active=false;
		LOG(LOG_KEYBOARD,LOG_NORMAL)("De-Activated");
		break;
	case 0xd0:		/* Outport on buffer */
		KEYBOARD_SetPort60(MEM_A20_Enabled() ? 0x02 : 0);
		break;
	case 0xd1:		/* Write to outport */
		keyb.command=CMD_SETOUTPORT;
		break;
	default:
		LOG(LOG_KEYBOARD, LOG_ERROR)("Port 64 write with val %x", val);
		break;
	}
}

static uint8_t read_p64(io_port_t, io_width_t)
{
	uint8_t status = 0x1c | (keyb.p60changed ? 0x1 : 0x0);
	return status;
}

void KEYBOARD_AddKey(KBD_KEYS keytype,bool pressed) {
	uint8_t ret=0;bool extend=false;
	switch (keytype) {
	case KBD_esc:ret=1;break;
	case KBD_1:ret=2;break;
	case KBD_2:ret=3;break;
	case KBD_3:ret=4;break;
	case KBD_4:ret=5;break;
	case KBD_5:ret=6;break;
	case KBD_6:ret=7;break;
	case KBD_7:ret=8;break;
	case KBD_8:ret=9;break;
	case KBD_9:ret=10;break;
	case KBD_0:ret=11;break;

	case KBD_minus:ret=12;break;
	case KBD_equals:ret=13;break;
	case KBD_backspace:ret=14;break;
	case KBD_tab:ret=15;break;

	case KBD_q:ret=16;break;
	case KBD_w:ret=17;break;
	case KBD_e:ret=18;break;
	case KBD_r:ret=19;break;
	case KBD_t:ret=20;break;
	case KBD_y:ret=21;break;
	case KBD_u:ret=22;break;
	case KBD_i:ret=23;break;
	case KBD_o:ret=24;break;
	case KBD_p:ret=25;break;

	case KBD_leftbracket:ret=26;break;
	case KBD_rightbracket:ret=27;break;
	case KBD_enter:ret=28;break;
	case KBD_leftctrl:ret=29;break;

	case KBD_a:ret=30;break;
	case KBD_s:ret=31;break;
	case KBD_d:ret=32;break;
	case KBD_f:ret=33;break;
	case KBD_g:ret=34;break;
	case KBD_h:ret=35;break;
	case KBD_j:ret=36;break;
	case KBD_k:ret=37;break;
	case KBD_l:ret=38;break;

	case KBD_semicolon:ret=39;break;
	case KBD_quote:ret=40;break;
	case KBD_grave:ret=41;break;
	case KBD_leftshift:ret=42;break;
	case KBD_backslash:ret=43;break;
	case KBD_z:ret=44;break;
	case KBD_x:ret=45;break;
	case KBD_c:ret=46;break;
	case KBD_v:ret=47;break;
	case KBD_b:ret=48;break;
	case KBD_n:ret=49;break;
	case KBD_m:ret=50;break;

	case KBD_comma:ret=51;break;
	case KBD_period:ret=52;break;
	case KBD_slash:ret=53;break;
	case KBD_rightshift:ret=54;break;
	case KBD_kpmultiply:ret=55;break;
	case KBD_leftalt:ret=56;break;
	case KBD_space:ret=57;break;
	case KBD_capslock:ret=58;break;

	case KBD_f1:ret=59;break;
	case KBD_f2:ret=60;break;
	case KBD_f3:ret=61;break;
	case KBD_f4:ret=62;break;
	case KBD_f5:ret=63;break;
	case KBD_f6:ret=64;break;
	case KBD_f7:ret=65;break;
	case KBD_f8:ret=66;break;
	case KBD_f9:ret=67;break;
	case KBD_f10:ret=68;break;

	case KBD_numlock:ret=69;break;
	case KBD_scrolllock:ret=70;break;

	case KBD_kp7:ret=71;break;
	case KBD_kp8:ret=72;break;
	case KBD_kp9:ret=73;break;
	case KBD_kpminus:ret=74;break;
	case KBD_kp4:ret=75;break;
	case KBD_kp5:ret=76;break;
	case KBD_kp6:ret=77;break;
	case KBD_kpplus:ret=78;break;
	case KBD_kp1:ret=79;break;
	case KBD_kp2:ret=80;break;
	case KBD_kp3:ret=81;break;
	case KBD_kp0:ret=82;break;
	case KBD_kpperiod:ret=83;break;

	case KBD_extra_lt_gt:ret=86;break;
	case KBD_f11:ret=87;break;
	case KBD_f12:ret=88;break;

	//The Extended keys

	case KBD_kpenter:extend=true;ret=28;break;
	case KBD_rightctrl:extend=true;ret=29;break;
	case KBD_kpdivide:extend=true;ret=53;break;
	case KBD_rightalt:extend=true;ret=56;break;
	case KBD_home:extend=true;ret=71;break;
	case KBD_up:extend=true;ret=72;break;
	case KBD_pageup:extend=true;ret=73;break;
	case KBD_left:extend=true;ret=75;break;
	case KBD_right:extend=true;ret=77;break;
	case KBD_end:extend=true;ret=79;break;
	case KBD_down:extend=true;ret=80;break;
	case KBD_pagedown:extend=true;ret=81;break;
	case KBD_insert:extend=true;ret=82;break;
	case KBD_delete:extend=true;ret=83;break;
	case KBD_leftgui: ret = 89; break;
	case KBD_rightgui:
		extend = true;
		ret = 89;
		break;
	case KBD_pause:
		KEYBOARD_AddBuffer(0xe1);
		KEYBOARD_AddBuffer(29|(pressed?0:0x80));
		KEYBOARD_AddBuffer(69|(pressed?0:0x80));
		return;
	case KBD_printscreen:
		KEYBOARD_AddBuffer(0xe0);
		KEYBOARD_AddBuffer(42|(pressed?0:0x80));
		KEYBOARD_AddBuffer(0xe0);
		KEYBOARD_AddBuffer(55|(pressed?0:0x80));
		return;
	default:
		E_Exit("Unsupported key press");
		break;
	}
	/* Add the actual key in the keyboard queue */
	if (pressed) {
		if (keyb.repeat.key == keytype) keyb.repeat.wait = keyb.repeat.rate;
		else keyb.repeat.wait = keyb.repeat.pause;
		keyb.repeat.key = keytype;
	} else {
		if (keyb.repeat.key == keytype) {
			/* repeated key being released */
			keyb.repeat.key  = KBD_NONE;
			keyb.repeat.wait = 0;
		}
		ret += 128;
	}
	if (extend) KEYBOARD_AddBuffer(0xe0);
	KEYBOARD_AddBuffer(ret);
}

static void KEYBOARD_TickHandler(void) {
	if (keyb.repeat.wait) {
		keyb.repeat.wait--;
		if (!keyb.repeat.wait) KEYBOARD_AddKey(keyb.repeat.key,true);
	}
}

void KEYBOARD_Init(Section* /*sec*/) {
	IO_RegisterWriteHandler(0x60, write_p60, io_width_t::byte);
	IO_RegisterReadHandler(0x60, read_p60, io_width_t::byte);
	IO_RegisterWriteHandler(0x61, write_p61, io_width_t::byte);
	IO_RegisterReadHandler(0x61, read_p61, io_width_t::byte);
	if (machine == MCH_CGA || machine == MCH_HERC)
		IO_RegisterReadHandler(0x62, read_p62, io_width_t::byte);
	IO_RegisterWriteHandler(0x64, write_p64, io_width_t::byte);
	IO_RegisterReadHandler(0x64, read_p64, io_width_t::byte);
	TIMER_AddTickHandler(&KEYBOARD_TickHandler);
	write_p61(0, 0, io_width_t::byte);
	/* Init the keyb struct */
	keyb.active = true;
	keyb.scanning = true;
	keyb.command = CMD_NONE;
	keyb.p60changed = false;
	keyb.repeat.key = KBD_NONE;
	keyb.repeat.pause = 500;
	keyb.repeat.rate = 33;
	keyb.repeat.wait = 0;
	KEYBOARD_ClrBuffer();
}
