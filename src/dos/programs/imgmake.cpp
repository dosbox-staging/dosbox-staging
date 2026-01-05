// SPDX-FileCopyrightText: 2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "imgmake.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
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
std::array<uint8_t, 3> lba_to_chs(uint64_t lba, uint32_t max_cylinders, uint32_t max_heads,
                               uint32_t max_sectors)
{
    uint32_t cylinders = 0, heads = 0, sectors = 0;
    if (lba < static_cast<uint64_t>(max_cylinders) * max_heads * max_sectors) {
        sectors             = static_cast<uint32_t>((lba % max_sectors) + 1);
        uint64_t temp = lba / max_sectors;
        heads             = static_cast<uint32_t>(temp % max_heads);
        cylinders             = static_cast<uint32_t>(temp / max_heads);
        // Clamp for legacy CHS
        if (cylinders > 1023) {
            cylinders = 1023;
        }
    } else {
        cylinders = 1023;
        heads = max_heads - 1;
        sectors = max_sectors;
    }

    return {static_cast<uint8_t>(heads),
            static_cast<uint8_t>((sectors & 0x3f) | ((cylinders >> 2) & 0xc0)),
            static_cast<uint8_t>(cylinders & 0xff)};
}

// Write a string into a fixed-width buffer and pad with spaces
void write_padded_string(uint8_t* dest, const std::string& str, size_t length)
{
    std::string temp = str;
    temp.resize(length, ' ');
    std::copy(temp.begin(), temp.end(), dest);
}

// RAII wrapper
struct FileCloser {
    void operator()(FILE* fp) const {
        if (fp) {
            std::fclose(fp);
        }
    }
};
using FilePtr = std::unique_ptr<FILE, FileCloser>;

} // namespace

namespace ImgmakeCommand {

enum class ErrorType { None, UnknownArgument, MissingArgument, InvalidValue };

struct CommandSettings {
    std::filesystem::path filename = {};
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
        } else {
            return ErrorType::UnknownArgument;
        }
    }

    if (settings.type.empty()) {
        return ErrorType::MissingArgument;
    }

    return settings;
}

