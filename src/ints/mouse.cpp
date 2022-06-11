/*
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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

#include "mouse.h"

#include <algorithm>
#include <string.h>
#include <math.h>

#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "cpu.h"
#include "pic.h"
#include "inout.h"
#include "int10.h"
#include "bios.h"
#include "dos_inc.h"

enum DOS_EV:uint8_t { // compatible with DOS driver mask in driver function 0x0c
    NOT_DOS_EVENT   = 0x00,
    MOUSE_HAS_MOVED = 0x01,
    PRESSED_LEFT    = 0x02,
    RELEASED_LEFT   = 0x04,
    PRESSED_RIGHT   = 0x08,
    RELEASED_RIGHT  = 0x10,
    PRESSED_MIDDLE  = 0x20,
    RELEASED_MIDDLE = 0x40,
    WHEEL_HAS_MOVED = 0x80,
};

static const uint8_t QUEUE_SIZE  = 32;   // if over 255, increase 'queue_used' type size
static const uint8_t KEY_MASKS[] = { 0x01, 0x02, 0x04, 0x08, 0x10 };

static uint8_t buttons_12  = 0; // state of buttons 1 (left), 2 (right), as visible on host side
static uint8_t buttons_345 = 0; // state of mouse buttons 3 (middle), 4, and 5 as visible on host side

typedef struct MouseEvent {
    uint8_t dos_type    = 0;
    uint8_t dos_buttons = 0; 

    MouseEvent() {}
    MouseEvent(uint8_t dos_type) : dos_type(dos_type) {}

} MouseEvent;

static MouseEvent queue[QUEUE_SIZE];
static uint8_t    queue_used = 0;
static bool       timer_in_progress = false;

MouseInfoConfig   mouse_config;
MouseInfoVideo    mouse_video;

static Bitu call_int33,call_int74,int74_ret_callback,call_mouse_bd;
static uint16_t ps2cbseg,ps2cbofs;
static bool useps2callback,ps2callbackinit;
static Bitu call_ps2,call_uir;
static RealPt ps2_callback,uir_callback;
static int16_t oldmouseX, oldmouseY;

#define MOUSE_BUTTONS 3
#define MOUSE_IRQ 12
#define POS_X (static_cast<int16_t>(mouse.x) & mouse.gran_x)
#define POS_Y (static_cast<int16_t>(mouse.y) & mouse.gran_y)

#define CURSORX 16
#define CURSORY 16
#define HIGHESTBIT (1<<(CURSORX-1))

static uint16_t defaultTextAndMask = 0x77FF;
static uint16_t defaultTextXorMask = 0x7700;

static uint16_t defaultScreenMask[CURSORY] = {
		0x3FFF, 0x1FFF, 0x0FFF, 0x07FF,
		0x03FF, 0x01FF, 0x00FF, 0x007F,
		0x003F, 0x001F, 0x01FF, 0x00FF,
		0x30FF, 0xF87F, 0xF87F, 0xFCFF
};

static uint16_t defaultCursorMask[CURSORY] = {
		0x0000, 0x4000, 0x6000, 0x7000,
		0x7800, 0x7C00, 0x7E00, 0x7F00,
		0x7F80, 0x7C00, 0x6C00, 0x4600,
		0x0600, 0x0300, 0x0300, 0x0000
};

static uint16_t userdefScreenMask[CURSORY];
static uint16_t userdefCursorMask[CURSORY];

static struct {

    // TODO - DANGER, WILL ROBINSON!
    // This whole structure can be read or written from the guest side via virtual DOS driver,
    // functions 0x15 / 0x16 / 0x17; we need to make sure nothing can be broken by malicious code!

	uint16_t times_pressed[MOUSE_BUTTONS];
	uint16_t times_released[MOUSE_BUTTONS];
	uint16_t last_released_x[MOUSE_BUTTONS];
	uint16_t last_released_y[MOUSE_BUTTONS];
	uint16_t last_pressed_x[MOUSE_BUTTONS];
	uint16_t last_pressed_y[MOUSE_BUTTONS];
    uint16_t last_wheel_moved_x;
    uint16_t last_wheel_moved_y;

    uint8_t  buttons;
	float    x,y;
    int16_t  wheel;

	uint16_t hidden;
	float add_x,add_y;
	int16_t min_x,max_x,min_y,max_y;
	float mickey_x,mickey_y;
	uint16_t sub_seg,sub_ofs;
	uint16_t sub_mask;

	bool	background;
	int16_t	backposx, backposy;
	uint8_t	backData[CURSORX*CURSORY];
	uint16_t*	screenMask;
	uint16_t* cursorMask;
	int16_t	clipx,clipy;
	int16_t  hotx,hoty;
	uint16_t  textAndMask, textXorMask;

	float	mickeysPerPixel_x;
	float	mickeysPerPixel_y;
	float	pixelPerMickey_x;
	float	pixelPerMickey_y;
	uint16_t	senv_x_val;
	uint16_t	senv_y_val;
	uint16_t	dspeed_val;
	float	senv_x;
	float	senv_y;
	int16_t  updateRegion_x[2];
	int16_t  updateRegion_y[2];
	uint16_t  doubleSpeedThreshold;
	uint16_t  language;
	uint16_t  cursorType;
	uint16_t	oldhidden;
	uint8_t  page;
	bool enabled;
	bool cute_mouse;
	bool inhibit_draw;
	bool in_UIR;
	uint8_t mode;
	int16_t gran_x,gran_y;
} mouse;

bool Mouse_SetPS2State(bool use) {
	if (use && (!ps2callbackinit)) {
		useps2callback = false;
		PIC_SetIRQMask(MOUSE_IRQ,true);
		return false;
	}
	useps2callback = use;
	PIC_SetIRQMask(MOUSE_IRQ,!useps2callback);
	return true;
}

void Mouse_ChangePS2Callback(uint16_t pseg, uint16_t pofs) {
	if ((pseg==0) && (pofs==0)) {
		ps2callbackinit = false;
	} else {
		ps2callbackinit = true;
		ps2cbseg = pseg;
		ps2cbofs = pofs;
	}
}

void DoPS2Callback(uint16_t data, int16_t mouseX, int16_t mouseY) {
	if (useps2callback) {
		uint16_t mdat = (data & 0x03) | 0x08;
		int16_t xdiff = mouseX-oldmouseX;
		int16_t ydiff = oldmouseY-mouseY;
		oldmouseX = mouseX;
		oldmouseY = mouseY;
		if ((xdiff>0xff) || (xdiff<-0xff)) mdat |= 0x40;		// x overflow
		if ((ydiff>0xff) || (ydiff<-0xff)) mdat |= 0x80;		// y overflow
		xdiff %= 256;
		ydiff %= 256;
		if (xdiff<0) {
			xdiff = (0x100+xdiff);
			mdat |= 0x10;
		}
		if (ydiff<0) {
			ydiff = (0x100+ydiff);
			mdat |= 0x20;
		}
		CPU_Push16((uint16_t)mdat); 
		CPU_Push16((uint16_t)(xdiff % 256)); 
		CPU_Push16((uint16_t)(ydiff % 256)); 
		CPU_Push16((uint16_t)0); 
		CPU_Push16(RealSeg(ps2_callback));
		CPU_Push16(RealOff(ps2_callback));
		SegSet16(cs, ps2cbseg);
		reg_ip = ps2cbofs;
	}
}

Bitu PS2_Handler(void) {
	CPU_Pop16();CPU_Pop16();CPU_Pop16();CPU_Pop16();// remove the 4 words
	return CBRET_NONE;
}

#define X_MICKEY 8
#define Y_MICKEY 8

#define MOUSE_DELAY 5.0

void MOUSE_Limit_Events(uint32_t /*val*/)
{
	timer_in_progress = false;
	if (queue_used) {
		timer_in_progress = true;
		PIC_AddEvent(MOUSE_Limit_Events,MOUSE_DELAY);
		PIC_ActivateIRQ(MOUSE_IRQ);
	}
}

