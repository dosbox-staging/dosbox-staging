// SPDX-FileCopyrightText: 2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "makeimg.h"

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

#include "cpu/callback.h"
#include "dos/dos.h"
#include "dos/drive_local.h"
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

struct DiskGeometry {
	uint32_t cylinders           = 0;
	uint32_t heads               = 0;
	uint32_t sectors             = 0;
	uint8_t media_descriptor     = 0;
	uint16_t root_entries        = 0;
	uint32_t sectors_per_fat     = 0;
	uint16_t sectors_per_cluster = 0;
	uint64_t total_size_kb       = 0;
	bool is_floppy               = false;
};

// Maps preset names (e.g., "fd_1440") to geometries
// clang-format off
const std::unordered_map<std::string, DiskGeometry> GeometryPresets = {
		// name,          cyl, hds, sec, media_desc, root_entr, sect_per_fat, sect_per_cluster, total_size_kb, is_floppy}
        { "fd_160kb",     {40,   1,   8,       0xFE,        64,            1,                1,           160, true}},
        { "fd_180kb",     {40,   1,   9,       0xFC,        64,            2,                1,           180, true}},
        { "fd_320kb",    {40,    2,   8,       0xFF,       112,            1,                2,           320, true}},
        { "fd_360kb",    {40,    2,   9,       0xFD,       112,            2,                2,           360, true}},
        { "fd_720kb",    {80,    2,   9,       0xF9,       112,            3,                2,           720, true}},
        {"fd_1200kb",  {80,      2,  15,       0xF9,       224,            7,                1,          1200, true}},
        {"fd_1440kb",  {80,      2,  18,       0xF0,       224,            9,                1,          1440, true}},
        {"fd_2880kb",  {80,      2,  36,       0xF0,       240,            9,                2,          2880, true}},
        // HD presets
        { "hd_20mb",  {40,      16,  63,       0xF8,       512,            0,                0,             0, false}},
        { "hd_40mb",  {81,      16,  63,       0xF8,       512,            0,                0,             0, false}},
        { "hd_80mb",  {162,     16,  63,       0xF8,       512,            0,                0,             0, false}},
        { "hd_120mb", {243,     16,  63,       0xF8,       512,            0,                0,             0, false}},
        { "hd_250mb", {489,     16,  63,       0xF8,       512,            0,                0,             0, false}},
        { "hd_520mb", {1023,    16,  63,       0xF8,       512,            0,                0,             0, false}},
        {   "hd_1gb", {1023,    32,  63,       0xF8,       512,            0,                0,             0, false}},
        {   "hd_2gb", {1023,    64,  63,       0xF8,       512,            0,                0,             0, false}}
};
// clang-format on

// Return a 3-byte array [heads, sectors|cylinders_high, cylinders_low]
std::array<uint8_t, 3> lba_to_chs(int64_t lba, int max_cylinders, int max_heads,
                                  int max_sectors)
{
	int cylinders = 0, heads = 0, sectors = 0;
	constexpr int MaxLegacyCylinders = 1023;

	if (lba < static_cast<int64_t>(max_cylinders) * max_heads * max_sectors) {
		sectors   = static_cast<int>((lba % max_sectors) + 1);
		auto temp = static_cast<int64_t>(lba / max_sectors);
		heads     = static_cast<int>(temp % max_heads);
		cylinders = static_cast<int>(temp / max_heads);
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

struct FileCloser {
	void operator()(FILE* fp) const
	{
		if (fp) {
			fclose(fp);
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
	auto year  = static_cast<uint32_t>(local_time->tm_year + 1900);
	auto month = static_cast<uint32_t>(local_time->tm_mon + 1);
	auto day   = static_cast<uint32_t>(local_time->tm_mday);
	auto hour  = static_cast<uint32_t>(local_time->tm_hour);
	auto min   = static_cast<uint32_t>(local_time->tm_min);
	auto sec   = static_cast<uint32_t>(local_time->tm_sec);

	// DOS Algorithm
	auto lo = static_cast<uint32_t>(year + (month << 8) + day);
	auto hi = static_cast<uint32_t>((sec << 8) + min + hour);

	return (hi << 16) + lo;
}

// Write a little-endian value to a destination pointer
template <typename PtrT, typename ValT>
inline void write_le(PtrT* dest, ValT value)
{
	// Ensure we are writing to an integral type (uint16_t, uint32_t, etc.)
	static_assert(std::is_integral_v<PtrT>, "Destination must be an integer type");

	// Automatically cast the pointer to what host_write expects
	auto* raw_dest = reinterpret_cast<uint8_t*>(dest);

	if constexpr (sizeof(PtrT) == 2) {
		// Narrows value
		host_writew(raw_dest, static_cast<uint16_t>(value));

	} else if constexpr (sizeof(PtrT) == 4) {
		host_writed(raw_dest, static_cast<uint32_t>(value));

	} else {
		// Stop compilation if someone tries to use this on a uint8_t or
		// uint64_t
		static_assert(sizeof(PtrT) == 2 || sizeof(PtrT) == 4,
		              "write_le only supports 16-bit and 32-bit destinations");
	}
}

constexpr auto DefaultRootEntries = 512;

} // namespace

#pragma pack(push, 1)
struct FatBootSector {
	// Common Bios Parameter Block (BPB) (Bytes 0x00 - 0x23)
	uint8_t jump[3];
	char oem_name[8];
	uint16_t bytes_per_sector   = 0;
	uint8_t sectors_per_cluster = 0;
	uint16_t reserved_sectors   = 0;
	uint8_t fat_copies          = 0;
	uint16_t root_entries       = 0;
	uint16_t total_sectors_16   = 0;
	uint8_t media_descriptor    = 0;
	uint16_t sectors_per_fat_16 = 0;
	uint16_t sectors_per_track  = 0;
	uint16_t heads              = 0;
	uint32_t hidden_sectors     = 0;
	uint32_t total_sectors_32   = 0;

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
	} ext = {};

	uint8_t signature[2] = {};
};
#pragma pack(pop)

// Sanity check to ensure the compiler packed it correctly
static_assert(sizeof(FatBootSector) == 512,
              "FAT BootSector must be exactly 512 bytes");

// Standard DOS Directory Entry (32 bytes)
#pragma pack(push, 1)
struct DirectoryEntry {
	// Filename is in 8.3 format and padded with spaces
	char filename[11] = {};
	// Attributes: 0x08 = Volume Label, 0x10 = Subdir, etc.
	uint8_t attributes = 0;
	// Reserved for Windows NT / OS/2
	uint8_t reserved          = 0;
	uint8_t create_time_tenth = 0;
	uint16_t create_time      = 0;
	uint16_t create_date      = 0;
	uint16_t last_access_date = 0;
	// FAT32 only
	uint16_t first_cluster_high = 0;
	uint16_t write_time         = 0;
	uint16_t write_date         = 0;
	uint16_t first_cluster_low  = 0;
	uint32_t file_size          = 0;
};
#pragma pack(pop)

static_assert(sizeof(DirectoryEntry) == 32, "DirectoryEntry must be 32 bytes");

#pragma pack(push, 1)
struct Fat32FsInfo {
	// Offset 0x00: Always 0x41615252
	uint32_t lead_signature = 0;
	// Offset 0x04: Huge gap of zeros
	uint8_t reserved1[480] = {};
	// Offset 0x1E4 (484): Always 0x61417272
	uint32_t struct_signature = 0;
	// Offset 0x1E8 (488): Last known free cluster count
	uint32_t free_count = 0;
	// Offset 0x1EC (492): Hint for next free cluster
	uint32_t next_free = 0;
	// Offset 0x1F0: Small gap of zeros
	uint8_t reserved2[12] = {};
	// Offset 0x1FC (508): Always 0xAA550000
	uint32_t trail_signature = 0;
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

	// Show ASCII art of a HDD for +1 disk charisma
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
constexpr uint16_t OffsetFat16 = 0x3E;
constexpr uint16_t OffsetFat32 = 0x5A;

// BIOS loads boot sector at 0x7C00.
// The string starts at offset 40 inside the Printer array.
constexpr auto StringOffsetInCode = 40;
// The SI patch is at offset 21 in the code.
constexpr auto PatchOffsetInCode = 21;
constexpr auto PhysicalAddress   = 0x7C00;

} // namespace BootCode
namespace FatMarkers {

// FAT Limits (Max clusters)
constexpr uint32_t MaxClustersFat12 = 4084;
constexpr uint32_t MaxClustersFat16 = 65524;
// FAT32 theoretically can go higher, limit for safety/compatibility
} // namespace FatMarkers

enum class FatPartitionType : uint8_t {
	Fat12 = 0x01,

	// For partitions smaller than 32MB
	Fat16_Small = 0x04,

	// For partitions larger than 32MB but not LBA
	Fat16B = 0x06,

	Fat16_LBA = 0x0E,
	Fat32_LBA = 0x0C,
	Unknown   = 0x00
};

namespace MakeimgCommand {

enum class ErrorType { None, UnknownArgument, MissingArgument, InvalidValue };

struct CommandSettings {
	std_fs::path filename = {};
	std::string type      = {};
	std::string label     = {};

	int64_t size_bytes = 0;
	int cylinders      = 0;
	int heads          = 0;
	int sectors        = 0;

	int fat_type            = -1;
	int sectors_per_cluster = 0;
	int root_entries        = DefaultRootEntries;
	int fat_copies          = 2;

	bool force      = false;
	bool no_format  = false;
	bool use_chs    = false;
	bool use_dos_fs = false;
};

using ParseResult = std::variant<ErrorType, CommandSettings>;

static void notify_warning(const std::string& message_name, const char* arg = nullptr)
{
	if (arg) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MAKEIMG",
		                      message_name,
		                      arg);

	} else {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MAKEIMG",
		                      message_name);
	}
}

