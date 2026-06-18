// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CONFIG_MIGRATE_FILE_WALKER_H
#define DOSBOX_CONFIG_MIGRATE_FILE_WALKER_H

#include <vector>

#include "misc/std_filesystem.h"

namespace ConfigMigrate {

// Walk `root` recursively (if a directory) or check the single file. Return
// paths that look like DOSBox config files (`.conf` extension AND a
// recognised section header in the first ~100 lines). Symlinks and hidden
// directories are skipped.
std::vector<std_fs::path> FindConfigFiles(const std_fs::path& root);

// Sniff a single file: returns true if it looks like a DOSBox config (has a
// known section header in the first ~100 lines).
bool LooksLikeDosboxConfig(const std_fs::path& path);

} // namespace ConfigMigrate

#endif