inline void AddEvent(uint8_t type) {
	if (queue_used < QUEUE_SIZE) {
		if (queue_used > 0) {
			/* Skip duplicate events */
			if (type==DOS_EV::MOUSE_HAS_MOVED || type==DOS_EV::WHEEL_HAS_MOVED) return;
			/* Always put the newest element in the front as that the events are 
			 * handled backwards (prevents doubleclicks while moving)
			 */
            for (auto i = queue_used ; i ; i--)
                queue[i] = queue[i - 1];
		}
		queue[0].dos_type    = type;
		queue[0].dos_buttons = mouse.buttons;
		queue_used++;
	}
	if (!timer_in_progress) {
		timer_in_progress = true;
		PIC_AddEvent(MOUSE_Limit_Events,MOUSE_DELAY);
		PIC_ActivateIRQ(MOUSE_IRQ);
	}
}

static inline DOS_EV SelectEventPressed(uint8_t idx, bool changed_12S) {
    switch (idx) {
    case 0:  return DOS_EV::PRESSED_LEFT;
    case 1:  return DOS_EV::PRESSED_RIGHT;
    case 2:  return DOS_EV::PRESSED_MIDDLE;
    case 3:
    case 4:  return changed_12S ? DOS_EV::PRESSED_MIDDLE : DOS_EV::NOT_DOS_EVENT;
    default: return DOS_EV::NOT_DOS_EVENT;
    }
}

static inline DOS_EV SelectEventReleased(uint8_t idx, bool changed_12S) {
    switch (idx) {
    case 0:  return DOS_EV::RELEASED_LEFT;
    case 1:  return DOS_EV::RELEASED_RIGHT;
    case 2:  return DOS_EV::RELEASED_MIDDLE;
    case 3:
    case 4:  return changed_12S ? DOS_EV::RELEASED_MIDDLE : DOS_EV::NOT_DOS_EVENT;
    default: return DOS_EV::NOT_DOS_EVENT;
    }
}

// ***************************************************************************
// Mouse cursor - text mode
// ***************************************************************************
/* Write and read directly to the screen. Do no use int_setcursorpos (LOTUS123) */
extern void WriteChar(uint16_t col,uint16_t row,uint8_t page,uint8_t chr,uint8_t attr,bool useattr);
extern void ReadCharAttr(uint16_t col,uint16_t row,uint8_t page,uint16_t * result);

void RestoreCursorBackgroundText() {
	if (mouse.hidden || mouse.inhibit_draw) return;

	if (mouse.background) {
		WriteChar(mouse.backposx,mouse.backposy,real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE),mouse.backData[0],mouse.backData[1],true);
		mouse.background = false;
	}
}

void DrawCursorText() {	
	// Restore Background
	RestoreCursorBackgroundText();

	// Check if cursor in update region
	if ((POS_Y <= mouse.updateRegion_y[1]) && (POS_Y >= mouse.updateRegion_y[0]) &&
		(POS_X <= mouse.updateRegion_x[1]) && (POS_X >= mouse.updateRegion_x[0])) {
		return;
	}

	// Save Background
	mouse.backposx		= POS_X>>3;
	mouse.backposy		= POS_Y>>3;
	if (mouse.mode < 2) mouse.backposx >>= 1; 

	//use current page (CV program)
	uint8_t page = real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
	
	if (mouse.cursorType == 0) {
		uint16_t result;
		ReadCharAttr(mouse.backposx,mouse.backposy,page,&result);
		mouse.backData[0]	= (uint8_t)(result & 0xFF);
		mouse.backData[1]	= (uint8_t)(result>>8);
		mouse.background	= true;
		// Write Cursor
		result = (result & mouse.textAndMask) ^ mouse.textXorMask;
		WriteChar(mouse.backposx,mouse.backposy,page,(uint8_t)(result&0xFF),(uint8_t)(result>>8),true);
	} else {
		uint16_t address=page * real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE);
		address += (mouse.backposy * real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS) + mouse.backposx) * 2;
		address /= 2;
		uint16_t cr = real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
		IO_Write(cr    , 0xe);
		IO_Write(cr + 1, (address>>8) & 0xff);
		IO_Write(cr    , 0xf);
		IO_Write(cr + 1, address & 0xff);
	}
}

// ***************************************************************************
// Mouse cursor - graphic mode
// ***************************************************************************

static uint8_t gfxReg3CE[9];
static uint8_t index3C4,gfxReg3C5;
void SaveVgaRegisters() {
	if (IS_VGA_ARCH) {
		for (uint8_t i=0; i<9; i++) {
			IO_Write	(0x3CE,i);
			gfxReg3CE[i] = IO_Read(0x3CF);
		}
		/* Setup some default values in GFX regs that should work */
		IO_Write(0x3CE,3); IO_Write(0x3Cf,0);				//disable rotate and operation
		IO_Write(0x3CE,5); IO_Write(0x3Cf,gfxReg3CE[5]&0xf0);	//Force read/write mode 0

		//Set Map to all planes. Celtic Tales
		index3C4 = IO_Read(0x3c4);  IO_Write(0x3C4,2);
		gfxReg3C5 = IO_Read(0x3C5); IO_Write(0x3C5,0xF);
	} else if (machine==MCH_EGA) {
		//Set Map to all planes.
		IO_Write(0x3C4,2);
		IO_Write(0x3C5,0xF);
	}
}

void RestoreVgaRegisters() {
	if (IS_VGA_ARCH) {
		for (uint8_t i=0; i<9; i++) {
			IO_Write(0x3CE,i);
			IO_Write(0x3CF,gfxReg3CE[i]);
		}

		IO_Write(0x3C4,2);
		IO_Write(0x3C5,gfxReg3C5);
		IO_Write(0x3C4,index3C4);
	}
}

void ClipCursorArea(int16_t& x1, int16_t& x2, int16_t& y1, int16_t& y2,
					uint16_t& addx1, uint16_t& addx2, uint16_t& addy) {
	addx1 = addx2 = addy = 0;
	// Clip up
	if (y1<0) {
		addy += (-y1);
		y1 = 0;
	}
	// Clip down
	if (y2>mouse.clipy) {
		y2 = mouse.clipy;		
	};
	// Clip left
	if (x1<0) {
		addx1 += (-x1);
		x1 = 0;
	};
	// Clip right
	if (x2>mouse.clipx) {
		addx2 = x2 - mouse.clipx;
		x2 = mouse.clipx;
	};
}

