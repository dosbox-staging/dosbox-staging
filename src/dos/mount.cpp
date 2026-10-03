// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/mount.h"

#include <optional>
#include <string>

#include "misc/support.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

std::string to_string(const MountType mount_type)
{
	switch (mount_type) {
	case MountType::Directory: return "dir";
	case MountType::Overlay: return "overlay";
	case MountType::FloppyImage: return "floppy";
	case MountType::CdRomImage: return "iso";
	case MountType::HardDiskImage: return "hdd";
	default: assertm(false, "Invalid mount type format"); return {};
	}
}

std::optional<MountType> parse_mount_type(const std::string& s)
{
	if (iequals(s, "floppy") || iequals(s, "fdd")) {
		return MountType::FloppyImage;

	} else if (iequals(s, "hdd")) {
		return MountType::HardDiskImage;

	} else if (iequals(s, "iso") || iequals(s, "cdrom")) {
		return MountType::CdRomImage;

	} else if (iequals(s, "dir")) {
		return MountType::Directory;

	} else if (iequals(s, "overlay")) {
		return MountType::Overlay;

	} else {
		return {};
	}
}
