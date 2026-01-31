// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MOUNT_H
#define DOSBOX_PROGRAM_MOUNT_H

#include "dos/programs.h"
#include <array>
#include <string>
#include <vector>

// Struct to hold all parameters required for a mount operation
struct MountParameters {
	char drive                     = '\0';
	std::vector<std::string> paths = {};
	std::string type               = "dir";
	std::string fstype             = "fat";
	std::string label              = "";

	// Geometry: [0]=BytesPerSector, [1]=Sectors, [2]=Heads, [3]=Cylinders
	std::array<uint16_t, 4> sizes = {0, 0, 0, 0};

	bool roflag               = false;
	bool is_ide               = false;
	int8_t ide_index          = -1;
	bool is_second_cable_slot = false;
	uint8_t mediaid           = 0;
	// 0-3 vs A-Z
	bool is_drive_number = false;
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

private:
	static void AddMessages();
	void ShowUsage();

	bool HandleUnmount();
	void ParseArguments(MountParameters& params, bool& explicit_fs,
	                    bool& path_relative_to_last_config);
	void ParseGeometry(MountParameters& params);
	bool ParseDrive(MountParameters& params, bool explicit_fs);
	bool ProcessPaths(MountParameters& params, bool path_relative_to_last_config);
	void MountLocal(MountParameters& params, const std::string& local_path);

	bool MountImageFat(MountParameters& params);
	bool MountImageIso(MountParameters& params);
	bool MountImageRaw(MountParameters& params);

	void WriteMountStatus(const char* image_type,
	                      const std::vector<std::string>& images,
	                      char drive_letter);
};

#endif // DOSBOX_PROGRAM_MOUNT_H