void RestoreCursorBackground() {
	if (mouse.hidden || mouse.inhibit_draw) return;

	SaveVgaRegisters();
	if (mouse.background) {
		// Restore background
		int16_t x,y;
		uint16_t addx1,addx2,addy;
		uint16_t dataPos	= 0;
		int16_t x1		= mouse.backposx;
		int16_t y1		= mouse.backposy;
		int16_t x2		= x1 + CURSORX - 1;
		int16_t y2		= y1 + CURSORY - 1;	

		ClipCursorArea(x1, x2, y1, y2, addx1, addx2, addy);

		dataPos = addy * CURSORX;
		for (y=y1; y<=y2; y++) {
			dataPos += addx1;
			for (x=x1; x<=x2; x++) {
				INT10_PutPixel(x,y,mouse.page,mouse.backData[dataPos++]);
			};
			dataPos += addx2;
		};
		mouse.background = false;
	};
	RestoreVgaRegisters();
}

void DrawCursor() {
	if (mouse.hidden || mouse.inhibit_draw) return;
	INT10_SetCurMode();
	// In Textmode ?
	if (CurMode->type==M_TEXT) {
		DrawCursorText();
		return;
	}

	// Check video page. Seems to be ignored for text mode. 
	// hence the text mode handled above this
	// >>> removed because BIOS page is not actual page in some cases, e.g. QQP games
//	if (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE)!=mouse.page) return;

// Check if cursor in update region
/*	if ((POS_X >= mouse.updateRegion_x[0]) && (POS_X <= mouse.updateRegion_x[1]) &&
	    (POS_Y >= mouse.updateRegion_y[0]) && (POS_Y <= mouse.updateRegion_y[1])) {
		if (CurMode->type==M_TEXT16)
			RestoreCursorBackgroundText();
		else
			RestoreCursorBackground();
		mouse.shown--;
		return;
	}
   */ /*Not sure yet what to do update region should be set to ??? */
		 
	// Get Clipping ranges


	mouse.clipx = (int16_t)((Bits)CurMode->swidth-1);	/* Get from bios ? */
	mouse.clipy = (int16_t)((Bits)CurMode->sheight-1);

	/* might be vidmode == 0x13?2:1 */
	int16_t xratio = 640;
	if (CurMode->swidth>0) xratio/=CurMode->swidth;
	if (xratio==0) xratio = 1;
	
	RestoreCursorBackground();

	SaveVgaRegisters();

	// Save Background
	int16_t x,y;
	uint16_t addx1,addx2,addy;
	uint16_t dataPos	= 0;
	int16_t x1		= POS_X / xratio - mouse.hotx;
	int16_t y1		= POS_Y - mouse.hoty;
	int16_t x2		= x1 + CURSORX - 1;
	int16_t y2		= y1 + CURSORY - 1;	

	ClipCursorArea(x1,x2,y1,y2, addx1, addx2, addy);

	dataPos = addy * CURSORX;
	for (y=y1; y<=y2; y++) {
		dataPos += addx1;
		for (x=x1; x<=x2; x++) {
			INT10_GetPixel(x,y,mouse.page,&mouse.backData[dataPos++]);
		};
		dataPos += addx2;
	};
	mouse.background= true;
	mouse.backposx	= POS_X / xratio - mouse.hotx;
	mouse.backposy	= POS_Y - mouse.hoty;

	// Draw Mousecursor
	dataPos = addy * CURSORX;
	for (y=y1; y<=y2; y++) {
		uint16_t scMask = mouse.screenMask[addy+y-y1];
		uint16_t cuMask = mouse.cursorMask[addy+y-y1];
		if (addx1>0) { scMask<<=addx1; cuMask<<=addx1; dataPos += addx1; };
		for (x=x1; x<=x2; x++) {
			uint8_t pixel = 0;
			// ScreenMask
			if (scMask & HIGHESTBIT) pixel = mouse.backData[dataPos];
			scMask<<=1;
			// CursorMask
			if (cuMask & HIGHESTBIT) pixel = pixel ^ 0x0F;
			cuMask<<=1;
			// Set Pixel
			INT10_PutPixel(x,y,mouse.page,pixel);
			dataPos++;
		};
		dataPos += addx2;
	};
	RestoreVgaRegisters();
}

inline void Mouse_CursorMoved(float xrel,float yrel,float x,float y,bool emulate) {
	float dx = xrel * mouse.pixelPerMickey_x;
	float dy = yrel * mouse.pixelPerMickey_y;

	if((fabs(xrel) > 1.0) || (mouse.senv_x < 1.0)) dx *= mouse.senv_x;
	if((fabs(yrel) > 1.0) || (mouse.senv_y < 1.0)) dy *= mouse.senv_y;
	if (useps2callback) dy *= 2;	

	mouse.mickey_x += (dx * mouse.mickeysPerPixel_x);
	mouse.mickey_y += (dy * mouse.mickeysPerPixel_y);
	if (mouse.mickey_x >= 32768.0) mouse.mickey_x -= 65536.0;
	else if (mouse.mickey_x <= -32769.0) mouse.mickey_x += 65536.0;
	if (mouse.mickey_y >= 32768.0) mouse.mickey_y -= 65536.0;
	else if (mouse.mickey_y <= -32769.0) mouse.mickey_y += 65536.0;
	if (emulate) {
		mouse.x += dx;
		mouse.y += dy;
	} else {
		if (CurMode->type == M_TEXT) {
			mouse.x = x*real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8;
			mouse.y = y*(IS_EGAVGA_ARCH?(real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1):25)*8;
		} else if ((mouse.max_x < 2048) || (mouse.max_y < 2048) || (mouse.max_x != mouse.max_y)) {
			if ((mouse.max_x > 0) && (mouse.max_y > 0)) {
				mouse.x = x*mouse.max_x;
				mouse.y = y*mouse.max_y;
			} else {
				mouse.x += xrel;
				mouse.y += yrel;
			}
		} else { // Games faking relative movement through absolute coordinates. Quite surprising that this actually works..
			mouse.x += xrel;
			mouse.y += yrel;
		}
	}

	/* ignore constraints if using PS2 mouse callback in the bios */

	if (!useps2callback) {		
		if (mouse.x > mouse.max_x) mouse.x = mouse.max_x;
		if (mouse.x < mouse.min_x) mouse.x = mouse.min_x;
		if (mouse.y > mouse.max_y) mouse.y = mouse.max_y;
		if (mouse.y < mouse.min_y) mouse.y = mouse.min_y;
	} else {
		if (mouse.x >= 32768.0) mouse.x -= 65536.0;
		else if (mouse.x <= -32769.0) mouse.x += 65536.0;
		if (mouse.y >= 32768.0) mouse.y -= 65536.0;
		else if (mouse.y <= -32769.0) mouse.y += 65536.0;
	}
	AddEvent(DOS_EV::MOUSE_HAS_MOVED);
	DrawCursor();
}

static inline uint8_t GetResetWheel8bit() {
    if (!mouse.cute_mouse) return 0;
    int8_t tmp = static_cast<int8_t>(std::clamp(mouse.wheel, static_cast<int16_t>(-0x80), static_cast<int16_t>(0x7f)));
    mouse.wheel = 0;
    return (tmp >= 0) ? tmp : 0x100 + tmp;
}

static inline uint16_t GetResetWheel16bit() {
    if (!mouse.cute_mouse) return 0;
    int16_t tmp = (mouse.wheel >= 0) ? mouse.wheel : 0x10000 + mouse.wheel;
    mouse.wheel = 0;
    return tmp;
}

static void Mouse_SetMickeyPixelRate(int16_t px, int16_t py){
	if ((px!=0) && (py!=0)) {
		mouse.mickeysPerPixel_x	 = (float)px/X_MICKEY;
		mouse.mickeysPerPixel_y  = (float)py/Y_MICKEY;
		mouse.pixelPerMickey_x	 = X_MICKEY/(float)px;
		mouse.pixelPerMickey_y 	 = Y_MICKEY/(float)py;	
	}
}

