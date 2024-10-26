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

#include "dos_inc.h"

#include <cstring>

#include "../ints/int10.h"

#define NUMBER_ANSI_DATA 10

class device_CON final : public DOS_Device {
public:
	device_CON()
	{
		SetName("CON");
	}

	bool Read(uint8_t* data, uint16_t* size) override;
	bool Write(uint8_t* data, uint16_t* size) override;
	bool Seek(uint32_t* pos, uint32_t type) override;
	void Close() override;
	uint16_t GetInformation() override;
	bool ReadFromControlChannel([[maybe_unused]] PhysPt bufptr,
	                            [[maybe_unused]] uint16_t size,
	                            [[maybe_unused]] uint16_t* retcode) override
	{
		return false;
	}
	bool WriteToControlChannel([[maybe_unused]] PhysPt bufptr,
	                           [[maybe_unused]] uint16_t size,
	                           [[maybe_unused]] uint16_t* retcode) override
	{
		return false;
	}

private:
	void ClearAnsi();
	void Output(uint8_t chr);

	uint8_t readcache = 0;
	struct ansi {
		bool esc                       = false;
		bool sci                       = false;
		bool enabled                   = false;
		uint8_t attr                   = 0x7;
		uint8_t data[NUMBER_ANSI_DATA] = {};
		uint8_t numberofarg            = 0;
		int8_t savecol                 = 0;
		int8_t saverow                 = 0;
		bool warned                    = false;
	} ansi = {};
};

void device_CON::ClearAnsi()
{
	memset(ansi.data, 0, NUMBER_ANSI_DATA);
	ansi.esc         = false;
	ansi.sci         = false;
	ansi.numberofarg = 0;
}

bool device_CON::Read(uint8_t* data, uint16_t* size)
{
	uint16_t oldax = reg_ax;
	uint16_t count = 0;
	INT10_SetCurMode();
	if ((readcache) && (*size)) {
		data[count++] = readcache;
		if (dos.echo) {
			INT10_TeletypeOutputViaInterrupt(readcache, 7);
		}
		readcache = 0;
	}
	while (*size > count) {
		reg_ah = (IS_EGAVGA_ARCH) ? 0x10 : 0x0;
		CALLBACK_RunRealInt(0x16);
		switch (reg_al) {
		case 13:
			data[count++] = 0x0D;

			// It's only expanded if there's room for it
			if (*size > count) {
				data[count++] = 0x0A;
			}
			*size  = count;
			reg_ax = oldax;
			if (dos.echo) {
				// Maybe don't do this (no need for it actually)
				// (but it's compatible)
				INT10_TeletypeOutputViaInterrupt(13, 7);
				INT10_TeletypeOutputViaInterrupt(10, 7);
			}
			return true;
			break;
		case 8:
			// One char at the time so give back that BS
			if (*size == 1) {
				data[count++] = reg_al;
			}

			// Remove data if it exists (extended keys don't go right)
			else if (count) {
				data[count--] = 0;
				INT10_TeletypeOutputViaInterrupt(8, 7);
				INT10_TeletypeOutputViaInterrupt(' ', 7);
			} else {
				// No data read yet so restart while loop
				continue;
			}
			break;
		case 0xe0:
			// Extended keys in the  int 16 0x10 case
			if (!reg_ah) {
				// Extended key if reg_ah isn't 0
				data[count++] = reg_al;
			} else {
				data[count++] = 0;
				if (*size > count) {
					data[count++] = reg_ah;
				} else {
					readcache = reg_ah;
				}
			}
			break;
		case 0:
			// Extended keys in the int 16 0x0 case
			data[count++] = reg_al;
			if (*size > count) {
				data[count++] = reg_ah;
			} else {
				readcache = reg_ah;
			}
			break;
		default: data[count++] = reg_al; break;
		}
		if (dos.echo) {
			// What to do if *size==1 and character is BS ?????
			INT10_TeletypeOutputViaInterrupt(reg_al, 7);
		}
	}
	*size  = count;
	reg_ax = oldax;
	return true;
}

