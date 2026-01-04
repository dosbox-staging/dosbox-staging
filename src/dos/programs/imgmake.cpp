// SPDX-FileCopyrightText: 2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "imgmake.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "dos/dos.h"
#include "dosbox.h"
#include "misc/ansi_code_markup.h"
#include "misc/notifications.h"
#include "misc/support.h"
#include "more_output.h"
#include "utils/byteorder.h"
#include "utils/checks.h"
#include "utils/math_utils.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

namespace {

// Standard FreeDOS MBR
constexpr std::array<uint8_t, 512> freedos_mbr = {
        0xfa, 0x33, 0xc0, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0x8b, 0xf4, 0x50, 0x07,
        0x50, 0x1f, 0xfb, 0xfc, 0xbf, 0x00, 0x06, 0xb9, 0x00, 0x01, 0xf2, 0xa5,
        0xea, 0x1d, 0x06, 0x00, 0x00, 0xbe, 0xbe, 0x07, 0xb3, 0x04, 0x80, 0x3c,
        0x80, 0x74, 0x0e, 0x80, 0x3c, 0x00, 0x75, 0x1c, 0x83, 0xc6, 0x10, 0xfe,
        0xcb, 0x75, 0xef, 0xcd, 0x18, 0x8b, 0x14, 0x8b, 0x4c, 0x02, 0x8b, 0xee,
        0x83, 0xc6, 0x10, 0xfe, 0xcb, 0x74, 0x13, 0x80, 0x3c, 0x00, 0x74, 0xf4,
        0xbe, 0x8b, 0x06, 0xac, 0x3c, 0x00, 0x74, 0x0b, 0x56, 0xbb, 0x07, 0x00,
        0xb4, 0x0e, 0xcd, 0x10, 0x5e, 0xeb, 0xf0, 0xeb, 0xfe, 0xbf, 0x05, 0x00,
        0xbb, 0x00, 0x7c, 0xb8, 0x01, 0x02, 0x57, 0xcd, 0x13, 0x5f, 0x73, 0x0c,
        0x33, 0xc0, 0xcd, 0x13, 0x4f, 0x75, 0xed, 0xbe, 0xa3, 0x06, 0xeb, 0xd3,
        0xbe, 0xc2, 0x06, 0xbf, 0xfe, 0x7d, 0x81, 0x3d, 0x55, 0xaa, 0x75, 0xc7,
        0x8b, 0xf5, 0xea, 0x00, 0x7c, 0x00, 0x00, 0x45, 0x72, 0x72, 0x6f, 0x72,
        0x20, 0x6c, 0x6f, 0x61, 0x64, 0x69, 0x6e, 0x67, 0x20, 0x4f, 0x53, 0x00,
        0x4d, 0x69, 0x73, 0x73, 0x69, 0x6e, 0x67, 0x20, 0x4f, 0x53, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        // ... remaining zeros
};

struct DiskGeometry {
	uint32_t cyl;
	uint32_t heads;
	uint32_t sectors;
	uint8_t media_desc;
	uint16_t root_entries;
	uint32_t sect_per_fat;
	uint16_t sect_per_cluster;
	uint64_t total_size_kb;
	bool is_floppy;
};

// Maps preset names (e.g., "fd_1440") to geometries
const std::map<std::string, DiskGeometry> GeometryPresets = {
        { "fd_160",     {40, 1, 8, 0xFE, 64, 1, 1, 160, true}},
        { "fd_180",     {40, 1, 9, 0xFC, 64, 2, 1, 180, true}},
        { "fd_320",    {40, 2, 8, 0xFF, 112, 1, 2, 320, true}},
        { "fd_360",    {40, 2, 9, 0xFD, 112, 2, 2, 360, true}},
        { "fd_720",    {80, 2, 9, 0xF9, 112, 3, 2, 720, true}},
        {"fd_1200",  {80, 2, 15, 0xF9, 224, 7, 1, 1200, true}},
        {"fd_1440",  {80, 2, 18, 0xF0, 224, 9, 1, 1440, true}},
        {     "fd",  {80, 2, 18, 0xF0, 224, 9, 1, 1440, true}},
        { "floppy",  {80, 2, 18, 0xF0, 224, 9, 1, 1440, true}},
        {"fd_2880",  {80, 2, 36, 0xF0, 240, 9, 2, 2880, true}},
        // HD presets
        { "hd_250",  {489, 16, 63, 0xF8, 512, 0, 0, 0, false}},
        { "hd_520", {1023, 16, 63, 0xF8, 512, 0, 0, 0, false}},
        {"hd_1gig", {1023, 32, 63, 0xF8, 512, 0, 0, 0, false}},
        {"hd_2gig", {1023, 64, 63, 0xF8, 512, 0, 0, 0, false}}
};

// Return a 3-byte array [h, s|c_high, c_low]
std::array<uint8_t, 3> lba2chs(uint64_t lba, uint32_t max_c, uint32_t max_h,
                               uint32_t max_s)
{
	uint32_t c = 0, h = 0, s = 0;
	if (lba < static_cast<uint64_t>(max_c) * max_h * max_s) {
		s             = static_cast<uint32_t>((lba % max_s) + 1);
		uint64_t temp = lba / max_s;
		h             = static_cast<uint32_t>(temp % max_h);
		c             = static_cast<uint32_t>(temp / max_h);
		// Clamp for legacy CHS
		if (c > 1023) {
			c = 1023;
		}
	} else {
		c = 1023;
		h = max_h - 1;
		s = max_s;
	}

	return {static_cast<uint8_t>(h),
	        static_cast<uint8_t>((s & 0x3f) | ((c >> 2) & 0xc0)),
	        static_cast<uint8_t>(c & 0xff)};
}

// Write a string into a fixed-width buffer and pad with spaces
void write_padded_string(uint8_t* dest, const std::string& str, size_t length)
{
	std::string temp = str;
	temp.resize(length, ' ');
	std::copy(temp.begin(), temp.end(), dest);
}

} // namespace

