// SPDX-FileCopyrightText: 2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "imgmake.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <map>
#include <memory>
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

namespace fs = std::filesystem;

struct DiskGeometry {
	uint32_t cylinders;
	uint32_t heads;
	uint32_t sectors;
	uint8_t media_descriptor;
	uint16_t root_entries;
	uint32_t sectors_per_fat;
	uint16_t sectors_per_cluster;
	uint64_t total_size_kb;
	bool is_floppy;
};

// Maps preset names (e.g., "fd_1440") to geometries
const std::map<std::string, DiskGeometry> GeometryPresets = {
        { "fd_160kb",     {40, 1, 8, 0xFE, 64, 1, 1, 160, true}},
        { "fd_180kb",     {40, 1, 9, 0xFC, 64, 2, 1, 180, true}},
        { "fd_320kb",    {40, 2, 8, 0xFF, 112, 1, 2, 320, true}},
        { "fd_360kb",    {40, 2, 9, 0xFD, 112, 2, 2, 360, true}},
        { "fd_720kb",    {80, 2, 9, 0xF9, 112, 3, 2, 720, true}},
        {"fd_1200kb",  {80, 2, 15, 0xF9, 224, 7, 1, 1200, true}},
        {"fd_1440kb",  {80, 2, 18, 0xF0, 224, 9, 1, 1440, true}},
        {"fd_2880kb",  {80, 2, 36, 0xF0, 240, 9, 2, 2880, true}},
        // HD presets
        { "hd_250mb",  {489, 16, 63, 0xF8, 512, 0, 0, 0, false}},
        { "hd_520mb", {1023, 16, 63, 0xF8, 512, 0, 0, 0, false}},
        {   "hd_1gb", {1023, 32, 63, 0xF8, 512, 0, 0, 0, false}},
        {   "hd_2gb", {1023, 64, 63, 0xF8, 512, 0, 0, 0, false}}
};

// Return a 3-byte array [heads, sectors|cylinders_high, cylinders_low]
std::array<uint8_t, 3> lba_to_chs(int64_t lba, int max_cylinders, int max_heads,
                                  int max_sectors)
{
	int cylinders = 0, heads = 0, sectors = 0;
	constexpr int MaxLegacyCylinders = 1023;

	if (lba < static_cast<int64_t>(max_cylinders) * max_heads * max_sectors) {
		sectors      = static_cast<int>((lba % max_sectors) + 1);
		int64_t temp = lba / max_sectors;
		heads        = static_cast<int>(temp % max_heads);
		cylinders    = static_cast<int>(temp / max_heads);
		// Clamp for legacy CHS
		cylinders = std::min(cylinders, MaxLegacyCylinders);
	} else {
		cylinders = MaxLegacyCylinders;
		heads     = max_heads - 1;
		sectors   = max_sectors;
	}

	return {static_cast<uint8_t>(heads),
	        static_cast<uint8_t>((sectors & 0x3f) | ((cylinders >> 2) & 0xc0)),
	        static_cast<uint8_t>(cylinders & 0xff)};
}

// Write a string into a fixed-width buffer and pad with spaces
void write_padded_string(uint8_t* dest, const std::string& str, int length)
{
	std::string temp = str;
	temp.resize(length, ' ');
	std::copy(temp.begin(), temp.end(), dest);
}

struct FileCloser {
	void operator()(FILE* fp) const
	{
		if (fp) {
			std::fclose(fp);
		}
	}
};
using FilePtr = std::unique_ptr<FILE, FileCloser>;

// Generate a Volume Serial Number based on the current date/time.
// High Word = (Seconds << 8) + Minutes + Hours
// Low Word  = (Year) + (Month << 8) + Day
// Result    = (High Word << 16) + Low Word
uint32_t generate_volume_serial()
{
	auto now         = std::time(nullptr);
	auto* local_time = std::localtime(&now);

	// Fallback
	if (!local_time) {
		return 0xDEADBEEF;
	}

	// tm_year is years since 1900.
	uint32_t year  = static_cast<uint32_t>(local_time->tm_year + 1900);
	uint32_t month = static_cast<uint32_t>(local_time->tm_mon + 1);
	uint32_t day   = static_cast<uint32_t>(local_time->tm_mday);
	uint32_t hour  = static_cast<uint32_t>(local_time->tm_hour);
	uint32_t min   = static_cast<uint32_t>(local_time->tm_min);
	uint32_t sec   = static_cast<uint32_t>(local_time->tm_sec);

	// DOS Algorithm
	uint32_t lo = year + (month << 8) + day;
	uint32_t hi = (sec << 8) + min + hour;

	return (hi << 16) + lo;
}

constexpr auto DefaultRootEntries = 512;

} // namespace