bool device_CON::Write(uint8_t* data, uint16_t* size)
{
	constexpr uint8_t code_escape = 0x1b;
	uint16_t count                = 0;
	Bitu i;
	uint8_t col, row, page;
	uint16_t ncols, nrows;
	uint8_t tempdata;
	INT10_SetCurMode();
	while (*size > count) {
		if (!ansi.esc) {
			if (data[count] == code_escape) {
				// Clear the datastructure
				ClearAnsi();
				// Start the sequence
				ansi.esc = true;
				count++;
				continue;
			} else if (data[count] == '\t' && !dos.direct_output) {
				// Expand tab if not direct output
				page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
				do {
					Output(' ');
					col = CURSOR_POS_COL(page);
				} while (col % 8);
				count++;
				continue;
			} else {
				Output(data[count]);
				count++;
				continue;
			}
		}

		if (!ansi.sci) {

			switch (data[count]) {
			case '[': ansi.sci = true; break;
			case '7': // Save cursor pos + attr
			case '8': // Restore this (wonder if this is actually used)
			case 'D': // Scrolling down
			case 'M': // Scrolling up
			default:
				// prob ()
				LOG(LOG_IOCTL, LOG_NORMAL)
				("ANSI: unknown char %c after a esc", data[count]);
				ClearAnsi();
				break;
			}
			count++;
			continue;
		}
		// ansi.esc and ansi.sci are true
		if (!dos.internal_output) {
			ansi.enabled = true;
		}
		page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
		switch (data[count]) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			ansi.data[ansi.numberofarg] = 10 * ansi.data[ansi.numberofarg] +
			                              (data[count] - '0');
			break;
		case ';': // Till a max of NUMBER_ANSI_DATA
			ansi.numberofarg++;
			break;
		case 'm': // SGR
			for (i = 0; i <= ansi.numberofarg; i++) {
				switch (ansi.data[i]) {
				case 0: // Normal
					// Real ansi does this as well. (should
					// do current defaults)
					ansi.attr = 0x07;
					break;
				case 1: // Bold mode on
					ansi.attr |= 0x08;
					break;
				case 4: // Underline
					LOG(LOG_IOCTL, LOG_NORMAL)
					("ANSI:no support for underline yet");
					break;
				case 5: // Blinking
					ansi.attr |= 0x80;
					break;
				case 7: // Reverse
					// Just like real ansi. (should do use
					// current colors reversed)
					ansi.attr = 0x70;
					break;
				case 30: // Foreground color black
					ansi.attr &= 0xf8;
					ansi.attr |= 0x0;
					break;
				case 31: // Foreground color red
					ansi.attr &= 0xf8;
					ansi.attr |= 0x4;
					break;
				case 32: // Foreground color green
					ansi.attr &= 0xf8;
					ansi.attr |= 0x2;
					break;
				case 33: // Foreground color yellow
					ansi.attr &= 0xf8;
					ansi.attr |= 0x6;
					break;
				case 34: // Foreground color blue
					ansi.attr &= 0xf8;
					ansi.attr |= 0x1;
					break;
				case 35: // Foreground color magenta
					ansi.attr &= 0xf8;
					ansi.attr |= 0x5;
					break;
				case 36: // Foreground color cyan
					ansi.attr &= 0xf8;
					ansi.attr |= 0x3;
					break;
				case 37: // Foreground color white
					ansi.attr &= 0xf8;
					ansi.attr |= 0x7;
					break;
				case 40:
					ansi.attr &= 0x8f;
					ansi.attr |= 0x0;
					break;
				case 41:
					ansi.attr &= 0x8f;
					ansi.attr |= 0x40;
					break;
				case 42:
					ansi.attr &= 0x8f;
					ansi.attr |= 0x20;
					break;
				case 43:
					ansi.attr &= 0x8f;
					ansi.attr |= 0x60;
					break;
				case 44:
					ansi.attr &= 0x8f;
					ansi.attr |= 0x10;
					break;
				case 45:
					ansi.attr &= 0x8f;
					ansi.attr |= 0x50;
					break;
				case 46:
					ansi.attr &= 0x8f;
					ansi.attr |= 0x30;
					break;
				case 47:
					ansi.attr &= 0x8f;
					ansi.attr |= 0x70;
					break;
				default: break;
				}
			}
			ClearAnsi();
			break;
		case 'f':
		case 'H': // Cursor Position
			if (!ansi.warned) {
				// Inform the debugger that ansi is used
				ansi.warned = true;
				LOG(LOG_IOCTL, LOG_WARN)("ANSI SEQUENCES USED");
			}
			ncols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
			nrows = IS_EGAVGA_ARCH
			              ? (real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS) + 1)
			              : 25;
			// Turn them into positions that are on the screen
			if (ansi.data[0] == 0) {
				ansi.data[0] = 1;
			}
			if (ansi.data[1] == 0) {
				ansi.data[1] = 1;
			}
			if (ansi.data[0] > nrows) {
				ansi.data[0] = (uint8_t)nrows;
			}
			if (ansi.data[1] > ncols) {
				ansi.data[1] = (uint8_t)ncols;
			}

			// ansi=1 based,  int10 is 0 based
			INT10_SetCursorPosViaInterrupt(--(ansi.data[0]),
			                               --(ansi.data[1]),
			                               page);
			ClearAnsi();
			break;
			// Cursor up down and forward and backward only change
			// the row or the col not both
		case 'A': // Cursor up
			col      = CURSOR_POS_COL(page);
			row      = CURSOR_POS_ROW(page);
			tempdata = (ansi.data[0] ? ansi.data[0] : 1);
			if (tempdata > row) {
				row = 0;
			} else {
				row -= tempdata;
			}
			INT10_SetCursorPosViaInterrupt(row, col, page);
			ClearAnsi();
			break;
		case 'B': // Cursor Down
			col      = CURSOR_POS_COL(page);
			row      = CURSOR_POS_ROW(page);
			nrows    = IS_EGAVGA_ARCH
			                 ? (real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS) + 1)
			                 : 25;
			tempdata = (ansi.data[0] ? ansi.data[0] : 1);
			if (tempdata + static_cast<Bitu>(row) >= nrows) {
				row = nrows - 1;
			} else {
				row += tempdata;
			}
			INT10_SetCursorPosViaInterrupt(row, col, page);
			ClearAnsi();
			break;
		case 'C': // Cursor forward
			col      = CURSOR_POS_COL(page);
			row      = CURSOR_POS_ROW(page);
			ncols    = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
			tempdata = (ansi.data[0] ? ansi.data[0] : 1);
			if (tempdata + static_cast<Bitu>(col) >= ncols) {
				col = ncols - 1;
			} else {
				col += tempdata;
			}
			INT10_SetCursorPosViaInterrupt(row, col, page);
			ClearAnsi();
			break;
		case 'D': // Cursor backward
			col      = CURSOR_POS_COL(page);
			row      = CURSOR_POS_ROW(page);
			tempdata = (ansi.data[0] ? ansi.data[0] : 1);
			if (tempdata > col) {
				col = 0;
			} else {
				col -= tempdata;
			}
			INT10_SetCursorPosViaInterrupt(row, col, page);
			ClearAnsi();
			break;
		case 'J': // Erase screen and move cursor home
			if (ansi.data[0] == 0) {
				ansi.data[0] = 2;
			}
			if (ansi.data[0] != 2) {
				// Every version behaves like type 2
				LOG(LOG_IOCTL, LOG_NORMAL)
				("ANSI: esc[%dJ called : not supported handling as 2",
				 ansi.data[0]);
			}
			INT10_ScrollWindow(0, 0, 255, 255, 0, ansi.attr, page);
			ClearAnsi();
			INT10_SetCursorPosViaInterrupt(0, 0, page);
			break;
		case 'h': // Set mode (if code =7 enable linewrap)
		case 'I': // Reset mode
			LOG(LOG_IOCTL, LOG_NORMAL)
			("ANSI: set/reset mode called(not supported)");
			ClearAnsi();
			break;
		case 'u': // Restore Cursor Pos
			INT10_SetCursorPosViaInterrupt(ansi.saverow, ansi.savecol, page);
			ClearAnsi();
			break;
		case 's': // Save cursor position
			ansi.savecol = CURSOR_POS_COL(page);
			ansi.saverow = CURSOR_POS_ROW(page);
			ClearAnsi();
			break;
		case 'K': // Erase till end of line (don't touch cursor)
			col   = CURSOR_POS_COL(page);
			row   = CURSOR_POS_ROW(page);
			ncols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);

			// Use this one to prevent scrolling when end of screen
			// is reached
			INT10_WriteChar(' ', ansi.attr, page, ncols - col, true);

			// for(i = col;i<(Bitu) ncols; i++)
			// INT10_TeletypeOutputAttr(' ',ansi.attr,true);
			INT10_SetCursorPosViaInterrupt(row, col, page);
			ClearAnsi();
			break;
		case 'M': // Delete line (NANSI)
			row   = CURSOR_POS_ROW(page);
			ncols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
			nrows = IS_EGAVGA_ARCH
			              ? (real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS) + 1)
			              : 25;
			INT10_ScrollWindow(row,
			                   0,
			                   nrows - 1,
			                   ncols - 1,
			                   ansi.data[0] ? -ansi.data[0] : -1,
			                   ansi.attr,
			                   0xFF);
			ClearAnsi();
			break;
		case 'l': // (if code =7) disable linewrap
		case 'p': // Reassign keys (needs strings)
		case 'i': // Printer stuff
		default:
			LOG(LOG_IOCTL, LOG_NORMAL)
			("ANSI: unhandled char %c in esc[", data[count]);
			ClearAnsi();
			break;
		}
		count++;
	}
	*size = count;
	return true;
}