static void Mouse_SetSensitivity(uint16_t px, uint16_t py, uint16_t dspeed){
	if(px>100) px=100;
	if(py>100) py=100;
	if(dspeed>100) dspeed=100;
	// save values
	mouse.senv_x_val=px;
	mouse.senv_y_val=py;
	mouse.dspeed_val=dspeed;
	if ((px!=0) && (py!=0)) {
		px--;  //Inspired by cutemouse 
		py--;  //Although their cursor update routine is far more complex then ours
		mouse.senv_x=(static_cast<float>(px)*px)/3600.0f +1.0f/3.0f;
		mouse.senv_y=(static_cast<float>(py)*py)/3600.0f +1.0f/3.0f;
     }
}


static void Mouse_ResetHardware(void){
	PIC_SetIRQMask(MOUSE_IRQ,false);
}

void Mouse_BeforeNewVideoMode()
{
	if (CurMode->type!=M_TEXT) RestoreCursorBackground();
	else RestoreCursorBackgroundText();
	mouse.hidden = 1;
	mouse.oldhidden = 1;
	mouse.background = false;
}

//Does way to much. Many things should be moved to mouse reset one day
void Mouse_AfterNewVideoMode(bool setmode) {
	mouse.inhibit_draw = false;
	/* Get the correct resolution from the current video mode */
	uint8_t mode = mem_readb(BIOS_VIDEO_MODE);
	if (setmode && mode == mouse.mode) LOG(LOG_MOUSE,LOG_NORMAL)("New video mode is the same as the old");
	mouse.gran_x = (int16_t)0xffff;
	mouse.gran_y = (int16_t)0xffff;
	switch (mode) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x07: {
		mouse.gran_x = (mode<2)?0xfff0:0xfff8;
		mouse.gran_y = (int16_t)0xfff8;
		Bitu rows = IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS):24;
		if ((rows == 0) || (rows > 250)) rows = 25 - 1;
		mouse.max_y = 8*(rows+1) - 1;
		break;
	}
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0d:
	case 0x0e:
	case 0x13:
		if (mode == 0x0d || mode == 0x13) mouse.gran_x = (int16_t)0xfffe;
		mouse.max_y = 199;
		break;
	case 0x0f:
	case 0x10:
		mouse.max_y = 349;
		break;
	case 0x11:
	case 0x12:
		mouse.max_y = 479;
		break;
	default:
		LOG(LOG_MOUSE,LOG_ERROR)("Unhandled videomode %X on reset",mode);
		mouse.inhibit_draw = true;
		return;
	}
	mouse.mode = mode;
	mouse.max_x = 639;
	mouse.min_x = 0;
	mouse.min_y = 0;

	queue_used = 0;
	timer_in_progress = false;
	PIC_RemoveEvents(MOUSE_Limit_Events);

	mouse.hotx		 = 0;
	mouse.hoty		 = 0;
	mouse.screenMask = defaultScreenMask;
	mouse.cursorMask = defaultCursorMask;
	mouse.textAndMask= defaultTextAndMask;
	mouse.textXorMask= defaultTextXorMask;
	mouse.language   = 0;
	mouse.page               = 0;
	mouse.doubleSpeedThreshold = 64;
	mouse.updateRegion_y[1] = -1; //offscreen
	mouse.cursorType = 0; //Test
	mouse.enabled=true;

	oldmouseX = static_cast<int16_t>(mouse.x);
	oldmouseY = static_cast<int16_t>(mouse.y);


}

//Much too empty, Mouse_NewVideoMode contains stuff that should be in here
static void Mouse_Reset()
{
	Mouse_BeforeNewVideoMode();
	Mouse_AfterNewVideoMode(false);
	Mouse_SetMickeyPixelRate(8,16);

	mouse.mickey_x   = 0;
	mouse.mickey_y   = 0;
	mouse.buttons    = 0;
    mouse.wheel      = 0;
    mouse.cute_mouse = false;

    mouse.last_wheel_moved_x = 0;
    mouse.last_wheel_moved_y = 0;

	for (uint8_t but=0; but<MOUSE_BUTTONS; but++) {
		mouse.times_pressed[but] = 0;
		mouse.times_released[but] = 0;
		mouse.last_pressed_x[but] = 0;
		mouse.last_pressed_y[but] = 0;
		mouse.last_released_x[but] = 0;
		mouse.last_released_y[but] = 0;
	}

	// Dont set max coordinates here. it is done by SetResolution!
	mouse.x = static_cast<float>((mouse.max_x + 1)/ 2);
	mouse.y = static_cast<float>((mouse.max_y + 1)/ 2);
	mouse.sub_mask = 0;
	mouse.in_UIR = false;
}