// Expand leading ~ to user's home directory and strip quotes
static std_fs::path resolve_path(const std::string& input_path)
{
	if (input_path.empty()) {
		return {};
	}

	std::string path_str = input_path;

	// Strip surrounding double quotes if present.
	trim(path_str, "\"");

	// Check if path starts with "~"
	if (path_str.starts_with('~')) {
		// Only expand if it's just "~", "~/", or "~\" (to allow mixed
		// separators on Win)
		bool is_separator = (path_str.length() > 1 &&
		                     (path_str[1] == '/' || path_str[1] == '\\'));

		if (path_str.length() == 1 || is_separator) {
			// Try HOME (Linux/macOS) then USERPROFILE (Windows)
			const char* home = std::getenv("HOME");
			if (!home) {
				home = std::getenv("USERPROFILE");
			}

			if (home) {
				std_fs::path expanded = home;

				// Append the rest of the path (skipping the ~)
				if (path_str.length() > 1) {
					// Convert the substring to a path object
					std_fs::path remainder(path_str.substr(1));
					// Strip any leading separator
					expanded /= remainder.relative_path();
				}
				return expanded;
			}
		}
	}

	// Return original if no tilde or HOME not found
	return std_fs::path(path_str);
}

// Resolves a DOS path to a Host path, ensuring the target is a mounted local
// directory.
// Returns a pair: { pointer to the localDrive, resolved host path }
// If failure, pointer is null.
static std::tuple<std::shared_ptr<localDrive>, std_fs::path, std::string> resolve_dos_target(
        const std::string& input_dos_path)
{
	// Resolve to absolute DOS path (e.g., C:\MYDIR\FILE.IMG)
	char abs_dos_path[DOS_PATHLENGTH];
	uint8_t drive_idx;
	if (!DOS_MakeName(input_dos_path.c_str(), abs_dos_path, &drive_idx)) {
		notify_warning("SHELL_CMD_MAKEIMG_INVALID_PATH",
		               input_dos_path.c_str());
		return {nullptr, {}, {}};
	}

	// Check if drive is valid
	if (!Drives[drive_idx]) {
		notify_warning("SHELL_CMD_MAKEIMG_INVALID_DRIVE");
		return {nullptr, {}, {}};
	}

	// Check if drive is a local directory mount
	auto local_drive = std::dynamic_pointer_cast<localDrive>(Drives[drive_idx]);
	if (!local_drive) {
		notify_warning("SHELL_CMD_MAKEIMG_NOT_LOCAL_DRIVE");
		return {nullptr, {}, {}};
	}

	// Don't allow image creation on read-only drives
	if (local_drive->IsReadOnly()) {
		notify_warning("SHELL_CMD_MAKEIMG_DRIVE_READONLY");
		return {nullptr, {}, {}};
	}

	// Resolve to Host Path
	// We strip the drive letter (e.g. "C:") by finding the colon.
	const char* path_part = strchr(abs_dos_path, ':');

	if (path_part) {
		// Skip the colon.
		// This handles "C:\FILE" -> "\FILE" and "C:/FILE" -> "/FILE"
		path_part++;
	} else {
		// Fallback if no colon found (unlikely for absolute DOS path)
		path_part = abs_dos_path;
	}

	// MapDosToHostFilename concatenates the drive's BaseDir with path_part.
	std::string host_path_str = local_drive->MapDosToHostFilename(path_part);

	// Construct the full DOS path (C:\DIR\FILE.IMG)
	// Fetch the drive letter from the index ('A' + drive_idx)
	std::string full_dos_path = "";
	full_dos_path += static_cast<char>('A' + drive_idx);
	full_dos_path += ":\\";

	// If abs_dos_path is not empty, append it.
	std::string internal_path = abs_dos_path;
	if (!internal_path.empty()) {
		if (internal_path[0] == '\\' || internal_path[0] == '/') {
			full_dos_path += internal_path.substr(1);
		} else {
			full_dos_path += internal_path;
		}
	}

	return {local_drive, std_fs::path(host_path_str), full_dos_path};
}

