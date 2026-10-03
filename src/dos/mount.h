// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DOS_MOUNT_H
#define DOSBOX_DOS_MOUNT_H

#include <optional>
#include <string>

enum class MountType {
	FloppyImage,
	HardDiskImage,
	CdRomImage,
	Directory,
	Overlay
};

std::string to_string(const MountType mount_type);

std::optional<MountType> parse_mount_type(const std::string& s);

#endif // DOSBOX_DOS_MOUNT_H
