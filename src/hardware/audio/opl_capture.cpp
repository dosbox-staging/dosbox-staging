// SPDX-FileCopyrightText:  2002-2024 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "opl_capture.h"

#include <memory>

#include "capture/capture.h"
#include "byteorder.h"
#include "checks.h"

CHECK_NARROWING();

enum { HwOpl2 = 0, HwDualOpl2 = 1, HwOpl3 = 2 };

OplCapture::OplCapture(OplRegisterCache* _cache) : header(), cache(_cache)
{
	LOG_MSG("CAPTURE: Preparing to capture raw OPL output; "
	        "capturing will start when OPL output starts");
	MakeTables();
}

OplCapture::~OplCapture()
{
	CloseFile();
	LOG_MSG("CAPTURE: Stopped capturing raw OPL output");
}

bool OplCapture::DoWrite(const io_port_t reg_full, const uint8_t val)
{
	const auto reg_mask = reg_full & 0xff;

	// Check the raw index for this register if we actually have to
	// save it
	if (handle) {
		// Check if we actually care for this to be logged,
		// else just ignore it
		uint8_t raw = to_raw[reg_mask];
		if (raw == 0xff) {
			return true;
		}
		// Check if this command will not just replace the same
		// value in a reg that doesn't do anything with it
		if ((*cache)[reg_full] == val) {
			return true;
		}

		// Check how much time has passed
		uint32_t passed = PIC_Ticks - lastTicks;
		lastTicks       = PIC_Ticks;
		header.milliseconds += passed;

		// if ( passed > 0 ) LOG_MSG( "Delay %d", passed ) ;

		// If we passed more than 30 seconds since the last
		// command, we'll restart the capture
		if (passed > 30000) {
			CloseFile();
			goto skipWrite;
		}
		while (passed > 0) {
			// 1-256 millisecond delay
			if (passed < 257) {
				AddBuf(delay256, check_cast<uint8_t>(passed - 1));
				passed = 0;
			} else {
				const auto shift = (passed >> 8);
				passed -= shift << 8;
				AddBuf(delay_shift8, check_cast<uint8_t>(shift - 1));
			}
		}
		AddWrite(reg_full, val);
		return true;
	}

skipWrite:
	// Not yet capturing to a file here. Check for commands that
	// would start capturing, if it's not one of them return.

	// Note on in any channel
	const auto note_on = reg_mask >= 0xb0 && reg_mask <= 0xb8 && (val & 0x020);

	// Percussion mode enabled and a note on in any percussion
	// instrument
	const auto percussion_on = reg_mask == 0xbd && ((val & 0x3f) > 0x20);

	if (!(note_on || percussion_on)) {
		return true;
	}

	handle = CAPTURE_CreateFile(CaptureType::RawOplStream);
	if (!handle) {
		return false;
	}

	InitHeader();

	// Prepare space at start of the file for the header
	fwrite(&header, 1, sizeof(header), handle);

	// Write the Raw To Reg table
	fwrite(&to_reg, 1, raw_used, handle);

	// Write the cache of last commands
	WriteCache();

	// Write the command that triggered this
	AddWrite(reg_full, val);

	// Init the timing information for the next commands
	lastTicks  = PIC_Ticks;
	startTicks = PIC_Ticks;
	return true;
}

void OplCapture::MakeEntry(const uint8_t reg, uint8_t& raw)
{
	to_reg[raw] = reg;
	to_raw[reg] = raw;
	++raw;
}

void OplCapture::MakeTables()
{
	uint8_t index = 0;
	memset(to_reg, 0xff, sizeof(to_reg));
	memset(to_raw, 0xff, sizeof(to_raw));

	// Select the entries that are valid and the index is the
	// mapping to the index entry

	// 0x01: Waveform select
	MakeEntry(0x01, index);

	// 104: Four-Operator Enable
	MakeEntry(0x04, index);

	// 105: OPL3 Mode Enable
	MakeEntry(0x05, index);

	// 08: CSW / NOTE-SEL
	MakeEntry(0x08, index);

	// BD: Tremolo Depth / Vibrato D Percussion Mode /
	// BD/SD/TT/CY/HH Onepth /
	MakeEntry(0xbd, index);

	// Add the 32 byte range that hold the 18 operators
	for (uint8_t i = 0; i < 24; ++i) {
		if ((i & 7) < 6) {
			// 20-35: Tremolo / Vibrato / Sustain / KSR /
			// Frequency Multiplication Facto
			MakeEntry(0x20 + i, index);

			// 40-55: Key Scale Level / Output Level
			MakeEntry(0x40 + i, index);

			// 60-75: Attack Rate / Decay Rate
			MakeEntry(0x60 + i, index);

			// 80-95: Sustain Level / Release Rate
			MakeEntry(0x80 + i, index);

			// E0-F5: Waveform Select
			MakeEntry(0xe0 + i, index);
		}
	}

	// Add the 9 byte range that hold the 9 channels
	for (uint8_t i = 0; i < 9; ++i) {
		// A0-A8: Frequency Number
		MakeEntry(0xa0 + i, index);

		// B0-B8: Key On / Block Number / F-Number(hi bits)
		MakeEntry(0xb0 + i, index);

		// C0-C8: FeedBack Modulation Factor / Synthesis Type
		MakeEntry(0xc0 + i, index);
	}

	// Store the amount of bytes the table contains
	raw_used = index;

	// assert( raw_used <= 127 );
	delay256     = raw_used;
	delay_shift8 = raw_used + 1;
}

