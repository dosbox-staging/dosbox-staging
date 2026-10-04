// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2026 The DOSBox-X Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ints/imagedisk_teledisk.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ints/bios_disk.h"
#include "misc/logging.h"
#include "utils/checks.h"

CHECK_NARROWING();

// The DiskGeometry table does not carry a sector size, so derive
// one and use it to match table entries against a .td0 sector layout.
static uint32_t bytes_per_sector(const DiskGeometry& geo)
{
	const auto total_sectors = static_cast<uint32_t>(geo.cylcount) *
	                           geo.headscyl * geo.secttrack;
	if (total_sectors == 0) {
		return 0;
	}
	return (geo.ksize * 1024) / total_sectors;
}

static uint16_t read_u16_le(const uint8_t *bytes)
{
	return static_cast<uint16_t>(bytes[0] |
	                             static_cast<uint16_t>(bytes[1] << 8u));
}

// CRC-16 used by the TeleDisk format (polynomial 0xA097). The running CRC is
// passed in 'crc' so several blocks can be chained.
static uint16_t td0_crc16(const uint8_t *data, const size_t length, uint16_t crc = 0)
{
	for (size_t i = 0; i < length; ++i) {
		crc = static_cast<uint16_t>(crc ^
		                            static_cast<uint16_t>(data[i] << 8));
		for (int bit = 0; bit < 8; ++bit) {
			crc = static_cast<uint16_t>(
			        (crc << 1) ^ ((crc & 0x8000) ? 0xa097 : 0));
		}
	}
	return crc;
}

static std::optional<std::vector<uint8_t>> decode_raw(const std::vector<uint8_t>& encoded,
                                                      const uint16_t sector_size)
{
	if (encoded.size() != sector_size) {
		return {};
	}
	return encoded;
}

static std::optional<std::vector<uint8_t>> decode_repeated_2byte_pattern(
        const std::vector<uint8_t>& encoded, const uint16_t sector_size)
{
	std::vector<uint8_t> decoded = {};
	decoded.reserve(sector_size);

	size_t pos = 0;
	while (decoded.size() < sector_size) {
		if (pos + 4 > encoded.size()) {
			return {};
		}

		const auto count = read_u16_le(&encoded[pos]);
		pos += 2;
		const auto b0 = encoded[pos++];
		const auto b1 = encoded[pos++];

		for (int i = 0; i < count && decoded.size() < sector_size; ++i) {
			decoded.push_back(b0);
			if (decoded.size() >= sector_size) {
				break;
			}
			decoded.push_back(b1);
		}
	}

	return decoded;
}

static std::optional<std::vector<uint8_t>> decode_rle(const std::vector<uint8_t>& encoded,
                                                      const uint16_t sector_size)
{
	std::vector<uint8_t> decoded = {};
	decoded.reserve(sector_size);

	size_t pos = 0;
	while (decoded.size() < sector_size) {
		if (pos >= encoded.size()) {
			return {};
		}

		const auto token = encoded[pos++];
		if (token == 0) {
			// A zero token introduces a run of literal bytes.
			if (pos >= encoded.size()) {
				return {};
			}
			const auto literal_count = encoded[pos++];
			if (pos + literal_count > encoded.size()) {
				return {};
			}
			for (int i = 0;
			     i < literal_count && decoded.size() < sector_size;
			     ++i) {
				decoded.push_back(encoded[pos++]);
			}
		} else {
			// Any other token gives the block length in 16-bit
			// words, followed by the number of times to repeat the
			// block.
			const auto block_len = static_cast<size_t>(token) * 2u;
			if (pos >= encoded.size()) {
				return {};
			}
			const auto repeat_count = encoded[pos++];
			if (pos + block_len > encoded.size()) {
				return {};
			}

			for (int r = 0;
			     r < repeat_count && decoded.size() < sector_size;
			     ++r) {
				for (size_t i = 0;
				     i < block_len && decoded.size() < sector_size;
				     ++i) {
					decoded.push_back(encoded[pos + i]);
				}
			}
			pos += block_len;
		}
	}

	return decoded;
}