static Bitu INT33_Handler(void) {
//	LOG(LOG_MOUSE,LOG_NORMAL)("MOUSE: %04X %X %X %d %d",reg_ax,reg_bx,reg_cx,POS_X,POS_Y);
	switch (reg_ax) {
	case 0x00: // MS MOUSE - reset driver and read status
		Mouse_ResetHardware();
		[[fallthrough]];
	case 0x21: // MS MOUSE v6.0+ - software reset
		reg_ax=0xffff;
		reg_bx=MOUSE_BUTTONS;
		Mouse_Reset();
		break;
	case 0x01: // MS MOUSE v1.0+ - show mouse cursor
		if(mouse.hidden) mouse.hidden--;
		mouse.updateRegion_y[1] = -1; //offscreen
		DrawCursor();
		break;
	case 0x02: // MS MOUSE v1.0+ - hide mouse cursor
		{
			if (CurMode->type!=M_TEXT) RestoreCursorBackground();
			else RestoreCursorBackgroundText();
			mouse.hidden++;
		}
		break;
	case 0x03: // MS MOUSE v1.0+ / CuteMouse - return position and button status
		reg_bl = mouse.buttons;
		reg_bh = GetResetWheel8bit(); // original CuteMouse clears mouse wheel status here
		reg_cx = POS_X;
		reg_dx = POS_Y;
		break;
	case 0x04: // MS MOUSE v1.0+ - position mouse cursor
        // If position isn't different from current position, don't change it.
        // (position is rounded so numbers get lost when the rounded number is set)
        // (arena/simulation Wolf)
		if ((int16_t)reg_cx >= mouse.max_x) mouse.x = static_cast<float>(mouse.max_x);
		else if (mouse.min_x >= (int16_t)reg_cx) mouse.x = static_cast<float>(mouse.min_x); 
		else if ((int16_t)reg_cx != POS_X) mouse.x = static_cast<float>(reg_cx);

		if ((int16_t)reg_dx >= mouse.max_y) mouse.y = static_cast<float>(mouse.max_y);
		else if (mouse.min_y >= (int16_t)reg_dx) mouse.y = static_cast<float>(mouse.min_y); 
		else if ((int16_t)reg_dx != POS_Y) mouse.y = static_cast<float>(reg_dx);
		DrawCursor();
		break;
	case 0x05: // MS MOUSE v1.0+ / CuteMouse - return button press data / mouse wheel data
		{
            uint16_t but = reg_bx;
            if (but == 0xffff && mouse.cute_mouse) {
                reg_bx = GetResetWheel16bit();
                reg_cx = mouse.last_wheel_moved_x;
                reg_dx = mouse.last_wheel_moved_y;
            } else {
                reg_ax = mouse.buttons;
                if (but >= MOUSE_BUTTONS) but = MOUSE_BUTTONS - 1;
                reg_cx = mouse.last_pressed_x[but];
                reg_dx = mouse.last_pressed_y[but];
                reg_bx = mouse.times_pressed[but];
                mouse.times_pressed[but] = 0;
            }
		}
		break;
	case 0x06: // MS MOUSE v1.0+ / CuteMouse - return button release data / mouse wheel data
		{
            uint16_t but = reg_bx;
            if (but == 0xffff && mouse.cute_mouse) {
                reg_bx = GetResetWheel16bit();
                reg_cx = mouse.last_wheel_moved_x;
                reg_dx = mouse.last_wheel_moved_y;
            } else {
			    reg_ax = mouse.buttons;
			    if (but >= MOUSE_BUTTONS) but = MOUSE_BUTTONS - 1;
			    reg_cx = mouse.last_released_x[but];
			    reg_dx = mouse.last_released_y[but];
			    reg_bx = mouse.times_released[but];
			    mouse.times_released[but] = 0;
            }
		}
		break;
	case 0x07: // MS MOUSE v1.0+ - define horizontal cursor range
        {   // Lemmings set 1-640 and wants that. iron seeds set 0-640 but doesn't like 640
            // Iron seed works if newvideo mode with mode 13 sets 0-639
            // Larry 6 actually wants newvideo mode with mode 13 to set it to 0-319
			int16_t max,min;
			if ((int16_t)reg_cx<(int16_t)reg_dx) { min=(int16_t)reg_cx;max=(int16_t)reg_dx;}
			else { min=(int16_t)reg_dx;max=(int16_t)reg_cx;}
			mouse.min_x=min;
			mouse.max_x=max;
            // Battlechess wants this
			if(mouse.x > mouse.max_x) mouse.x = mouse.max_x;
			if(mouse.x < mouse.min_x) mouse.x = mouse.min_x;
			// Or alternatively this: 
			// mouse.x = (mouse.max_x - mouse.min_x + 1)/2;
			LOG(LOG_MOUSE,LOG_NORMAL)("Define Hortizontal range min:%d max:%d",min,max);
		}
		break;
	case 0x08: // MS MOUSE v1.0+ - define vertical cursor range
        {   // not sure what to take instead of the CurMode (see case 0x07 as well)
            // especially the cases where sheight= 400 and we set it with the mouse_reset to 200
            // disabled it at the moment. Seems to break syndicate who want 400 in mode 13
			int16_t max,min;
			if ((int16_t)reg_cx<(int16_t)reg_dx) { min=(int16_t)reg_cx;max=(int16_t)reg_dx;}
			else { min=(int16_t)reg_dx;max=(int16_t)reg_cx;}
			mouse.min_y=min;
			mouse.max_y=max;
            // Battlechess wants this
			if(mouse.y > mouse.max_y) mouse.y = mouse.max_y;
			if(mouse.y < mouse.min_y) mouse.y = mouse.min_y;
			// Or alternatively this: 
			// mouse.y = (mouse.max_y - mouse.min_y + 1)/2;
			LOG(LOG_MOUSE,LOG_NORMAL)("Define Vertical range min:%d max:%d",min,max);
		}
		break;
	case 0x09: // MS MOUSE v3.0+ - define GFX cursor
		{
			PhysPt src = SegPhys(es)+reg_dx;
			MEM_BlockRead(src          ,userdefScreenMask,CURSORY*2);
			MEM_BlockRead(src+CURSORY*2,userdefCursorMask,CURSORY*2);
			mouse.screenMask = userdefScreenMask;
			mouse.cursorMask = userdefCursorMask;
			mouse.hotx		 = reg_bx;
			mouse.hoty		 = reg_cx;
			mouse.cursorType = 2;
			DrawCursor();
		}
		break;
	case 0x0a: // MS MOUSE v3.0+ - define text cursor
		mouse.cursorType = (reg_bx?1:0);
		mouse.textAndMask = reg_cx;
		mouse.textXorMask = reg_dx;
		if (reg_bx) {
			INT10_SetCursorShape(reg_cl,reg_dl);
			LOG(LOG_MOUSE,LOG_NORMAL)("Hardware Text cursor selected");
		}
		DrawCursor();
		break;
	case 0x27: // MS MOUSE v7.01+ - get screen/cursor masks and mickey counts
		reg_ax=mouse.textAndMask;
		reg_bx=mouse.textXorMask;
		[[fallthrough]];
	case 0x0b: // MS MOUSE v1.0+ - read motion data
		reg_cx=static_cast<int16_t>(mouse.mickey_x);
		reg_dx=static_cast<int16_t>(mouse.mickey_y);
		mouse.mickey_x=0;
		mouse.mickey_y=0;
		break;
	case 0x0c: // MS MOUSE v1.0+ - define interrupt subroutine parameters
		mouse.sub_mask=reg_cx;
		mouse.sub_seg=SegValue(es);
		mouse.sub_ofs=reg_dx;
		break;
    case 0x0d: // MS MOUSE v1.0+ - light pen emulation on
    case 0x0e: // MS MOUSE v1.0+ - light pen emulation off
        LOG(LOG_MOUSE,LOG_ERROR)("Mouse light pen emulation not implemented");
        break;
	case 0x0f: // MS MOUSE v1.0+ - define mickey/pixel rate
		Mouse_SetMickeyPixelRate(reg_cx,reg_dx);
		break;
	case 0x10: // MS MOUSE v1.0+ - define screen region for updating
		mouse.updateRegion_x[0]=(int16_t)reg_cx;
		mouse.updateRegion_y[0]=(int16_t)reg_dx;
		mouse.updateRegion_x[1]=(int16_t)reg_si;
		mouse.updateRegion_y[1]=(int16_t)reg_di;
		DrawCursor();
		break;
    case 0x11: // CuteMouse - get mouse capabilities
        reg_ax = 0x574d; // Identifier for detection purposes
        reg_bx = 0;      // Reserved capabilities flags
        reg_cx = 1;      // Wheel is supported
        mouse.cute_mouse = true; // This call enables CuteMouse extensions
        mouse.wheel = 0;
        // Previous implementation provided Genius Mouse 9.06 function to get
        // number of buttons (https://sourceforge.net/p/dosbox/patches/32/), it was
        // returning 0xffff in reg_ax and number of buttons in reg_bx; I suppose
        // the CuteMouse extensions are more useful
        break;
    case 0x12: // MS MOUSE - set large graphics cursor block
        LOG(LOG_MOUSE,LOG_ERROR)("Large graphics cursor block not implemented");
        break;
	case 0x13: // MS MOUSE v5.0+ - set double-speed threshold
		mouse.doubleSpeedThreshold=(reg_bx ? reg_bx : 64);
 		break;
	case 0x14: // MS MOUSE v3.0+ - exchange event-handler
		{
			uint16_t oldSeg = mouse.sub_seg;
			uint16_t oldOfs = mouse.sub_ofs;
			uint16_t oldMask= mouse.sub_mask;
			// Set new values
			mouse.sub_mask= reg_cx;
			mouse.sub_seg = SegValue(es);
			mouse.sub_ofs = reg_dx;
			// Return old values
			reg_cx = oldMask;
			reg_dx = oldOfs;
			SegSet16(es,oldSeg);
		}
		break;
	case 0x15: // MS MOUSE v6.0+ - get driver storage space requirements
		reg_bx = sizeof(mouse);
		break;
	case 0x16: // MS MOUSE v6.0+ - save driver state
		{
			LOG(LOG_MOUSE,LOG_WARN)("Saving driver state...");
			PhysPt dest = SegPhys(es)+reg_dx;
			MEM_BlockWrite(dest, &mouse, sizeof(mouse));
		}
		break;
	case 0x17: // MS MOUSE v6.0+ - load driver state
		{
			LOG(LOG_MOUSE,LOG_WARN)("Loading driver state...");
			PhysPt src = SegPhys(es)+reg_dx;
			MEM_BlockRead(src, &mouse, sizeof(mouse));
		}
		break;
    case 0x18: // MS MOUSE v6.0+ - set alternate mouse user handler
    case 0x19: // MS MOUSE v6.0+ - set alternate mouse user handler
        LOG(LOG_MOUSE,LOG_WARN)("Alternate mouse user handler not implemented");
        break;
	case 0x1a: // MS MOUSE v6.0+ - set mouse sensitivity
		// ToDo : double mouse speed value
		Mouse_SetSensitivity(reg_bx,reg_cx,reg_dx);

		LOG(LOG_MOUSE,LOG_WARN)("Set sensitivity used with %d %d (%d)",reg_bx,reg_cx,reg_dx);
		break;
	case 0x1b: //  MS MOUSE v6.0+ - get mouse sensitivity
		reg_bx = mouse.senv_x_val;
		reg_cx = mouse.senv_y_val;
		reg_dx = mouse.dspeed_val;

		LOG(LOG_MOUSE,LOG_WARN)("Get sensitivity %d %d",reg_bx,reg_cx);
		break;
    case 0x1c: // MS MOUSE v6.0+ - set interrupt rate
        // Can't really set a rate this is host determined
        break;
    case 0x1d: // MS MOUSE v6.0+ - set display page number
		mouse.page=reg_bl;
		break;
	case 0x1e: // MS MOUSE v6.0+ - get display page number
		reg_bx=mouse.page;
		break;
	case 0x1f: // MS MOUSE v6.0+ - disable mouse driver
        // ES:BX old mouse driver Zero at the moment TODO
		reg_bx=0;
		SegSet16(es,0);	   
		mouse.enabled=false; // Just for reporting not doing a thing with it
		mouse.oldhidden=mouse.hidden;
		mouse.hidden=1;
		break;
	case 0x20: // MS MOUSE v6.0+ - enable mouse driver
		mouse.enabled=true;
		mouse.hidden=mouse.oldhidden;
		break;
	case 0x22: // MS MOUSE v6.0+ - set language for messages
        // 00h = English, 01h = French, 02h = Dutch, 03h = German, 04h = Swedish
        // 05h = Finnish, 06h = Spanish, 07h = Portugese, 08h = Italian
		mouse.language=reg_bx;
		break;
	case 0x23: // MS MOUSE v6.0+ - get language for messages
		reg_bx=mouse.language;
		break;
	case 0x24: // MS MOUSE v6.26+ - get Software version, mouse type, and IRQ number
		reg_bx=0x805; // version 8.05 woohoo 
		reg_ch=0x04;  // PS/2 type
		reg_cl=0;     // PS/2 mouse; for any other type it would be IRQ number
		break;
    case 0x25: // MS MOUSE v6.26+ - get general driver information
        // TODO: According to PC sourcebook reference
        //       Returns:
        //       AH = status
        //         bit 7 driver type: 1=sys 0=com
        //         bit 6: 0=non-integrated 1=integrated mouse driver
        //         bits 4-5: cursor type  00=software text cursor 01=hardware text cursor 1X=graphics cursor
        //         bits 0-3: Function 28 mouse interrupt rate
        //       AL = Number of MDDS (?)
        //       BX = fCursor lock
        //       CX = FinMouse code
        //       DX = fMouse busy
        LOG(LOG_MOUSE,LOG_ERROR)("General driver information not implemented");
        break;
	case 0x26: // MS MOUSE v6.26+ - get maximum virtual coordinates
		reg_bx=(mouse.enabled ? 0x0000 : 0xffff);
		reg_cx=(uint16_t)mouse.max_x;
		reg_dx=(uint16_t)mouse.max_y;
		break;
    case 0x28: // MS MOUSE v7.0+ - set video mode
        // TODO: According to PC sourcebook
        //       Entry:
        //       CX = Requested video mode
        //       DX = Font size, 0 for default
        //       Returns:
        //       DX = 0 on success, nonzero (requested video mode) if not
        LOG(LOG_MOUSE,LOG_ERROR)("Set video mode not implemented");
        break;
    case 0x29: // MS MOUSE v7.0+ - enumerate video modes
        // TODO: According to PC sourcebook
        //       Entry:
        //       CX = 0 for first, != 0 for next
        //       Exit:
        //       BX:DX = named string far ptr
        //       CX = video mode number
        LOG(LOG_MOUSE,LOG_ERROR)("Enumerate video modes not implemented");
        break;
	case 0x2a: // MS MOUSE v7.01+ - get cursor hot spot
		reg_al=(uint8_t)-mouse.hidden;	// Microsoft uses a negative byte counter for cursor visibility
		reg_bx=(uint16_t)mouse.hotx;
		reg_cx=(uint16_t)mouse.hoty;
		reg_dx=0x04; // PS/2 mouse type
		break;
    case 0x2b: // MS MOUSE v7.0+ - load acceleration profiles
        LOG(LOG_MOUSE,LOG_ERROR)("Load acceleration profiles not implemented");
        break;
    case 0x2c: // MS MOUSE v7.0+ - get acceleration profiles
        LOG(LOG_MOUSE,LOG_ERROR)("Get acceleration profiles not implemented");
        break;
    case 0x2d: // MS MOUSE v7.0+ - select acceleration profile
        LOG(LOG_MOUSE,LOG_ERROR)("Select acceleration profile not implemented");
        break;
    case 0x2e: // MS MOUSE v8.10+ - set acceleration profile names
        LOG(LOG_MOUSE,LOG_ERROR)("Set acceleration profile names not implemented");
        break;
    case 0x2f: // MS MOUSE v7.02+ - mouse hardware reset
        LOG(LOG_MOUSE,LOG_ERROR)("INT 33 AX=2F mouse hardware reset not implemented");
        break;
    case 0x30: // MS MOUSE v7.04+ - get/set BallPoint information
        LOG(LOG_MOUSE,LOG_ERROR)("Get/set BallPoint information not implemented");
        break;
	case 0x31: // MS MOUSE v7.05+ - get current minimum/maximum virtual coordinates
		reg_ax=(uint16_t)mouse.min_x;
		reg_bx=(uint16_t)mouse.min_y;
		reg_cx=(uint16_t)mouse.max_x;
		reg_dx=(uint16_t)mouse.max_y;
		break;
    case 0x32: // MS MOUSE v7.05+ - get active advanced functions
        LOG(LOG_MOUSE,LOG_ERROR)("Get active advanced functions not implemented");
        break;
    case 0x33: // MS MOUSE v7.05+ - get switch settings and accelleration profile data
        LOG(LOG_MOUSE,LOG_ERROR)("Get switch settings and acceleration profile data not implemented");
        break;
    case 0x34: // MS MOUSE v8.0+ - get initialization file
        LOG(LOG_MOUSE,LOG_ERROR)("Get initialization file not implemented");
        break;
    case 0x35: // MS MOUSE v8.10+ - LCD screen large pointer support
        LOG(LOG_MOUSE,LOG_ERROR)("LCD screen large pointer support not implemented");
        break;
    case 0x4d: // MS MOUSE - return pointer to copyright string
        LOG(LOG_MOUSE,LOG_ERROR)("Return pointer to copyright string not implemented");
        break;
    case 0x6d: // MS MOUSE - get version string
        LOG(LOG_MOUSE,LOG_ERROR)("Get version string not implemented");
        break;
    case 0x53C1: // Logitech CyberMan
        LOG(LOG_MOUSE,LOG_NORMAL)("Mouse function 53C1 for Logitech CyberMan called. Ignored by regular mouse driver.");
        break;
	default:
		LOG(LOG_MOUSE,LOG_ERROR)("Mouse Function %04X not implemented!",reg_ax);
		break;
	}
	return CBRET_NONE;
}