#pragma pack(push, 1)
struct FatBootSector {
	// Common Bios Parameter Block (BPB) (Bytes 0x00 - 0x23)
	uint8_t jump[3];
	char oem_name[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors;
	uint8_t fat_copies;
	uint16_t root_entries;
	uint16_t total_sectors_16;
	uint8_t media_descriptor;
	uint16_t sectors_per_fat_16;
	uint16_t sectors_per_track;
	uint16_t heads;
	uint32_t hidden_sectors;
	uint32_t total_sectors_32;

	union {
		// FAT12 / FAT16 Extended BPB (Bytes 0x24 - 0x1FE)
		struct {
			uint8_t drive_number;
			uint8_t reserved1;
			uint8_t boot_signature;
			uint32_t serial_number;
			char label[11];
			char fs_type[8];
			uint8_t boot_code[448];
		} fat16;

		// FAT32 Extended BPB (Bytes 0x24 - 0x1FE)
		struct {
			uint32_t sectors_per_fat_32;
			uint16_t ext_flags;
			uint16_t fs_version;
			uint32_t root_cluster;
			uint16_t fs_info_sector;
			uint16_t backup_boot_sector;
			uint8_t reserved[12];
			uint8_t drive_number;
			uint8_t reserved1;
			uint8_t boot_signature;
			uint32_t serial_number;
			char label[11];
			char fs_type[8];
			uint8_t boot_code[420];
		} fat32;
	} ext;

	uint8_t signature[2];
};
#pragma pack(pop)

// Sanity check to ensure the compiler packed it correctly
static_assert(sizeof(FatBootSector) == 512,
              "FAT BootSector must be exactly 512 bytes");

// Standard DOS Directory Entry (32 bytes)
#pragma pack(push, 1)
struct DirectoryEntry {
	// Filename is in 8.3 format and padded with spaces
	char filename[11];
	// Attributes: 0x08 = Volume Label, 0x10 = Subdir, etc.
	uint8_t attributes;
	// Reserved for Windows NT / OS/2
	uint8_t reserved;
	uint8_t create_time_tenth;
	uint16_t create_time;
	uint16_t create_date;
	uint16_t last_access_date;
	// FAT32 only
	uint16_t first_cluster_high;
	uint16_t write_time;
	uint16_t write_date;
	uint16_t first_cluster_low;
	uint32_t file_size;
};
#pragma pack(pop)

static_assert(sizeof(DirectoryEntry) == 32, "DirectoryEntry must be 32 bytes");

#pragma pack(push, 1)
struct Fat32FsInfo {
	// Offset 0x00: Always 0x41615252
	uint32_t lead_signature;
	// Offset 0x04: Huge gap of zeros
	uint8_t reserved1[480];
	// Offset 0x1E4 (484): Always 0x61417272
	uint32_t struct_signature;
	// Offset 0x1E8 (488): Last known free cluster count
	uint32_t free_count;
	// Offset 0x1EC (492): Hint for next free cluster
	uint32_t next_free;
	// Offset 0x1F0: Small gap of zeros
	uint8_t reserved2[12];
	// Offset 0x1FC (508): Always 0xAA550000
	uint32_t trail_signature;
};
#pragma pack(pop)

static_assert(sizeof(Fat32FsInfo) == 512, "Fat32FsInfo must be exactly 512 bytes");

constexpr uint32_t Fat32LeadSignature   = 0x41615252;
constexpr uint32_t Fat32StructSignature = 0x61417272;
constexpr uint32_t Fat32TrailSignature  = 0xAA550000;

// Constants for FAT markers
namespace FatMarkers {
// End of Chain (EOC) markers indicate the end of a file/chain.
constexpr uint32_t Fat32Eoc = 0x0FFFFFFF;
constexpr uint16_t Fat16Eoc = 0xFFFF;

// The first entry usually contains the Media Descriptor in the low byte.
// The upper bits are set to 1.
constexpr uint32_t Fat32MediaMask = 0x0FFFFFF0;
constexpr uint16_t Fat16MediaMask = 0xFF00;
} // namespace FatMarkers

namespace BootCode {

// Assembly code to print "Disk not bootable." message:
// clang-format off
constexpr uint8_t Printer[] = {
    // Stack Setup
    0xFA,             // 0:  CLI              ; Disable interrupts
    0x31, 0xC0,       // 1:  XOR AX, AX       ; AX = 0
    0x8E, 0xD0,       // 3:  MOV SS, AX       ; SS = 0
    0xBC, 0x00, 0x7C, // 5:  MOV SP, 7C00     ; SP = 7C00 (Grow down)
    0xFB,             // 8:  STI              ; Enable interrupts

    // Video Mode Setup
    0xB8, 0x03, 0x00, // 9:  MOV AX, 0003h    ; AH=00 (Set Mode), AL=03 (80x25 Color)
    0xCD, 0x10,       // 12: INT 10h

    // Segment Setup
    0x31, 0xC0,       // 14: XOR AX, AX       ; AX = 0
    0x8E, 0xD8,       // 16: MOV DS, AX       ; DS = 0
    0x8E, 0xC0,       // 18: MOV ES, AX       ; ES = 0

    // Print Loop
    0xBE, 0x00, 0x00, // 20: MOV SI, [addr]   ; (Patched later at offset 21)
    0xFC,             // 23: CLD              ; Clear Direction Flag
    
    // .loop:
    0xAC,             // 24: LODSB            ; AL = [SI++]
    0x08, 0xC0,       // 25: OR AL, AL
    0x74, 0x05,       // 27: JZ +5            ; -> HANG

    0xB4, 0x0E,       // 29: MOV AH, 0E       ; Teletype
    0x31, 0xDB,       // 31: XOR BX, BX       ; Page 0, Color 0
    0xCD, 0x10,       // 33: INT 10           ; Print
    0xEB, 0xF3,       // 35: JMP -13          ; -> .loop

    // .hang:
    0xF4,             // 37: HLT              ; Save host CPU usage
    0xEB, 0xFD,       // 38: JMP -3           ; Infinite HLT Loop

	// --- Data Section (Offset 40 / 0x28) ---
    
    // Leading Newline
    0x0D, 0x0A,

    // Row 1:   ▛▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▜
    0x20, 0xDB, 
    0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 
    0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF, 0xDF,
    0xDB, 0x0D, 0x0A,

    // Row 2:   ▌ ________________ ┌┐ ▐
    0x20, 0xDD, 0x20,
    0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F,
    0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F,
    0x20, 0xDA, 0xBF, 0x20, 0xDE, 0x0D, 0x0A,

    // Row 3:   ▌ ________________ └┘ ▐
    0x20, 0xDD, 0x20,
    0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F,
    0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F, 0x5F,
    0x20, 0xC0, 0xD9, 0x20, 0xDE, 0x0D, 0x0A,

    // Row 4:   ▌                     ▐
    0x20, 0xDD,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0xDE, 0x0D, 0x0A,

    // Row 5:   ▙▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▟
    0x20, 0xDB,
    0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC,
    0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC,
    0xDB, 0x0D, 0x0A,

	// Spacer Newline
    0x0D, 0x0A,
	
    // Message String
    0x0D, 0x0A,       // CR LF
    'D', 'i', 's', 'k', ' ', 'n', 'o', 't', ' ',
    'b', 'o', 'o', 't', 'a', 'b', 'l', 'e', '.', 
    0x0D, 0x0A, 0x00  // CR LF Null
};
// clang-format on

// Offsets where the boot code starts in the 512-byte sector
constexpr uint16_t OffsetFAT16 = 0x3E;
constexpr uint16_t OffsetFAT32 = 0x5A;

// BIOS loads boot sector at 0x7C00.
// The string starts at offset 40 inside the Printer array.
constexpr auto StringOffsetInCode = 40;
// The SI patch is at offset 21 in the code.
constexpr auto PatchOffsetInCode = 21;
constexpr auto PhysicalAddress   = 0x7C00;

} // namespace BootCode
namespace FAT_Markers {

// FAT Limits (Max clusters)
constexpr uint32_t MaxClustersFAT12 = 4084;
constexpr uint32_t MaxClustersFAT16 = 65524;
// FAT32 theoretically can go higher, limit for safety/compatibility
} // namespace FAT_Markers

namespace ImgmakeCommand {

enum class ErrorType { None, UnknownArgument, MissingArgument, InvalidValue };

struct CommandSettings {
	std::filesystem::path filename = {};
	std::string type               = {};
	std::string label              = {};