static std::optional<std::vector<uint8_t>> decode_sector_data(
        const ImageDiskTeledisk::SectorEncodingMethod method,
        const std::vector<uint8_t>& encoded, const uint16_t sector_size)
{
	switch (method) {
	case ImageDiskTeledisk::SectorEncodingMethod::Raw:
		return decode_raw(encoded, sector_size);
	case ImageDiskTeledisk::SectorEncodingMethod::Repeated2BytePattern:
		return decode_repeated_2byte_pattern(encoded, sector_size);
	case ImageDiskTeledisk::SectorEncodingMethod::Rle:
		return decode_rle(encoded, sector_size);
	default:
		// Unsupported/unknown encoding
		return {};
	}
}

// True if the file begins with a plausible TeleDisk image header. Leaves the
// file position unspecified.
bool IsTelediskImage(FILE *img_file)
{
	Td0ImageHeader header = {};

	if (fseek(img_file, 0, SEEK_SET) != 0) {
		return false;
	}
	if (fread(&header, 1, sizeof(header), img_file) != sizeof(header)) {
		return false;
	}

	// "TD" is normal compression and "td" is the advanced compression
	// variant. Both are TeleDisk images, so both are recognised here.
	const auto is_normal   = header.signature[0] == 'T' &&
	                         header.signature[1] == 'D';
	const auto is_advanced = header.signature[0] == 't' &&
	                         header.signature[1] == 'd';
	if (!is_normal && !is_advanced) {
		return false;
	}

	// Only the first volume of a multi-volume set is useful on its own.
	if (header.sequence != 0) {
		return false;
	}

	// The stepping field's low 7 bits hold a drive step rate of 0, 1 or 2.
	if ((header.stepping & 0x7f) > 2) {
		return false;
	}

	const auto crc = td0_crc16(reinterpret_cast<const uint8_t*>(&header),
	                           offsetof(Td0ImageHeader, crc));
	return crc == read_u16_le(header.crc);
}

const ImageDiskTeledisk::Entry *ImageDiskTeledisk::FindSector(
        const uint8_t head, const uint8_t track, const uint8_t sector,
        const unsigned int required_size) const
{
	const Entry *best = nullptr;

	for (const auto& entry : entries) {
		if (entry.phys_head != head || entry.phys_track != track ||
		    entry.logical_sector != sector) {
			continue;
		}
		if (required_size != 0 && entry.sector_size != required_size) {
			continue;
		}
		// Tracks can list the same address twice; prefer one with data.
		if (best == nullptr || (!best->has_data && entry.has_data)) {
			best = &entry;
		}
	}

	return best;
}

uint8_t ImageDiskTeledisk::ReadSector(const uint32_t head, const uint32_t cylinder,
                                      const uint32_t sector, void *data)
{
	const auto *ent = FindSector(static_cast<uint8_t>(head),
	                             static_cast<uint8_t>(cylinder),
	                             static_cast<uint8_t>(sector));
	if (ent == nullptr || !ent->has_data) {
		return 0x04; // Sector not found
	}

	// Copy the data regardless, mirroring real hardware which still
	// transfers the (possibly corrupt) sector before reporting the flagged
	// condition. The caller's buffer holds getSectSize() bytes, so a short
	// sector is zero-padded and a long one truncated.
	const auto out      = static_cast<uint8_t*>(data);
	const auto copy_len = std::min<size_t>(sector_size, ent->data.size());
	std::copy_n(ent->data.begin(), copy_len, out);
	if (copy_len < sector_size) {
		std::fill_n(out + copy_len, sector_size - copy_len, uint8_t{0});
	}

	// Convert sector flags recorded in the .td0 image to INT 13h status.
	if (ent->HasFlag(Td0SectorFlags::CrcError)) {
		return 0x10; // Data error
	}
	if (ent->HasFlag(Td0SectorFlags::DeletedMark)) {
		return 0x02; // Address mark not found
	}
	return 0x00;
}