namespace ImgmakeCommand {

enum class ErrorType { None, UnknownArgument, MissingArgument, InvalidValue };

struct CommandSettings {
	std::string filename   = {};
	std::string type       = {};
	std::string label      = {};
	std::string batch_file = {};

	uint64_t size_bytes = 0;
	uint32_t cylinders  = 0;
	uint32_t heads      = 0;
	uint32_t sectors    = 0;

	int fat_type            = -1;
	int sectors_per_cluster = 0;
	int root_entries        = 512;
	int fat_copies          = 2;

	bool force        = false;
	bool no_format    = false;
	bool use_chs      = false;
	bool batch_create = false;
};

using ParseResult = std::variant<ErrorType, CommandSettings>;

static void notify_warning(const std::string& message_name, const char* arg = nullptr)
{
	if (arg) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "IMGMAKE",
		                      message_name,
		                      arg);
	} else {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "IMGMAKE",
		                      message_name);
	}
}

ParseResult ParseArgs(const std::vector<std::string>& args)
{
	CommandSettings settings;
	const auto MegabytesToBytes = 1024 * 1024;

	if (args.empty()) {
		return ErrorType::None;
	}

	// First argument is usually the filename (unless it starts with -)
	size_t start_idx = 0;
	if (args[0][0] != '-') {
		settings.filename = args[0];
		start_idx         = 1;
	} else {
		settings.filename = "IMGMAKE.IMG";
	}

	for (size_t i = start_idx; i < args.size(); ++i) {
		std::string arg = args[i];
		lowcase(arg);

		if (arg == "-t") {
			if (i + 1 >= args.size()) {
				return ErrorType::MissingArgument;
			}
			settings.type = args[++i];
			lowcase(settings.type);
		} else if (arg == "-size") {
			if (i + 1 >= args.size()) {
				return ErrorType::MissingArgument;
			}
			const auto& size_str = args[++i];
			auto size_mb         = parse_int(size_str, 10);
			if (!size_mb) {
				return ErrorType::InvalidValue;
			}
			settings.size_bytes = static_cast<uint64_t>(*size_mb) *
			                      MegabytesToBytes;
		} else if (arg == "-chs") {
			if (i + 1 >= args.size()) {
				return ErrorType::MissingArgument;
			}
			const auto& chs_str = args[++i];
			auto parts          = split_with_empties(chs_str, ',');

			if (parts.size() != 3) {
				return ErrorType::InvalidValue;
			}
			auto c = parse_int(parts[0], 10);
			auto h = parse_int(parts[1], 10);
			auto s = parse_int(parts[2], 10);

			if (!c || !h || !s) {
				return ErrorType::InvalidValue;
			}

			settings.cylinders = static_cast<uint32_t>(*c);
			settings.heads     = static_cast<uint32_t>(*h);
			settings.sectors   = static_cast<uint32_t>(*s);
			settings.use_chs   = true;
		} else if (arg == "-label") {
			if (i + 1 >= args.size()) {
				return ErrorType::MissingArgument;
			}
			settings.label = args[++i];
		} else if (arg == "-fat") {
			if (i + 1 >= args.size()) {
				return ErrorType::MissingArgument;
			}
			if (auto f = parse_int(args[++i], 10)) {
				settings.fat_type = *f;
			}
		} else if (arg == "-spc") {
			if (i + 1 >= args.size()) {
				return ErrorType::MissingArgument;
			}
			if (auto s = parse_int(args[++i], 10)) {
				settings.sectors_per_cluster = *s;
			}
		} else if (arg == "-force") {
			settings.force = true;
		} else if (arg == "-noformat") {
			settings.no_format = true;
		} else if (arg == "-bat") {
			settings.batch_create = true;
		} else {
			return ErrorType::UnknownArgument;
		}
	}

	if (settings.type.empty()) {
		return ErrorType::MissingArgument;
	}

	return settings;
}