bool Execute(Program* program, CommandSettings& command_settings)
{
    DiskGeometry geom         = {};
    uint64_t total_size       = 0;
    constexpr uint64_t SectorSize = 512;

    // Determine Geometry and Size
    if (auto it = GeometryPresets.find(command_settings.type); it != GeometryPresets.end()) {
        geom       = it->second;
        total_size = static_cast<uint64_t>(geom.cyl) * geom.heads *
                     geom.sectors * SectorSize;
    } else if (command_settings.type == "hd") {
        geom.media_desc = 0xF8;
        geom.is_floppy  = false;

        if (command_settings.use_chs) {
            geom.cyl     = command_settings.cylinders;
            geom.heads   = command_settings.heads;
            geom.sectors = command_settings.sectors;
            total_size   = static_cast<uint64_t>(geom.cyl) *
                         geom.heads * geom.sectors * SectorSize;
        } else if (command_settings.size_bytes > 0) {
            total_size           = command_settings.size_bytes;
            uint64_t total_sectors = total_size / SectorSize;

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
                    total_sectors / (geom.heads * geom.sectors));
            // Cap for legacy geometry
            if (geom.cyl > 1023) {
                geom.cyl = 1023;
            }
        } else {
            notify_warning("SHELL_CMD_IMGMAKE_MISSING_SIZE");
            return false;
        }
    } else {
        notify_warning("SHELL_CMD_IMGMAKE_INVALID_TYPE", command_settings.type.c_str());
        return false;
    }

    if (total_size == 0) {
        notify_warning("SHELL_CMD_IMGMAKE_BAD_SIZE");
        return false;
    }

    // Check File Existence
    std::error_code ec;
    if (fs::exists(command_settings.filename, ec) && !command_settings.force) {
        notify_warning("SHELL_CMD_IMGMAKE_FILE_EXISTS", command_settings.filename.string().c_str());
        return false;
    }

    // Create file (truncate if exists)
    {
        FilePtr temp_fp(std::fopen(command_settings.filename.string().c_str(), "wb"));
        if (!temp_fp) {
            notify_warning("SHELL_CMD_IMGMAKE_CANNOT_WRITE", command_settings.filename.string().c_str());
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
        notify_warning("SHELL_CMD_IMGMAKE_CANNOT_WRITE", command_settings.filename.string().c_str());
        return false;
    }

    if (command_settings.no_format) {
        return true;
    }

    std::array<uint8_t, 512> buffer = {};
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
        auto start_chs = lba_to_chs(static_cast<uint64_t>(boot_sector_pos),
                                 geom.cyl,
                                 geom.heads,
                                 geom.sectors);
        std::copy(start_chs.begin(), start_chs.end(), p + 1);

        // Partition Type
        uint64_t volume_sectors = (total_size / SectorSize) -
                               static_cast<uint64_t>(boot_sector_pos);
        if (command_settings.fat_type == 32 || volume_sectors > 4194304) {
            // FAT32 LBA
            p[4] = 0x0C;
        } else if (volume_sectors > 65535) {
            // FAT16B
            p[4] = 0x06;
        } else {
            // FAT16 < 32M
            p[4] = 0x04;
        }

        // End CHS
        auto end_chs = lba_to_chs((total_size / 512) - 1,
                               geom.cyl,
                               geom.heads,
                               geom.sectors);
        std::copy(end_chs.begin(), end_chs.end(), p + 5);

        // LBA Start & Size
        host_writed(p + 8, static_cast<uint32_t>(boot_sector_pos));
        host_writed(p + 12, static_cast<uint32_t>(volume_sectors));

        // Signature
        buffer[510] = 0x55;
        buffer[511] = 0xAA;

        std::fwrite(buffer.data(), 1, buffer.size(), fs.get());
    }

    // Move to Boot Sector
    std::fseek(fs.get(), static_cast<long>(boot_sector_pos * SectorSize), SEEK_SET);
    buffer = {};

    // Determines FAT parameters
    uint64_t vol_sectors = (total_size / SectorSize) -
                           static_cast<uint64_t>(boot_sector_pos);
    int fat_bits = command_settings.fat_type;

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
    uint8_t spc = (command_settings.sectors_per_cluster > 0)
                        ? static_cast<uint8_t>(command_settings.sectors_per_cluster)
                        : 1;
    if (command_settings.sectors_per_cluster == 0) {
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
    buffer[16] = static_cast<uint8_t>(command_settings.fat_copies);

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

    // Sectors per Track
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
    std::fwrite(buffer.data(), 1, buffer.size(), fs.get());

    // Write FATs
    std::array<uint8_t, 512> empty_sect = {};

    // Position after reserved
    std::fseek(fs.get(), static_cast<long>((boot_sector_pos + reserved_sectors) * SectorSize), SEEK_SET);

    // Initialize FAT start
    std::array<uint8_t, 512> fat_header = {};

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

    for (int i = 0; i < command_settings.fat_copies; ++i) {
        auto current_fat_start = std::ftell(fs.get());
        std::fwrite(fat_header.data(), 1, fat_header.size(), fs.get());

        // Fill rest of FAT with zeros
        for (uint32_t k = 1; k < fat_size; ++k) {
            std::fwrite(empty_sect.data(), 1, empty_sect.size(), fs.get());
        }

        std::fseek(fs.get(), current_fat_start + static_cast<long>(fat_size * SectorSize), SEEK_SET);
    }

    // Write optional Root Directory Label
    if (!command_settings.label.empty()) {
        buffer = {};
        write_padded_string(buffer.data(), command_settings.label, 11);
        // Volume Label Attribute
        buffer[11] = 0x08;
        std::fwrite(buffer.data(), 1, buffer.size(), fs.get());
    }

    // File closes automatically via RAII

    program->WriteOut(MSG_Get("SHELL_CMD_IMGMAKE_CREATED"),
                      command_settings.filename.string().c_str(),
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