static Bitu MOUSE_BD_Handler(void) {
	// the stack contains offsets to register values
	uint16_t raxpt=real_readw(SegValue(ss),reg_sp+0x0a);
	uint16_t rbxpt=real_readw(SegValue(ss),reg_sp+0x08);
	uint16_t rcxpt=real_readw(SegValue(ss),reg_sp+0x06);
	uint16_t rdxpt=real_readw(SegValue(ss),reg_sp+0x04);

	// read out the actual values, registers ARE overwritten
	uint16_t rax=real_readw(SegValue(ds),raxpt);
	reg_ax=rax;
	reg_bx=real_readw(SegValue(ds),rbxpt);
	reg_cx=real_readw(SegValue(ds),rcxpt);
	reg_dx=real_readw(SegValue(ds),rdxpt);
//	LOG_MSG("MOUSE BD: %04X %X %X %X %d %d",reg_ax,reg_bx,reg_cx,reg_dx,POS_X,POS_Y);
	
	// some functions are treated in a special way (additional registers)
	switch (rax) {
		case 0x09:	/* Define GFX Cursor */
		case 0x16:	/* Save driver state */
		case 0x17:	/* load driver state */
			SegSet16(es,SegValue(ds));
			break;
		case 0x0c:	/* Define interrupt subroutine parameters */
		case 0x14:	/* Exchange event-handler */ 
			if (reg_bx!=0) SegSet16(es,reg_bx);
			else SegSet16(es,SegValue(ds));
			break;
		case 0x10:	/* Define screen region for updating */
			reg_cx=real_readw(SegValue(ds),rdxpt);
			reg_dx=real_readw(SegValue(ds),rdxpt+2);
			reg_si=real_readw(SegValue(ds),rdxpt+4);
			reg_di=real_readw(SegValue(ds),rdxpt+6);
			break;
		default:
			break;
	}

	INT33_Handler();

	// save back the registers, too
	real_writew(SegValue(ds),raxpt,reg_ax);
	real_writew(SegValue(ds),rbxpt,reg_bx);
	real_writew(SegValue(ds),rcxpt,reg_cx);
	real_writew(SegValue(ds),rdxpt,reg_dx);
	switch (rax) {
		case 0x1f:	/* Disable Mousedriver */
			real_writew(SegValue(ds),rbxpt,SegValue(es));
			break;
		case 0x14: /* Exchange event-handler */ 
			real_writew(SegValue(ds),rcxpt,SegValue(es));
			break;
		default:
			break;
	}

	reg_ax=rax;
	return CBRET_NONE;
}