void OplCapture::ClearBuf()
{
	fwrite(buf, 1, bufUsed, handle);
	header.commands += bufUsed / 2;
	bufUsed = 0;
}

void OplCapture::AddBuf(const uint8_t raw, const uint8_t val)
{
	buf[bufUsed++] = raw;
	buf[bufUsed++] = val;

	if (bufUsed >= sizeof(buf)) {
		ClearBuf();
	}
}

void OplCapture::AddWrite(const io_port_t reg_full, const uint8_t val)
{
	// Do some special checks if we're doing opl3 or dualopl2
	// commands. Although you could pretty much just stick to always
	// doing OPL3 on the player side.

	// Enabling opl3 4op modes will make us go into opl3 mode
	if (header.hardware != HwOpl3 && reg_full == 0x104 && val && (*cache)[0x105]) {
		header.hardware = HwOpl3;
	}

	// Writing a keyon to a 2nd address enables dual opl2 otherwise
	// Maybe also check for rhythm
	if (header.hardware == HwOpl2 && reg_full >= 0x1b0 &&
	    reg_full <= 0x1b8 && val) {
		header.hardware = HwDualOpl2;
	}

	const uint8_t reg_mask = check_cast<uint8_t>(reg_full & 0xff);

	uint8_t raw = to_raw[reg_mask];
	if (raw == 0xff) {
		return;
	}
	if (reg_full & 0x100) {
		raw |= 0x80;
	}

	AddBuf(raw, val);
}

void OplCapture::WriteCache()
{
	// Check the registers to add
	for (uint16_t i = 0; i < 256; ++i) {
		auto val = (*cache)[i];

		// Silence the note on entries
		if (i >= 0xb0 && i <= 0xb8) {
			val &= static_cast<uint8_t>(~0x20);
		}
		if (i == 0xbd) {
			val &= static_cast<uint8_t>(~0x1f);
		}
		if (val) {
			AddWrite(i, val);
		}

		val = (*cache)[0x100 + i];

		if (i >= 0xb0 && i <= 0xb8) {
			val &= static_cast<uint8_t>(~0x20);
		}
		if (val) {
			AddWrite(0x100 + i, val);
		}
	}
}

void OplCapture::InitHeader()
{
	memset(&header, 0, sizeof(header));
	memcpy(header.id, "DBRAWOPL", 8);

	header.version_low     = 0;
	header.version_high    = 2;
	header.delay256        = delay256;
	header.delay_shift8    = delay_shift8;
	header.conv_table_size = raw_used;
}

void OplCapture::CloseFile()
{
	if (handle) {
		ClearBuf();

		// Endianise the header and write it to beginning of the
		// file
		header.version_high = host_to_le(header.version_high);
		header.version_low  = host_to_le(header.version_low);
		header.commands     = host_to_le(header.commands);
		header.milliseconds = host_to_le(header.milliseconds);

		fseek(handle, 0, SEEK_SET);
		fwrite(&header, 1, sizeof(header), handle);
		fclose(handle);

		handle = nullptr;
	}
}

// Save the current state of the operators as instruments in an Reality AdLib
// Tracker (RAD) file
void OPLCAPTURE_SaveRad(const OplRegisterCache* cache)
{
	uint8_t b[16 * 1024];
	uint8_t w = 0;

	FILE* handle = CAPTURE_CreateFile(CaptureType::RadOplInstruments);
	if (!handle) {
		return;
	}

	// Header
	fwrite("RAD by REALiTY!!", 1, 16, handle);

	// Version
	b[w++] = 0x10;

	// Default speed and no description
	b[w++] = 0x06;

	// Write 18 instuments for all operators in the cache
	for (uint8_t i = 0; i < 18; ++i) {
		const uint8_t set_offs = check_cast<uint8_t>((i / 9) * 256);

		const uint8_t base_offs = check_cast<uint8_t>(
		        set_offs + ((i % 9) / 3) * 8 + (i % 3));

		b[w++] = 1 + i; // instrument number
		b[w++] = (*cache)[base_offs + 0x23];
		b[w++] = (*cache)[base_offs + 0x20];
		b[w++] = (*cache)[base_offs + 0x43];
		b[w++] = (*cache)[base_offs + 0x40];
		b[w++] = (*cache)[base_offs + 0x63];
		b[w++] = (*cache)[base_offs + 0x60];
		b[w++] = (*cache)[base_offs + 0x83];
		b[w++] = (*cache)[base_offs + 0x80];
		b[w++] = (*cache)[set_offs + 0xc0 + (i % 9)];
		b[w++] = (*cache)[base_offs + 0xe3];
		b[w++] = (*cache)[base_offs + 0xe0];
	}

	// Instrument 0, no more instruments following
	b[w++] = 0;

	// 1 pattern following
	b[w++] = 1;

	// Zero out the remaining part of the file a bit to make rad happy
	for (uint8_t i = 0; i < 64; ++i) {
		b[w++] = 0;
	}

	fwrite(b, 1, w, handle);
	fclose(handle);
}