// Prompt user for Confirmation (Polling STDIN)
bool ask_confirmation(Program* program, const std::string& path,
                      bool is_dos_fs = false, const std::string& dos_path = "")
{
	constexpr auto EscKey = 0x1B;
	if (is_dos_fs) {
		program->WriteOut(MSG_Get("SHELL_CMD_MAKEIMG_CONFIRM_DOS"),
		                  dos_path.c_str(),
		                  path.c_str());
	} else {
		program->WriteOut(MSG_Get("SHELL_CMD_MAKEIMG_CONFIRM_HOST"),
		                  path.c_str());
	}

	while (true) {
		// Check if there is a character in the input buffer
		if (DOS_GetSTDINStatus()) {
			uint8_t response = 0;
			uint16_t length  = 1;

			// Read the character (Handle 0 = STDIN)
			if (!DOS_ReadFile(0, &response, &length)) {
				return false;
			}
			if (length == 0) {
				// Should not happen if Status was true, but as
				// a safeguard
				continue;
			}

			if (response == EscKey || response == 'n' || response == 'N') {
				program->WriteOut(
				        MSG_Get("SHELL_CMD_MAKEIMG_ABORTED"));
				return false;
			}

			if (response == 'y' || response == 'Y') {
				return true;
			}

		} else {
			// Yield and return false if we are shutting down
			if (CALLBACK_Idle()) {
				return false;
			}
		}
	}
}

static ParseResult parse_args(const std::vector<std::string>& args)
{
	CommandSettings settings;

	if (args.empty()) {
		return ErrorType::None;
	}

	// First argument is usually the filename (unless it starts with -)
	auto start_idx = 0;
	if (args[0][0] != '-') {
		settings.filename = resolve_path(args[0]);
		start_idx         = 1;

	} else {
		settings.filename = "MAKEIMG.IMG";
	}

	auto arg_size = static_cast<int>(args.size());
	for (auto i = start_idx; i < arg_size; ++i) {
		auto arg = args[i];
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

		} else if (arg == "-writetodos" || arg == "-d") {
			settings.use_dos_fs = true;

		} else {
			return ErrorType::UnknownArgument;
		}
	}

	if (settings.type.empty()) {
		return ErrorType::MissingArgument;
	}

	return settings;
}

// Struct to maintain state across image creation functions
struct ImageCreationContext {
	FILE* fs                     = nullptr;
	DiskGeometry geometry        = {};
	int64_t total_size           = 0;
	int64_t boot_sector_position = 0;
	int64_t volume_sectors       = 0;
	int fat_bits                 = 0;
	int fat_size_sectors         = 0;
	int sectors_per_cluster      = 0;

	static constexpr auto SectorSizeBytes = 512;
};

static bool compute_geometry(const CommandSettings& settings,
                             ImageCreationContext& ctx)
{
	if (auto it = GeometryPresets.find(settings.type);
	    it != GeometryPresets.end()) {
		ctx.geometry   = it->second;
		ctx.total_size = static_cast<int64_t>(ctx.geometry.cylinders) *
		                 ctx.geometry.heads * ctx.geometry.sectors *
		                 ImageCreationContext::SectorSizeBytes;
	} else if (settings.type == "hd") {
		ctx.geometry.media_descriptor = 0xF8;
		ctx.geometry.is_floppy        = false;

		if (!settings.use_chs && settings.size_bytes == 0) {
			notify_warning("SHELL_CMD_MAKEIMG_MISSING_SIZE");
			return false;
		}

		if (settings.use_chs) {
			ctx.geometry.cylinders = settings.cylinders;
			ctx.geometry.heads     = settings.heads;
			ctx.geometry.sectors   = settings.sectors;
			ctx.total_size = static_cast<int64_t>(ctx.geometry.cylinders) *
			                 ctx.geometry.heads * ctx.geometry.sectors *
			                 ImageCreationContext::SectorSizeBytes;
		} else if (settings.size_bytes > 0) {
			ctx.total_size     = settings.size_bytes;
			auto total_sectors = static_cast<int64_t>(
			        ctx.total_size / ImageCreationContext::SectorSizeBytes);

			// Calculate CHS from Size

			// Legacy BIOS Int 13h limit: 1023 cylinders, 16 heads,
			// 63 sectors (~528 MB).
			ctx.geometry.heads   = 16;
			ctx.geometry.sectors = 63;

			// To support larger disks, we must increase the Head
			// count to keep the Cylinder count below 1024.

			// > 528 MB: Shift to 64 Heads (Max ~2.1 GB)
			if (ctx.total_size > 528 * BytesPerMegabyte) {
				ctx.geometry.heads = 64;
			}
			// > 1 GB: Shift to 128 Heads (Max ~4.2 GB)
			if (ctx.total_size > 1 * BytesPerGigabyte) {
				ctx.geometry.heads = 128;
			}
			// > 4 GB: Shift to 255 Heads (Max ~8.4 GB)
			if (static_cast<int64_t>(ctx.total_size) >
			    4 * BytesPerGigabyte) {
				ctx.geometry.heads = 255;
			}

			// Calculate Cylinders based on the chosen Heads/Sectors
			ctx.geometry.cylinders = static_cast<uint32_t>(
			        total_sectors /
			        (ctx.geometry.heads * ctx.geometry.sectors));

			// Hard clamp for safety: Standard Int 13h cannot
			// address > 1023 cylinders. Or as Dosbox-X puts it:
			// "1024 toasts the stack"
			constexpr uint32_t MaxInt13Cylinders = 1023;
			ctx.geometry.cylinders = std::min(ctx.geometry.cylinders,
			                                  MaxInt13Cylinders);
		}
	} else {
		notify_warning("SHELL_CMD_MAKEIMG_INVALID_TYPE",
		               settings.type.c_str());
		return false;
	}

	if (ctx.total_size == 0) {
		notify_warning("SHELL_CMD_MAKEIMG_BAD_SIZE");
		return false;
	}
	return true;
}

