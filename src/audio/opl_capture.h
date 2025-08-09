// SPDX-FileCopyrightText:  2002-2024 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_OPL_CAPTURE_H
#define DOSBOX_OPL_CAPTURE_H

#include "dosbox.h"

#include "hardware/audio/opl.h"
#include "hardware/inout.h"


#ifdef _MSC_VER
	#pragma pack(1)
#endif

struct DroRawHeader {
	// 0x00, "DBRAWOPL"
	uint8_t id[8];

	// 0x08, size of the data following the m
	uint16_t version_high;

	// 0x0a, size of the data following the m
	uint16_t version_low;

	// 0x0c, uint32_t amount of command/data pairs
	uint32_t commands;

	// 0x10, uint32_t Total milliseconds of data in this chunk
	uint32_t milliseconds;

	// 0x14, uint8_t Hardware Type
	// 0=opl2,1=dual-opl2,2=opl3
	uint8_t hardware;

	// 0x15, uint8_t Format 0=cmd/data interleaved, 1 maybe all cdms,
	// followed by all data
	uint8_t format;

	// 0x16, uint8_t Compression Type, 0 = No Compression
	uint8_t compression;

	// 0x17, uint8_t Delay 1-256 msec command
	uint8_t delay256;

	// 0x18, uint8_t (delay + 1)*256
	uint8_t delay_shift8;

	// 0x191, uint8_t Raw Conversion Table size
	uint8_t conv_table_size;

} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
	#pragma pack()
#endif


class OplCapture {
public:
	OplCapture(OplRegisterCache* cache);
	~OplCapture();

	bool DoWrite(io_port_t reg_full, uint8_t val);

	// prevent copy
	OplCapture(OplCapture&) = delete;

	// prevent assignment
	OplCapture& operator=(OplCapture&) = delete;

private:
	// 127 entries to go from raw data to registers
	uint8_t to_reg[127];

	// How many entries in the ToPort are used
	uint8_t raw_used = 0;

	// 256 entries to go from port index to raw data
	uint8_t to_raw[256];

	uint8_t delay256     = 0;
	uint8_t delay_shift8 = 0;

	DroRawHeader header;

	// File used for writing
	FILE* handle = nullptr;

	// Start used to check total raw length on end
	uint32_t startTicks = 0;

	// Last ticks when last last cmd was added
	uint32_t lastTicks = 0;

	// 16 added for delay commands and what not
	uint8_t buf[1024];

	uint32_t bufUsed = 0;

	OplRegisterCache* cache;

	void MakeEntry(uint8_t reg, uint8_t& raw);
	void MakeTables();

	void ClearBuf();

	void AddBuf(uint8_t raw, uint8_t val);
	void AddWrite(io_port_t reg_full, uint8_t val);
	void WriteCache();

	void InitHeader();

	void CloseFile();
};

void OPLCAPTURE_SaveRad(const OplRegisterCache* cache);

#endif // DOSBOX_OPL_CAPTURE_H