uint8_t ImageDiskTeledisk::WriteSector(const uint32_t, const uint32_t,
                                       const uint32_t, const void *)
{
	// TeleDisk images are read-only: writes cannot be re-encoded back into
	// the .td0 archive, so accepting them in memory would silently discard
	// the guest's data on remount. Report write protection instead.
	return 0x03; // Write protected
}

uint8_t ImageDiskTeledisk::ReadAbsoluteSector(const uint32_t sectnum, void *data)
{
	if (sectors == 0 || heads == 0) {
		return 0x04; // Sector not found
	}

	const auto sector   = (sectnum % sectors) + 1u;
	const auto head     = (sectnum / sectors) % heads;
	const auto cylinder = sectnum / sectors / heads;

	return ReadSector(head, cylinder, sector, data);
}

uint8_t ImageDiskTeledisk::WriteAbsoluteSector(const uint32_t sectnum, const void *data)
{
	if (sectors == 0 || heads == 0) {
		return 0x04; // Sector not found
	}

	const auto sector   = (sectnum % sectors) + 1u;
	const auto head     = (sectnum / sectors) % heads;
	const auto cylinder = sectnum / sectors / heads;

	return WriteSector(head, cylinder, sector, data);
}

ImageDiskTeledisk::ImageDiskTeledisk(FILE *img_file, const char *img_name)
        : ImageDisk(img_file, img_name, 0, false)
{
	assert(diskimg);

	// A zero image size makes the base constructor skip its file-size
	// geometry probe: the archive is compressed, so its size says nothing
	// about the disk. Work the geometry out from the decoded sectors below.
	heads       = 1;
	cylinders   = 0;
	sectors     = 0;
	sector_size = 0;
	active      = false;
	is_readonly = true;

	Td0ImageHeader header = {};
	if (fread(&header, 1, sizeof(header), diskimg) != sizeof(header)) {
		return;
	}

	// The advanced compression variant cannot be decoded, so fail here
	// rather than letting the image fall through to the raw image path.
	if (header.signature[0] == 't' && header.signature[1] == 'd') {
		LOG_WARNING("TD0: Advanced compression is not supported");
		return;
	}

	// Only "TD" (normal compression) is decoded.
	if (header.signature[0] != 'T' || header.signature[1] != 'D') {
		return;
	}

	// Optional comment block, flagged by the high bit of the stepping field.
	if ((header.stepping & 0x80u) != 0) {
		Td0CommentHeader comment = {};
		if (fread(&comment, 1, sizeof(comment), diskimg) != sizeof(comment)) {
			return;
		}

		const auto length = read_u16_le(comment.length);
		std::string text(length, '\0');
		if (length != 0 && fread(text.data(), 1, length, diskimg) != length) {
			return;
		}

		// The CRC covers the comment header from after its own CRC
		// field, then the comment text. A mismatch only makes the text
		// untrustworthy, so warn and keep going.
		auto crc = td0_crc16(reinterpret_cast<const uint8_t*>(&comment) +
		                             offsetof(Td0CommentHeader, length),
		                     sizeof(comment) -
		                             offsetof(Td0CommentHeader, length));
		crc = td0_crc16(reinterpret_cast<const uint8_t*>(text.data()),
		                length,
		                crc);
		if (crc != read_u16_le(comment.crc)) {
			LOG_WARNING("TD0: Comment block CRC mismatch");
		}
		if (!text.empty()) {
			LOG_MSG("TD0: Comment: %s", text.c_str());
		}
	}

	auto found_end_marker = false;
	for (;;) {
		Td0TrackHeader track_header = {};
		if (fread(&track_header, 1, sizeof(track_header), diskimg) !=
		    sizeof(track_header)) {
			return;
		}

		const auto num_sectors = track_header.num_sectors;
		if (num_sectors == 0xff) {
			found_end_marker = true;
			break;
		}

		// A header with a bad CRC cannot be trusted to describe its
		// sectors. The sector records are still consumed so the next
		// track header can be found, but they stay out of the list.
		const auto track_crc = td0_crc16(reinterpret_cast<const uint8_t*>(
		                                         &track_header),
		                                 offsetof(Td0TrackHeader, crc));
		const auto skip_track = (track_crc & 0xff) != track_header.crc;
		if (skip_track) {
			LOG_WARNING("TD0: Bad track header CRC at C/H=%u/%u, skipping track",
			            track_header.cylinder,
			            track_header.head);
		}

		const auto phys_track = track_header.cylinder;
		const auto phys_head = static_cast<uint8_t>(track_header.head & 1u);

		for (int i = 0; i < num_sectors; ++i) {
			Td0SectorHeader sector_header = {};
			if (fread(&sector_header, 1, sizeof(sector_header), diskimg) !=
			    sizeof(sector_header)) {
				return;
			}

			Entry ent            = {};
			const auto size_code = (sector_header.size_code > 6u)
			                             ? uint8_t{0}
			                             : sector_header.size_code;

			ent.phys_track     = phys_track;
			ent.phys_head      = phys_head;
			ent.logical_sector = sector_header.sector;
			ent.logical_track  = sector_header.cylinder;
			ent.logical_head   = sector_header.head;
			ent.sector_size = static_cast<uint16_t>(128u << size_code);
			ent.flags = sector_header.flags;

			if (ent.HasFlag(Td0SectorFlags::IdNoData) ||
			    (sector_header.size_code & 0xf8u) != 0) {
				// No data block follows this header.
				ent.data.assign(ent.sector_size, 0);
			} else if (ent.HasFlag(Td0SectorFlags::DosSkipped)) {
				// TeleDisk omitted the data because the sector
				// sat in an unallocated cluster; assume a
				// freshly formatted disk, which DOS FORMAT
				// fills with 0xF6.
				ent.has_data = true;
				ent.data.assign(ent.sector_size, 0xf6);
			} else {
				Td0DataHeader data_header = {};
				const auto data_offset    = ftell(diskimg);
				if (fread(&data_header, 1, sizeof(data_header), diskimg) !=
				    sizeof(data_header)) {
					return;
				}

				const auto block_size = read_u16_le(
				        data_header.block_size);
				if (block_size == 0) {
					return;
				}

				const auto encoded_size = static_cast<size_t>(
				        block_size - 1u);
				std::vector<uint8_t> encoded(encoded_size, 0);
				if (encoded_size != 0 &&
				    fread(encoded.data(), 1, encoded_size, diskimg) !=
				            encoded_size) {
					return;
				}

				const auto method = static_cast<SectorEncodingMethod>(
				        data_header.method);
				const auto decoded = decode_sector_data(
				        method, encoded, ent.sector_size);
				if (decoded) {
					// The CRC covers the decompressed
					// payload only, not the sector header
					// or the data header, despite what
					// extant TeleDisk documentation claims.
					const auto crc = td0_crc16(decoded->data(),
					                           decoded->size());
					if ((crc & 0xff) == sector_header.crc) {
						ent.has_data = true;
						ent.data     = *decoded;
					} else {
						LOG_WARNING("TD0: Bad CRC for sector C/H/S=%u/%u/%u at file offset %ld",
						            ent.phys_track,
						            ent.phys_head,
						            ent.logical_sector,
						            data_offset);
						ent.data.assign(ent.sector_size, 0);
					}
				} else {
					LOG_WARNING("TD0: Failed to decode sector C/H/S=%u/%u/%u (method %u) at file offset %ld",
					            ent.phys_track,
					            ent.phys_head,
					            ent.logical_sector,
					            static_cast<unsigned>(method),
					            data_offset);
					ent.data.assign(ent.sector_size, 0);
				}
			}

			if (!skip_track) {
				entries.push_back(std::move(ent));
			}
		}
	}

	if (!found_end_marker || entries.empty()) {
		return;
	}

	const auto& geometries = BIOS_GetDiskGeometryList();
	auto found_disk        = false;

	// The geometry is not stored in the archive, so probe the sector layout
	// for the standard floppy formats and fall back to the highest CHS
	// addresses seen if none of them match.
	if (const auto *ent = FindSector(0, 0, 1); ent && ent->sector_size <= 1024) {
		sector_size = ent->sector_size;
	}

	if (sector_size != 0 && sector_size < 512) {
		if (const auto *ent = FindSector(0, 1, 1);
		    ent && ent->sector_size <= 1024 && sector_size < ent->sector_size) {
			sector_size = ent->sector_size;
		}
	}

	if (sector_size != 0) {
		for (const auto& geo : geometries) {
			if (bytes_per_sector(geo) != sector_size) {
				continue;
			}
			const auto *ent = FindSector(0,
			                             0,
			                             static_cast<uint8_t>(geo.secttrack),
			                             sector_size);
			if (ent && sectors < geo.secttrack) {
				sectors = geo.secttrack;
			}
		}
	}

	if (sector_size != 0 && sectors != 0) {
		for (const auto& geo : geometries) {
			if (bytes_per_sector(geo) != sector_size ||
			    geo.secttrack < sectors) {
				continue;
			}
			const auto last_cyl = static_cast<uint8_t>(geo.cylcount - 1);
			const auto *ent = FindSector(0,
			                             last_cyl,
			                             static_cast<uint8_t>(sectors),
			                             sector_size);
			if (ent && cylinders < geo.cylcount) {
				cylinders = geo.cylcount;
			}
		}
	}

	if (sector_size != 0 && sectors != 0 && cylinders != 0) {
		const auto *ent = FindSector(1, 0, static_cast<uint8_t>(sectors), sector_size);
		if (ent) {
			heads = 2;
		}
	}

	if (sector_size != 0 && sectors != 0 && cylinders != 0 && heads != 0) {
		found_disk = true;
	}

	if (!found_disk) {
		uint32_t max_sector = 0;
		uint32_t max_cyl    = 0;
		uint32_t max_head   = 0;
		uint32_t max_size   = 0;

		for (const auto& entry : entries) {
			max_size = std::max(max_size,
			                    static_cast<uint32_t>(entry.sector_size));
			max_sector = std::max(max_sector,
			                      static_cast<uint32_t>(
			                              entry.logical_sector));
			max_cyl = std::max(max_cyl,
			                   static_cast<uint32_t>(entry.phys_track));
			max_head = std::max(max_head,
			                    static_cast<uint32_t>(entry.phys_head));
		}

		if (max_sector != 0 && max_size != 0) {
			sector_size = max_size;
			sectors     = max_sector;
			cylinders   = max_cyl + 1;
			heads       = max_head + 1;
			found_disk  = true;
		}
	}

	LOG_MSG("TD0: Geometry C/H/S %u/%u/%u, %u bytes per sector, %u entries",
	        cylinders,
	        heads,
	        sectors,
	        sector_size,
	        static_cast<unsigned>(entries.size()));

	if (!found_disk) {
		return;
	}

	// Report the matching standard floppy type to the BIOS and the CMOS.
	for (size_t i = 0; i < geometries.size(); ++i) {
		const auto& geo = geometries[i];
		if (geo.headscyl == heads && geo.secttrack == sectors &&
		    geo.cylcount == cylinders) {
			floppytype = static_cast<uint8_t>(i);
			break;
		}
	}

	active = true;
	incrementFDD();
}
