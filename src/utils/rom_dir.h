// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_ROM_DIR_H
#define DOSBOX_ROM_DIR_H

#include <deque>
#include <string>
#include "misc/std_filesystem.h"

std::deque<std_fs::path> get_rom_dirs(std::string const& config_path, 
                                      const char* default_rom_dir, 
                                      const char* platform_rom_dir, 
                                      const char* platform_rom_dir_mac);

#endif // DOSBOX_ROM_DIR_H