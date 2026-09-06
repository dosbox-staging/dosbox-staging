#include "dos/mount.h"
#include "misc/support.h"

#include <optional>
#include <string>

std::string to_string(const MountType& mount_type)
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
	if (s == "floppy" || s == "fdd") {
		return MountType::FloppyImage;

	} else if (s == "hdd") {
		return MountType::HardDiskImage;

	} else if (s == "iso" || s == "cdrom") {
		return MountType::CdRomImage;

	} else if (s == "dir") {
		return MountType::Directory;

	} else if (s == "overlay") {
		return MountType::Overlay;

	} else {
		return {};
	}
}