bool Execute(Program* program, CommandSettings& s)
{
	DiskGeometry geom         = {};
	uint64_t total_size       = 0;
	const uint64_t SectorSize = 512;

	// Determine Geometry and Size
	if (auto it = GeometryPresets.find(s.type); it != GeometryPresets.end()) {
		geom       = it->second;
		total_size = static_cast<uint64_t>(geom.cyl) * geom.heads *
		             geom.sectors * SectorSize;
	} else if (s.type == "hd") {
		geom.media_desc = 0xF8;
		geom.is_floppy  = false;

		if (s.use_chs) {
			geom.cyl     = s.cylinders;
			geom.heads   = s.heads;
			geom.sectors = s.sectors;
			total_size   = static_cast<uint64_t>(geom.cyl) *
			             geom.heads * geom.sectors * SectorSize;
		} else if (s.size_bytes > 0) {
			total_size           = s.size_bytes;
			uint64_t tot_sectors = total_size / SectorSize;

			// Calculate CHS from Size
			geom.heads   = 16;
			geom.sectors = 63;
			// Adjust if too large
			if (total_size > 528 * 1024 * 1024) {
				geom.heads = 64;
			}
			if (total_size > 1024 * 1024 * 1024) {
				geom.heads = 128;
			}
			if (total_size > 2ull * 1024 * 1024 * 1024) {
				geom.heads = 255;
			}

			geom.cyl = static_cast<uint32_t>(
			        tot_sectors / (geom.heads * geom.sectors));
			// Cap for legacy geometry
			if (geom.cyl > 1023) {
				geom.cyl = 1023;
			}
		} else {
			notify_warning("SHELL_CMD_IMGMAKE_MISSING_SIZE");
			return false;
		}
	} else {
		notify_warning("SHELL_CMD_IMGMAKE_INVALID_TYPE", s.type.c_str());
		return false;
	}

	if (total_size == 0) {
		notify_warning("SHELL_CMD_IMGMAKE_BAD_SIZE");
		return false;
	}

	// Check File Existence
	if (std::ifstream(s.filename) && !s.force) {
		notify_warning("SHELL_CMD_IMGMAKE_FILE_EXISTS", s.filename.c_str());
		return false;
	}

	// Create File
	std::fstream fs(s.filename,
	                std::ios::binary | std::ios::out | std::ios::in |
	                        std::ios::trunc);
	if (!fs) {
		notify_warning("SHELL_CMD_IMGMAKE_CANNOT_WRITE", s.filename.c_str());
		return false;
	}

	// Expand file to size (write last byte)
	fs.seekp(static_cast<std::streamoff>(total_size - 1));
	if (fs.fail()) {
		notify_warning("SHELL_CMD_IMGMAKE_SPACE_ERROR");
		fs.close();
		std::remove(s.filename.c_str());
		return false;
	}

	char zero = 0;
	fs.write(&zero, 1);
	fs.seekp(0);

	if (s.no_format) {
		return true;
	}

	std::array<uint8_t, 512> buffer;
	buffer.fill(0);
	int64_t boot_sector_pos = 0;

	// Partition Table for Hard Disks
	if (!geom.is_floppy) {
		// Standard offset (track 0)
		boot_sector_pos = geom.sectors;
		std::copy(freedos_mbr.begin(), freedos_mbr.end(), buffer.begin());

		// Partition 1 Entry (0x1BE)
		uint8_t* p = buffer.data() + 0x1BE;
		// Active Partition
		p[0] = 0x80;

		// Start CHS (0, 1, 1) usually, but we use
		// boot_sector_pos LBA conversion
		auto start_chs = lba2chs(static_cast<uint64_t>(boot_sector_pos),
		                         geom.cyl,
		                         geom.heads,
		                         geom.sectors);
		std::copy(start_chs.begin(), start_chs.end(), p + 1);

		// Partition Type
		uint64_t vol_sectors = (total_size / SectorSize) -
		                       static_cast<uint64_t>(boot_sector_pos);
		if (s.fat_type == 32 || vol_sectors > 4194304) {
			// FAT32 LBA
			p[4] = 0x0C;
		} else if (vol_sectors > 65535) {
			// FAT16B
			p[4] = 0x06;
		} else {
			// FAT16 < 32M
			p[4] = 0x04;
		}

		// End CHS
		auto end_chs = lba2chs((total_size / 512) - 1,
		                       geom.cyl,
		                       geom.heads,
		                       geom.sectors);
		std::copy(end_chs.begin(), end_chs.end(), p + 5);

		// LBA Start & Size
		host_writed(p + 8, static_cast<uint32_t>(boot_sector_pos));
		host_writed(p + 12, static_cast<uint32_t>(vol_sectors));

		// Signature
		buffer[510] = 0x55;
		buffer[511] = 0xAA;

		fs.write(reinterpret_cast<const char*>(buffer.data()),
		         buffer.size());
	}

	// Move to Boot Sector
	fs.seekp(static_cast<std::streamoff>(boot_sector_pos * SectorSize));
	buffer.fill(0);

	// Determines FAT parameters
	uint64_t vol_sectors = (total_size / SectorSize) -
	                       static_cast<uint64_t>(boot_sector_pos);
	int fat_bits = s.fat_type;

	// Auto-detect FAT
	if (fat_bits == -1) {
		if (geom.is_floppy) {
			fat_bits = 12;
		} else if (vol_sectors < 32680) {
			fat_bits = 12;
		} else if (vol_sectors < 4194304) {
			fat_bits = 16;
		} else {
			fat_bits = 32;
		}
	}

	// BPB Construction

	// JMP Instruction
	buffer[0] = 0xEB;
	buffer[1] = 0x3C;
	buffer[2] = 0x90;
	// OEM Name
	write_padded_string(buffer.data() + 3, "DOSBOX-S", 8);
	// Bytes per sector
	host_writew(buffer.data() + 11, static_cast<uint16_t>(SectorSize));

	// Sectors per cluster
	uint8_t spc = (s.sectors_per_cluster > 0)
	                    ? static_cast<uint8_t>(s.sectors_per_cluster)
	                    : 1;
	if (s.sectors_per_cluster == 0) {
		if (vol_sectors > 2097152) {
			spc = 32;
		} else if (vol_sectors > 32680) {
			// HD usually 4k clusters
			spc = 8;
		}
	}
	buffer[13] = spc;

	uint16_t reserved_sectors = (fat_bits == 32) ? 32 : 1;
	host_writew(buffer.data() + 14, reserved_sectors);
	buffer[16] = static_cast<uint8_t>(s.fat_copies);

	uint16_t root_ent = (fat_bits == 32)
	                          ? 0
	                          : (geom.root_entries > 0
	                                     ? geom.root_entries
	                                     : static_cast<uint16_t>(SectorSize));
	host_writew(buffer.data() + 17, root_ent);

	if (vol_sectors < 65536 && fat_bits != 32) {
		host_writew(buffer.data() + 19, static_cast<uint16_t>(vol_sectors));
	} else {
		host_writew(buffer.data() + 19, 0);
	}

	buffer[21] = geom.media_desc;

	// FAT Size calculation
	uint32_t fat_size         = 0;
	uint32_t root_dir_sectors = static_cast<uint32_t>(
	        ((static_cast<uint64_t>(root_ent) * 32) + (SectorSize - 1)) /
	        SectorSize);
	uint64_t data_sectors = vol_sectors - reserved_sectors - root_dir_sectors;
	uint64_t total_clusters = data_sectors / spc;

	if (fat_bits == 12) {
		fat_size = static_cast<uint32_t>(((total_clusters + 2) * 3 / 2) /
		                                 SectorSize) +
		           1;
	} else if (fat_bits == 16) {
		fat_size = static_cast<uint32_t>(((total_clusters + 2) * 2) /
		                                 SectorSize) +
		           1;
	} else {
		fat_size = static_cast<uint32_t>(((total_clusters + 2) * 4) /
		                                 SectorSize) +
		           1;
	}

	if (fat_bits != 32) {
		host_writew(buffer.data() + 22, static_cast<uint16_t>(fat_size));
	}

	// SPT
	host_writew(buffer.data() + 24, static_cast<uint16_t>(geom.sectors));
	// Heads
	host_writew(buffer.data() + 26, static_cast<uint16_t>(geom.heads));
	// Hidden sectors
	host_writed(buffer.data() + 28, static_cast<uint32_t>(boot_sector_pos));
	if (fat_bits != 32 && vol_sectors >= 65536) {
		host_writed(buffer.data() + 32, static_cast<uint32_t>(vol_sectors));
	} else if (fat_bits == 32) {
		host_writed(buffer.data() + 32, static_cast<uint32_t>(vol_sectors));
	}

	if (fat_bits == 32) {
		host_writed(buffer.data() + 36, fat_size);
		// Root Cluster
		host_writed(buffer.data() + 44, 2);
		// FS Info Sector
		host_writew(buffer.data() + 48, 1);
		// Backup Boot Sector
		host_writew(buffer.data() + 50, 6);
		// Ext Sig
		buffer[66] = 0x29;
		// Serial
		host_writed(buffer.data() + 67, 0xDEADBEEF);

		write_padded_string(buffer.data() + 71, "NO NAME", 11);
		write_padded_string(buffer.data() + 82, "FAT32", 8);
	} else {
		// Ext Sig
		buffer[38] = 0x29;
		// Serial
		host_writed(buffer.data() + 39, 0xDEADBEEF);
		write_padded_string(buffer.data() + 43, "NO NAME", 11);

		if (fat_bits == 16) {
			write_padded_string(buffer.data() + 54, "FAT16", 8);
		} else {
			write_padded_string(buffer.data() + 54, "FAT12", 8);
		}
	}

	buffer[SectorSize - 2] = 0x55;
	buffer[SectorSize - 1] = 0xAA;

	// Write Boot Sector
	fs.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

	// Write FATs
	std::array<uint8_t, 512> empty_sect;
	empty_sect.fill(0);

	// Position after reserved
	fs.seekp(static_cast<std::streamoff>(
	        (boot_sector_pos + reserved_sectors) * SectorSize));

	// Initialize FAT start
	std::array<uint8_t, 512> fat_header;
	fat_header.fill(0);

	if (fat_bits == 32) {
		// Media + reserved
		host_writed(fat_header.data(), 0x0FFFFFF8);
		// EOC
		host_writed(fat_header.data() + 4, 0x0FFFFFFF);
		// Root dir EOC
		host_writed(fat_header.data() + 8, 0x0FFFFFFF);
	} else if (fat_bits == 16) {
		host_writed(fat_header.data(), 0xFFFFFFF8);
	} else {
		fat_header[0] = 0xF8;
		fat_header[1] = 0xFF;
		fat_header[2] = 0xFF;
	}

	for (int i = 0; i < s.fat_copies; ++i) {
		auto current_fat_start = fs.tellp();
		fs.write(reinterpret_cast<const char*>(fat_header.data()),
		         fat_header.size());

		// Fill rest of FAT with zeros
		for (uint32_t k = 1; k < fat_size; ++k) {
			fs.write(reinterpret_cast<const char*>(empty_sect.data()),
			         empty_sect.size());
		}

		fs.seekp(current_fat_start +
		         static_cast<std::streamoff>(fat_size * SectorSize));
	}

	// Write Root Directory Label (Optional)
	if (!s.label.empty()) {
		buffer.fill(0);
		write_padded_string(buffer.data(), s.label, 11);
		// Volume Label Attribute
		buffer[11] = 0x08;
		fs.write(reinterpret_cast<const char*>(buffer.data()),
		         buffer.size());
	}

	// File closes automatically via RAII

	// Batch file creation
	if (s.batch_create) {
		std::filesystem::path bat_path(s.filename);
		bat_path.replace_extension(".BAT");

		std::ofstream bat(bat_path);
		if (bat.is_open()) {
			char drive_char = geom.is_floppy ? 'A' : 'C';
			const char* type_flag = geom.is_floppy ? " -t floppy" : "";

			bat << "IMGMOUNT " << drive_char << " " << s.filename
			    << type_flag << " -size 512," << geom.sectors << ","
			    << geom.heads << "," << geom.cyl << "\n";
		}
	}

	program->WriteOut(MSG_Get("SHELL_CMD_IMGMAKE_CREATED"),
	                  s.filename.c_str(),
	                  geom.cyl,
	                  geom.heads,
	                  geom.sectors);
	return true;
}

} // namespace ImgmakeCommand

