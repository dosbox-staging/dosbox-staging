#include "dosbox.h"
#include "cross.h"
#include "utils/rom_dir.h"
#include "utils/fs_utils.h"

#if defined(WIN32)

static std::deque<std_fs::path> get_platform_rom_dirs(const char* default_rom_dir, 
                                                      const char* platform_rom_dir, 
                                                      [[maybe_unused]] const char* platform_rom_dir_mac)
{
	return {
	        GetConfigDir() / default_rom_dir,
            std_fs::path("C:\\") / platform_rom_dir
	};
}

#elif defined(MACOSX)

static std::deque<std_fs::path> get_platform_rom_dirs(const char* default_rom_dir, 
                                                      const char* platform_rom_dir, 
                                                      const char* platform_rom_dir_mac)
{
	return {
	        GetConfigDir() / default_rom_dir,
	        resolve_home("~/Library/Audio/Sounds") / platform_rom_dir_mac,
	        std_fs::path("/usr/local/share") / platform_rom_dir,
	        std_fs::path("/usr/share") / platform_rom_dir,
	};
}
#else

static std::deque<std_fs::path> get_platform_rom_dirs(const char* default_rom_dir, 
                                                      const char* platform_rom_dir, 
                                                      [[maybe_unused]] const char* platform_rom_dir_mac)
{
	// First priority is user-specific data location
	const auto xdg_data_home = get_xdg_data_home();

	std::deque<std_fs::path> dirs = {
	        xdg_data_home / "dosbox" / default_rom_dir,
	        xdg_data_home / platform_rom_dir,
	};

	// Second priority are the $XDG_DATA_DIRS
	for (const auto& data_dir : get_xdg_data_dirs()) {
		dirs.emplace_back(data_dir / platform_rom_dir);
	}

	// Third priority is $XDG_CONF_HOME, for convenience
	dirs.emplace_back(GetConfigDir() / default_rom_dir);

	return dirs;
}

#endif

std::deque<std_fs::path> get_rom_dirs(std::string const& config_path, 
                                      const char* default_rom_dir, 
                                      const char* platform_rom_dir, 
                                      const char* platform_rom_dir_mac)
{
	// Get potential ROM directories from the environment and/or system
	auto rom_dirs = get_platform_rom_dirs(default_rom_dir, platform_rom_dir, platform_rom_dir_mac);

	// Get the user's configured ROM directory; otherwise use 'mt32-roms'
	std_fs::path selected_romdir = config_path;

	if (selected_romdir.empty()) { // already trimmed
		selected_romdir = default_rom_dir;
	}

	// Make sure we search the user's configured directory first
	rom_dirs.emplace_front(resolve_home(selected_romdir.string()));
	return rom_dirs;
}