	int64_t size_bytes = 0;
	int cylinders      = 0;
	int heads          = 0;
	int sectors        = 0;

	int fat_type            = -1;
	int sectors_per_cluster = 0;
	int root_entries        = DefaultRootEntries;
	int fat_copies          = 2;

	bool force     = false;
	bool no_format = false;
	bool use_chs   = false;
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

ParseResult parse_args(const std::vector<std::string>& args)
{
	CommandSettings settings;

	if (args.empty()) {
		return ErrorType::None;
	}

	// First argument is usually the filename (unless it starts with -)
	int start_idx = 0;
	if (args[0][0] != '-') {
		settings.filename = args[0];
		start_idx         = 1;
	} else {
		settings.filename = "IMGMAKE.IMG";
	}

	int arg_size = static_cast<int>(args.size());
	for (int i = start_idx; i < arg_size; ++i) {
		std::string arg = args[i];
		lowcase(arg);

		if (arg == "-t") {
			if (i + 1 >= arg_size) {
				return ErrorType::MissingArgument;
			}
			settings.type = args[++i];
			lowcase(settings.type);
		} else if (arg == "-size") {
			if (i + 1 >= arg_size) {
				return ErrorType::MissingArgument;
			}
			const auto& size_str = args[++i];
			auto size_mb         = parse_int(size_str, 10);
			if (!size_mb) {
				return ErrorType::InvalidValue;
			}
			settings.size_bytes = static_cast<uint64_t>(*size_mb) *
			                      BytesPerMegabyte;
		} else if (arg == "-chs") {
			if (i + 1 >= arg_size) {
				return ErrorType::MissingArgument;
			}
			const auto& chs_str = args[++i];
			auto parts          = split_with_empties(chs_str, ',');

			if (parts.size() != 3) {
				return ErrorType::InvalidValue;
			}
			auto cylinders = parse_int(parts[0], 10);
			auto heads     = parse_int(parts[1], 10);
			auto sectors   = parse_int(parts[2], 10);

			if (!cylinders || !heads || !sectors) {
				return ErrorType::InvalidValue;
			}

			settings.cylinders = static_cast<uint32_t>(*cylinders);
			settings.heads     = static_cast<uint32_t>(*heads);
			settings.sectors   = static_cast<uint32_t>(*sectors);
			settings.use_chs   = true;
		} else if (arg == "-label") {
			if (i + 1 >= arg_size) {
				return ErrorType::MissingArgument;
			}
			settings.label = args[++i];
		} else if (arg == "-fat") {
			if (i + 1 >= arg_size) {
				return ErrorType::MissingArgument;
			}
			if (auto f = parse_int(args[++i], 10)) {
				settings.fat_type = *f;
			}
		} else if (arg == "-spc") {
			if (i + 1 >= arg_size) {
				return ErrorType::MissingArgument;
			}
			if (auto s = parse_int(args[++i], 10)) {
				settings.sectors_per_cluster = *s;
			}
		} else if (arg == "-force") {
			settings.force = true;
		} else if (arg == "-noformat") {
			settings.no_format = true;
		} else {
			return ErrorType::UnknownArgument;
		}
	}

	if (settings.type.empty()) {
		return ErrorType::MissingArgument;
	}

	return settings;
}

bool execute(Program* program, CommandSettings& command_settings)
{
	DiskGeometry disk_geometry     = {};
	int64_t total_size             = 0;
	constexpr auto SectorSize      = 512;
	constexpr int64_t SectorsPerMB = (1024 * 1024) / SectorSize;
	constexpr int64_t SectorsPerGB = (1024 * 1024 * 1024) / SectorSize;

	constexpr auto PartitionFlagActive = 0x80;
	constexpr auto PartitionTypeFAT12  = 0x01;
	// For partitions smaller than 32MB
	constexpr auto PartitionTypeFAT16_Small = 0x04;
	// For partitions larger than 32MB but not LBA
	constexpr auto PartitionTypeFAT16B   = 0x06;
	constexpr auto PartitionTypeFAT16LBA = 0x0E;
	constexpr auto PartitionTypeFAT32LBA = 0x0C;
	constexpr auto PartitionEntry1Offset = 0x1BE;

	// Get DOS Version
	DOS_PSP psp(dos.psp());
	auto dos_version_major = psp.GetVersionMajor();
	auto dos_version_minor = psp.GetVersionMinor();

	// Determine Geometry and Size
	if (auto it = GeometryPresets.find(command_settings.type);
	    it != GeometryPresets.end()) {
		disk_geometry = it->second;
		total_size    = static_cast<int64_t>(disk_geometry.cylinders) *
		             disk_geometry.heads * disk_geometry.sectors * SectorSize;
	} else if (command_settings.type == "hd") {
		disk_geometry.media_descriptor = 0xF8;
		disk_geometry.is_floppy        = false;

		if (command_settings.use_chs) {
			disk_geometry.cylinders = command_settings.cylinders;
			disk_geometry.heads     = command_settings.heads;
			disk_geometry.sectors   = command_settings.sectors;
			total_size = static_cast<int64_t>(disk_geometry.cylinders) *
			             disk_geometry.heads *
			             disk_geometry.sectors * SectorSize;
		} else if (command_settings.size_bytes > 0) {
			total_size            = command_settings.size_bytes;
			int64_t total_sectors = total_size / SectorSize;

			// Calculate CHS from Size

			// Legacy BIOS Int 13h limit: 1023 cylinders, 16 heads,
			// 63 sectors (~528 MB).
			disk_geometry.heads   = 16;
			disk_geometry.sectors = 63;

			// To support larger disks, we must increase the Head
			// count to keep the Cylinder count below 1024.

			// > 528 MB: Shift to 64 Heads (Max ~2.1 GB)
			if (total_size > 528 * BytesPerMegabyte) {
				disk_geometry.heads = 64;
			}
			// > 1 GB: Shift to 128 Heads (Max ~4.2 GB)
			if (total_size > 1 * BytesPerGigabyte) {
				disk_geometry.heads = 128;
			}
			// > 4 GB: Shift to 255 Heads (Max ~8.4 GB)
			if (static_cast<int64_t>(total_size) > 4 * BytesPerGigabyte) {
				disk_geometry.heads = 255;
			}

			// Calculate Cylinders based on the chosen Heads/Sectors
			disk_geometry.cylinders = static_cast<uint32_t>(
			        total_sectors /
			        (disk_geometry.heads * disk_geometry.sectors));

			// Hard clamp for safety: Standard Int 13h cannot
			// address > 1023 cylinders. Or as Dosbox-X puts it:
			// "1024 toasts the stack"
			uint32_t MaxInt13Cylinders = 1023;
			disk_geometry.cylinders =
			        std::min(disk_geometry.cylinders,
			                 static_cast<uint32_t>(MaxInt13Cylinders));
		} else {
			notify_warning("SHELL_CMD_IMGMAKE_MISSING_SIZE");
			return false;
		}
	} else {
		notify_warning("SHELL_CMD_IMGMAKE_INVALID_TYPE",
		               command_settings.type.c_str());
		return false;
	}

	if (total_size == 0) {
		notify_warning("SHELL_CMD_IMGMAKE_BAD_SIZE");
		return false;
	}

	// Check File Existence
	std::error_code ec;
	if (fs::exists(command_settings.filename, ec) && !command_settings.force) {
		notify_warning("SHELL_CMD_IMGMAKE_FILE_EXISTS",
		               command_settings.filename.string().c_str());
		return false;
	}

	// Create file (truncate if exists)
	{
		FilePtr temp_fp(
		        std::fopen(command_settings.filename.string().c_str(), "wb"));
		if (!temp_fp) {
			notify_warning("SHELL_CMD_IMGMAKE_CANNOT_WRITE",
			               command_settings.filename.string().c_str());
			return false;
		}
		// Closes on scope exit
	}

	// Expand file to avoid fseek/long limits
	try {
		fs::resize_file(command_settings.filename, total_size);
	} catch (const fs::filesystem_error&) {
		notify_warning("SHELL_CMD_IMGMAKE_SPACE_ERROR");
		fs::remove(command_settings.filename, ec);
		return false;
	}

	FilePtr fs(std::fopen(command_settings.filename.string().c_str(), "rb+"));
	if (!fs) {
		notify_warning("SHELL_CMD_IMGMAKE_CANNOT_WRITE",
		               command_settings.filename.string().c_str());
		return false;
	}

	if (command_settings.no_format) {
		return true;
	}

	std::array<uint8_t, 512> buffer = {};
	int64_t boot_sector_position    = 0;

	// Write Partition Table (MBR) for Hard Disks
	if (!disk_geometry.is_floppy) {
		// Hard disks usually start partition at track 1
		// (head 0, sector 1 is MBR)
		boot_sector_position = disk_geometry.sectors;
	}

	// Calculate volume sectors (Total - Hidden/MBR gap)
	int64_t volume_sectors = (total_size / SectorSize) -
	                         static_cast<int64_t>(boot_sector_position);
	int fat_bits = command_settings.fat_type;

	// Auto-detect FAT bits for the configured DOS Version
	if (fat_bits == -1) {
		constexpr int64_t SectorsPerMB = (1024 * 1024) / SectorSize;
		constexpr int64_t SectorsPerGB = (1024 * 1024 * 1024) / SectorSize;
		// Thresholds
		// FAT12 Limit: ~16 MB
		// Maximum FAT12 clusters (4085) * Max standard FAT12 cluster
		// size (8 sectors / 4KB)
		constexpr auto FAT12MaxClusters = 4085;
		constexpr auto FAT12MaxSPC      = 8;
		constexpr auto FAT12MaxSectors = FAT12MaxClusters * FAT12MaxSPC;

		// FAT16 Efficiency Limit: 2047 MB
		// At 512MB, FAT16 requires 16KB clusters, which is inefficient.
		// Windows 98/DOS 7.1 switch to FAT32 here by default.
		// However, dosbox-staging does not support FAT32 yet, so for
		// now we accept this inefficiency in favour of compatibility.
		constexpr int64_t FAT16EfficiencyLimit = 2047 * SectorsPerMB;

		// FAT16 Hard Limit: 2 GB
		// The mathematical limit of unsigned 16-bit math + 32KB clusters.
		constexpr int64_t FAT16HardLimit = 2 * SectorsPerGB;

		// Check DOS Version (User provided variables)
		// DOS 7.10 is required for FAT32 (Windows 95 OSR2 / Windows 98)
		bool dos_supports_fat32 = (dos_version_major > 7) ||
		                          (dos_version_major == 7 &&
		                           dos_version_minor >= 10);

		if (disk_geometry.is_floppy) {
			fat_bits = 12;
		} else if (volume_sectors < FAT12MaxSectors) {
			fat_bits = 12;
		} else {
			// Decision between FAT16 and FAT32
			if (volume_sectors >= FAT16HardLimit) {
				// > 2GB: Must be FAT32
				fat_bits = 32;
			} else if (volume_sectors >= FAT16EfficiencyLimit &&
			           dos_supports_fat32) {
				// 512MB - 2GB: Use FAT32 if DOS supports it
				fat_bits = 32;
			} else {
				// < 512MB OR Old DOS: Use FAT16
				fat_bits = 16;
			}
		}
	}

	// Write Partition Table (MBR)
	if (!disk_geometry.is_floppy) {
		buffer = {};
		std::copy(std::begin(BootCode::Printer),
		          std::end(BootCode::Printer),
		          buffer.begin());

		// Patch MBR Code
		uint16_t mbr_string_addr = BootCode::PhysicalAddress +
		                           BootCode::StringOffsetInCode;
		host_writew(buffer.data() + BootCode::PatchOffsetInCode,
		            mbr_string_addr);

		// Partition 1 Entry
		uint8_t* partition = buffer.data() + PartitionEntry1Offset;
		// Mark partition as active
		partition[0] = PartitionFlagActive;

		auto start_chs = lba_to_chs(boot_sector_position,
		                            disk_geometry.cylinders,
		                            disk_geometry.heads,
		                            disk_geometry.sectors);
		std::copy(start_chs.begin(), start_chs.end(), partition + 1);

		// Determine Partition Type ID
		// Note: Logic is similar but not identical to FAT bits
		// detection above
		if (fat_bits == 32) {
			partition[4] = PartitionTypeFAT32LBA;
		} else if (fat_bits == 12) {
			partition[4] = PartitionTypeFAT12;
		} else {
			// We are in FAT16 territory (16MB to 2GB)
			if (volume_sectors < 65536) {
				partition[4] = PartitionTypeFAT16_Small;
			} else if (total_size > 528 * BytesPerMegabyte) {
				// 528MB to 2GB: suggest LBA-aware FAT16
				partition[4] = PartitionTypeFAT16LBA;
			} else {
				// 32MB to 528MB: Use Standard FAT16
				partition[4] = PartitionTypeFAT16B;
			}
		}

		// End CHS
		auto end_chs = lba_to_chs((total_size / 512) - 1,
		                          disk_geometry.cylinders,
		                          disk_geometry.heads,
		                          disk_geometry.sectors);
		std::copy(end_chs.begin(), end_chs.end(), partition + 5);

		// LBA Start & Size
		host_writed(partition + 8,
		            static_cast<uint32_t>(boot_sector_position));
		host_writed(partition + 12, static_cast<uint32_t>(volume_sectors));

		buffer[510] = 0x55;
		buffer[511] = 0xAA;
		std::fwrite(buffer.data(), 1, buffer.size(), fs.get());
	}

	// Move to Boot Sector
	std::fseek(fs.get(),
	           static_cast<long>(boot_sector_position * SectorSize),
	           SEEK_SET);
	buffer            = {};
	auto* boot_sector = reinterpret_cast<FatBootSector*>(buffer.data());

	// Bios Parameter Block (BPB) Construction

	// JMP Instruction
	boot_sector->jump[0] = 0xEB;
	// Jump offset depends on BPB size
	boot_sector->jump[1] = (fat_bits == 32) ? 0x58 : 0x3C;
	boot_sector->jump[2] = 0x90;

	// OEM Name
	write_padded_string(reinterpret_cast<uint8_t*>(boot_sector->oem_name),
	                    "DOSBOX-S",
	                    8);
	host_writew(reinterpret_cast<uint8_t*>(&boot_sector->bytes_per_sector),
	            static_cast<uint16_t>(SectorSize));

	// Calculate Sectors Per Cluster (SPC) Safely
	uint32_t sectors_per_cluster = (command_settings.sectors_per_cluster > 0)
	                                     ? command_settings.sectors_per_cluster
	                                     : 0;

	if (sectors_per_cluster == 0) {

		// Beyond 32,680 sectors (~16MB) we switch from FAT12 to FAT16.
		constexpr auto FAT12_Limit_Sectors = 32680;

		if (fat_bits == 32) {
			// Win98 Standard alignments:
			// > 32GB -> 32KB Cluster (64 sectors)
			// > 16GB -> 16KB Cluster (32 sectors)
			// >  8GB ->  8KB Cluster (16 sectors)
			// <  8GB ->  4KB Cluster ( 8 sectors)
			if (volume_sectors >= 32 * SectorsPerGB) {
				sectors_per_cluster = 64;
			} else if (volume_sectors >= 16 * SectorsPerGB) {
				sectors_per_cluster = 32;
			} else if (volume_sectors >= 8 * SectorsPerGB) {
				sectors_per_cluster = 16;
			} else {
				sectors_per_cluster = 8;
			}
		} else {
			// FAT12/16 efficiency optimizations
			// If > 512MB, we are technically pushing FAT16 limits
			if (volume_sectors >= 512 * SectorsPerMB) {
				// Should usually be FAT32 here, but just in
				// case (128KB)
				sectors_per_cluster = 64;
			} else if (volume_sectors > FAT12_Limit_Sectors) {
				// HD Default (2KB)
				sectors_per_cluster = 4;
			} else {
				// Floppy/Small disk (512B)
				sectors_per_cluster = 1;
			}
		}
	}

	// FAT Limit Check & SPC Adjustment
	auto reserved_sectors_temp = (fat_bits == 32) ? 32 : 1;
	auto root_dir_sectors_temp = 0;

	if (fat_bits != 32) {
		auto root             = (disk_geometry.root_entries > 0)
		                              ? disk_geometry.root_entries
		                              : DefaultRootEntries;
		root_dir_sectors_temp = static_cast<uint32_t>(
		        ((static_cast<uint32_t>(root) * 32) + (SectorSize - 1)) /
		        SectorSize);
	}

	// Calculate approximate clusters to see if we overflow the FAT type
	// This loop increases SPC until the cluster count is safe.
	constexpr auto MaxFatLimit = 0x0FFFFFFF;
	auto fat_limit = (fat_bits == 12) ? FAT_Markers::MaxClustersFAT12
	               : (fat_bits == 16) ? FAT_Markers::MaxClustersFAT16
	                                  : MaxFatLimit;

	constexpr auto MaxSectorsPerCluster = 128;
	while (sectors_per_cluster < MaxSectorsPerCluster) {
		// Rough estimate of data area
		auto data_sec = volume_sectors - reserved_sectors_temp -
		                root_dir_sectors_temp;
		// We subtract rough FAT size (very small compared to volume) or
		// ignore for rough check
		auto clusters = data_sec / sectors_per_cluster;

		// Quit if it fits
		if (clusters < fat_limit) {
			break;
		}
		// Otherwise, double SPC and try again
		sectors_per_cluster <<= 1;
	}

	boot_sector->sectors_per_cluster = static_cast<uint8_t>(sectors_per_cluster);

	// Remaining BPB
	auto reserved_sectors = (fat_bits == 32) ? 32 : 1;
	host_writew(reinterpret_cast<uint8_t*>(&boot_sector->reserved_sectors),
	            static_cast<uint16_t>(reserved_sectors));
	boot_sector->fat_copies = static_cast<uint8_t>(command_settings.fat_copies);

	auto root_ent = (fat_bits == 32) ? 0
	                                 : (disk_geometry.root_entries > 0
	                                            ? disk_geometry.root_entries
	                                            : DefaultRootEntries);
	host_writew(reinterpret_cast<uint8_t*>(&boot_sector->root_entries),
	            static_cast<uint16_t>(root_ent));

	if (volume_sectors < 65536 && fat_bits != 32) {
		host_writew(reinterpret_cast<uint8_t*>(&boot_sector->total_sectors_16),
		            static_cast<uint16_t>(volume_sectors));
	} else {
		host_writew(reinterpret_cast<uint8_t*>(&boot_sector->total_sectors_16),
		            0);
	}

	boot_sector->media_descriptor = static_cast<uint8_t>(
	        disk_geometry.media_descriptor);

	// Calculate Exact FAT Size
	int fat_size_sectors = 0;
	int root_dir_sectors = ((root_ent * 32) + (SectorSize - 1)) / SectorSize;

	// We need to solve for FAT Size:
	// Data_Sectors = Total - Reserved - (Fat_Copies * Fat_Size) - Root_Dir
	// Total_Clusters = Data_Sectors / SPC
	auto tmp_data_sectors = volume_sectors - reserved_sectors - root_dir_sectors;
	auto tmp_clusters = tmp_data_sectors / sectors_per_cluster;

	// The FAT must track every data cluster plus two reserved entries
	// at the start (Index 0: Media Descriptor, Index 1: EOC/Flags).
	constexpr auto ReservedFATEntries = 2;
	auto total_entries                = tmp_clusters + ReservedFATEntries;

	int64_t fat_size_bytes = 0;

	if (fat_bits == 12) {
		// FAT12: 12 bits per entry = 1.5 bytes.
		// Calculation: Entries * 1.5  -->  (Entries * 3) / 2
		fat_size_bytes = (total_entries * 3) / 2;
	} else if (fat_bits == 16) {
		// FAT16: 16 bits per entry = 2 bytes
		fat_size_bytes = total_entries * 2;
	} else {
		// FAT32: 32 bits per entry = 4 bytes
		fat_size_bytes = total_entries * 4;
	}

	// Convert bytes to sectors, rounding up (Ceiling Division).
	// Formula: (Numerator + Denominator - 1) / Denominator
	fat_size_sectors = static_cast<uint32_t>(
	        (fat_size_bytes + SectorSize - 1) / SectorSize);

	if (fat_bits != 32) {
		host_writew(reinterpret_cast<uint8_t*>(&boot_sector->sectors_per_fat_16),
		            static_cast<uint16_t>(fat_size_sectors));
	} else {
		host_writew(reinterpret_cast<uint8_t*>(&boot_sector->sectors_per_fat_16),
		            0);
	}

	host_writew(reinterpret_cast<uint8_t*>(&boot_sector->sectors_per_track),
	            static_cast<uint16_t>(disk_geometry.sectors));
	host_writew(reinterpret_cast<uint8_t*>(&boot_sector->heads),
	            static_cast<uint16_t>(disk_geometry.heads));
	host_writed(reinterpret_cast<uint8_t*>(&boot_sector->hidden_sectors),
	            static_cast<uint32_t>(boot_sector_position));
	host_writed(reinterpret_cast<uint8_t*>(&boot_sector->total_sectors_32),
	            (fat_bits == 32 || volume_sectors >= 65536)
	                    ? static_cast<uint32_t>(volume_sectors)
	                    : 0);

	// Extended BPB & Fallback Boot Code
	if (fat_bits == 32) {
		host_writed(reinterpret_cast<uint8_t*>(
		                    &boot_sector->ext.fat32.sectors_per_fat_32),
		            fat_size_sectors);
		host_writew(reinterpret_cast<uint8_t*>(&boot_sector->ext.fat32.ext_flags),
		            0);
		host_writew(reinterpret_cast<uint8_t*>(&boot_sector->ext.fat32.fs_version),
		            0);
		host_writed(reinterpret_cast<uint8_t*>(
		                    &boot_sector->ext.fat32.root_cluster),
		            2);
		host_writew(reinterpret_cast<uint8_t*>(
		                    &boot_sector->ext.fat32.fs_info_sector),
		            1);
		host_writew(reinterpret_cast<uint8_t*>(
		                    &boot_sector->ext.fat32.backup_boot_sector),
		            6);

		boot_sector->ext.fat32.boot_signature = 0x29;

		// Generate volume serial number
		host_writed(reinterpret_cast<uint8_t*>(
		                    &boot_sector->ext.fat32.serial_number),
		            generate_volume_serial());

		write_padded_string(reinterpret_cast<uint8_t*>(
		                            boot_sector->ext.fat32.label),
		                    "NO NAME",
		                    11);
		write_padded_string(reinterpret_cast<uint8_t*>(
		                            boot_sector->ext.fat32.fs_type),
		                    "FAT32",
		                    8);

		// Copy fallback boot code
		std::copy(std::begin(BootCode::Printer),
		          std::end(BootCode::Printer),
		          boot_sector->ext.fat32.boot_code);
		// Patch MOV SI address
		int string_addr = BootCode::PhysicalAddress + BootCode::OffsetFAT32 +
		                  BootCode::StringOffsetInCode;
		host_writew(reinterpret_cast<uint8_t*>(boot_sector->ext.fat32.boot_code +
		                                       BootCode::PatchOffsetInCode),
		            static_cast<uint16_t>(string_addr));
	} else {
		constexpr auto BootSectorDriveNumberFloppy   = 0x00;
		constexpr auto BootSectorDriveNumberHardDisk = 0x80;
		boot_sector->ext.fat16.drive_number = (disk_geometry.is_floppy)
		                                            ? BootSectorDriveNumberFloppy
		                                            : BootSectorDriveNumberHardDisk;
		boot_sector->ext.fat16.boot_signature = 0x29;

		// Generate volume serial number
		host_writed(reinterpret_cast<uint8_t*>(
		                    &boot_sector->ext.fat16.serial_number),
		            generate_volume_serial());

		write_padded_string(reinterpret_cast<uint8_t*>(
		                            boot_sector->ext.fat16.label),
		                    "NO NAME",
		                    11);
		write_padded_string(reinterpret_cast<uint8_t*>(
		                            boot_sector->ext.fat16.fs_type),
		                    (fat_bits == 16) ? "FAT16" : "FAT12",
		                    8);

		// Copy fallback boot code
		std::copy(std::begin(BootCode::Printer),
		          std::end(BootCode::Printer),
		          boot_sector->ext.fat16.boot_code);
		// Patch MOV SI address
		uint16_t string_addr = BootCode::PhysicalAddress +
		                       BootCode::OffsetFAT16 +
		                       BootCode::StringOffsetInCode;
		host_writew(boot_sector->ext.fat16.boot_code + BootCode::PatchOffsetInCode,
		            string_addr);
	}

	// Boot Sector Signature
	boot_sector->signature[0] = 0x55;
	boot_sector->signature[1] = 0xAA;

	// Write Main Boot Sector
	std::fwrite(buffer.data(), 1, buffer.size(), fs.get());

	// Write Extra FAT32 Structures (FSInfo + Backup)
	if (fat_bits == 32) {
		// Construct FSInfo using the packed struct
		std::array<uint8_t, 512> fs_info_buffer = {};
		auto* fs_info = reinterpret_cast<Fat32FsInfo*>(fs_info_buffer.data());

		// Fill signatures and hints
		host_writed(reinterpret_cast<uint8_t*>(&fs_info->lead_signature),
		            Fat32LeadSignature);
		host_writed(reinterpret_cast<uint8_t*>(&fs_info->struct_signature),
		            Fat32StructSignature);
		host_writed(reinterpret_cast<uint8_t*>(&fs_info->free_count),
		            static_cast<uint32_t>(tmp_clusters - 1));

		// Tell DOS where to start looking for free clusters
		// FAT Index 0 is reserved
		// FAT Index 1 is reserved
		// FAT Index 2 is Root
		// FAT Index 3 is the first available one
		host_writed(reinterpret_cast<uint8_t*>(&fs_info->next_free), 3);
		host_writed(reinterpret_cast<uint8_t*>(&fs_info->trail_signature),
		            Fat32TrailSignature);
		// Write FSInfo at Sector 1
		std::fwrite(fs_info_buffer.data(), 1, SectorSize, fs.get());

		// Write Backup Boot Sector at Sector 6
		// Note: 'buffer' currently holds the main Boot Sector we just
		// created
		std::fseek(fs.get(),
		           static_cast<long>((boot_sector_position + 6) * SectorSize),
		           SEEK_SET);
		std::fwrite(buffer.data(), 1, SectorSize, fs.get());

		// Write Backup FSInfo at Sector 7
		std::fwrite(fs_info_buffer.data(), 1, SectorSize, fs.get());
	}

	// FAT Initialization
	std::array<uint8_t, 512> fat_sector_buffer = {};
	std::array<uint8_t, 512> empty_sector      = {};

	// Move to FAT 1 start
	std::fseek(fs.get(),
	           static_cast<long>((boot_sector_position + reserved_sectors) *
	                             SectorSize),
	           SEEK_SET);

	if (fat_bits == 32) {
		auto* entries = reinterpret_cast<uint32_t*>(fat_sector_buffer.data());
		host_writed(reinterpret_cast<uint8_t*>(&entries[0]),
		            FatMarkers::Fat32MediaMask |
		                    disk_geometry.media_descriptor);
		host_writed(reinterpret_cast<uint8_t*>(&entries[1]),
		            FatMarkers::Fat32Eoc);
		host_writed(reinterpret_cast<uint8_t*>(&entries[2]),
		            FatMarkers::Fat32Eoc); // Cluster 2 (Root)
	} else if (fat_bits == 16) {
		auto* entries = reinterpret_cast<uint16_t*>(fat_sector_buffer.data());
		host_writew(reinterpret_cast<uint8_t*>(&entries[0]),
		            FatMarkers::Fat16MediaMask |
		                    disk_geometry.media_descriptor);
		host_writew(reinterpret_cast<uint8_t*>(&entries[1]),
		            FatMarkers::Fat16Eoc);
	} else {
		fat_sector_buffer[0] = disk_geometry.media_descriptor;
		fat_sector_buffer[1] = 0xFF;
		fat_sector_buffer[2] = 0xFF;
	}

	for (int i = 0; i < command_settings.fat_copies; ++i) {
		auto current_fat_start = std::ftell(fs.get());
		std::fwrite(fat_sector_buffer.data(),
		            1,
		            fat_sector_buffer.size(),
		            fs.get());

		// Fill remaining FAT sectors with zeroes
		for (int k = 1; k < fat_size_sectors; ++k) {
			std::fwrite(empty_sector.data(),
			            1,
			            empty_sector.size(),
			            fs.get());
		}
		std::fseek(fs.get(),
		           current_fat_start +
		                   static_cast<long>(fat_size_sectors * SectorSize),
		           SEEK_SET);
	}

	// Write Volume Label (Root Directory)
	if (!command_settings.label.empty()) {
		std::array<uint8_t, 512> root_buffer = {};
		auto* entry = reinterpret_cast<DirectoryEntry*>(root_buffer.data());

		write_padded_string(reinterpret_cast<uint8_t*>(entry->filename),
		                    command_settings.label,
		                    11);
		// Volume ID
		constexpr auto AttributeVolumeId = 0x08;
		entry->attributes                = AttributeVolumeId;

		std::fwrite(root_buffer.data(), 1, root_buffer.size(), fs.get());
	}

	program->WriteOut(MSG_Get("SHELL_CMD_IMGMAKE_CREATED"),
	                  command_settings.filename.string().c_str(),
	                  disk_geometry.cylinders,
	                  disk_geometry.heads,
	                  disk_geometry.sectors);
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
	auto result     = ImgmakeCommand::parse_args(args);

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
		ImgmakeCommand::execute(this, settings);
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
	        "  [color=white]-t type[reset]      Disk type: [color=light-cyan]fd_1440kb, fd_720kb, hd_250mb, hd_1gb[reset]...\n"
	        "                  or [color=light-cyan]hd[reset] (requires -size or -chs).\n"
	        "  [color=white]-size x[reset]      Size in MB (for hd type).\n"
	        "  [color=white]-chs c,h,s[reset]   Geometry (Cylinders, Heads, Sectors).\n"
	        "  [color=white]-noformat[reset]        Do not format the filesystem (raw image).\n"
	        "  [color=white]-label name[reset]  Volume label.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]imgmake[reset] [color=light-cyan]floppy.img[reset] -t fd_1440kb\n"
	        "  [color=light-green]imgmake[reset] [color=light-cyan]hdd.img[reset] -t hd -size 500");

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