void IMGMAKE::Run()
{
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("SHELL_CMD_IMGMAKE_HELP"));
		output.Display();
		return;
	}

	const auto args = cmd->GetArguments();
	auto result     = ImgmakeCommand::ParseArgs(args);

	if (std::holds_alternative<ImgmakeCommand::ErrorType>(result)) {
		auto err = std::get<ImgmakeCommand::ErrorType>(result);
		if (err != ImgmakeCommand::ErrorType::None) {
			// Map error to message
			ImgmakeCommand::notify_warning("SHELL_CMD_IMGMAKE_INVALID_ARGS");
		}
		// If error is None, it means empty args, just show help logic
		// implicitly or exit
		if (err == ImgmakeCommand::ErrorType::None) {
			MoreOutputStrings output(*this);
			output.AddString(MSG_Get("SHELL_CMD_IMGMAKE_HELP"));
			output.Display();
		}
	} else {
		auto settings = std::get<ImgmakeCommand::CommandSettings>(result);
		ImgmakeCommand::Execute(this, settings);
	}
}

void IMGMAKE::AddMessages()
{
	MSG_Add("SHELL_CMD_IMGMAKE_HELP",
	        "Creates a disk image.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]imgmake[reset] [color=light-cyan]file[reset] [color=white]-t type[reset] [options]\n"
	        "\n"
	        "Options:\n"
	        "  [color=white]-t type[reset]      Disk type: [color=light-cyan]fd_1440, fd_720, hd_250, hd_1gig[reset]...\n"
	        "                  or [color=light-cyan]hd[reset] (requires -size or -chs).\n"
	        "  [color=white]-size x[reset]      Size in MB (for hd type).\n"
	        "  [color=white]-chs c,h,s[reset]   Geometry (Cylinders, Heads, Sectors).\n"
	        "  [color=white]-nofs[reset]        Do not format the filesystem (raw image).\n"
	        "  [color=white]-bat[reset]         Create a batch file to mount the image.\n"
	        "  [color=white]-label name[reset]  Volume label.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]imgmake[reset] [color=light-cyan]floppy.img[reset] -t fd_1440\n"
	        "  [color=light-green]imgmake[reset] [color=light-cyan]hdd.img[reset] -t hd -size 500 -bat");

	MSG_Add("SHELL_CMD_IMGMAKE_INVALID_ARGS",
	        "Invalid arguments. Use /? for help.");
	MSG_Add("SHELL_CMD_IMGMAKE_MISSING_SIZE",
	        "You must specify -size or -chs for custom hard disks.");
	MSG_Add("SHELL_CMD_IMGMAKE_INVALID_TYPE",
	        "Unknown disk type: [color=light-cyan]%s[reset]");
	MSG_Add("SHELL_CMD_IMGMAKE_BAD_SIZE", "Invalid disk size calculated.");
	MSG_Add("SHELL_CMD_IMGMAKE_FILE_EXISTS",
	        "File [color=light-cyan]%s[reset] already exists. Use -force to overwrite.");
	MSG_Add("SHELL_CMD_IMGMAKE_CANNOT_WRITE",
	        "Cannot open file [color=light-cyan]%s[reset] for writing.");
	MSG_Add("SHELL_CMD_IMGMAKE_SPACE_ERROR",
	        "Disk full or cannot allocate image size.");
	MSG_Add("SHELL_CMD_IMGMAKE_CREATED",
	        "Created [color=light-cyan]%s[reset] [CHS: %u, %u, %u]");
}