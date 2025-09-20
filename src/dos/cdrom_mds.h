// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CDROM_MDS_H
#define DOSBOX_CDROM_MDS_H

#include <fstream>
#include <optional>
#include <stdint.h>

#include "utils/byteorder.h"

// All the structs in this header must be tighly packed.
// They are directly read in from a file.
#pragma pack(push, 1)

// MDS structs taken and de-glib'd from https://github.com/cdemu/cdemu/blob/master/libmirage/images/image-mds/image-mds.h

struct MdsHeader
{
	uint8_t signature[16];
	uint8_t version[2];
	uint16_t medium_type;
	uint16_t num_sessions;
	uint16_t dummy1[2];
	uint16_t bca_len;
	uint32_t dummy2[2];
	uint32_t bca_data_offset;
	uint32_t dummy3[6];
	uint32_t disc_structures_offset;
	uint32_t dummy4[3];
	uint32_t session_block_offset;
	uint32_t dpm_blocks_offset;
};
static_assert(sizeof(MdsHeader) == 88);

inline std::optional<MdsHeader> read_mds_header(std::ifstream &stream)
{
	stream.seekg(0);
	if (stream.fail()) {
		return {};
	}
	MdsHeader mds_header = {};
	stream.read(reinterpret_cast<char *>(&mds_header), sizeof(mds_header));
	if (stream.fail()) {
		return {};
	}

	#ifdef WORDS_BIGENDIAN
	mds_header.medium_type = bswap_u16(mds_header.medium_type);
	mds_header.num_sessions = bswap_u16(mds_header.num_sessions);
	mds_header.bca_len = bswap_u16(mds_header.bca_len);
	mds_header.bca_data_offset = bswap_u32(mds_header.bca_data_offset);
	mds_header.disc_structures_offset = bswap_u32(mds_header.disc_structures_offset);
	mds_header.session_block_offset = bswap_u32(mds_header.session_block_offset);
	mds_header.dpm_blocks_offset = bswap_u32(mds_header.dpm_blocks_offset);
	#endif

	return mds_header;
}

struct MdsSessionBlock
{
	int32_t session_start;
	int32_t session_end;
	uint16_t session_number;
	uint8_t num_all_blocks;
	uint8_t num_nontrack_blocks;
	uint16_t first_track;
	uint16_t last_track;
	uint32_t dummy1;
	uint32_t track_block_offset;
};
static_assert(sizeof(MdsSessionBlock) == 24);

inline std::optional<MdsSessionBlock> read_mds_session_block(std::ifstream &stream, const std::ifstream::pos_type pos)
{
	stream.seekg(pos);
	if (stream.fail()) {
		return {};
	}
	MdsSessionBlock mds_session_block = {};
	stream.read(reinterpret_cast<char *>(&mds_session_block), sizeof(mds_session_block));
	if (stream.fail()) {
		return {};
	}

	#ifdef WORDS_BIGENDIAN
	mds_session_block.session_start = bswap_u32(mds_session_block.session_start);
	mds_session_block.session_end = bswap_u32(mds_session_block.session_end);
	mds_session_block.session_number = bswap_u16(mds_session_block.session_number);
	mds_session_block.first_track = bswap_u16(mds_session_block.first_track);
	mds_session_block.last_track = bswap_u16(mds_session_block.last_track);
	mds_session_block.track_block_offset = bswap_u32(mds_session_block.track_block_offset);
	#endif

	return mds_session_block;
}

struct MdsTrackBlock
{
	uint8_t mode;
	uint8_t subchannel;
	uint8_t adr_ctl;

	// We always use point instead of track number.
	// point == track_number for track entires
	// and can also differentiate non-track entires.
	uint8_t track_number;
	uint8_t point;
	uint8_t min;
	uint8_t sec;
	uint8_t frame;
	uint8_t zero;
	uint8_t pmin;
	uint8_t psec;
	uint8_t pframe;

	uint32_t extra_offset;
	uint16_t sector_size;

	uint8_t dummy1[18];
	uint32_t start_sector;
	uint64_t start_offset;
	uint32_t number_of_files;
	uint32_t footer_offset;
	uint8_t dummy2[24];
};
static_assert(sizeof(MdsTrackBlock) == 80);

inline std::optional<MdsTrackBlock> read_mds_track_block(std::ifstream &stream, const std::ifstream::pos_type pos)
{
	stream.seekg(pos);
	if (stream.fail()) {
		return {};
	}
	MdsTrackBlock mds_track_block = {};
	stream.read(reinterpret_cast<char *>(&mds_track_block), sizeof(mds_track_block));
	if (stream.fail()) {
		return {};
	}

	#ifdef WORDS_BIGENDIAN
	mds_track_block.extra_offset = bswap_u32(mds_track_block.extra_offset);
	mds_track_block.sector_size = bswap_u16(mds_track_block.sector_size);
	mds_track_block.start_sector = bswap_u32(mds_track_block.start_sector);
	mds_track_block.start_offset = bswap_u64(mds_track_block.start_offset);
	mds_track_block.number_of_files = bswap_u32(mds_track_block.number_of_files);
	mds_track_block.footer_offset = bswap_u32(mds_track_block.footer_offset);
	#endif

	return mds_track_block;
}

struct MdsExtraBlock
{
    uint32_t pregap;
    uint32_t length;
};
static_assert(sizeof(MdsExtraBlock) == 8);

inline std::optional<MdsExtraBlock> read_mds_extra_block(std::ifstream &stream, const std::ifstream::pos_type pos)
{
	stream.seekg(pos);
	if (stream.fail()) {
		return {};
	}
	MdsExtraBlock mds_extra_block = {};
	stream.read(reinterpret_cast<char *>(&mds_extra_block), sizeof(mds_extra_block));
	if (stream.fail()) {
		return {};
	}

	#ifdef WORDS_BIGENDIAN
	mds_extra_block.pregap = bswap_u32(mds_extra_block.pregap);
	mds_extra_block.length = bswap_u32(mds_extra_block.length);
	#endif

	return mds_extra_block;
}

struct MdsFooter
{
    uint32_t filename_offset;
    uint32_t widechar_filename;
    uint32_t dummy1;
    uint32_t dummy2;
};
static_assert(sizeof(MdsFooter) == 16);

inline std::optional<MdsFooter> read_mds_footer(std::ifstream &stream, const std::ifstream::pos_type pos)
{
	stream.seekg(pos);
	if (stream.fail()) {
		return {};
	}
	MdsFooter mds_footer = {};
	stream.read(reinterpret_cast<char *>(&mds_footer), sizeof(mds_footer));
	if (stream.fail()) {
		return {};
	}

	#ifdef WORDS_BIGENDIAN
	mds_footer.filename_offset = bswap_u32(mds_footer.filename_offset);
	mds_footer.widechar_filename = bswap_u32(mds_footer.widechar_filename);
	#endif

	return mds_footer;
}

#pragma pack(pop)

#endif
