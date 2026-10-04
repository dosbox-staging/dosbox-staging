// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2026 The DOSBox-X Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_IMAGEDISK_TELEDISK_H
#define DOSBOX_IMAGEDISK_TELEDISK_H

#include <cstdint>
#include <cstdio>
#include <vector>

#include "ints/bios_disk.h"

// TeleDisk sector flags (sector header, spec section 6.5). OR-combined.
enum class Td0SectorFlags : uint8_t {
	None        = 0x00,
	Duplicate   = 0x01, // sector duplicated within a track
	CrcError    = 0x02, // sector was read with a CRC error
	DeletedMark = 0x04, // deleted-data address mark
	DosSkipped  = 0x10, // skipped via DOS allocation; no data block follows
	IdNoData    = 0x20, // had ID field but no data; no data block follows
	DataNoId    = 0x40, // had data but no ID field (bogus header)
};

#pragma pack(push, 1)
// On-disk TeleDisk (.TD0) structures. All multi-byte fields are little-endian;
// read them through read_u16_le() rather than dereferencing directly.
//
// clang-format off
struct Td0ImageHeader {
	uint8_t signature[2]; // "TD" normal, "td" advanced compression
	uint8_t sequence;     // 0 for first/only volume
	uint8_t check_sequence;
	uint8_t version;      // high.low nibble, e.g. 0x15 == 1.5
	uint8_t data_rate;
	uint8_t drive_type;
	uint8_t stepping;     // bit 7 set => optional comment block present
	uint8_t dos_alloc_flag;
	uint8_t sides;        // 1 == one side, otherwise two
	uint8_t crc[2];
};

struct Td0CommentHeader {
	uint8_t crc[2];
	uint8_t length[2]; // size of comment data block that follows
	uint8_t year;      // years since 1900
	uint8_t month;     // 0 == January
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
};

struct Td0TrackHeader {
	uint8_t num_sectors; // 0xff marks end of image
	uint8_t cylinder;    // physical cylinder
	uint8_t head;        // bit 0 == side; bit 7 == single-density
	uint8_t crc;
};

struct Td0SectorHeader {
	uint8_t cylinder;  // logical cylinder in the sector ID field
	uint8_t head;      // logical side/head
	uint8_t sector;    // logical sector number
	uint8_t size_code; // sector size = 128 << size_code
	Td0SectorFlags flags;
	uint8_t crc;
};

struct Td0DataHeader {
	uint8_t block_size[2]; // size of data block including the method byte
	uint8_t method;        // SectorEncodingMethod
};
// clang-format on
#pragma pack(pop)

static_assert(sizeof(Td0ImageHeader) == 12);
static_assert(sizeof(Td0CommentHeader) == 10);
static_assert(sizeof(Td0TrackHeader) == 4);
static_assert(sizeof(Td0SectorHeader) == 6);
static_assert(sizeof(Td0DataHeader) == 3);

// TeleDisk (.td0) floppy image. The archive is decoded into 'entries' when the
// image is opened; sector I/O is then served from that list. TeleDisk images
// are read-only, as the compressed track layout cannot be written back.
//
class imageDiskTeledisk final : public imageDisk {
public:
	enum class SectorEncodingMethod : uint8_t {
		Raw                  = 0,
		Repeated2BytePattern = 1,
		Rle                  = 2,
	};

	// TeleDisk archives always describe a floppy and carry their own sector
	// layout, so the host file size does not describe the geometry.
	imageDiskTeledisk(FILE* img_file, const char* img_name);

	// prevent copying
	imageDiskTeledisk(const imageDiskTeledisk&) = delete;
	// prevent assignment
	imageDiskTeledisk& operator=(const imageDiskTeledisk&) = delete;

	uint8_t Read_Sector(uint32_t head, uint32_t cylinder, uint32_t sector,
	                    void* data) override;
	uint8_t Write_Sector(uint32_t head, uint32_t cylinder, uint32_t sector,
	                     void* data) override;
	uint8_t Read_AbsoluteSector(uint32_t sectnum, void* data) override;
	uint8_t Write_AbsoluteSector(uint32_t sectnum, void* data) override;

	struct Entry {
		uint8_t phys_track        = 0;
		uint8_t phys_head         = 0;
		uint8_t phys_sector       = 0;
		uint8_t logical_track     = 0;
		uint8_t logical_head      = 0;
		uint16_t sector_size      = 0;
		Td0SectorFlags flags      = Td0SectorFlags::None;
		bool has_data             = false;
		std::vector<uint8_t> data = {};

		bool HasFlag(const Td0SectorFlags flag) const
		{
			return (static_cast<uint8_t>(flags) &
			        static_cast<uint8_t>(flag)) != 0;
		}
	};

	// Returns the entry at the given physical CHS address, or nullptr. A
	// 'required_size' of 0 matches any sector size; anything else requires
	// an exact match.
	const Entry* FindSector(uint8_t head, uint8_t track, uint8_t sector,
	                        unsigned int required_size = 0) const;

	std::vector<Entry> entries = {};
};

#endif