bool device_CON::Seek(uint32_t* pos, [[maybe_unused]] uint32_t type)
{
	// Seek is valid
	*pos = 0;
	return true;
}

void device_CON::Close() {}

uint16_t device_CON::GetInformation()
{
	uint16_t head = mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
	uint16_t tail = mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);

	// No Key Available
	if ((head == tail) && !readcache) {
		return 0x80D3;
	}

	// Key Available
	if (readcache || real_readw(0x40, head)) {
		return 0x8093;
	}

	// Remove the zero from keyboard buffer
	uint16_t start = mem_readw(BIOS_KEYBOARD_BUFFER_START);
	uint16_t end   = mem_readw(BIOS_KEYBOARD_BUFFER_END);
	head += 2;
	if (head >= end) {
		head = start;
	}
	mem_writew(BIOS_KEYBOARD_BUFFER_HEAD, head);

	// No Key Available
	return 0x80D3;
}

void device_CON::Output(uint8_t chr)
{
	if (dos.internal_output || ansi.enabled) {
		if (CurMode->type == M_TEXT) {
			uint8_t page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
			uint8_t col = CURSOR_POS_COL(page);
			uint8_t row = CURSOR_POS_ROW(page);
			BIOS_NCOLS;
			BIOS_NROWS;
			if (nrows == row + 1 &&
			    (chr == '\n' || (ncols == col + 1 && chr != '\r' &&
			                     chr != 8 && chr != 7))) {

				INT10_ScrollWindow(0,
				                   0,
				                   (uint8_t)(nrows - 1),
				                   (uint8_t)(ncols - 1),
				                   -1,
				                   ansi.attr,
				                   page);

				INT10_SetCursorPosViaInterrupt(row - 1, col, page);
			}
		}
		constexpr auto use_attribute = true;
		INT10_TeletypeOutputAttrViaInterrupt(chr, ansi.attr, use_attribute);
	} else {
		INT10_TeletypeOutputViaInterrupt(chr, 7);
	}
}