static bool open_and_expand_file(const CommandSettings& settings,
                                 ImageCreationContext& ctx)
{
	// Check File Existence
	std::error_code ec;
	if (std_fs::exists(settings.filename, ec) && !settings.force) {
		notify_warning("SHELL_CMD_MAKEIMG_FILE_EXISTS",
		               settings.filename.string().c_str());
		return false;
	}

	// Create file (truncate if exists)
	FILE* raw_fs = fopen(settings.filename.string().c_str(), "wb+");
	if (!raw_fs) {
		notify_warning("SHELL_CMD_MAKEIMG_CANNOT_WRITE",
		               settings.filename.string().c_str());
		return false;
	}

	// Wrap immediately in a temporary smart pointer.
	// If we return false below, this guard will auto-close the file.
	FilePtr temp_fs_guard(raw_fs);

	// Seek to the last byte (total_size - 1)
	if (cross_fseeko(temp_fs_guard.get(), ctx.total_size - 1, SEEK_SET) != 0) {
		notify_warning("SHELL_CMD_MAKEIMG_SPACE_ERROR");
		return false;
	}

	// Write a single zero byte at the end, causing the file to
	// be filled up to total_size with zeros.
	if (fputc(0, temp_fs_guard.get()) == EOF) {
		notify_warning("SHELL_CMD_MAKEIMG_SPACE_ERROR");
		return false;
	}

	rewind(temp_fs_guard.get());

	// Release ownership from the local guard and transfer it to the context.
	ctx.fs = temp_fs_guard.release();

	return true;
}

static void determine_fat_type(const CommandSettings& settings,
                               ImageCreationContext& ctx)
{
	// Write Partition Table (MBR) for Hard Disks
	if (!ctx.geometry.is_floppy) {
		// Hard disks usually start partition at track 1
		// (head 0, sector 1 is MBR)

		ctx.boot_sector_position = ctx.geometry.sectors;
	}

	// Calculate volume sectors (Total - Hidden/MBR gap)
	ctx.volume_sectors = static_cast<int64_t>(
	        (ctx.total_size / ImageCreationContext::SectorSizeBytes) -
	        ctx.boot_sector_position);

	// If manually set via -fat X or implicit logic
	if (settings.fat_type != -1) {
		ctx.fat_bits = settings.fat_type;
		return;
	}

	// Auto-detect Logic
	constexpr int64_t SectorsPerMB = (1024 * 1024) /
	                                 ImageCreationContext::SectorSizeBytes;
	constexpr int64_t SectorsPerGB = (1024 * 1024 * 1024) /
	                                 ImageCreationContext::SectorSizeBytes;
	// Thresholds
	// FAT12 Limit: ~16 MB
	// Maximum FAT12 clusters (4085) * Max standard FAT12 cluster
	// size (8 sectors / 4KB)
	constexpr auto Fat12MaxClusters = 4085;
	constexpr auto Fat12MaxSPC      = 8;
	constexpr auto Fat12MaxSectors  = Fat12MaxClusters * Fat12MaxSPC;
	// FAT16 Efficiency Limit: 2047 MB
	// At 512MB, FAT16 requires 16KB clusters, which is inefficient.
	// Windows 98/DOS 7.1 switch to FAT32 here by default.
	// However, dosbox-staging does not support FAT32 yet, so for
	// now we accept this inefficiency in favour of compatibility.
	constexpr int64_t Fat16EfficiencyLimit = 2047 * SectorsPerMB;
	// FAT16 Hard Limit: 2 GB
	// The mathematical limit of unsigned 16-bit math + 32KB clusters.
	constexpr int64_t Fat16HardLimit = 2 * SectorsPerGB;

	// Check DOS Version (User provided variables)
	// DOS 7.10 is required for FAT32 (Windows 95 OSR2 / Windows 98)
	DOS_PSP psp(dos.psp());
	auto dos_version_major  = psp.GetVersionMajor();
	auto dos_version_minor  = psp.GetVersionMinor();
	auto dos_supports_fat32 = (dos_version_major > 7) ||
	                          (dos_version_major == 7 && dos_version_minor >= 10);

	if (ctx.geometry.is_floppy) {
		ctx.fat_bits = 12;
	} else if (ctx.volume_sectors < Fat12MaxSectors) {
		ctx.fat_bits = 12;
	} else {
		// Decision between FAT16 and FAT32
		if (ctx.volume_sectors >= Fat16HardLimit) {
			// > 2GB: Must be FAT32
			ctx.fat_bits = 32;
		} else if (ctx.volume_sectors >= Fat16EfficiencyLimit &&
		           dos_supports_fat32) {
			// 512MB - 2GB: Use FAT32 if DOS supports it
			ctx.fat_bits = 32;
		} else {
			// < 512MB OR Old DOS: Use FAT16
			ctx.fat_bits = 16;
		}
	}
}

static void write_mbr(ImageCreationContext& ctx)
{
	if (ctx.geometry.is_floppy) {
		return;
	}

	std::array<uint8_t, ImageCreationContext::SectorSizeBytes> buffer = {};
	std::copy(std::begin(BootCode::Printer),
	          std::end(BootCode::Printer),
	          buffer.begin());

	// Patch MBR Code
	auto mbr_string_addr = static_cast<uint16_t>(
	        BootCode::PhysicalAddress + BootCode::StringOffsetInCode);
	host_writew(buffer.data() + BootCode::PatchOffsetInCode, mbr_string_addr);

	constexpr auto PartitionFlagActive   = 0x80;
	constexpr auto PartitionEntry1Offset = 0x1BE;

	// Partition 1 Entry
	auto* partition = static_cast<uint8_t*>(buffer.data() + PartitionEntry1Offset);
	// Mark partition as active
	partition[0] = PartitionFlagActive;

	auto start_chs = lba_to_chs(ctx.boot_sector_position,
	                            ctx.geometry.cylinders,
	                            ctx.geometry.heads,
	                            ctx.geometry.sectors);
	std::copy(start_chs.begin(), start_chs.end(), partition + 1);

	// Determine Partition Type ID
	// Note: Logic is similar but not identical to FAT bits
	// detection
	if (ctx.fat_bits == 32) {
		partition[4] = enum_val(FatPartitionType::Fat32_LBA);
	} else if (ctx.fat_bits == 12) {
		partition[4] = enum_val(FatPartitionType::Fat12);
	} else {
		// We are in FAT16 territory (16MB to 2GB)
		if (ctx.volume_sectors < 65536) {
			partition[4] = enum_val(FatPartitionType::Fat16_Small);
		} else if (ctx.total_size > 528 * BytesPerMegabyte) {
			// 528MB to 2GB: suggest LBA-aware FAT16
			partition[4] = enum_val(FatPartitionType::Fat16_LBA);
		} else {
			// 32MB to 528MB: Use Standard FAT16
			partition[4] = enum_val(FatPartitionType::Fat16B);
		}
	}

	// End CHS
	auto end_chs = lba_to_chs(
	        (ctx.total_size / ImageCreationContext::SectorSizeBytes) - 1,
	        ctx.geometry.cylinders,
	        ctx.geometry.heads,
	        ctx.geometry.sectors);
	std::copy(end_chs.begin(), end_chs.end(), partition + 5);

	// LBA Start & Size
	host_writed(partition + 8, static_cast<uint32_t>(ctx.boot_sector_position));
	host_writed(partition + 12, static_cast<uint32_t>(ctx.volume_sectors));

	buffer[510] = 0x55;
	buffer[511] = 0xAA;
	fwrite(buffer.data(), 1, buffer.size(), ctx.fs);
}