static Bitu INT74_Handler(void) {
	if (queue_used && !mouse.in_UIR) {
		queue_used--;
		/* Check for an active Interrupt Handler that will get called */
		if (mouse.sub_mask & queue[queue_used].dos_type) {
			reg_ax=queue[queue_used].dos_type;
            reg_bl=queue[queue_used].dos_buttons;
            reg_bh=GetResetWheel8bit();
			reg_cx=POS_X;
			reg_dx=POS_Y;
			reg_si=static_cast<int16_t>(mouse.mickey_x);
			reg_di=static_cast<int16_t>(mouse.mickey_y);
			CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
			CPU_Push16(RealOff(CALLBACK_RealPointer(int74_ret_callback))+7);
			CPU_Push16(RealSeg(uir_callback));
			CPU_Push16(RealOff(uir_callback));
			CPU_Push16(mouse.sub_seg);
			CPU_Push16(mouse.sub_ofs);
			mouse.in_UIR = true;
			//LOG(LOG_MOUSE,LOG_ERROR)("INT 74 %X",queue[queue_used].dos_type );
		} else if (useps2callback) {
			CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
			CPU_Push16(RealOff(CALLBACK_RealPointer(int74_ret_callback)));
			DoPS2Callback(queue[queue_used].dos_buttons, static_cast<int16_t>(mouse.x), static_cast<int16_t>(mouse.y));
		} else {
			SegSet16(cs, RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
			reg_ip = RealOff(CALLBACK_RealPointer(int74_ret_callback));
			//LOG(LOG_MOUSE,LOG_ERROR)("INT 74 not interested"); 
		}
	} else {
		SegSet16(cs, RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
		reg_ip = RealOff(CALLBACK_RealPointer(int74_ret_callback));
		//LOG(LOG_MOUSE,LOG_ERROR)("INT 74 no events");
	}
	return CBRET_NONE;
}

Bitu INT74_Ret_Handler(void) {
	if (queue_used) {
		if (!timer_in_progress) {
			timer_in_progress = true;
			PIC_AddEvent(MOUSE_Limit_Events,MOUSE_DELAY);
		}
	}
	return CBRET_NONE;
}

Bitu UIR_Handler(void) {
	mouse.in_UIR = false;
	return CBRET_NONE;
}

// ***************************************************************************
// External notifications
// ***************************************************************************

void Mouse_SetSensitivity(int32_t sensitivity_x, int32_t sensitivity_y) {
    static constexpr float MIN = 0.01f;
    static constexpr float MAX = 100.0f;

    mouse_config.sensitivity_x = std::clamp(sensitivity_x / 100.0f, -MAX, MAX);
    if (!std::signbit(mouse_config.sensitivity_x))
        mouse_config.sensitivity_x = std::max(mouse_config.sensitivity_x, MIN);
    else
        mouse_config.sensitivity_x = std::min(mouse_config.sensitivity_x, -MIN);

    mouse_config.sensitivity_y = std::clamp(sensitivity_y / 100.0f, -MAX, MAX);
    if (!std::signbit(mouse_config.sensitivity_y))
        mouse_config.sensitivity_y = std::max(mouse_config.sensitivity_y, MIN);
    else
        mouse_config.sensitivity_y = std::min(mouse_config.sensitivity_y, -MIN);
}

void Mouse_NewScreenParams(uint16_t clip_x, uint16_t clip_y, uint16_t res_x, uint16_t res_y,
                           bool fullscreen, int32_t x_abs, int32_t y_abs) {

    mouse_video.clip_x     = clip_x;
    mouse_video.clip_y     = clip_y;
    mouse_video.res_x      = res_x;
    mouse_video.res_y      = res_y;
    mouse_video.fullscreen = fullscreen;

    MouseVMW_NewScreenParams(x_abs, y_abs);
}

void Mouse_EventMoved(int32_t x_rel, int32_t y_rel, int32_t x_abs, int32_t y_abs, bool is_captured) {

	MouseVMW_NotifyMoved(x_abs, y_abs);

    // TODO: change the code so that in seamless mode sensitivity is not applied
	Mouse_CursorMoved(x_rel * mouse_config.sensitivity_x,
					  y_rel * mouse_config.sensitivity_y,
					  (x_abs - mouse_video.clip_x) / (mouse_video.res_x - 1) * mouse_config.sensitivity_x,
					  (y_abs - mouse_video.clip_y) / (mouse_video.res_y - 1) * mouse_config.sensitivity_y,
					  is_captured);

	MouseSER_NotifyMoved(x_rel, y_rel);
}

void Mouse_NotifyMovedVMW() {

	AddEvent(DOS_EV::MOUSE_HAS_MOVED);
}

void Mouse_EventPressed(uint8_t idx) {
    uint8_t buttons_12S_old = buttons_12 + (buttons_345 ? 4 : 0);

    if (idx < 2) {
        // left/right button
        if (buttons_12 & KEY_MASKS[idx]) return;
        buttons_12 |= KEY_MASKS[idx];
    } else if (idx < 5) {
        // middle/extra button
        if (buttons_345 & KEY_MASKS[idx]) return;
        buttons_345 |= KEY_MASKS[idx];
    } else
        return; // button not supported

    mouse.buttons    = buttons_12 + (buttons_345 ? 4 : 0);
    bool changed_12S = (buttons_12S_old != mouse.buttons);
    
    if (changed_12S) {
    	uint8_t idx_12S = idx < 2 ? idx : 2;
    	mouse.buttons   = buttons_12 + (buttons_345 ? 4 : 0);

    	MouseVMW_NotifyPressedReleased(mouse.buttons);
        MouseSER_NotifyPressed(mouse.buttons, idx_12S);

	 	mouse.times_pressed[idx_12S]++;
		mouse.last_pressed_x[idx_12S] = POS_X;
		mouse.last_pressed_y[idx_12S] = POS_Y;
        AddEvent(SelectEventPressed(idx, changed_12S));
    }
}

void Mouse_EventReleased(uint8_t idx) {
    uint8_t buttons_12S_old = buttons_12 + (buttons_345 ? 4 : 0);

    if (idx < 2) {
        // left/right button
        if (!(buttons_12 & KEY_MASKS[idx])) return;
        buttons_12 &= ~KEY_MASKS[idx];
    } else if (idx < 5) {
        // middle/extra button
        if (!(buttons_345 & KEY_MASKS[idx])) return;
        buttons_345 &= ~KEY_MASKS[idx];
    } else
        return; // button not supported

    mouse.buttons    = buttons_12 + (buttons_345 ? 4 : 0);
    bool changed_12S = (buttons_12S_old != mouse.buttons);

    if (changed_12S) {
    	uint8_t idx_12S = idx < 2 ? idx : 2;
    	
    	MouseVMW_NotifyPressedReleased(mouse.buttons);
        MouseSER_NotifyReleased(mouse.buttons, idx_12S);

		mouse.times_released[idx_12S]++;	
		mouse.last_released_x[idx_12S] = POS_X;
		mouse.last_released_y[idx_12S] = POS_Y;
        AddEvent(SelectEventReleased(idx, changed_12S));
    }
}

void Mouse_EventWheel(int32_t w_rel) {
    MouseSER_NotifyWheel(w_rel);

    if (mouse.cute_mouse) {
	    mouse.wheel = std::clamp(w_rel + mouse.wheel, -0x8000, 0x7fff);
	    mouse.last_wheel_moved_x = POS_X;
	    mouse.last_wheel_moved_y = POS_Y;

	    AddEvent(DOS_EV::WHEEL_HAS_MOVED);
    }
}

// ***************************************************************************
// Initialization
// ***************************************************************************

void MOUSE_Init(Section* /*sec*/) {
	// Callback for mouse interrupt 0x33
	call_int33=CALLBACK_Allocate();
//	RealPt i33loc=RealMake(CB_SEG+1,(call_int33*CB_SIZE)-0x10);
	RealPt i33loc=RealMake(DOS_GetMemory(0x1)-1,0x10);
	CALLBACK_Setup(call_int33,&INT33_Handler,CB_MOUSE,Real2Phys(i33loc),"Mouse");
	// Wasteland needs low(seg(int33))!=0 and low(ofs(int33))!=0
	real_writed(0,0x33<<2,i33loc);

	call_mouse_bd=CALLBACK_Allocate();
	CALLBACK_Setup(call_mouse_bd,&MOUSE_BD_Handler,CB_RETF8,
		PhysMake(RealSeg(i33loc),RealOff(i33loc)+2),"MouseBD");
	// pseudocode for CB_MOUSE (including the special backdoor entry point):
	//	jump near i33hd
	//	callback MOUSE_BD_Handler
	//	retf 8
	//  label i33hd:
	//	callback INT33_Handler
	//	iret


	// Callback for ps2 irq
	call_int74=CALLBACK_Allocate();
	CALLBACK_Setup(call_int74,&INT74_Handler,CB_IRQ12,"int 74");
	// pseudocode for CB_IRQ12:
	//	sti
	//	push ds
	//	push es
	//	pushad
	//	callback INT74_Handler
	//		ps2 or user callback if requested
	//		otherwise jumps to CB_IRQ12_RET
	//	push ax
	//	mov al, 0x20
	//	out 0xa0, al
	//	out 0x20, al
	//	pop	ax
	//	cld
	//	retf

	int74_ret_callback=CALLBACK_Allocate();
	CALLBACK_Setup(int74_ret_callback,&INT74_Ret_Handler,CB_IRQ12_RET,"int 74 ret");
	// pseudocode for CB_IRQ12_RET:
	//	cli
	//	mov al, 0x20
	//	out 0xa0, al
	//	out 0x20, al
	//	callback INT74_Ret_Handler
	//	popad
	//	pop es
	//	pop ds
	//	iret

	uint8_t hwvec=(MOUSE_IRQ>7)?(0x70+MOUSE_IRQ-8):(0x8+MOUSE_IRQ);
	RealSetVec(hwvec,CALLBACK_RealPointer(call_int74));

	// Callback for ps2 user callback handling
	useps2callback = false; ps2callbackinit = false;
 	call_ps2=CALLBACK_Allocate();
	CALLBACK_Setup(call_ps2,&PS2_Handler,CB_RETF,"ps2 bios callback");
	ps2_callback=CALLBACK_RealPointer(call_ps2);

	// Callback for mouse user routine return
	call_uir=CALLBACK_Allocate();
	CALLBACK_Setup(call_uir,&UIR_Handler,CB_RETF_CLI,"mouse uir ret");
	uir_callback=CALLBACK_RealPointer(call_uir);

	memset(&mouse,0,sizeof(mouse));
	mouse.hidden = 1; //Hide mouse on startup
	mouse.mode = 0xFF; //Non existing mode

   	mouse.sub_mask=0;
	mouse.sub_seg=0x6362;	// magic value
	mouse.sub_ofs=0;

	memset(&mouse_config,0,sizeof(mouse_config));
	mouse_config.sensitivity_x = 0.3f;
    mouse_config.sensitivity_y = 0.3f;

	memset(&mouse_video,0,sizeof(mouse_video));
    mouse_video.res_x = 320;
    mouse_video.res_y = 200;

	Mouse_ResetHardware();
	Mouse_Reset();
	Mouse_SetSensitivity(50,50,50);

	MouseVMW_Init();
}
