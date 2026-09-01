// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MOUNT_H
#define DOSBOX_PROGRAM_MOUNT_H

#include "dos/dos.h"
#include "dos/programs.h"
#include "shell/command_line.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

enum class MountType {
	FloppyImage,
	HardDiskImage,
	CdRomImage,
	Directory,
	Overlay
};

enum class MountFileSystemType { Fat16, Iso, None };

// Raw geometry option strings, as given on the command line. These are
// extracted before the path arguments are processed so they are not mistaken
// for image paths, but can only be interpreted afterwards because their
// meaning depends on the auto-detected mount type.
struct GeometryArgs {
	std::optional<std::string> size     = {};
	std::optional<std::string> freesize = {};
	std::optional<std::string> chs      = {};
};

// Struct to hold all parameters required for a mount operation
struct MountParameters {
	char drive                 = '\0';
	MountType type             = MountType::Directory;
	MountFileSystemType fstype = MountFileSystemType::Fat16;

	std::vector<std::string> paths = {};

	std::string label = "";

	// Geometry: [0]=BytesPerSector, [1]=Sectors, [2]=Heads, [3]=Cylinders
	std::array<uint16_t, 4> sizes = {0, 0, 0, 0};

	bool roflag               = false;
	bool is_ide               = false;
	int8_t ide_index          = -1;
	bool is_second_cable_slot = false;

	// Defaulting to Hard Disk prevents obscure issues in games like Hyperspace 
	uint8_t mediaid           = MediaId::HardDisk;

	// 0-3 vs A-Z
	bool is_drive_number = false;

	bool is_image_mode = false;
};

class MOUNT final : public Program {
public:
	MOUNT()
	{
		AddMessages();
		help_detail = {HELP_Filter::Common,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "MOUNT"};
	}
	void ListMounts();
	void Run() override;
	bool AddWildcardPaths(const std::string& path_arg,
	                      std::vector<std::string>& paths);

	bool MountImage(MountParameters& params);

	std::optional<MountParameters> ProcessArguments(CommandLine* cmd);

private:
	static void AddMessages();
	void ShowUsage();

	bool HandleUnmount();
	bool ParseArguments(MountParameters& params, GeometryArgs& geometry,
	                    bool& explicit_fs, bool& path_relative_to_last_config);
	bool ParseGeometry(MountParameters& params, const GeometryArgs& geometry);
	bool HasUnknownOptions();
	bool ParseDrive(MountParameters& params, bool explicit_fs);

	std::string ApplyRelativePath(const std::string& path,
	                              bool is_relative_to_last_config) const;
	std::string GetDosMappedHostPath(const std::string& dos_path) const;

	void ProcessPaths(const std::string first_path, MountParameters& params,
	                  bool path_relative_to_last_config);

	bool MountPaths(MountParameters& params);

	void MountLocal(MountParameters& params, const std::string& local_path);

	bool MountImageFat(MountParameters& params);
	bool MountImageIso(const MountParameters& params);
	bool MountImageRaw(MountParameters& params);

	void WriteMountStatus(const std::string& image_type,
	                      const std::vector<std::string>& images,
	                      char drive_letter, bool readonly);
};

#endif // DOSBOX_PROGRAM_MOUNT_H