static void write_boot_sector(const CommandSettings& settings,
                              ImageCreationContext& ctx)
{
	// Move to Boot Sector
	cross_fseeko(ctx.fs,
	             ctx.boot_sector_position * ImageCreationContext::SectorSizeBytes,
	             SEEK_SET);

	std::array<uint8_t, ImageCreationContext::SectorSizeBytes> buffer = {};
	auto* boot_sector = reinterpret_cast<FatBootSector*>(buffer.data());

	// Bios Parameter Block (BPB) Construction

	// JMP Instruction
	boot_sector->jump[0] = 0xEB;
	// Jump offset depends on BPB size
	boot_sector->jump[1] = (ctx.fat_bits == 32) ? 0x58 : 0x3C;
	boot_sector->jump[2] = 0x90;

	// OEM Name
	auto oem_name_str = right_pad("DOSBOX-S", 8, ' ');
	std::copy(oem_name_str.begin(), oem_name_str.end(), boot_sector->oem_name);
	write_le(&boot_sector->bytes_per_sector,
	         ImageCreationContext::SectorSizeBytes);

	// Calculate Sectors Per Cluster (SPC) Safely
	auto sectors_per_cluster       = (settings.sectors_per_cluster > 0)
	                                       ? settings.sectors_per_cluster
	                                       : 0;
	constexpr int64_t SectorsPerGB = (1024 * 1024 * 1024) /
	                                 ImageCreationContext::SectorSizeBytes;
	constexpr int64_t SectorsPerMB = (1024 * 1024) /
	                                 ImageCreationContext::SectorSizeBytes;

	// Beyond 32,680 sectors (~16MB) we switch from FAT12 to FAT16.
	constexpr auto Fat12_Limit_Sectors = 32680;

	if (sectors_per_cluster == 0) {
		if (ctx.fat_bits == 32) {

			// Win98 Standard alignments:
			// > 32GB -> 32KB Cluster (64 sectors)
			// > 16GB -> 16KB Cluster (32 sectors)
			// >  8GB ->  8KB Cluster (16 sectors)
			// <  8GB ->  4KB Cluster ( 8 sectors)

			if (ctx.volume_sectors >= 32 * SectorsPerGB) {
				sectors_per_cluster = 64;

			} else if (ctx.volume_sectors >= 16 * SectorsPerGB) {
				sectors_per_cluster = 32;

			} else if (ctx.volume_sectors >= 8 * SectorsPerGB) {
				sectors_per_cluster = 16;

			} else {
				sectors_per_cluster = 8;
			}
		} else {
			// FAT12/16 efficiency optimizations
			// If > 512MB, we are technically pushing FAT16 limits
			if (ctx.volume_sectors >= 512 * SectorsPerMB) {
				// Should usually be FAT32 here, but just in
				// case (128KB)
				sectors_per_cluster = 64;

			} else if (ctx.volume_sectors > Fat12_Limit_Sectors) {
				// HD Default (2KB)
				sectors_per_cluster = 4;
			} else {
				// Floppy/Small disk (512B)
				sectors_per_cluster = 1;
			}
		}
	}

	// FAT Limit Check & SPC Adjustment
	auto reserved_sectors = (ctx.fat_bits == 32) ? 32 : 1;
	auto root_entries     = (ctx.fat_bits == 32)
	                              ? 0
	                              : ((ctx.geometry.root_entries > 0)
	                                         ? ctx.geometry.root_entries
	                                         : DefaultRootEntries);
	auto root_dir_sectors = ((root_entries * 32) +
	                         (ImageCreationContext::SectorSizeBytes - 1)) /
	                        ImageCreationContext::SectorSizeBytes;

	// Calculate approximate clusters to see if we overflow the FAT type
	// This loop increases SPC until the cluster count is safe.
	constexpr auto MaxFatLimit = 0x0FFFFFFF;
	auto fat_limit = (ctx.fat_bits == 12) ? FatMarkers::MaxClustersFat12
	               : (ctx.fat_bits == 16) ? FatMarkers::MaxClustersFat16
	                                      : MaxFatLimit;

	constexpr auto MaxSectorsPerCluster = 128;
	while (sectors_per_cluster < MaxSectorsPerCluster) {
		// Rough estimate of data area
		auto data_sec = ctx.volume_sectors - reserved_sectors -
		                root_dir_sectors;
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

	ctx.sectors_per_cluster = sectors_per_cluster;
	boot_sector->sectors_per_cluster = static_cast<uint8_t>(sectors_per_cluster);

	// Remaining BPB
	write_le(&boot_sector->reserved_sectors, reserved_sectors);
	boot_sector->fat_copies = static_cast<uint8_t>(settings.fat_copies);
	write_le(&boot_sector->root_entries, root_entries);

	if (ctx.volume_sectors < 65536 && ctx.fat_bits != 32) {
		write_le(&boot_sector->total_sectors_16, ctx.volume_sectors);
	} else {
		write_le(&boot_sector->total_sectors_16, 0);
	}

	boot_sector->media_descriptor = ctx.geometry.media_descriptor;

	// We need to solve for FAT Size:
	// Data_Sectors = Total - Reserved - (Fat_Copies * Fat_Size) - Root_Dir
	// Total_Clusters = Data_Sectors / SPC
	auto tmp_data_sectors = ctx.volume_sectors - reserved_sectors -
	                        root_dir_sectors;
	auto tmp_clusters = tmp_data_sectors / sectors_per_cluster;

	// The FAT must track every data cluster plus two reserved entries
	// at the start (Index 0: Media Descriptor, Index 1: EOC/Flags).
	constexpr auto ReservedFatEntries = 2;
	auto total_entries                = tmp_clusters + ReservedFatEntries;
	int64_t fat_size_bytes            = 0;

	if (ctx.fat_bits == 12) {
		// FAT12: 12 bits per entry = 1.5 bytes.
		// Calculation: Entries * 1.5  -->  (Entries * 3) / 2
		fat_size_bytes = (total_entries * 3) / 2;

	} else if (ctx.fat_bits == 16) {
		// FAT16: 16 bits per entry = 2 bytes
		fat_size_bytes = total_entries * 2;

	} else {
		// FAT32: 32 bits per entry = 4 bytes
		fat_size_bytes = total_entries * 4;
	}

	// Convert bytes to sectors, rounding up (Ceiling Division).
	// Formula: (Numerator + Denominator - 1) / Denominator
	ctx.fat_size_sectors = static_cast<uint32_t>(
	        (fat_size_bytes + ImageCreationContext::SectorSizeBytes - 1) /
	        ImageCreationContext::SectorSizeBytes);

	if (ctx.fat_bits != 32) { //-V1051
		write_le(&boot_sector->sectors_per_fat_16, ctx.fat_size_sectors);

	} else {
		write_le(&boot_sector->sectors_per_fat_16, 0);
	}
	write_le(&boot_sector->sectors_per_track, ctx.geometry.sectors);
	write_le(&boot_sector->heads, ctx.geometry.heads);
	write_le(&boot_sector->hidden_sectors, ctx.boot_sector_position);
	write_le(&boot_sector->total_sectors_32,
	         (ctx.fat_bits == 32 || ctx.volume_sectors > UINT16_MAX) ? ctx.volume_sectors
	                                                                 : 0);

	// Extended BPB & Fallback Boot Code
	if (ctx.fat_bits == 32) {

		write_le(&boot_sector->ext.fat32.sectors_per_fat_32,
		         ctx.fat_size_sectors);
		write_le(&boot_sector->ext.fat32.ext_flags, 0);
		write_le(&boot_sector->ext.fat32.fs_version, 0);
		write_le(&boot_sector->ext.fat32.root_cluster, 2);
		write_le(&boot_sector->ext.fat32.fs_info_sector, 1);
		write_le(&boot_sector->ext.fat32.backup_boot_sector, 6);

		boot_sector->ext.fat32.boot_signature = 0x29;

		// Generate volume serial number
		write_le(&boot_sector->ext.fat32.serial_number,
		         generate_volume_serial());

		auto label_str = right_pad("NO NAME", 11, ' ');
		std::copy(label_str.begin(),
		          label_str.end(),
		          boot_sector->ext.fat32.label);

		auto fs_type_str = right_pad("FAT32", 8, ' ');
		std::copy(fs_type_str.begin(),
		          fs_type_str.end(),
		          boot_sector->ext.fat32.fs_type);

		// Copy fallback boot code
		std::copy(std::begin(BootCode::Printer),
		          std::end(BootCode::Printer),
		          boot_sector->ext.fat32.boot_code);
		// Patch MOV SI address
		int string_addr = BootCode::PhysicalAddress + BootCode::OffsetFat32 +
		                  BootCode::StringOffsetInCode;
		host_writew(reinterpret_cast<uint8_t*>(boot_sector->ext.fat32.boot_code +
		                                       BootCode::PatchOffsetInCode),
		            static_cast<uint16_t>(string_addr));

	} else {

		constexpr auto BootSectorDriveNumberFloppy   = 0x00;
		constexpr auto BootSectorDriveNumberHardDisk = 0x80;
		boot_sector->ext.fat16.drive_number   = (ctx.geometry.is_floppy)
		                                              ? BootSectorDriveNumberFloppy
		                                              : BootSectorDriveNumberHardDisk;
		boot_sector->ext.fat16.boot_signature = 0x29;

		// Generate volume serial number
		write_le(&boot_sector->ext.fat16.serial_number,
		         generate_volume_serial());
		auto label_str = right_pad("NO NAME", 11, ' ');
		std::copy(label_str.begin(),
		          label_str.end(),
		          boot_sector->ext.fat16.label);
		auto fs_type_str = right_pad((ctx.fat_bits == 16) ? "FAT16" : "FAT12",
		                             8,
		                             ' ');
		std::copy(fs_type_str.begin(),
		          fs_type_str.end(),
		          boot_sector->ext.fat16.fs_type);
		// Copy fallback boot code
		std::copy(std::begin(BootCode::Printer),
		          std::end(BootCode::Printer),
		          boot_sector->ext.fat16.boot_code);
		// Patch MOV SI address
		auto string_addr = static_cast<uint16_t>(
		        BootCode::PhysicalAddress + BootCode::OffsetFat16 +
		        BootCode::StringOffsetInCode);
		host_writew(boot_sector->ext.fat16.boot_code + BootCode::PatchOffsetInCode,
		            string_addr);
	}

	// Boot Sector Signature
	boot_sector->signature[0] = 0x55;
	boot_sector->signature[1] = 0xAA;

	// Write Main Boot Sector
	fwrite(buffer.data(), 1, buffer.size(), ctx.fs);

	// Write Extra FAT32 Structures (FSInfo + Backup)
	if (ctx.fat_bits == 32) {
		// Construct FSInfo using the packed struct
		std::array<uint8_t, ImageCreationContext::SectorSizeBytes> fs_info_buffer = {};
		auto* fs_info = reinterpret_cast<Fat32FsInfo*>(fs_info_buffer.data());

		// Fill signatures and hints
		write_le(&fs_info->lead_signature, Fat32LeadSignature);
		write_le(&fs_info->struct_signature, Fat32StructSignature);
		write_le(&fs_info->free_count, tmp_clusters - 1);

		// Tell DOS where to start looking for free clusters
		// FAT Index 0 is reserved
		// FAT Index 1 is reserved
		// FAT Index 2 is Root
		// FAT Index 3 is the first available one
		write_le(&fs_info->next_free, 3);
		write_le(&fs_info->trail_signature, Fat32TrailSignature);

		// Write FSInfo at Sector 1
		fwrite(fs_info_buffer.data(),
		       1,
		       ImageCreationContext::SectorSizeBytes,
		       ctx.fs);

		// Write Backup Boot Sector at Sector 6
		// Note: 'buffer' currently holds the main Boot Sector we just
		// created
		cross_fseeko(ctx.fs,
		             (ctx.boot_sector_position + 6) *
		                     ImageCreationContext::SectorSizeBytes,
		             SEEK_SET);
		fwrite(buffer.data(), 1, ImageCreationContext::SectorSizeBytes, ctx.fs);

		// Write Backup FSInfo at Sector 7
		fwrite(fs_info_buffer.data(),
		       1,
		       ImageCreationContext::SectorSizeBytes,
		       ctx.fs);
	}
}

static void write_fats(const CommandSettings& settings, ImageCreationContext& ctx)
{
	std::array<uint8_t, ImageCreationContext::SectorSizeBytes> fat_sector_buffer = {};
	std::array<uint8_t, ImageCreationContext::SectorSizeBytes> empty_sector = {};

	auto reserved_sectors = (ctx.fat_bits == 32) ? 32 : 1;

	// Move to FAT 1 start
	cross_fseeko(ctx.fs,
	             (ctx.boot_sector_position + reserved_sectors) *
	                     ImageCreationContext::SectorSizeBytes,
	             SEEK_SET);

	if (ctx.fat_bits == 32) {

		auto* entries = reinterpret_cast<uint32_t*>(fat_sector_buffer.data());
		write_le(&entries[0],
		         FatMarkers::Fat32MediaMask | ctx.geometry.media_descriptor);
		// Cluster 1 (reserved)
		write_le(&entries[1], FatMarkers::Fat32Eoc);
		// Cluster 2 (root)
		write_le(&entries[2], FatMarkers::Fat32Eoc);

	} else if (ctx.fat_bits == 16) {

		auto* entries = reinterpret_cast<uint16_t*>(fat_sector_buffer.data());
		write_le(&entries[0],
		         FatMarkers::Fat16MediaMask | ctx.geometry.media_descriptor);
		write_le(&entries[1], FatMarkers::Fat16Eoc);

	} else {

		fat_sector_buffer[0] = ctx.geometry.media_descriptor;
		fat_sector_buffer[1] = 0xFF;
		fat_sector_buffer[2] = 0xFF;
	}

	for (auto i = 0; i < settings.fat_copies; ++i) {
		auto current_fat_start = cross_ftello(ctx.fs);
		fwrite(fat_sector_buffer.data(), 1, fat_sector_buffer.size(), ctx.fs);

		// Fill remaining FAT sectors with zeroes
		for (int k = 1; k < ctx.fat_size_sectors; ++k) {
			fwrite(empty_sector.data(), 1, empty_sector.size(), ctx.fs);
		}

		// Seek to start of next FAT copy
		cross_fseeko(ctx.fs,
		             current_fat_start +
		                     (static_cast<int64_t>(ctx.fat_size_sectors) *
		                      ImageCreationContext::SectorSizeBytes),
		             SEEK_SET);
	}
}

static void write_root_dir(const CommandSettings& settings, ImageCreationContext& ctx)
{
	if (settings.label.empty()) {
		return;
	}

	std::array<uint8_t, ImageCreationContext::SectorSizeBytes> root_buffer = {};
	auto* entry = reinterpret_cast<DirectoryEntry*>(root_buffer.data());
	auto filename_str = right_pad(settings.label, 11, ' ');
	std::copy(filename_str.begin(), filename_str.end(), entry->filename);

	// Volume ID
	entry->attributes = 0x08;
	fwrite(root_buffer.data(), 1, root_buffer.size(), ctx.fs);
}

static bool execute(Program* program, CommandSettings& settings)
{
	ImageCreationContext ctx;
	std_fs::path full_host_path;
	std::string display_path;
	std::shared_ptr<localDrive> dos_drive = nullptr;

	// Don't allow users to create disk images in secure mode.
	if (control->SecureMode()) {
		program->WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
		return false;
	}

	if (!compute_geometry(settings, ctx)) {
		return false;
	}

	if (settings.use_dos_fs) {
		// Resolve DOS path to Host path and get the drive pointer
		auto [drive, host_path, abs_dos_path] = resolve_dos_target(
		        settings.filename.string());
		dos_drive      = drive;
		full_host_path = host_path;

		if (!dos_drive) {
			return false;
		}

		// Use the absolute DOS path for the display instead of the raw
		// input
		display_path      = abs_dos_path;
		settings.filename = full_host_path;
	} else {
		// Resolve Host path (handle ~ expansion, etc.)
		settings.filename = resolve_path(settings.filename.string());
		std::error_code ec;
		full_host_path = std_fs::absolute(settings.filename, ec);
		// Fallback to relative path if absolute resolution fails for
		// whatever reason
		if (ec) {
			full_host_path = settings.filename;
		}
		display_path = full_host_path.string();
	}

	// Confirmation prompt
	if (!ask_confirmation(program,
	                      full_host_path.string(),
	                      settings.use_dos_fs,
	                      display_path)) {
		return true;
	}

	{
		if (!open_and_expand_file(settings, ctx)) {
			return false;
		}
		FilePtr fs_guard(ctx.fs);

		// Only write filesystem structures if not in no-format mode
		if (!settings.no_format) {

			determine_fat_type(settings, ctx);
			write_mbr(ctx);
			write_boot_sector(settings, ctx);
			write_fats(settings, ctx);
			write_root_dir(settings, ctx);
		}
		// fs_guard goes out of scope here, closing the file.
		// This is important so the OS flushes changes before we tell
		// DOSBox to rescan.
	}

	program->WriteOut(MSG_Get("SHELL_CMD_MAKEIMG_CREATED"),
	                  display_path.c_str(),
	                  ctx.geometry.cylinders,
	                  ctx.geometry.heads,
	                  ctx.geometry.sectors);

	if (!settings.no_format) {
		program->WriteOut(MSG_Get("SHELL_CMD_MAKEIMG_FORMATTED"),
		                  (ctx.fat_bits == 12)   ? "12"
		                  : (ctx.fat_bits == 16) ? "16"
		                                         : "32");
	}

	// Refresh DOSBox directory cache if we wrote to a mounted DOS drive
	if (dos_drive) {
		dos_drive->EmptyCache();
	}

	return true;
}

} // namespace MakeimgCommand

void MAKEIMG::Run()
{
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("SHELL_CMD_MAKEIMG_HELP_LONG"));
		output.Display();
		return;
	}

	const auto args = cmd->GetArguments();
	auto result     = MakeimgCommand::parse_args(args);

	if (std::holds_alternative<MakeimgCommand::ErrorType>(result)) {
		auto err = std::get<MakeimgCommand::ErrorType>(result);
		if (err != MakeimgCommand::ErrorType::None) {
			// Map error to message
			MakeimgCommand::notify_warning("SHELL_SYNTAX_ERROR");
		}
		// If error is None, it means empty args, just show help logic
		// implicitly or exit
		if (err == MakeimgCommand::ErrorType::None) {
			MoreOutputStrings output(*this);
			output.AddString(MSG_Get("SHELL_CMD_MAKEIMG_HELP_LONG"));
			output.Display();
		}

	} else {
		auto settings = std::get<MakeimgCommand::CommandSettings>(result);
		MakeimgCommand::execute(this, settings);
	}
}

void MAKEIMG::AddMessages()
{
	MSG_Add("SHELL_CMD_MAKEIMG_HELP_LONG",
	        "Create a new empty disk image.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]makeimg[reset] [color=light-cyan]FILE[reset] [color=white]-t TYPE[reset] [PARAMETERS]\n"
	        "\n"
	        "Parameters:\n"
	        "  -t [color=white]TYPE[reset]      Disk type:\n"
	        "               [color=light-cyan]fd_2880kb[reset], [color=light-cyan]fd_1440kb[reset], [color=light-cyan]fd_1200kb[reset], [color=light-cyan]fd_720kb[reset], \n"
	        "               [color=light-cyan]fd_360kb[reset], [color=light-cyan]fd_320kb[reset], [color=light-cyan]fd_180kb[reset], [color=light-cyan]fd_160kb[reset],\n"
	        "               [color=light-cyan]hd_20mb[reset], [color=light-cyan]hd_40mb[reset], [color=light-cyan]hd_80mb[reset], [color=light-cyan]hd_120mb[reset],\n"
	        "               [color=light-cyan]hd_250mb[reset], [color=light-cyan]hd_520mb[reset], [color=light-cyan]hd_1gb[reset], [color=light-cyan]hd_2gb[reset]\n"
	        "               or [color=light-cyan]hd[reset] (requires -size or -chs).\n"
	        "\n"
	        "  -size [color=white]X[reset]      Size in MB (for [color=light-cyan]hd[reset] type).\n"
	        "  -chs [color=white]C,H,S[reset]   Geometry (Cylinders, Heads, Sectors).\n"
	        "  -fat [color=white]FF[reset]      Filesystem type ([color=light-cyan]-fat 12[reset], [color=light-cyan]-fat 16[reset] or [color=light-cyan]-fat 32[reset]).\n"
	        "               Default is determined automatically.\n"
	        "  -noformat    Do not format the filesystem (raw image).\n"
	        "  -label [color=white]NAME[reset]  Volume label.\n"
	        "\n"
	        "  -writetodos\n"
	        "  -d           Write image to the emulated DOS filesystem instead\n"
	        "               of the host filesystem.\n"

	        "\n"
	        "Examples:\n"
	        "  [color=light-green]makeimg[reset] [color=light-cyan]floppy.img[reset] -t [color=light-cyan]fd_1440kb[reset] -label [color=white]MYDISK[reset]\n"
	        "  [color=light-green]makeimg[reset] [color=light-cyan]hdd.img[reset] -t [color=light-cyan]hd[reset] -size [color=white]500[reset]\n"
	        "  [color=light-green]makeimg[reset] [color=light-cyan]C:\\IMAGES\\HDD120.IMG[reset] -t [color=light-cyan]hd_120mb[reset] -fat [color=white]32[reset] -d\n"
	        "\n"
	        "Notes:\n"
	        "  - By default, the image file will be created in the current working\n"
	        "    directory on the host filesystem, or at the absolute host path.\n"
	        "  - When using the [color=white]-writetodos[reset] option, ensure the target path is a mounted\n"
	        "    local directory (not inside another disk image).\n");

	MSG_Add("SHELL_CMD_MAKEIMG_MISSING_SIZE",
	        "You must specify -size or -chs for custom hard disks.");
	MSG_Add("SHELL_CMD_MAKEIMG_INVALID_TYPE",
	        "Unknown disk type: [color=light-cyan]%s[reset]");
	MSG_Add("SHELL_CMD_MAKEIMG_BAD_SIZE", "Invalid disk size calculated.");
	MSG_Add("SHELL_CMD_MAKEIMG_FILE_EXISTS",
	        "File [color=light-cyan]%s[reset] already exists. Use -force to overwrite.");
	MSG_Add("SHELL_CMD_MAKEIMG_CANNOT_WRITE",
	        "Cannot open file [color=light-cyan]%s[reset] for writing.");
	MSG_Add("SHELL_CMD_MAKEIMG_SPACE_ERROR",
	        "Disk full or cannot allocate image size.");
	MSG_Add("SHELL_CMD_MAKEIMG_CREATED",
	        "Created [color=light-cyan]%s[reset] [CHS: %u, %u, %u]");
	MSG_Add("SHELL_CMD_MAKEIMG_FORMATTED",
	        "\nFormatted as [color=light-cyan]FAT%s[reset]");
	MSG_Add("SHELL_CMD_MAKEIMG_CONFIRM_HOST",
	        "Image will be created on the [color=light-green]HOST[reset] filesystem at:\n  [color=light-cyan]%s[reset]\n\n"
	        "Proceed? (Y/N)\n");
	MSG_Add("SHELL_CMD_MAKEIMG_CONFIRM_DOS",
	        "Image will be created on the [color=light-green]DOS[reset] filesystem at:\n  [color=light-cyan]%s[reset]\n"
	        "  Host path: %s\n\n"
	        "Proceed? (Y/N)\n");
	MSG_Add("SHELL_CMD_MAKEIMG_ABORTED", "\nOperation aborted.");
	MSG_Add("SHELL_CMD_MAKEIMG_INVALID_PATH", "Invalid DOS path: %s");
	MSG_Add("SHELL_CMD_MAKEIMG_INVALID_DRIVE", "Target drive is invalid.");
	MSG_Add("SHELL_CMD_MAKEIMG_DRIVE_READONLY", "Target drive is read-only.");
	MSG_Add("SHELL_CMD_MAKEIMG_NOT_LOCAL_DRIVE",
	        "Cannot create image inside another disk image.\nTarget must be a mounted local directory.");
}